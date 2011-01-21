#include "volhlp.h"
#include "../../../devctrl/inc/devctrlex.h"
#include "../inc/memmgr.h"

NTSTATUS
QueryDeviceProperty (
    __in PDEVICE_OBJECT Device,
    __in DEVICE_REGISTRY_PROPERTY DevProperty,
    __out PVOID* Buffer,
    __out PULONG ResultLenght
    )
{
    ASSERT( ARGUMENT_PRESENT( Device ) );
    NTSTATUS status;
    PVOID pBuffer = NULL;
    ULONG BufferSize = 0;

    status = IoGetDeviceProperty(
        Device,
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

    while ( STATUS_BUFFER_TOO_SMALL == status )
    {
        pBuffer = ExAllocatePoolWithTag( PagedPool, BufferSize, 'bvSA' );
        if ( !pBuffer )
        {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        status = IoGetDeviceProperty(
            Device,
            DevProperty,
            BufferSize,
            pBuffer,
            &BufferSize
            );

        if ( NT_SUCCESS( status ) )
        {
            *Buffer = pBuffer;
            *ResultLenght = BufferSize;
            return status;
        }

        FREE_POOL( pBuffer );
    }

    ASSERT( !pBuffer );

    return status;
}

__checkReturn
NTSTATUS
GetRemovableProperty (
    __in PDEVICE_OBJECT Device,
    __in PVolumeContext VolumeCtx
    )
{
    PVOID pBuffer = NULL;
    ULONG PropertySize;

    NTSTATUS status = QueryDeviceProperty(
        Device,
        DevicePropertyRemovalPolicy,
        &pBuffer,
        &PropertySize
        );

    if ( NT_SUCCESS( status ) )
    {
        PDEVICE_REMOVAL_POLICY pRemovalPolicy = (PDEVICE_REMOVAL_POLICY) pBuffer;
        
        VolumeCtx->m_RemovablePolicy = *pRemovalPolicy;

        FREE_POOL( pBuffer );
    }

    return status;
}

NTSTATUS
GetMediaSerialNumber (
    __in PDEVICE_OBJECT Device,
    __deref_out_opt PUNICODE_STRING DeviceId
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
        PDEVCTRL_DEVICEINFO pDeviceInfo = NULL;

        QueryBuffer = ExAllocatePoolWithTag( PagedPool, QuerySize, 'smSA' );
        if ( !QueryBuffer )
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
        }

        memset( QueryBuffer, 0, QuerySize );

        KeInitializeEvent( &Event, NotificationEvent, FALSE );

        Irp = IoBuildDeviceIoControlRequest(
            IOCTL_STORAGE_GET_MEDIA_SERIAL_NUMBER,
            Device,
            (PVOID) &GET_MEDIA_SERIAL_NUMBER_GUID,
            sizeof( GET_MEDIA_SERIAL_NUMBER_GUID ),
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

        status = IoCallDriver( Device, Irp );

        if ( STATUS_PENDING == status )
        {
            KeWaitForSingleObject(
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

        pDeviceInfo = (PDEVCTRL_DEVICEINFO) QueryBuffer;
        if ( !pDeviceInfo->IdLenght )
        {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        DeviceId->Buffer = (PWCH) ExAllocatePoolWithTag(
            PagedPool,
            pDeviceInfo->IdLenght,
            'bdSA'
            );

        if ( !DeviceId->Buffer )
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
        }

        RtlCopyMemory(
            DeviceId->Buffer,
            Add2Ptr( pDeviceInfo, pDeviceInfo->IdOffset),
            pDeviceInfo->IdLenght
            );

        DeviceId->Length = (USHORT) pDeviceInfo->IdLenght;
        DeviceId->MaximumLength = (USHORT) pDeviceInfo->IdLenght;
    }
    __finally
    {
        if ( QueryBuffer )
        {
            FREE_POOL( QueryBuffer );
        }
    }

    return status;
}

__checkReturn
NTSTATUS
GetDeviceInfo (
    __in PDEVICE_OBJECT Device,
    __in PVolumeContext VolumeCtx
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
        QueryBuffer = ExAllocatePoolWithTag( PagedPool, QuerySize, 'dgSA' );
        if ( !QueryBuffer )
        {
            status= STATUS_INSUFFICIENT_RESOURCES;
            __leave;
        }

        memset( &PropQuery, 0, sizeof( PropQuery ) );
        memset( QueryBuffer, 0, QuerySize );
        PropQuery.PropertyId = StorageDeviceProperty;
        PropQuery.QueryType = PropertyStandardQuery;

        KeInitializeEvent( &Event, NotificationEvent, FALSE );

        Irp = IoBuildDeviceIoControlRequest(
            IOCTL_STORAGE_QUERY_PROPERTY,
            Device,
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

        status = IoCallDriver( Device, Irp );

        if ( STATUS_PENDING == status )
        {
            KeWaitForSingleObject(
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

        PSTORAGE_DEVICE_DESCRIPTOR pDesc = (PSTORAGE_DEVICE_DESCRIPTOR) QueryBuffer;
        
        VolumeCtx->m_BusType = pDesc->BusType;
    }
    __finally
    {
        if ( QueryBuffer )
        {
            FREE_POOL( QueryBuffer );
        }
    }

    return status;
}

__checkReturn
NTSTATUS
FillVolumeProperties (
     __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVolumeContext VolumeCtx
    )
{
    ASSERT( ARGUMENT_PRESENT( VolumeCtx ) );

    NTSTATUS status;
    PDEVICE_OBJECT pDevice = NULL;

    __try
    {
        status = FltGetDiskDeviceObject( FltObjects->Volume, &pDevice );
        if ( !NT_SUCCESS( status ) )
        {
            pDevice = NULL;
            __leave;
        }

        status = GetDeviceInfo( pDevice, VolumeCtx );
        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }
        //ASSERT( NT_SUCCESS( status ) );
      
        /// \todo - need PDO object for GetRemovableProperty - Verifier BUGCHECK
        // status = GetRemovableProperty( pDevice, pVolumeCtx );
        //ASSERT( NT_SUCCESS( status ) );

        UNICODE_STRING deviceid;
        status = GetMediaSerialNumber( pDevice, &deviceid );
        if ( NT_SUCCESS( status ))
        {
            VolumeCtx->m_DeviceId = deviceid;
        }

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
__drv_maxIRQL( APC_LEVEL )
NTSTATUS
GetInstanceFromFileObject (
    __in PFLT_FILTER FltFilter,
    __in PFILE_OBJECT FileObject,
    __out PFLT_INSTANCE *RetInstance
    )
{
    PFLT_VOLUME volume = NULL;
    PVolumeContext volumeCtx = NULL;

    NTSTATUS status = FltGetVolumeFromFileObject(
        FltFilter,
        FileObject,
        &volume
        );

    if ( !NT_SUCCESS( status ) )
    {
        return status;
    }

    ASSERT( volume != NULL );

    status = FltGetVolumeContext(
        FltFilter,
        volume,
        (PFLT_CONTEXT*) &volumeCtx
        );

    if ( NT_SUCCESS( status ) )
    {
        ASSERT( volumeCtx->m_Instance );

        status = FltObjectReference( volumeCtx->m_Instance );

        if ( NT_SUCCESS( status ) )
        {
            *RetInstance = volumeCtx->m_Instance;
        }

        FltReleaseContext( volumeCtx );
    }

    FltObjectDereference( volume );

    return status;
}