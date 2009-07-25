// author - Andrey Sobko (andrey.sobko@gmail.com)
// date - 06.07.2009
// description - sample driver

#include "main.h"

#define ACCESSCH_PORT_NAME          L"accessch_port"
#define _ALLOC_TAG                  'hcca'
#define _ACCESSCH_MAX_CONNECTIONS   1

// ----------------------------------------------------------------------------
// structs

typedef struct _GLOBALS
{
    PDRIVER_OBJECT          m_FilterDriverObject;
    PFLT_FILTER             m_Filter;
    PFLT_PORT               m_Port;
}GLOBALS, *PGLOBALS;

typedef struct _PORT_CONTEXT
{
    PFLT_PORT               m_Connection;
}PORT_CONTEXT, *PPORT_CONTEXT;

typedef struct _INSTANCE_CONTEXT
{
    DEVICE_TYPE             m_VolumeDeviceType;
    FLT_FILESYSTEM_TYPE     m_VolumeFilesystemType;
} INSTANCE_CONTEXT, *PINSTANCE_CONTEXT;

typedef struct _STREAM_CONTEXT
{
    PFLT_FILE_NAME_INFORMATION m_pFileNameInfo;
} STREAM_CONTEXT, *PSTREAM_CONTEXT;

typedef struct _STREAM_HANDLE_CONTEXT
{
} STREAM_HANDLE_CONTEXT, *PSTREAM_HANDLE_CONTEXT;

typedef struct _VOLUME_CONTEXT
{
    PFLT_INSTANCE           m_Instance;
} VOLUME_CONTEXT, *PVOLUME_CONTEXT;

// ----------------------------------------------------------------------------
// prototypes
extern "C"
{
NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    );
}

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
GLOBALS Globals;

const FLT_CONTEXT_REGISTRATION ContextRegistration[] = {
    { FLT_INSTANCE_CONTEXT,      0, ContextCleanup,  sizeof(INSTANCE_CONTEXT),       _ALLOC_TAG, NULL, NULL, NULL },
    { FLT_STREAM_CONTEXT,        0, ContextCleanup,  sizeof(STREAM_CONTEXT),         _ALLOC_TAG, NULL, NULL, NULL },
    { FLT_STREAMHANDLE_CONTEXT,  0, ContextCleanup,  sizeof(STREAM_HANDLE_CONTEXT),  _ALLOC_TAG, NULL, NULL, NULL },
    { FLT_VOLUME_CONTEXT,        0, NULL,            sizeof(VOLUME_CONTEXT),         _ALLOC_TAG, NULL, NULL, NULL} ,
    { FLT_CONTEXT_END }
};

#define _NO_PAGING  FLTFL_OPERATION_REGISTRATION_SKIP_PAGING_IO

// dont remove PreCreate - self defence (open+truncate will leave zero size file)
// w2k server will hang if IRP_MJ_FILE_SYSTEM_CONTROL intercepted - look at driver ver. 8.2 function Serv_CheckCallbacks
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
    NULL,                                            //
    InstanceSetup,                                   // InstanceSetup
    NULL,                                            // InstanceQueryTeardown
    NULL,                                            // InstanceTeardownStart
    NULL,                                            // InstanceTeardownComplete
    NULL, NULL,                                      // NameProvider callbacks
    NULL,
#if FLT_MGR_LONGHORN
    NULL,                                            // transaction callback for Vista
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
    __try
    {
        status = FltRegisterFilter( DriverObject, ( PFLT_REGISTRATION )&filterRegistration, &Globals.m_Filter );
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

// ----------------------------------------------------------------------------
void
ReleaseFileNameInfo (
    __in_opt PFLT_FILE_NAME_INFORMATION* ppFileNameInfo
    )
{
    if ( ppFileNameInfo )
    {
        FltReleaseFileNameInformation( *ppFileNameInfo );
        *ppFileNameInfo = NULL;
    };
}

FORCEINLINE
void
ReleaseContext (
    __in_opt PFLT_CONTEXT* ppContext
    )
{
    if ( !*ppContext )
        return;

    FltReleaseContext( *ppContext );
    *ppContext = NULL;
}

void
ContextCleanup (
    __in PVOID Pool,
    __in FLT_CONTEXT_TYPE ContextType
    )
{
     switch (ContextType)
    {
    case FLT_INSTANCE_CONTEXT:
        {
        }
        break;

    case FLT_STREAM_CONTEXT:
        {
            PSTREAM_CONTEXT pStreamContext = (PSTREAM_CONTEXT) Pool;
            ASSERT ( pStreamContext );
            ReleaseFileNameInfo( &pStreamContext->m_pFileNameInfo );
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
    
        RtlInitUnicodeString ( &usName, ACCESSCH_PORT_NAME );
        InitializeObjectAttributes( &oa, &usName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, sd );

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

NTSTATUS
PortConnect (
    __in PFLT_PORT ClientPort,
    __in_opt PVOID ServerPortCookie,
    __in_bcount_opt(SizeOfContext) PVOID ConnectionContext,
    __in ULONG SizeOfContext,
    __deref_out_opt PVOID *ConnectionCookie
    )
{
    PPORT_CONTEXT pPortContext = NULL;

    UNREFERENCED_PARAMETER( ServerPortCookie );
    UNREFERENCED_PARAMETER( ConnectionContext );
    UNREFERENCED_PARAMETER( SizeOfContext );

    pPortContext = (PPORT_CONTEXT) ExAllocatePoolWithTag( NonPagedPool, sizeof(PORT_CONTEXT), _ALLOC_TAG );
    if ( !pPortContext )
    {
        return STATUS_NO_MEMORY;
    }
    
    RtlZeroMemory( pPortContext, sizeof(PORT_CONTEXT) ); 

    pPortContext->m_Connection = ClientPort;

    *ConnectionCookie = pPortContext;

    return STATUS_SUCCESS;
}

void
PortDisconnect (
    __in PVOID ConnectionCookie
    )
{
    PPORT_CONTEXT pPortContext = (PPORT_CONTEXT) ConnectionCookie;

    ASSERT( pPortContext != NULL );

    FltCloseClientPort( Globals.m_Filter, &pPortContext->m_Connection );
    ExFreePool( pPortContext );
}

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

NTSTATUS
PortAskUser (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects
    )
{
    UNREFERENCED_PARAMETER( Data );

    NTSTATUS status;
    PINSTANCE_CONTEXT pInstanceContext = NULL;
    PSTREAM_CONTEXT pStreamContext = NULL;

    __try
    {
        status = FltGetInstanceContext( FltObjects->Instance, (PFLT_CONTEXT*) &pInstanceContext );
        if ( !NT_SUCCESS( status ) )
        {
            pInstanceContext = NULL;
            __leave;
        }

        status = FltGetStreamContext( FltObjects->Instance, FltObjects->FileObject, (PFLT_CONTEXT*) &pStreamContext );
        if ( !NT_SUCCESS( status ) )
        {
            pStreamContext = NULL;
            __leave;
        }
    }
    __finally
    {
        ReleaseContext( (PFLT_CONTEXT*) &pInstanceContext );
        ReleaseContext( (PFLT_CONTEXT*) &pStreamContext );
    }

    return STATUS_SUCCESS;
}

// ----------------------------------------------------------------------------
// file

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
            sizeof(VOLUME_CONTEXT),
            NonPagedPool,
            (PFLT_CONTEXT*) &pVolumeContext
            );
        
        if ( !NT_SUCCESS( status ) )
        {
            pVolumeContext = NULL;
            __leave;
        }

        // just for fun
        pInstanceContext->m_VolumeDeviceType = VolumeDeviceType;
        pInstanceContext->m_VolumeFilesystemType = VolumeFilesystemType;

        ASSERT( VolumeDeviceType != FILE_DEVICE_NETWORK_FILE_SYSTEM );

        status = FltSetInstanceContext (
            FltObjects->Instance,
            FLT_SET_CONTEXT_KEEP_IF_EXISTS,
            pInstanceContext,
            NULL
            );

           pVolumeContext->m_Instance = FltObjects->Instance;
        status = FltSetVolumeContext( FltObjects->Volume, FLT_SET_CONTEXT_KEEP_IF_EXISTS, pVolumeContext, NULL );
    }
    __finally
    {
        ReleaseContext( (PFLT_CONTEXT*) &pInstanceContext );
        ReleaseContext( (PFLT_CONTEXT*) &pVolumeContext );
    }

    return STATUS_SUCCESS;
}

__checkReturn
NTSTATUS
GenerateFileNameInfo (
    __in PFLT_CALLBACK_DATA Data,
    __deref_out_opt PFLT_FILE_NAME_INFORMATION* ppFileNameInfo
    )
{
    ULONG QueryNameFlags = FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT;
    NTSTATUS status = FltGetFileNameInformation( Data, QueryNameFlags, ppFileNameInfo );
    if ( !NT_SUCCESS( status ) )
    {
        ppFileNameInfo = NULL;
       
        return status;
    }

    status = FltParseFileNameInformation( *ppFileNameInfo );

    ASSERT( !NT_SUCCESS( status ) );
    
    return STATUS_SUCCESS;
}

__checkReturn
NTSTATUS
GenerateStreamContext (
    __in PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PSTREAM_CONTEXT* ppStreamContext
    )
{
    NTSTATUS status;
    PFLT_FILE_NAME_INFORMATION pFileNameInfo = NULL;

    if ( !FsRtlSupportsPerStreamContexts( FltObjects->FileObject ) )
        return STATUS_NOT_SUPPORTED;;

    status = FltGetStreamContext( FltObjects->Instance, FltObjects->FileObject, (PFLT_CONTEXT*) ppStreamContext );
    
    if ( NT_SUCCESS( status ) )
        return status;

    status = FltAllocateContext (
        Globals.m_Filter,
        FLT_STREAM_CONTEXT,
        sizeof(STREAM_CONTEXT),
        NonPagedPool,
        (PFLT_CONTEXT*) ppStreamContext
        );

    if ( NT_SUCCESS( status ) )
    {
        //todo: stream context func
        RtlZeroMemory( *ppStreamContext, sizeof(STREAM_CONTEXT) );

        status = GenerateFileNameInfo( Data, &pFileNameInfo );
        if ( NT_SUCCESS( status ) )
        {
            (*ppStreamContext)->m_pFileNameInfo = pFileNameInfo;

            status = FltSetStreamContext (
                FltObjects->Instance,
                FltObjects->FileObject,
                FLT_SET_CONTEXT_REPLACE_IF_EXISTS,
                *ppStreamContext,
                NULL
                );
        }

        if ( !NT_SUCCESS( status ) )
            ReleaseContext( (PFLT_CONTEXT*) ppStreamContext );
    }

    return status;
}

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
 
    if ( IsPassThrough( FltObjects, Flags ) )
    {
        // wrong state
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

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
    
    __try
    {
        status = GenerateStreamContext( Data, FltObjects, &pStreamContext );
        if ( !NT_SUCCESS( status ) )
        {
            pStreamContext = NULL;
            __leave;
        }

        PortAskUser( Data, FltObjects );
    }
    __finally
    {
        ReleaseContext( (PFLT_CONTEXT*) &pStreamContext );
    }
    
    return fltStatus;
}

FLT_PREOP_CALLBACK_STATUS
PreCleanup (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __out PVOID *CompletionContext
    )
{
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( CompletionContext );

    FLT_PREOP_CALLBACK_STATUS fltStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;
    NTSTATUS status = PortAskUser( Data, FltObjects );

    return fltStatus;
}

FLT_POSTOP_CALLBACK_STATUS
PostWrite (
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVOID CompletionContext,
    __in FLT_POST_OPERATION_FLAGS Flags
    )
{
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( FltObjects );
    UNREFERENCED_PARAMETER( CompletionContext );
    UNREFERENCED_PARAMETER( Flags );

    FLT_POSTOP_CALLBACK_STATUS fltStatus = FLT_POSTOP_FINISHED_PROCESSING;

    return fltStatus;
}