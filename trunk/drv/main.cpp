//!
//    \author - Andrey Sobko (andrey.sobko@gmail.com)
//    \date - 06.07.2009
//    \description - sample driver
//!

//! \todo check necessary headers
#include "main.h"
#include "filehlp.h"

#include <specstrings.h>

#include "../inc/accessch.h"
#include "flt.h"
#include "volhlp.h"

#define _ACCESSCH_MAX_CONNECTIONS   1

Parameters gFileCommonParams[] = {
    PARAMETER_FILE_NAME,
    PARAMETER_REQUESTOR_PROCESS_ID,
    PARAMETER_CURRENT_THREAD_ID,
    PARAMETER_LUID,
    PARAMETER_SID
};

// prototypes
extern "C"
{
NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    );
}

NTSTATUS
Unload (
    __in FLT_FILTER_UNLOAD_FLAGS Flags
    );

void
ContextCleanup (
    __in PVOID Pool,
    __in FLT_CONTEXT_TYPE ContextType
    );

NTSTATUS
PortCreate (
    __in PFLT_FILTER pFilter,
    __deref_out_opt PFLT_PORT* ppPort
    );

NTSTATUS
PortConnect (
    __in PFLT_PORT ClientPort,
    __in_opt PVOID ServerPortCookie,
    __in_bcount_opt(SizeOfContext) PVOID ConnectionContext,
    __in ULONG SizeOfContext,
    __deref_out_opt PVOID *ConnectionCookie
    );

void
PortDisconnect (
    __in PVOID ConnectionCookie
    );

NTSTATUS
PortMessageNotify (
    __in PVOID ConnectionCookie,
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer,
    __in ULONG InputBufferSize,
    __out_bcount_part_opt(OutputBufferSize,*ReturnOutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferSize,
    __out PULONG ReturnOutputBufferLength
    );

NTSTATUS
InstanceSetup (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_SETUP_FLAGS Flags,
    __in DEVICE_TYPE VolumeDeviceType,
    __in FLT_FILESYSTEM_TYPE VolumeFilesystemType
    );

FLT_POSTOP_CALLBACK_STATUS
PostCreate (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVOID CompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );

FLT_PREOP_CALLBACK_STATUS
PreCleanup (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __out PVOID *CompletionContext
    );

FLT_POSTOP_CALLBACK_STATUS
PostWrite (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVOID CompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    );

// ----------------------------------------------------------------------------
// end init block

//////////////////////////////////////////////////////////////////////////
GLOBALS Globals;

const FLT_CONTEXT_REGISTRATION ContextRegistration[] = {
    { FLT_INSTANCE_CONTEXT,      0, ContextCleanup,  sizeof(INSTANCE_CONTEXT),       _ALLOC_TAG, NULL, NULL, NULL },
    { FLT_STREAM_CONTEXT,        0, ContextCleanup,  sizeof(STREAM_CONTEXT),         _ALLOC_TAG, NULL, NULL, NULL },
    { FLT_STREAMHANDLE_CONTEXT,  0, ContextCleanup,  sizeof(STREAM_HANDLE_CONTEXT),  _ALLOC_TAG, NULL, NULL, NULL },
    { FLT_VOLUME_CONTEXT,        0, NULL,            sizeof(VOLUME_CONTEXT),         _ALLOC_TAG, NULL, NULL, NULL} ,
    { FLT_CONTEXT_END }
};

#define _NO_PAGING  FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO

CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    { IRP_MJ_CREATE,    0,          NULL,           PostCreate },
    { IRP_MJ_CLEANUP,   0,          PreCleanup,     NULL },
    { IRP_MJ_WRITE,     _NO_PAGING, NULL,           PostWrite },
    { IRP_MJ_OPERATION_END}
};

FLT_REGISTRATION filterRegistration = {
    sizeof( FLT_REGISTRATION ),                      // Size
    FLT_REGISTRATION_VERSION,                        // Version
    FLTFL_REGISTRATION_DO_NOT_SUPPORT_SERVICE_STOP,  // Flags
    ContextRegistration,                             // Context
    Callbacks,                                       // Operation callbacks
    Unload,                                            //
    InstanceSetup,                                   // InstanceSetup
    NULL,                                            // InstanceQueryTeardown
    NULL,                                            // InstanceTeardownStart
    NULL,                                            // InstanceTeardownComplete
    NULL, NULL,                                      // NameProvider callbacks
    NULL,
#if FLT_MGR_LONGHORN
    NULL,                                            // transaction callback
    NULL                                             //
#endif //FLT_MGR_LONGHORN
};

NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    UNREFERENCED_PARAMETER( RegistryPath );

    RtlZeroMemory( &Globals, sizeof( Globals) );

    Globals.m_FilterDriverObject = DriverObject;
    FltInitializePushLock( &Globals.m_ClientPortLock );
    __try
    {
        status = FltRegisterFilter (
            DriverObject,
            (PFLT_REGISTRATION) &filterRegistration,
            &Globals.m_Filter
            );

        if ( !NT_SUCCESS( status ) )
        {
            Globals.m_Filter = NULL;
            __leave;
        }

        status = PortCreate( Globals.m_Filter, &Globals.m_Port );
        if ( !NT_SUCCESS( status ) )
        {
            Globals.m_Port = NULL;
            __leave;
        }

        status = FltStartFiltering( Globals.m_Filter );
        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        ASSERT( NT_SUCCESS( status ) );
    }
    __finally
    {
        if ( !NT_SUCCESS( status ) )
        {
            if ( Globals.m_Port )
            {
                FltCloseCommunicationPort( Globals.m_Port );
            }

            if ( Globals.m_Filter )
            {
                FltUnregisterFilter( Globals.m_Filter );
            }
        }
    }

    return status;
}

__checkReturn
NTSTATUS
Unload (
    __in FLT_FILTER_UNLOAD_FLAGS Flags
    )
{
    if ( !FlagOn(Flags, FLTFL_FILTER_UNLOAD_MANDATORY) )
    {
        //! \todo checks during Unload
        //return STATUS_FLT_DO_NOT_DETACH;
    }

    FltCloseCommunicationPort( Globals.m_Port );
    FltUnregisterFilter( Globals.m_Filter );
    FltDeletePushLock( &Globals.m_ClientPortLock );

    return STATUS_SUCCESS;
}


// ----------------------------------------------------------------------------

void
ContextCleanup (
    __in PVOID Pool,
    __in FLT_CONTEXT_TYPE ContextType
    )
{
    switch ( ContextType )
    {
    case FLT_INSTANCE_CONTEXT:
        {
        }
        break;

    case FLT_STREAM_CONTEXT:
        {
            PSTREAM_CONTEXT pStreamContext = (PSTREAM_CONTEXT) Pool;
            ReleaseContext( &pStreamContext->m_InstanceContext );
            ASSERT ( pStreamContext );
        }
        break;

    case FLT_STREAMHANDLE_CONTEXT:
        {
        }
        break;

    default:
        {
            ASSERT( "cleanup for unknown context!" );
        }
        break;
    }
}

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
            _ACCESSCH_MAX_CONNECTIONS
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

    ExFreePool( pPortContext );
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

    *ReturnOutputBufferLength = 0;

    ASSERT( pPortContext != NULL );
    if ( !pPortContext )
    {
        return STATUS_INVALID_PARAMETER;
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

// ----------------------------------------------------------------------------
// file

__checkReturn
FORCEINLINE
BOOLEAN
IsPassThrough (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{
    if ( FlagOn( Flags, FLTFL_POST_OPERATION_DRAINING ) )
    {
        return TRUE;
    }

    if ( !FltObjects->Instance )
    {
        return TRUE;
    }

    if ( !FltObjects->FileObject )
    {
        return TRUE;
    }

    if ( FlagOn( FltObjects->FileObject->Flags, FO_NAMED_PIPE ) )
    {
        return TRUE;
    }

    PIRP pTopLevelIrp = IoGetTopLevelIrp();
    if ( pTopLevelIrp )
    {
        return TRUE;
    }

    return FALSE;
}

__checkReturn
NTSTATUS
InstanceSetup (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in FLT_INSTANCE_SETUP_FLAGS Flags,
    __in DEVICE_TYPE VolumeDeviceType,
    __in FLT_FILESYSTEM_TYPE VolumeFilesystemType
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PINSTANCE_CONTEXT pInstanceContext = NULL;
    PVOLUME_CONTEXT pVolumeContext = NULL;

    UNREFERENCED_PARAMETER( Flags );

    ASSERT( FltObjects->Filter == Globals.m_Filter );

    if (FLT_FSTYPE_RAW == VolumeFilesystemType)
    {
        return STATUS_FLT_DO_NOT_ATTACH;
    }

    if ( FILE_DEVICE_NETWORK_FILE_SYSTEM == VolumeDeviceType )
    {
        return STATUS_FLT_DO_NOT_ATTACH;
    }

    __try
    {
        status = FltAllocateContext (
            Globals.m_Filter,
            FLT_INSTANCE_CONTEXT,
            sizeof( INSTANCE_CONTEXT ),
            NonPagedPool,
            (PFLT_CONTEXT*) &pInstanceContext
            );

        if ( !NT_SUCCESS( status ) )
        {
            pInstanceContext = NULL;
            __leave;
        }

        RtlZeroMemory( pInstanceContext, sizeof( INSTANCE_CONTEXT ) );

        status = FltAllocateContext (
            Globals.m_Filter,
            FLT_VOLUME_CONTEXT,
            sizeof( VOLUME_CONTEXT ),
            NonPagedPool,
            (PFLT_CONTEXT*) &pVolumeContext
            );

        if ( !NT_SUCCESS( status ) )
        {
            pVolumeContext = NULL;
            __leave;
        }
        RtlZeroMemory( pVolumeContext, sizeof( VOLUME_CONTEXT ) );

        // just for fun
        pInstanceContext->m_VolumeDeviceType = VolumeDeviceType;
        pInstanceContext->m_VolumeFilesystemType = VolumeFilesystemType;

        status = FillVolumeProperties( FltObjects, pVolumeContext );
        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        ASSERT( VolumeDeviceType != FILE_DEVICE_NETWORK_FILE_SYSTEM );

        status = FltSetInstanceContext (
            FltObjects->Instance,
            FLT_SET_CONTEXT_KEEP_IF_EXISTS,
            pInstanceContext,
            NULL
            );

        pVolumeContext->m_Instance = FltObjects->Instance;
        status = FltSetVolumeContext (
            FltObjects->Volume,
            FLT_SET_CONTEXT_KEEP_IF_EXISTS,
            pVolumeContext,
            NULL
            );
    }
    __finally
    {
        ReleaseContext( &pInstanceContext );
        ReleaseContext( &pVolumeContext );
    }

    return STATUS_SUCCESS;
}

__checkReturn
NTSTATUS
PortAllocateMessage (
    __in EventData *Event,
    __deref_out_opt PVOID* ppMessage,
    __out_opt PULONG pMessageSize
    )
{
    ASSERT( Event );
    
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

    ExFreePool( *ppMessage );
    *ppMessage = NULL;
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

        status = PortAllocateMessage (
            Event,
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
        PortRelease( &pPort );
        PortReleaseMessage( &pMessage );
    }

    return STATUS_SUCCESS;
}

__checkReturn
FLT_POSTOP_CALLBACK_STATUS
PostCreate (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVOID CompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( CompletionContext );

    FLT_POSTOP_CALLBACK_STATUS fltStatus = FLT_POSTOP_FINISHED_PROCESSING;
    NTSTATUS status;

    PSTREAM_CONTEXT pStreamContext = NULL;

    if ( STATUS_REPARSE == Data->IoStatus.Status )
    {
        // skip reparse op
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if ( !NT_SUCCESS( Data->IoStatus.Status ) )
    {
        // skip failed op
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if ( IsPassThrough( FltObjects, Flags ) )
    {
        // wrong state
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    if ( FlagOn( FltObjects->FileObject->Flags,  FO_VOLUME_OPEN ) )
    {
        // volume open
    }

    __try
    {
        status = GenerateStreamContext (
            Globals.m_Filter,
            FltObjects,
            &pStreamContext
            );

        if ( !NT_SUCCESS( status ) )
        {
            pStreamContext = NULL;
            __leave;
        }

        VERDICT Verdict = VERDICT_NOT_FILTERED;
        FileInterceptorContext Context( Data, FltObjects );
        EventData event (
            &Context,
            QueryFileParameter,
            FILE_MINIFILTER,
            IRP_MJ_CLEANUP,
            0,
            ARRAYSIZE( gFileCommonParams ),
            gFileCommonParams
            );

        status = FilterEvent( &event, &Verdict );

        if ( NT_SUCCESS( status )
            &&
            FlagOn( Verdict, VERDICT_ASK ) )
        {
            PortAskUser( &event );
        }
    }
    __finally
    {
        ReleaseContext( &pStreamContext );
    }

    return fltStatus;
}

__checkReturn
FLT_PREOP_CALLBACK_STATUS
PreCleanup (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __out PVOID *CompletionContext
    )
{
    NTSTATUS status;
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( CompletionContext );

    FLT_PREOP_CALLBACK_STATUS fltStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

    VERDICT Verdict = VERDICT_NOT_FILTERED;
    FileInterceptorContext Context( Data, FltObjects );
    EventData event (
        &Context,
        QueryFileParameter,
        FILE_MINIFILTER,
        IRP_MJ_CREATE,
        0,
        ARRAYSIZE( gFileCommonParams ),
        gFileCommonParams
        );;

    status = FilterEvent( &event, &Verdict );

    if ( NT_SUCCESS( status )
        &&
        FlagOn( Verdict, VERDICT_ASK ) )
    {
        status = PortAskUser( &event );
        if ( NT_SUCCESS( status ) )
        {
            //! \todo PreCleanup( AskUser complete )
        }
    }

    return fltStatus;
}

__checkReturn
FLT_POSTOP_CALLBACK_STATUS
PostWrite (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVOID CompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );
    UNREFERENCED_PARAMETER( Flags );

    FLT_POSTOP_CALLBACK_STATUS fltStatus = FLT_POSTOP_FINISHED_PROCESSING;

    if ( !NT_SUCCESS( Data->IoStatus.Status ) )
    {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    return fltStatus;
}
