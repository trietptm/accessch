#include "pch.h"
#include "../inc/accessch.h"

#include "main.h"
#include "eventqueue.h"
#include "flt.h"

#include "commport.h"

typedef struct _PORT_CONTEXT
{
    PFLT_PORT               m_Connection;
}PORT_CONTEXT, *PPORT_CONTEXT;

// ----------------------------------------------------------------------------
// communications

__checkReturn
NTSTATUS
PortCreate (
    __in PFLT_FILTER pFilter,
    __deref_out_opt PFLT_PORT* ppPort
    )
{
    NTSTATUS status;

    PSECURITY_DESCRIPTOR sd;
    status = FltBuildDefaultSecurityDescriptor( &sd, FLT_PORT_ALL_ACCESS );

    if ( NT_SUCCESS( status ) )
    {
        UNICODE_STRING usName;
        OBJECT_ATTRIBUTES oa;

        RtlInitUnicodeString( &usName, ACCESSCH_PORT_NAME );
        InitializeObjectAttributes (
            &oa,
            &usName,
            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
            NULL,
            sd
            );

        status = FltCreateCommunicationPort (
            pFilter,
            ppPort,
            &oa,
            NULL,
            PortConnect,
            PortDisconnect,
            PortMessageNotify,
            ACCESSCH_MAX_CONNECTIONS
            );

        FltFreeSecurityDescriptor( sd );

        if ( !NT_SUCCESS( status ) )
        {
            *ppPort = NULL;
        }
    }

    return status;
}

__checkReturn
NTSTATUS
PortConnect (
    __in PFLT_PORT ClientPort,
    __in_opt PVOID ServerPortCookie,
    __in_bcount_opt(SizeOfContext) PVOID ConnectionContext,
    __in ULONG SizeOfContext,
    __deref_out_opt PVOID *ConnectionCookie
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PPORT_CONTEXT pPortContext = NULL;

    UNREFERENCED_PARAMETER( ServerPortCookie );
    UNREFERENCED_PARAMETER( ConnectionContext );
    UNREFERENCED_PARAMETER( SizeOfContext );

    FltAcquirePushLockExclusive( &Globals.m_ClientPortLock );
    __try
    {
        if ( Globals.m_ClientPort )
        {
            status = STATUS_ALREADY_REGISTERED;
            __leave;
        }

        pPortContext = (PPORT_CONTEXT) ExAllocatePoolWithTag (
            NonPagedPool,
            sizeof( PORT_CONTEXT ),
            _ALLOC_TAG
            );

        if ( !pPortContext )
        {
            status = STATUS_NO_MEMORY;
            __leave;
        }

        RtlZeroMemory( pPortContext, sizeof( PORT_CONTEXT ) );

        pPortContext->m_Connection = ClientPort;

        //! \todo  revise single port connection
        Globals.m_ClientPort = ClientPort;

        *ConnectionCookie = pPortContext;
    }
    __finally
    {
        FltReleasePushLock( &Globals.m_ClientPortLock );
    }

    return status;
}

void
PortDisconnect (
    __in PVOID ConnectionCookie
    )
{
    PPORT_CONTEXT pPortContext = (PPORT_CONTEXT) ConnectionCookie;

    ASSERT( ARGUMENT_PRESENT( pPortContext ) );

    FltAcquirePushLockExclusive( &Globals.m_ClientPortLock );
    Globals.m_ClientPort = NULL;
    FltReleasePushLock( &Globals.m_ClientPortLock );

    FltCloseClientPort( Globals.m_Filter, &pPortContext->m_Connection );

    FREE_POOL( pPortContext );
}

__checkReturn
NTSTATUS
PortMessageNotify (
    __in PVOID ConnectionCookie,
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer,
    __in ULONG InputBufferSize,
    __out_bcount_part_opt(OutputBufferSize,*ReturnOutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferSize,
    __out PULONG ReturnOutputBufferLength
    )
{
    NTSTATUS status = STATUS_INVALID_PARAMETER;

    UNREFERENCED_PARAMETER( InputBuffer );
    UNREFERENCED_PARAMETER( InputBufferSize );
    UNREFERENCED_PARAMETER( OutputBuffer );
    UNREFERENCED_PARAMETER( OutputBufferSize );

    PPORT_CONTEXT pPortContext = (PPORT_CONTEXT) ConnectionCookie;

    QueuedItem *pItem = NULL;
    EventData *pEvent = NULL;

    *ReturnOutputBufferLength = 0;

    //! \todo: remove
    __debugbreak();

    ASSERT( pPortContext != NULL );
    if (
        !pPortContext
        ||
        !InputBuffer
        ||
        InputBufferSize < sizeof( NOTIFY_COMMAND)
        )
    {
        return STATUS_INVALID_PARAMETER;
    }
    __try
    {
        PNOTIFY_COMMAND pCommand = (PNOTIFY_COMMAND) InputBuffer;
        switch( pCommand->m_Command )
        {
        case ntfcom_PrepareIO:
            status = STATUS_INVALID_PARAMETER;
            if ( OutputBuffer && OutputBufferSize >= sizeof(NC_IOPREPARE) )
            {
                status = EventQueue_Lookup (
                    pCommand->m_EventId,
                    &pItem,
                    (PVOID*) &pEvent
                    );
            }

            if ( NT_SUCCESS( status ) )
            {
                NC_IOPREPARE prepare;

                status = pEvent->ObjectRequst( ntfcom_PrepareIO, &prepare );

                if ( NT_SUCCESS( status ) )
                {
                    *(PNC_IOPREPARE) OutputBuffer = prepare;
                }
            }
            break;
        
        default:
            ASSERT( FALSE );
            break;
        }
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        status = GetExceptionCode();
    }

    if ( pItem )
    {
        pItem->Release();
    }

    return status;
}

__checkReturn
NTSTATUS
PortQueryConnected (
    __deref_out_opt PFLT_PORT* ppPort
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    FltAcquirePushLockShared( &Globals.m_ClientPortLock );
    if ( Globals.m_ClientPort)
    {
        *ppPort = Globals.m_ClientPort;
        status = STATUS_SUCCESS;
    }

    FltReleasePushLock( &Globals.m_ClientPortLock );

    return status;
}

void
PortRelease (
    __deref_in PFLT_PORT* ppPort
    )
{
    if ( *ppPort )
        *ppPort = NULL;
}

__checkReturn
NTSTATUS
PortAllocateMessage (
    __in EventData *Event,
    __in QueuedItem* QueuedItem,
    __deref_out_opt PVOID* ppMessage,
    __out_opt PULONG pMessageSize
    )
{
    ASSERT( ARGUMENT_PRESENT( Event ) );
    ASSERT( ARGUMENT_PRESENT( QueuedItem ) );
    
    NTSTATUS status;

    PMESSAGE_DATA pMsg;
    ULONG MessageSize = FIELD_OFFSET( MESSAGE_DATA, m_Parameters );

    ULONG count = Event->GetParametersCount();

    PVOID data;
    ULONG datasize;
    Parameters parameterId;
    for ( ULONG cou = 0; cou < count; cou++ )
    {
        parameterId = Event->GetParameterId( cou );
        status = Event->QueryParameter (
            parameterId,
            &data,
            &datasize
            );
        if ( !NT_SUCCESS( status ) )
        {
            return status;
        }

        MessageSize += FIELD_OFFSET( SINGLE_PARAMETER, m_Data ) + datasize;
    }

    if ( DRV_EVENT_CONTENT_SIZE < MessageSize )
    {
        ASSERT( !MessageSize );
        return STATUS_NOT_SUPPORTED;
    }

    pMsg = (PMESSAGE_DATA) ExAllocatePoolWithTag (
        PagedPool,
        MessageSize,
        _ALLOC_TAG
        );

    if ( !pMsg )
    {
        return STATUS_NO_MEMORY;
    }

    pMsg->m_EventId = QueuedItem->GetId();
    pMsg->m_ParametersCount = count;
    
    PSINGLE_PARAMETER parameter = pMsg->m_Parameters;
    for ( ULONG cou = 0; cou < count; cou++ )
    {
        parameterId = Event->GetParameterId( cou );
        status = Event->QueryParameter (
            parameterId,
            &data,
            &datasize
            );

        ASSERT( NT_SUCCESS ( status ) );
        parameter->m_Id = parameterId;
        parameter->m_Size = datasize;
        RtlCopyMemory( parameter->m_Data, data, datasize );

        parameter = (PSINGLE_PARAMETER) Add2Ptr (
            parameter,
            FIELD_OFFSET( SINGLE_PARAMETER, m_Data ) + datasize
            );
    }

    *ppMessage = pMsg;
    *pMessageSize = MessageSize;

    return STATUS_SUCCESS;
}

void
PortReleaseMessage (
    __deref_in PVOID* ppMessage
    )
{
    if ( !*ppMessage )
        return;

    FREE_POOL( *ppMessage );
}

__checkReturn
NTSTATUS
PortAskUser (
    __in EventData *Event
    )
{
    NTSTATUS status;
    PFLT_PORT pPort = NULL;
    PVOID pMessage = NULL;
    QueuedItem* pQueuedItem = NULL;

    __try
    {
        status = PortQueryConnected( &pPort );
        if ( !NT_SUCCESS( status ) )
        {
            pPort = NULL;
            __leave;
        }

        // send data to R3
        REPLY_RESULT ReplyResult;
        ULONG ReplyLength = sizeof( ReplyResult );
        ULONG MessageSize = 0;
        
        status = EventQueue_Add( Event, &pQueuedItem );
        if ( NT_SUCCESS( pQueuedItem ) )
        {
            pQueuedItem = NULL;
            __leave;
        }

        status = PortAllocateMessage (
            Event,
            pQueuedItem,
            &pMessage,
            &MessageSize
            );

        if ( !NT_SUCCESS( status ) )
        {
            pMessage = NULL;
            __leave;
        }

        status = FltSendMessage (
            Globals.m_Filter,
            &pPort,
            pMessage,
            MessageSize,
            &ReplyResult,
            &ReplyLength,
            NULL
            );

        if ( !NT_SUCCESS( status ) || ReplyLength != sizeof( ReplyResult) )
        {
            RtlZeroMemory( &ReplyResult, sizeof( ReplyResult) );
        }
    }
    __finally
    {
        EventQueue_WaitAndDestroy( &pQueuedItem );
        PortRelease( &pPort );
        PortReleaseMessage( &pMessage );
    }

    return STATUS_SUCCESS;
}
