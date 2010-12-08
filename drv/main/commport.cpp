#include "../inc/commonkrnl.h"
#include "../inc/memmgr.h"
#include "../../inc/accessch.h"

#include "main.h"
#include "eventqueue.h"
#include "excludes.h"

#include "commport.h"

typedef struct _PORT_CONTEXT
{
    PFLT_PORT           m_Connection;
    FiltersStorage*     m_pFltStorage;
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

    __try
    {
        if ( GlobalData.m_ClientPort )
        {
            status = STATUS_ALREADY_REGISTERED;
            __leave;
        }

        pPortContext = (PPORT_CONTEXT) ExAllocatePoolWithTag (
            NonPagedPool,
            sizeof( PORT_CONTEXT ),
            'cpSA'
            );

        if ( !pPortContext )
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
        }

        RtlZeroMemory( pPortContext, sizeof( PORT_CONTEXT ) );

        pPortContext->m_Connection = ClientPort;
        pPortContext->m_pFltStorage = new (
            PagedPool,
            FiltersStorage::m_AllocTag
            ) FiltersStorage;

        if ( !pPortContext->m_pFltStorage )
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
        }

        /// \todo  revise single port connection
        GlobalData.m_FilteringSystem->Attach( pPortContext->m_pFltStorage );

        GlobalData.m_ClientPort = ClientPort;
        RegisterInvisibleProcess( PsGetCurrentProcessId() );
        ExReInitializeRundownProtection( &GlobalData.m_RefClientPort );

        *ConnectionCookie = pPortContext;
    }
    __finally
    {   
        if ( !NT_SUCCESS( status ) )
        {
            if ( pPortContext )
            {
                if ( pPortContext->m_pFltStorage )
                {
                    FREE_OBJECT( pPortContext->m_pFltStorage );
                }

                FREE_POOL( pPortContext );
            }
        }
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

    GlobalData.m_FilteringSystem->Detach( pPortContext->m_pFltStorage );

    ExWaitForRundownProtectionRelease( &GlobalData.m_RefClientPort );
    GlobalData.m_ClientPort = NULL;

    FltCloseClientPort( GlobalData.m_Filter, &pPortContext->m_Connection );

    UnregisterInvisibleProcess( PsGetCurrentProcessId() );

    ExRundownCompleted( &GlobalData.m_RefClientPort );

    FREE_OBJECT( pPortContext->m_pFltStorage );
    FREE_POOL( pPortContext );
}

void
PortPostEmptyMessages (
    __in PPORT_CONTEXT PortContext
    )
{
    ASSERT( PortContext );
    UNREFERENCED_PARAMETER( PortContext );
    
    PFLT_PORT pPort;
    NTSTATUS status = PortQueryConnected( &pPort );
    if ( !NT_SUCCESS( status ) )
    {
        return;
    }

    for( ULONG cou = 0; cou < 256; cou++ )
    {
        LARGE_INTEGER timeout = { 1, 0 };
        
        FltSendMessage (
            GlobalData.m_Filter,
            &pPort,
            NULL,
            0,
            NULL,
            NULL,
            &timeout
            );
    }

    PortRelease( pPort );
}

__checkReturn
NTSTATUS
ProceedChainGeneric (
    __in FiltersStorage* FltStorage,
    __in PCHAIN_ENTRY Entry,
    __out PULONG FilterId
    )
{
    NTSTATUS status = FltStorage->AddFilterUnsafe (
        Entry->m_Filter->m_Interceptor,
        Entry->m_Filter->m_OperationId,
        Entry->m_Filter->m_FunctionMi,
        Entry->m_Filter->m_OperationType,
        Entry->m_Filter->m_GroupId,
        Entry->m_Filter->m_Verdict,
        UlongToHandle( Entry->m_Filter->m_CleanupProcessId ),
        Entry->m_Filter->m_RequestTimeout,
        Entry->m_Filter->m_WishMask,
        Entry->m_Filter->m_ParamsCount,
        Entry->m_Filter->m_Params,
        FilterId
        );

    return status;
}

__checkReturn
NTSTATUS
ProceedChainBox (
    __in FiltersStorage* FltStorage,
    __in PCHAIN_ENTRY pEntry,
    __out PULONG FilterId
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PFLTBOX pBox = pEntry[0].m_Box;

    switch ( pBox->m_Operation )
    {
    case _fltbox_add:
        status = FltStorage->CreateBoxUnsafe (
            &pBox->m_Guid,
            pBox->Items.m_ParamsCount,
            pBox->Items.m_Params,
            FilterId
            );
        
        break;

    default:
        __debugbreak();
    }

    return status;
}

__checkReturn
NTSTATUS
ProceedChain (
    __in FiltersStorage* FltStorage,
    __in PFILTERS_CHAIN Chain,
    __out_opt PULONG FilterId
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    ASSERT( ARGUMENT_PRESENT( Chain ) );

    if ( Chain->m_Count != 1 )
    {
        __debugbreak();     // поправить возвращаемые типы
        return STATUS_NOT_SUPPORTED;
    }

    FltStorage->Lock();

    PCHAIN_ENTRY pEntry = Chain->m_Entry;

    for ( ULONG item = 0; item < Chain->m_Count; item++ )
    {
        switch( pEntry->m_Operation )
        {
        case _fltchain_add:
            status = ProceedChainGeneric( FltStorage, pEntry, FilterId );
            break;

        case _fltchain_del:
            __debugbreak();

            break;

        case _fltbox_create:
            status = ProceedChainBox( FltStorage, pEntry, FilterId );

            break;
        }
    }

    FltStorage->UnLock();

    return status;
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

    *ReturnOutputBufferLength = 0;

    PPORT_CONTEXT pPortContext = (PPORT_CONTEXT) ConnectionCookie;

    status = PortQueryConnected( 0 );
    if ( !NT_SUCCESS( status ) )
    {
        return STATUS_INVALID_PARAMETER;
    }

    QueuedItem *pItem = NULL;
    EventData *pEvent = NULL;

    __try
    {
        ASSERT( pPortContext != NULL );
        if (
            !pPortContext
            ||
            !InputBuffer
            ||
            InputBufferSize < sizeof( NOTIFY_COMMAND)
            )
        {
            status = STATUS_INVALID_PARAMETER;
            __leave;
        }

        PNOTIFY_COMMAND pCommand = (PNOTIFY_COMMAND) InputBuffer;
        switch( pCommand->m_Command )
        {
        case ntfcom_Activate:
            status =  pPortContext->m_pFltStorage->ChangeState( TRUE );
            if ( !NT_SUCCESS( status ) )
            {
                // nct
            }
            break;

        case ntfcom_Pause:
            status = pPortContext->m_pFltStorage->ChangeState( FALSE );
            if ( !NT_SUCCESS( status ) )
            {
                // nct
            }

            PortPostEmptyMessages( pPortContext );
            break;

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
                if ( !pItem )
                {
                    ASSERT( pItem );
                    status = STATUS_UNSUCCESSFUL;
                    break;
                }

                pEvent = (EventData*) pItem->GetData();
                NC_IOPREPARE prepare;
                ULONG preparesize = sizeof( prepare );

                status = pEvent->ObjectRequest (
                    ntfcom_PrepareIO,
                    &prepare,
                    &preparesize
                    );

                if ( NT_SUCCESS( status ) )
                {
                    *(PNC_IOPREPARE) OutputBuffer = prepare;
                    *ReturnOutputBufferLength = sizeof( NC_IOPREPARE );
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

                ULONG FilterId;

                status = ProceedChain (
                    pPortContext->m_pFltStorage,
                    pChain,
                    &FilterId
                    );

                if ( NT_SUCCESS( status ) )
                {
                    if ( OutputBuffer && OutputBufferSize == sizeof( ULONG ) )
                    {
                        *(PULONG) OutputBuffer = FilterId;
                        *ReturnOutputBufferLength = sizeof( ULONG );
                    }
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

        PortRelease( 0 );
    }

    return status;
}

__checkReturn
NTSTATUS
PortQueryConnected (
    __drv_when(return==0, __deref_out_opt __drv_valueIs(!=0)) PFLT_PORT* Port
    )
{
    if ( !ExAcquireRundownProtection( &GlobalData.m_RefClientPort ) )
    {
        return STATUS_UNSUCCESSFUL;
    }

    if ( Port )
    {
        *Port = GlobalData.m_ClientPort;
        ASSERT( *Port );
    }
   
    return STATUS_SUCCESS;
}

void
PortRelease (
    __in_opt PFLT_PORT Port
    )
{
    UNREFERENCED_PARAMETER( Port );

    ExReleaseRundownProtection( &GlobalData.m_RefClientPort );
}

__checkReturn
NTSTATUS
PortAllocateMessage (
    __in EventData *Event,
    __in QueuedItem* QueuedItem,
    __drv_when(return==0, __deref_out_opt __drv_valueIs(!=0)) PVOID* Message,
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

    // calculate parameters size
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
            messageSize += FIELD_OFFSET (
                EVENT_PARAMETER, Value.m_Data
                ) + datasize;
        }
    }

    // calculate aggregation info size

    messageSize += Event->m_Aggregator.GetCount() * sizeof( EVENT_PARAMETER );

    // end calculating

    if ( DRV_EVENT_CONTENT_SIZE < messageSize )
    {
        ASSERT( !MessageSize );
     
        return STATUS_NOT_SUPPORTED;
    }

    pMsg = (PMESSAGE_DATA) ExAllocatePoolWithTag (
        PagedPool,
        messageSize,
        'gmSA'
        );

    if ( !pMsg )
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    pMsg->m_EventId = QueuedItem->GetId();
    
    pMsg->m_InterceptorId = ( Interceptors ) Event->GetInterceptorId();
    pMsg->m_OperationId = ( DriverOperationId ) Event->GetOperationId();
    pMsg->m_FuncionMi = Event->GetMinor();
    pMsg->m_OperationType = ( OperationPoint ) Event->GetOperationType();

    pMsg->m_ParametersCount = params2user;
    
    // place data
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
            parameter->Value.m_Id = (Parameters) cou;
            parameter->Value.m_Size = datasize;
            RtlCopyMemory( parameter->Value.m_Data, data, datasize );

            parameter = (PEVENT_PARAMETER) Add2Ptr (
                parameter,
                FIELD_OFFSET( EVENT_PARAMETER, Value.m_Data ) + datasize
                );
        }
    }

    // place aggregation info
    pMsg->m_AggregationInfoCount = Event->m_Aggregator.GetCount();

    for ( ULONG cou = 0; cou < pMsg->m_AggregationInfoCount; cou++ )
    {
        parameter->Aggregator.m_FilterId = Event->m_Aggregator.GetFilterId( cou );
        parameter->Aggregator.m_Verdict = Event->m_Aggregator.GetVerdict( cou );

        parameter = (PEVENT_PARAMETER) Add2Ptr (
            parameter,
            sizeof( EVENT_PARAMETER)
            );
    }

    *Message = pMsg;
    *MessageSize = messageSize;

    return STATUS_SUCCESS;
}

void
PortReleaseMessage (
    __in_opt PVOID Message
    )
{
    if ( !Message )
    {
        return;
    }

    FREE_POOL( Message );
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

    // \todo fill result verdict
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
            GlobalData.m_Filter,
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
        else
        {
            *Verdict = ReplyResult.m_Flags;
        }
    }
    __finally
    {
        if ( pQueuedItem )
        {
            pQueuedItem->WaitAndDestroy();
        }

        PortRelease( pPort );
        PortReleaseMessage( pMessage );
    }

    return STATUS_SUCCESS;
}
