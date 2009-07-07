#include "main.h"

#define ACCESSCH_PORT_NAME          L"accessch_port"
#define _ALLOC_TAG                  'hcca'
#define _ACCESSCH_MAX_CONNECTIONS   1

// ----------------------------------------------------------------------------
// structs

typedef struct _GLOBALS
{
    PDRIVER_OBJECT   m_FilterDriverObject;
    PFLT_FILTER     m_Filter;
    PFLT_PORT       m_Port;
}GLOBALS, *PGLOBALS;

typedef struct _PORT_CONTEXT
{
    PFLT_PORT       m_Connection;
}PORT_CONTEXT, *PPORT_CONTEXT;

typedef struct _INSTANCE_CONTEXT
{
} INSTANCE_CONTEXT, *PINSTANCE_CONTEXT;

typedef struct _STREAM_CONTEXT
{
} STREAM_CONTEXT, *PSTREAM_CONTEXT;

typedef struct _STREAM_HANDLE_CONTEXT
{
} STREAM_HANDLE_CONTEXT, *PSTREAM_HANDLE_CONTEXT;

typedef struct _VOLUME_CONTEXT
{
} VOLUME_CONTEXT, *PVOLUME_CONTEXT;

// ----------------------------------------------------------------------------
// prototypes
NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
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

    PAGED_CODE();

    RtlZeroMemory( &Globals, sizeof( Globals) );

    Globals.m_FilterDriverObject = DriverObject;
    __try
    {
        status = FltRegisterFilter( DriverObject, ( PFLT_REGISTRATION )&filterRegistration, &Globals.m_Filter );
        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        status = PortCreate( Globals.m_Filter, &Globals.m_Port );
        if ( !NT_SUCCESS( status ) )
        {
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
                FltCloseCommunicationPort( Globals.m_Port );
            if ( Globals.m_Filter )
                FltUnregisterFilter( Globals.m_Filter );
        }
    }

    return status;
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


NTSTATUS
PortCreate (
    __in PFLT_FILTER pFilter,
    __deref_out_opt PFLT_PORT* ppPort
    )
{
    NTSTATUS status;

    PAGED_CODE();

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
    PPORT_CONTEXT pPortContext = (PPORT_CONTEXT) ConnectionCookie;

    *ReturnOutputBufferLength = 0;

    ASSERT( pPortContext != NULL );
    if ( !pPortContext )
    {
        return STATUS_INVALID_PARAMETER;
    }

    return status;
}
