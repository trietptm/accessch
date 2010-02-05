//!
//    \author - Andrey Sobko (andrey.sobko@gmail.com)
//    \date - 06.07.2009
//    \description - sample driver
//!

//! \todo check nessesary headers
#include "main.h"

#include "ntddscsi.h"
#include "ata.h"
#include "scsi.h"
#include "ntddcdrm.h"
#include "ntddvol.h"

#include "../inc/accessch.h"

#define _ALLOC_TAG                  'hcca'
#define _ACCESSCH_MAX_CONNECTIONS   1

// ----------------------------------------------------------------------------
//

typedef struct _GLOBALS
{
    PDRIVER_OBJECT          m_FilterDriverObject;
    PFLT_FILTER             m_Filter;
    PFLT_PORT               m_Port;
    EX_PUSH_LOCK            m_ClientPortLock;
    PFLT_PORT               m_ClientPort;
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
    LUID                    m_Luid;
    LONG                    m_Flags;
} STREAM_CONTEXT, *PSTREAM_CONTEXT;

typedef struct _STREAM_HANDLE_CONTEXT
{
} STREAM_HANDLE_CONTEXT, *PSTREAM_HANDLE_CONTEXT;

#define _VOLUME_DESCRIPTION_LENGTH  0x20
typedef struct _VOLUME_CONTEXT
{
    PFLT_INSTANCE           m_Instance;
    STORAGE_BUS_TYPE        m_BusType;
    DEVICE_REMOVAL_POLICY   m_RemovablePolicy;
    UCHAR                   m_VendorId[_VOLUME_DESCRIPTION_LENGTH];
    UCHAR                   m_ProductId[_VOLUME_DESCRIPTION_LENGTH];
    UCHAR                   m_ProductRevisionLevel[_VOLUME_DESCRIPTION_LENGTH];
    UCHAR                   m_VendorSpecific[_VOLUME_DESCRIPTION_LENGTH];
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
        //! \todo check
        //return STATUS_FLT_DO_NOT_DETACH;
    }

    FltCloseCommunicationPort( Globals.m_Port );
    FltUnregisterFilter( Globals.m_Filter );
    FltDeletePushLock( &Globals.m_ClientPortLock );

    return STATUS_SUCCESS;
}


// ----------------------------------------------------------------------------
void
ReleaseFileNameInfo (
    __in_opt PFLT_FILE_NAME_INFORMATION* ppFileNameInfo
    )
{
    ASSERT( ppFileNameInfo );

    if ( *ppFileNameInfo )
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
    {
        return;
    }

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

        //! \todo
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

    //! \todo
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

NTSTATUS
QueryDeviceProperty (
    __in PDEVICE_OBJECT pDevice,
    __in DEVICE_REGISTRY_PROPERTY DevProperty,
    __out PVOID* ppBuffer,
    __out PULONG pResultLenght
    )
{
    ASSERT( ARGUMENT_PRESENT( pDevice ) );
    NTSTATUS status;
    PVOID pBuffer = NULL;
    ULONG BufferSize = 0;

    status = IoGetDeviceProperty (
        pDevice,
        DevProperty,
        BufferSize,
        NULL,
        &BufferSize
        );

    if ( NT_SUCCESS( status ) )
    {
        // no intresting data
        return STATUS_NOT_SUPPORTED;
    }

    while ( STATUS_BUFFER_TOO_SMALL == status)
    {
        pBuffer = ExAllocatePoolWithTag( PagedPool, BufferSize, _ALLOC_TAG );
        if ( !pBuffer )
        {
            return STATUS_NO_MEMORY;
        }

        status = IoGetDeviceProperty (
            pDevice,
            DevProperty,
            BufferSize,
            pBuffer,
            &BufferSize
            );

        if ( NT_SUCCESS( status ) )
        {
            *ppBuffer = pBuffer;
            *pResultLenght = BufferSize;
            return status;
        }

        ExFreePool( pBuffer );
        pBuffer = NULL;
    }

    ASSERT( !pBuffer );

    return status;
}

NTSTATUS
GetAtaDiskSignature (
    __in PDEVICE_OBJECT pDevice
    )
{
    PIRP Irp;
    KEVENT Event;
    NTSTATUS status;
    IO_STATUS_BLOCK Iosb;

    UCHAR Buffer[sizeof( ATA_PASS_THROUGH_EX ) + sizeof( IDENTIFY_DEVICE_DATA )];
    PATA_PASS_THROUGH_EX ataData = (PATA_PASS_THROUGH_EX) Buffer;

    RtlZeroMemory( Buffer, sizeof( Buffer ) );

    ataData->Length = sizeof( ATA_PASS_THROUGH_EX );
    ataData->DataBufferOffset = sizeof( ATA_PASS_THROUGH_EX );
    ataData->DataTransferLength = sizeof( IDENTIFY_DEVICE_DATA );
    ataData->AtaFlags = ATA_FLAGS_DATA_IN;
    ataData->TimeOutValue = 2;
    ataData->CurrentTaskFile[6] = 0xEC;

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    __try
    {
        Irp = IoBuildDeviceIoControlRequest (
            IOCTL_ATA_PASS_THROUGH,
            pDevice,
            Buffer,
            sizeof( Buffer ),
            Buffer,
            sizeof( Buffer ),
            FALSE,
            &Event,
            &Iosb
            );

        if ( !Irp )
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
        }
        else
        {
            status = IoCallDriver( pDevice, Irp );
            if ( STATUS_PENDING == status )
            {
                KeWaitForSingleObject (
                    &Event,
                    Executive,
                    KernelMode,
                    FALSE,
                    NULL
                    );

                status = Iosb.Status;
            }

            if ( !NT_SUCCESS ( status ) )
            {
                __leave;
            }

            PVOID ptrtmp = Add2Ptr( Buffer, sizeof( ATA_PASS_THROUGH_EX ) );
            PIDENTIFY_DEVICE_DATA identifyData = (PIDENTIFY_DEVICE_DATA) ptrtmp;

            __debugbreak();
        }
    }
    __finally
    {
    }

    return status;
}

__checkReturn
NTSTATUS
GetStorageProperty (
    __in PDEVICE_OBJECT pDevice,
    __in PVOLUME_CONTEXT pVolumeContext
    )
{
    PIRP Irp;
    KEVENT Event;
    NTSTATUS status;
    IO_STATUS_BLOCK Iosb;
    STORAGE_PROPERTY_QUERY PropQuery;
    PVOID QueryBuffer = NULL;
    ULONG QuerySize = 0x2000;

    __try
    {
        QueryBuffer = ExAllocatePoolWithTag( PagedPool, QuerySize, _ALLOC_TAG );
        if ( !QueryBuffer )
        {
            __leave;
        }

        memset( &PropQuery, 0, sizeof( PropQuery ) );
        memset( QueryBuffer, 0, QuerySize );
        PropQuery.PropertyId = StorageDeviceProperty;
        PropQuery.QueryType = PropertyStandardQuery;

        KeInitializeEvent( &Event, NotificationEvent, FALSE );

        Irp = IoBuildDeviceIoControlRequest (
            IOCTL_STORAGE_QUERY_PROPERTY,
            pDevice,
            &PropQuery,
            sizeof( PropQuery ),
            QueryBuffer,
            QuerySize,
            FALSE,
            &Event, 
            &Iosb
            );

        if ( !Irp )
        {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        status = IoCallDriver( pDevice, Irp );

        if ( STATUS_PENDING == status )
        {
            KeWaitForSingleObject (
                &Event,
                Executive,
                KernelMode,
                FALSE,
                (PLARGE_INTEGER) NULL
                );

            status = Iosb.Status;
        }

        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        if ( !Iosb.Information )
        {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        PSTORAGE_DEVICE_DESCRIPTOR pDesc = 
            (PSTORAGE_DEVICE_DESCRIPTOR) QueryBuffer;
        
        pVolumeContext->m_BusType = pDesc->BusType;

        if (
            BusTypeAtapi == pDesc->BusType
            ||
            BusTypeAta == pDesc->BusType
            )
        {
            status = GetAtaDiskSignature( pDevice );
        }

    }
    __finally
    {
        if ( QueryBuffer )
        {
            ExFreePool( QueryBuffer );
            QueryBuffer = NULL;
        }
    }

    return status;
}

NTSTATUS
GetSCSIInfo (
    __in PDEVICE_OBJECT pDevice,
    __in PVOLUME_CONTEXT pVolumeContext
    )
{
    PIRP Irp;
    KEVENT Event;
    NTSTATUS status;
    IO_STATUS_BLOCK Iosb;

    UCHAR Buffer[sizeof( SCSI_PASS_THROUGH ) + 24 + sizeof( INQUIRYDATA )];
    PSCSI_PASS_THROUGH scsiData = (PSCSI_PASS_THROUGH) Buffer;

    RtlZeroMemory( Buffer, sizeof( Buffer ) );

    scsiData->Length = sizeof( SCSI_PASS_THROUGH );
    scsiData->SenseInfoLength = 24; //if set 32 - will not filled by device
    scsiData->CdbLength = CDB6GENERIC_LENGTH;
    scsiData->DataIn = SCSI_IOCTL_DATA_IN; 
    scsiData->DataTransferLength = sizeof( INQUIRYDATA ); 
    scsiData->TimeOutValue = 10; //sec
    scsiData->DataBufferOffset = scsiData->Length + scsiData->SenseInfoLength;
    scsiData->SenseInfoOffset = scsiData->Length;

    scsiData->CdbLength = CDB6GENERIC_LENGTH;
    CDB::_CDB6INQUIRY* pInquiry = (CDB::_CDB6INQUIRY*) scsiData->Cdb;
    pInquiry->OperationCode = SCSIOP_INQUIRY;
    pInquiry->AllocationLength = sizeof( INQUIRYDATA );

    /*CDB::_CDB6INQUIRY3* pInquiry = (CDB::_CDB6INQUIRY3*) scsiData->Cdb;
    pInquiry->OperationCode = SCSIOP_INQUIRY;
    pInquiry->EnableVitalProductData = CDB_INQUIRY_EVPD;
    pInquiry->PageCode = 0x80;
    pInquiry->AllocationLength = sizeof( INQUIRYDATA );*/

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    PVOID PtrInquiry = Add2Ptr( scsiData, scsiData->DataBufferOffset );

    __try
    {
        Irp = IoBuildDeviceIoControlRequest (
            IOCTL_SCSI_PASS_THROUGH,
            pDevice,
            Buffer,
            sizeof( Buffer ),
            Buffer,
            sizeof( Buffer ),
            FALSE,
            &Event,
            &Iosb
            );

        if ( !Irp )
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
        }
        else
        {
            status = IoCallDriver( pDevice, Irp );
            if ( STATUS_PENDING == status )
            {
                KeWaitForSingleObject (
                    &Event,
                    Executive,
                    KernelMode,
                    FALSE,
                    NULL
                    );

                status = Iosb.Status;
            }

            if ( !NT_SUCCESS ( status ) )
            {
                __leave;
            }

            PINQUIRYDATA pInquiryData = (PINQUIRYDATA) PtrInquiry;

            NTSTATUS status_strcopy;
            status_strcopy = RtlStringCbCopyNW (
                (NTSTRSAFE_PWSTR) pVolumeContext->m_ProductId,
                sizeof( pVolumeContext->m_ProductId ),
                (NTSTRSAFE_PWSTR) pInquiryData->ProductId,
                sizeof( pInquiryData->ProductId)
                );
            ASSERT( NT_SUCCESS( status_strcopy ) );

            status_strcopy = RtlStringCbCopyNW (
                (NTSTRSAFE_PWSTR) pVolumeContext->m_VendorId,
                sizeof( pVolumeContext->m_VendorId ),
                (NTSTRSAFE_PWSTR) pInquiryData->VendorId,
                sizeof( pInquiryData->VendorId )
                );
            ASSERT( NT_SUCCESS( status_strcopy ) );

            status_strcopy = RtlStringCbCopyNW (
                (NTSTRSAFE_PWSTR) pVolumeContext->m_ProductRevisionLevel,
                sizeof( pVolumeContext->m_ProductRevisionLevel ),
                (NTSTRSAFE_PWSTR) pInquiryData->ProductRevisionLevel,
                sizeof( pInquiryData->ProductRevisionLevel )
                );
            ASSERT( NT_SUCCESS( status_strcopy ) );

            status_strcopy = RtlStringCbCopyNW (
                (NTSTRSAFE_PWSTR) pVolumeContext->m_VendorSpecific,
                sizeof( pVolumeContext->m_VendorSpecific ),
                (NTSTRSAFE_PWSTR) pInquiryData->VendorSpecific,
                sizeof( pInquiryData->VendorSpecific )
                );
            ASSERT( NT_SUCCESS( status_strcopy ) );
            //! \todo correct VendorSpecific buffer - replace 0 by ' '
        }
    }
    __finally
    {
    }

    return status;
}

__checkReturn
NTSTATUS
GetSmartInfo (
    __in PDEVICE_OBJECT pDevice
    )
{
    PIRP Irp;
    KEVENT Event;
    NTSTATUS status;
    IO_STATUS_BLOCK Iosb;
    PVOID QueryBuffer = NULL;
    ULONG QuerySize = 0x2000;

    __try
    {
        QueryBuffer = ExAllocatePoolWithTag( PagedPool, QuerySize, _ALLOC_TAG );
        if ( !QueryBuffer )
        {
            __leave;
        }

        memset( QueryBuffer, 0, QuerySize );
        KeInitializeEvent( &Event, NotificationEvent, FALSE );

        Irp = IoBuildDeviceIoControlRequest (
            IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
            pDevice,
            NULL,
            0,
            QueryBuffer,
            QuerySize,
            FALSE,
            &Event, 
            &Iosb
            );

        if ( !Irp )
        {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        status = IoCallDriver( pDevice, Irp );

        if ( STATUS_PENDING == status )
        {
            KeWaitForSingleObject (
                &Event,
                Executive,
                KernelMode,
                FALSE,
                (PLARGE_INTEGER) NULL
                );

            status = Iosb.Status;
        }

        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        if ( !Iosb.Information )
        {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        PVOLUME_DISK_EXTENTS pvdExtents = (PVOLUME_DISK_EXTENTS) QueryBuffer;
        __debugbreak();
    }
    __finally
    {
        if ( QueryBuffer )
        {
            ExFreePool( QueryBuffer );
            QueryBuffer = NULL;
        }
    }

    return status;

}

__checkReturn
NTSTATUS
GetMediaATIP (
    __in PDEVICE_OBJECT pDevice
    )
{
    PIRP Irp;
    KEVENT Event;
    NTSTATUS status;
    IO_STATUS_BLOCK Iosb;
    PVOID QueryBuffer = NULL;
    ULONG QuerySize = 0x2000;
    
    __try
    {
        QueryBuffer = ExAllocatePoolWithTag( PagedPool, QuerySize, _ALLOC_TAG );
        if ( !QueryBuffer )
        {
            __leave;
        }

        memset( QueryBuffer, 0, QuerySize );
        KeInitializeEvent( &Event, NotificationEvent, FALSE );

        CDROM_READ_TOC_EX ReadTocEx;
        RtlZeroMemory( &ReadTocEx, sizeof( ReadTocEx ) );
        ReadTocEx.Format = CDROM_READ_TOC_EX_FORMAT_ATIP;
        ReadTocEx.Msf = 1;

        Irp = IoBuildDeviceIoControlRequest (
            IOCTL_CDROM_READ_TOC_EX,
            pDevice,
            &ReadTocEx,
            sizeof( ReadTocEx ),
            QueryBuffer,
            QuerySize,
            FALSE,
            &Event, 
            &Iosb
            );
        
        if ( !Irp )
        {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }
        
        status = IoCallDriver( pDevice, Irp );
        
        if ( STATUS_PENDING == status )
        {
            KeWaitForSingleObject (
                &Event,
                Executive,
                KernelMode,
                FALSE,
                (PLARGE_INTEGER) NULL
                );

            status = Iosb.Status;
        }
        
        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        if ( !Iosb.Information )
        {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        PCDROM_TOC_ATIP_DATA pAtipData = (PCDROM_TOC_ATIP_DATA) QueryBuffer;
        PCDROM_TOC_ATIP_DATA_BLOCK pAtipBlock = pAtipData->Descriptors;
     }
    __finally
    {
        if ( QueryBuffer )
        {
            ExFreePool( QueryBuffer );
            QueryBuffer = NULL;
        }
    }
    
    return status;
}


NTSTATUS
GetMediaSerialNumber (
    __in PDEVICE_OBJECT pDevice
    )
{
    PIRP Irp;
    KEVENT Event;
    NTSTATUS status;
    IO_STATUS_BLOCK Iosb;
    PVOID QueryBuffer = NULL;
    ULONG QuerySize = 0x2000;

    __try
    {
        PSTORAGE_MEDIA_SERIAL_NUMBER_DATA pSerialNumber = NULL;

        QueryBuffer = ExAllocatePoolWithTag( PagedPool, QuerySize, _ALLOC_TAG );
        if ( !QueryBuffer )
        {
            __leave;
        }

        memset( QueryBuffer, 0, QuerySize );

        KeInitializeEvent( &Event, NotificationEvent, FALSE );

        Irp = IoBuildDeviceIoControlRequest (
            IOCTL_STORAGE_GET_MEDIA_SERIAL_NUMBER,
            pDevice,
            NULL,
            0,
            QueryBuffer,
            QuerySize,
            FALSE,
            &Event, 
            &Iosb
            );

        if ( !Irp )
        {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        status = IoCallDriver( pDevice, Irp );

        if ( STATUS_PENDING == status )
        {
            KeWaitForSingleObject (
                &Event,
                Executive,
                KernelMode,
                FALSE,
                (PLARGE_INTEGER) NULL
                );

            status = Iosb.Status;
        }

        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        if ( !Iosb.Information )
        {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        pSerialNumber = (PSTORAGE_MEDIA_SERIAL_NUMBER_DATA) QueryBuffer;
        __debugbreak();

    }
    __finally
    {
        if ( QueryBuffer )
        {
            ExFreePool( QueryBuffer );
            QueryBuffer = NULL;
        }
    }

    return status;
}

__checkReturn
NTSTATUS
FillVolumeProperties (
     __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVOLUME_CONTEXT pVolumeContext
    )
{
    ASSERT( ARGUMENT_PRESENT( pVolumeContext ) );

    NTSTATUS status;
    PDEVICE_OBJECT pDevice = NULL;

    __debugbreak();

    // changer - GetProductData
    // IDENTIFY_DEVICE_DATA - ata\atapi

    __try
    {
        status = FltGetDiskDeviceObject( FltObjects->Volume, &pDevice );
        if ( !NT_SUCCESS( status ) )
        {
            pDevice = NULL;
            __leave;
        }

        status = GetStorageProperty( pDevice, pVolumeContext );
        ASSERT( NT_SUCCESS( status ) );

        status = GetSCSIInfo( pDevice, pVolumeContext );
        ASSERT( NT_SUCCESS( status ) );

        status = GetSmartInfo( pDevice );

        // for CD\DVD only
        status = GetMediaATIP( pDevice );
        status = GetMediaSerialNumber( pDevice );

        PVOID pBuffer = NULL;
        ULONG PropertySize;

        status = QueryDeviceProperty (
            pDevice,
            DevicePropertyRemovalPolicy,
            &pBuffer,
            &PropertySize
            );

        if ( NT_SUCCESS( status ) )
        {
            PDEVICE_REMOVAL_POLICY pRemovalPolicy =
                (PDEVICE_REMOVAL_POLICY) pBuffer;
            
            pVolumeContext->m_RemovablePolicy = *pRemovalPolicy;

            ExFreePool( pBuffer );
            pBuffer = NULL;
        }

        /*status = QueryDeviceProperty( pDevice,
        DevicePropertyDeviceDescription, &pBuffer, &PropertySize );
        if ( NT_SUCCESS( status ) )
        {
            ExFreePool( pBuffer );
            pBuffer = NULL;
        }

        status = QueryDeviceProperty( pDevice,
        DevicePropertyHardwareID, &pBuffer, &PropertySize );
        if ( NT_SUCCESS( status ) )
        {
            ExFreePool( pBuffer );
            pBuffer = NULL;
        }*/
        
        status = STATUS_SUCCESS;
    }
    __finally
    {
        if ( pDevice )
        {
            ObDereferenceObject( pDevice );
        }
    }

    return status;
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
        ReleaseContext( (PFLT_CONTEXT*) &pInstanceContext );
        ReleaseContext( (PFLT_CONTEXT*) &pVolumeContext );
    }

    return STATUS_SUCCESS;
}

__checkReturn
NTSTATUS
QueryFileNameInfo (
    __in PFLT_CALLBACK_DATA Data,
    __deref_out_opt PFLT_FILE_NAME_INFORMATION* ppFileNameInfo
    )
{
    ULONG QueryNameFlags = FLT_FILE_NAME_NORMALIZED
        | FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP;

    NTSTATUS status = FltGetFileNameInformation (
        Data,
        QueryNameFlags,
        ppFileNameInfo
        );

    if ( !NT_SUCCESS( status ) )
    {
        return status;
    }

    status = FltParseFileNameInformation( *ppFileNameInfo );

    ASSERT( NT_SUCCESS( status ) ); //ignore unsuccessful parse

    return STATUS_SUCCESS;
}

__checkReturn
NTSTATUS
GenerateStreamContext (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PSTREAM_CONTEXT* ppStreamContext
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    if ( !FsRtlSupportsPerStreamContexts( FltObjects->FileObject ) )
        return STATUS_NOT_SUPPORTED;;

    status = FltGetStreamContext (
        FltObjects->Instance,
        FltObjects->FileObject,
        (PFLT_CONTEXT*) ppStreamContext
        );

    if ( NT_SUCCESS( status ) )
        return status;

    status = FltAllocateContext (
        Globals.m_Filter,
        FLT_STREAM_CONTEXT,
        sizeof( STREAM_CONTEXT ),
        NonPagedPool,
        (PFLT_CONTEXT*) ppStreamContext
        );

    if ( NT_SUCCESS( status ) )
    {
        RtlZeroMemory( *ppStreamContext, sizeof( STREAM_CONTEXT ) );

        BOOLEAN bIsDirectory;

        status = FltIsDirectory (
            FltObjects->FileObject,
            FltObjects->Instance,
            &bIsDirectory
            );

        if ( NT_SUCCESS( status ) )
        {
            if ( bIsDirectory )
            {
                InterlockedOr (
                    &(*ppStreamContext)->m_Flags,
                    _STREAM_FLAGS_DIRECTORY
                    );
            }
        }

        status = FltSetStreamContext (
            FltObjects->Instance,
            FltObjects->FileObject,
            FLT_SET_CONTEXT_REPLACE_IF_EXISTS,
            *ppStreamContext,
            NULL
            );

        ReleaseContext( (PFLT_CONTEXT*) ppStreamContext );
    }

    return status;
}

__checkReturn
NTSTATUS
SecurityGetLuid (
    __out PLUID pLuid
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PACCESS_TOKEN pToken = 0;

    SECURITY_SUBJECT_CONTEXT SubjectContext;

    SeCaptureSubjectContext( &SubjectContext );

    pToken = SeQuerySubjectContextToken( &SubjectContext );

    if ( pToken )
    {
        status = SeQueryAuthenticationIdToken( pToken, pLuid );
    }

    SeReleaseSubjectContext( &SubjectContext );

    return status;
}

__checkReturn
NTSTATUS
SecurityAllocateCopySid (
    __in PSID pSid,
    __deref_out_opt PSID* ppSid
    )
{
    NTSTATUS status;
    ASSERT( RtlValidSid( pSid ) );

    ULONG SidLength = RtlLengthSid( pSid );

    *ppSid = ExAllocatePoolWithTag( PagedPool, SidLength, _ALLOC_TAG );
    if ( !*ppSid )
    {
        return STATUS_NO_MEMORY;
    }

    status = RtlCopySid( SidLength, *ppSid, pSid );
    if ( !NT_SUCCESS( status ) )
    {
        ExFreePool( *ppSid );
    }

    return status;
}

void
SecurityFreeSid (
    __in PSID* ppSid
    )
{
    if ( !*ppSid )
        return;

    ExFreePool( *ppSid );
    *ppSid = NULL;
}

__checkReturn
NTSTATUS
SecurityGetSid (
     __in_opt PFLT_CALLBACK_DATA Data,
    __deref_out_opt PSID *ppSid
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    PACCESS_TOKEN pAccessToken = NULL;
    PTOKEN_USER pTokenUser = NULL;

    BOOLEAN CopyOnOpen;
    BOOLEAN EffectiveOnly;
    SECURITY_IMPERSONATION_LEVEL ImpersonationLevel;

    PSID pSid = NULL;
    __try
    {
        if ( PsIsThreadTerminating( PsGetCurrentThread() ) )
            __leave;

        pAccessToken = PsReferenceImpersonationToken (
            Data->Thread,
            &CopyOnOpen,
            &EffectiveOnly,
            &ImpersonationLevel
            );

        if ( !pAccessToken )
        {
            pAccessToken = PsReferencePrimaryToken (
                FltGetRequestorProcess( Data )
                );
        }

        if ( !pAccessToken )
        {
            pAccessToken = PsReferencePrimaryToken( PsGetCurrentProcess() );
        }

        if ( !pAccessToken )
        {
            __leave;
        }

        status = SeQueryInformationToken (
            pAccessToken,
            TokenUser,
            (PVOID*) &pTokenUser
            );

        if( !NT_SUCCESS( status ) )
        {
            pTokenUser = NULL;
            __leave;
        }

        status = SecurityAllocateCopySid( pTokenUser->User.Sid, &pSid );
        if( !NT_SUCCESS( status ) )
        {
            pSid = NULL;
            __leave;
        }

        *ppSid = pSid;
        pSid = NULL;
    }
    __finally
    {
        if ( pTokenUser )
        {
            ExFreePool( pTokenUser );
            pTokenUser = NULL;
        }

        if ( pAccessToken )
        {
            PsDereferenceImpersonationToken( pAccessToken );
            pAccessToken = NULL;
        }

        SecurityFreeSid( &pSid );
    }

    return status;
}

__checkReturn
NTSTATUS
PortAllocateMessage (
    __in_opt ULONG ProcessId,
    __in_opt ULONG ThreadId,
    __in ULONG StreamFlags,
    __in ULONG HandleFlags,
    __in PFLT_FILE_NAME_INFORMATION pFileNameInfo,
    __in PSID pSid,
    __in PLUID pLuid,
    __deref_out_opt PVOID* ppMessage,
    __out_opt PULONG pMessageSize
    )
{
    ASSERT( pSid );
    ASSERT( pLuid );

    PMESSAGE_DATA pMsg;
    ULONG MessageSize = sizeof( MESSAGE_DATA );

    MessageSize += pFileNameInfo->Name.Length + sizeof( WCHAR );
    MessageSize += pFileNameInfo->Volume.Length + sizeof( WCHAR );
    MessageSize += RtlLengthSid( pSid );

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

    RtlZeroMemory( pMsg, MessageSize );        //! \todo

    pMsg->m_ProcessId = ProcessId;
    pMsg->m_ThreadId = ThreadId;
    pMsg->m_Luid = *pLuid;
    pMsg->m_FlagsStream = StreamFlags;
    pMsg->m_FlagsHandle = HandleFlags;

    pMsg->m_FileNameOffset = sizeof( MESSAGE_DATA );
    pMsg->m_FileNameLen = pFileNameInfo->Name.Length + sizeof( WCHAR );

    PVOID pFN = Add2Ptr( pMsg, pMsg->m_FileNameOffset );
    RtlCopyMemory (
        pFN,
        pFileNameInfo->Name.Buffer,
        pFileNameInfo->Name.Length
        );

    pMsg->m_VolumeNameOffset = pMsg->m_FileNameOffset + pMsg->m_FileNameLen;
    pMsg->m_VolumeNameLen = pFileNameInfo->Volume.Length + sizeof( WCHAR );

    PVOID pVN = Add2Ptr( pFN, pMsg->m_FileNameLen );
    RtlCopyMemory (
        pVN,
        pFileNameInfo->Volume.Buffer,
        pFileNameInfo->Volume.Length
        );

    pMsg->m_SidOffset = pMsg->m_VolumeNameOffset + pMsg->m_VolumeNameLen;
    pMsg->m_SidLength = RtlLengthSid( pSid );

    PVOID pS = Add2Ptr( pVN, pMsg->m_VolumeNameLen );
    RtlCopySid( RtlLengthSid( pSid ), pS, pSid );

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
    __inout PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects
    )
{
    NTSTATUS status;
    PINSTANCE_CONTEXT pInstanceContext = NULL;
    PSTREAM_CONTEXT pStreamContext = NULL;
    PFLT_FILE_NAME_INFORMATION pFileNameInfo = NULL;
    PFLT_PORT pPort = NULL;
    PSID pSid = NULL;
    PVOID pMessage = NULL;

    __try
    {
        status = PortQueryConnected( &pPort );
        if ( !NT_SUCCESS( status ) )
        {
            pPort = NULL;
            __leave;
        }

        status = FltGetInstanceContext (
            FltObjects->Instance,
            (PFLT_CONTEXT*) &pInstanceContext
            );

        if ( !NT_SUCCESS( status ) )
        {
            pInstanceContext = NULL;
            __leave;
        }

        status = FltGetStreamContext (
            FltObjects->Instance,
            FltObjects->FileObject,
            (PFLT_CONTEXT*) &pStreamContext
            );

        if ( !NT_SUCCESS( status ) )
        {
            pStreamContext = NULL;
            __leave;
        }

        status = QueryFileNameInfo( Data, &pFileNameInfo );
        if ( !NT_SUCCESS( status ) )
        {
            pFileNameInfo = NULL;
            __leave;
        }

        ULONG ProcessId = FltGetRequestorProcessId( Data );
        ULONG ThreadId = HandleToUlong( PsGetCurrentThreadId() );
        LUID Luid;

        status = SecurityGetLuid( &Luid );
        if ( !NT_SUCCESS( status ) )
            __leave;

        status = SecurityGetSid( Data, &pSid );
        if ( !NT_SUCCESS( status ) )
        {
            pSid = NULL;
            __leave;
        }

        // send data to R3
        REPLY_RESULT ReplyResult;
        ULONG ReplyLength = sizeof( ReplyResult );
        ULONG MessageSize = 0;

        status = PortAllocateMessage (
            ProcessId,
            ThreadId,
            pStreamContext->m_Flags,
            0,
            pFileNameInfo,
            pSid,
            &Luid,
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
        ReleaseContext( (PFLT_CONTEXT*) &pInstanceContext );
        ReleaseContext( (PFLT_CONTEXT*) &pStreamContext );
        ReleaseFileNameInfo( &pFileNameInfo );
        PortRelease( &pPort );
        SecurityFreeSid( &pSid );
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
        status = GenerateStreamContext( FltObjects, &pStreamContext );
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

__checkReturn
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
    if ( NT_SUCCESS( status ) )
    {
        //! \todo
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
        return FLT_POSTOP_FINISHED_PROCESSING;

    return fltStatus;
}
