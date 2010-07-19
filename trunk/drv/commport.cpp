#include "pch.h"
#include "../inc/accessch.h"

#include "main.h"
#include "eventqueue.h"
#include "flt.h"
#include "excludes.h"

#include "commport.h"

typedef struct _PORT_CONTEXT
{
    PFLT_PORT           m_Connection;
}PORT_CONTEXT, *PPORT_CONTEXT;

// ----------------------------------------------------------------------------
// communications

__checkReturn
NTSTATUS
PortCreate (
    __in PFLT_FILTER Filter,
    __deref_out_opt PFLT_PORT* Port
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
            Filter,
            Port,
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
            *Port = NULL;
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
            status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
        }

        RtlZeroMemory( pPortContext, sizeof( PORT_CONTEXT ) );

        pPortContext->m_Connection = ClientPort;

        /// \todo  revise single port connection
        Globals.m_ClientPort = ClientPort;
        RegisterInvisibleProcess( PsGetCurrentProcessId() );

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

    RemoveAllFilters();

    FltAcquirePushLockExclusive( &Globals.m_ClientPortLock );
    Globals.m_ClientPort = NULL;
    FltReleasePushLock( &Globals.m_ClientPortLock );

    FltCloseClientPort( Globals.m_Filter, &pPortContext->m_Connection );

    UnregisterInvisibleProcess( PsGetCurrentProcessId() );

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
                status = QueuedItem::Lookup (
                    pCommand->m_EventId,
                    &pItem
                    );
            }

            if ( NT_SUCCESS( status ) )
            {
                pEvent = (EventData*) pItem->GetData();
                NC_IOPREPARE prepare;
                ULONG preparesize = sizeof( prepare );

                status = pEvent->ObjectRequst (
                    ntfcom_PrepareIO,
                    &prepare,
                    &preparesize
                    );

                if ( NT_SUCCESS( status ) )
                {
                    *(PNC_IOPREPARE) OutputBuffer = prepare;
                }
            }
            break;
        
        case  ntfcom_FiltersChain:
            {
                PFILTERS_CHAIN pChain = (PFILTERS_CHAIN) pCommand->m_Data;
                ULONG size = InputBufferSize - FIELD_OFFSET (
                    NOTIFY_COMMAND,
                    m_Data
                    );

                status = FilterProceedChain( pChain, size );
                if ( NT_SUCCESS( status ) )
                {
                    /// \todo prepare output data;
                }
            }
            break;
        
        default:
            ASSERT( FALSE );
            break;
        }
    }
    __finally
    {
        if ( AbnormalTermination() )
        {
            status = STATUS_UNHANDLED_EXCEPTION;
        }

        if ( pItem )
        {
            pItem->Release();
            pItem = NULL;
        }
    }

    return status;
}

__checkReturn
NTSTATUS
PortQueryConnected (
    __deref_out_opt PFLT_PORT* Port
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    FltAcquirePushLockShared( &Globals.m_ClientPortLock );
    if ( Globals.m_ClientPort)
    {
        *Port = Globals.m_ClientPort;
        status = STATUS_SUCCESS;
    }

    FltReleasePushLock( &Globals.m_ClientPortLock );

    return status;
}

void
PortRelease (
    __deref_in PFLT_PORT* Port
    )
{
    if ( *Port )
        *Port = NULL;
}

__checkReturn
NTSTATUS
PortAllocateMessage (
    __in EventData *Event,
    __in QueuedItem* QueuedItem,
    __deref_out_opt PVOID* Message,
    __out_opt PULONG MessageSize,
    __in PARAMS_MASK ParamsMask
    )
{
    ASSERT( ARGUMENT_PRESENT( Event ) );
    ASSERT( ARGUMENT_PRESENT( QueuedItem ) );
    ASSERT( ARGUMENT_PRESENT( ParamsMask ) );
    
    NTSTATUS status;

    PMESSAGE_DATA pMsg;
    ULONG messageSize = FIELD_OFFSET( MESSAGE_DATA, m_Parameters );

    PVOID data;
    ULONG datasize;
    ULONG params2user = 0;
    for ( ULONG cou = 0; cou < _PARAMS_COUNT; cou++ )
    {
        if ( FlagOn( ParamsMask, (PARAMS_MASK) 1 << cou ) )
        {
            status = Event->QueryParameter (
                (Parameters) cou,
                &data,
                &datasize
                );
            
            if ( !NT_SUCCESS( status ) )
            {
                ClearFlag( ParamsMask, (PARAMS_MASK) 1 << cou );
                continue;
            }

            params2user++;
            messageSize += FIELD_OFFSET( EVENT_PARAMETER, m_Data ) + datasize;
        }
    }

    if ( DRV_EVENT_CONTENT_SIZE < messageSize )
    {
        ASSERT( !MessageSize );
        return STATUS_NOT_SUPPORTED;
    }

    pMsg = (PMESSAGE_DATA) ExAllocatePoolWithTag (
        PagedPool,
        messageSize,
        _ALLOC_TAG
        );

    if ( !pMsg )
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    pMsg->m_EventId = QueuedItem->GetId();
    
    pMsg->m_Interceptor = Event->GetInterceptorId();
    pMsg->m_OperationId = Event->GetOperationId();
    pMsg->m_FuncionMi = Event->GetMinor();
    pMsg->m_OperationType = Event->GetOperationType();

    pMsg->m_ParametersCount = params2user;
    
    PEVENT_PARAMETER parameter = pMsg->m_Parameters;
    for ( ULONG cou = 0; cou < _PARAMS_COUNT; cou++ )
    {
        if ( FlagOn( ParamsMask, (PARAMS_MASK) 1 << cou ) )
        {
            status = Event->QueryParameter (
                (Parameters) cou,
                &data,
                &datasize
                );

            ASSERT( NT_SUCCESS ( status ) );
            parameter->m_Id = (Parameters) cou;
            parameter->m_Size = datasize;
            RtlCopyMemory( parameter->m_Data, data, datasize );

            parameter = (PEVENT_PARAMETER) Add2Ptr (
                parameter,
                FIELD_OFFSET( EVENT_PARAMETER, m_Data ) + datasize
                );
        }
    }

    *Message = pMsg;
    *MessageSize = messageSize;

    return STATUS_SUCCESS;
}

void
PortReleaseMessage (
    __deref_in PVOID* Message
    )
{
    if ( !*Message )
    {
        return;
    }

    FREE_POOL( *Message );
}

__checkReturn
NTSTATUS
PortAskUser (
    __in EventData *Event,
    __in PARAMS_MASK ParamsMask,
    __inout VERDICT* Verdict
    )
{
    NTSTATUS status;
    PFLT_PORT pPort = NULL;
    PVOID pMessage = NULL;
    QueuedItem* pQueuedItem = NULL;

    ASSERT( ARGUMENT_PRESENT( ParamsMask ) );

    // \toso fill result verdict
    UNREFERENCED_PARAMETER( Verdict );

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
        
        status = QueuedItem::Add( Event, &pQueuedItem );
        if ( !NT_SUCCESS( status ) )
        {
            pQueuedItem = NULL;
            __leave;
        }

        status = PortAllocateMessage (
            Event,
            pQueuedItem,
            &pMessage,
            &MessageSize,
            ParamsMask
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
        if ( pQueuedItem )
        {
            pQueuedItem->WaitAndDestroy();
        }

        PortRelease( &pPort );
        PortReleaseMessage( &pMessage );
    }

    return STATUS_SUCCESS;
}
