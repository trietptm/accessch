//!
//    \author - Andrey Sobko (andrey.sobko@gmail.com)
//    \date - 06.07.2009
//    \description - sample driver
//!

//! \todo check necessary headers
#include "main.h"

#include "../inc/accessch.h"
#include "flt.h"
#include "eventqueue.h"
#include "commport.h"
#include "filehlp.h"

#include "flt.h"
#include "volhlp.h"

Parameters gFileCommonParams[] = {
    PARAMETER_FILE_NAME,
    PARAMETER_REQUESTOR_PROCESS_ID,
    PARAMETER_CURRENT_THREAD_ID,
    PARAMETER_LUID,
    PARAMETER_SID
};

ULONG gPreviousModeOffset = 0;

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

/* Return relative offset from begin _KTHREAD */
__checkReturn
NTSTATUS
GetPreviousModeOffset (
    )
{
#if defined (_WIN64)
#define MOVE_OFFSET 9
	if (*((ULONG*)ExGetPreviousMode) != (ULONG)0x048B4865) /* mov eax,gs:[] */
#else
#define MOVE_OFFSET 6
	if (*((USHORT*)ExGetPreviousMode) != (USHORT)0xA164) /* mov eax,fs:[] */
#endif
    {
        return STATUS_UNSUCCESSFUL;
    }

    if ( *( ( USHORT* )( &( ( char* ) ExGetPreviousMode )[ MOVE_OFFSET ] ) )
        != (USHORT) 0x808A ) /* mov al,[eax+0xXX] */
    {
        return STATUS_UNSUCCESSFUL;
    }

    gPreviousModeOffset = ( ULONG )( *( ( USHORT* )( & ( 
        ( char* ) ExGetPreviousMode ) [ MOVE_OFFSET + 2 ] ) ) );
    
    return STATUS_SUCCESS;
}

/* Switch to disable NAPI hook protection
    return previos state of MODE */
MODE
SetPreviousMode (
    MODE OperationMode
    )
{
	char *pETO = NULL;
	MODE PreviousMode;
    
    ASSERT( gPreviousModeOffset );

	pETO = (char*) PsGetCurrentThread();

	PreviousMode = (MODE) pETO[ gPreviousModeOffset ];
	pETO[ gPreviousModeOffset ] = OperationMode;

	return PreviousMode;
}

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

    EventQueue_Init();

    __try
    {
        status = GetPreviousModeOffset();
        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }


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
    EventQueue_Done();
    FltUnregisterFilter( Globals.m_Filter );
    FltDeletePushLock( &Globals.m_ClientPortLock );

    return STATUS_SUCCESS;
}


// ----------------------------------------------------------------------------
VOID
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
BOOLEAN
IsSkipPostCreate (
     __in PFLT_CALLBACK_DATA Data,
     __in PCFLT_RELATED_OBJECTS FltObjects,
     __in FLT_POST_OPERATION_FLAGS Flags
    )
{
    if ( STATUS_REPARSE == Data->IoStatus.Status )
    {
        // skip reparse op
        return TRUE;
    }

    if ( !NT_SUCCESS( Data->IoStatus.Status ) )
    {
        // skip failed op
        return TRUE;
    }
    
    if ( IsPassThrough( FltObjects, Flags ) )
    {
        // wrong state
        return TRUE;
    }

    if ( FlagOn( FltObjects->FileObject->Flags,  FO_VOLUME_OPEN ) )
    {
        // volume open
        return TRUE;
    }

    return FALSE;
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

    if ( IsSkipPostCreate( Data, FltObjects, Flags ) )
    {
        return FLT_POSTOP_FINISHED_PROCESSING;
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
        FileInterceptorContext Context( Data, FltObjects, pStreamContext );
        EventData event (
            &Context,
            FileQueryParameter,
            FileObjectRequest,
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
            if ( !FlagOn( pStreamContext->m_Flags, _STREAM_FLAGS_DIRECTORY ) )
            {
                event.EventFlagsSet( _EVENT_FLAG_IO );
            }

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
    UNREFERENCED_PARAMETER( CompletionContext );

    PSTREAM_CONTEXT pStreamContext = NULL;

    FLT_PREOP_CALLBACK_STATUS fltStatus = FLT_PREOP_SUCCESS_NO_CALLBACK;

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
        FileInterceptorContext Context( Data, FltObjects, pStreamContext );
        EventData event (
            &Context,
            FileQueryParameter,
            FileObjectRequest,
            FILE_MINIFILTER,
            IRP_MJ_CREATE,
            0,
            ARRAYSIZE( gFileCommonParams ),
            gFileCommonParams
            );

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
    }
    __finally
    {
        ReleaseContext( &pStreamContext );
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
