#include "volhlp.h"

// {E636CD65-4021-4d20-97EC-5AE23189566B}
DEFINE_GUID( GET_MEDIA_SERIAL_NUMBER_GUID, 
            0xe636cd65,
            0x4021, 0x4d20, 0x97, 0xec, 0x5a, 0xe2, 0x31, 0x89, 0x56, 0x6b );
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

    status = IoGetDeviceProperty (
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
        pBuffer = ExAllocatePoolWithTag( PagedPool, BufferSize, _ALLOC_TAG );
        if ( !pBuffer )
        {
            return STATUS_NO_MEMORY;
        }

        status = IoGetDeviceProperty (
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
    __in PVOLUME_CONTEXT VolumeContext
    )
{
    PVOID pBuffer = NULL;
    ULONG PropertySize;

    NTSTATUS status = QueryDeviceProperty (
        Device,
        DevicePropertyRemovalPolicy,
        &pBuffer,
        &PropertySize
        );

    if ( NT_SUCCESS( status ) )
    {
        PDEVICE_REMOVAL_POLICY pRemovalPolicy =
            (PDEVICE_REMOVAL_POLICY) pBuffer;
        
        VolumeContext->m_RemovablePolicy = *pRemovalPolicy;

        FREE_POOL( pBuffer );
    }

    return status;
}

NTSTATUS
GetMediaSerialNumber (
    __in PDEVICE_OBJECT Device
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
            FREE_POOL( QueryBuffer );
        }
    }

    return status;
}

__checkReturn
NTSTATUS
GetDeviceInfo (
    __in PDEVICE_OBJECT Device,
    __in PVOLUME_CONTEXT VolumeContext
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
        
        VolumeContext->m_BusType = pDesc->BusType;
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
    __in PVOLUME_CONTEXT VolumeContext
    )
{
    ASSERT( ARGUMENT_PRESENT( VolumeContext ) );

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

        status = GetDeviceInfo( pDevice, VolumeContext );
        //ASSERT( NT_SUCCESS( status ) );
      
        // \todo - need PDO object for GetRemovableProperty - Verifier BUGCHECK
        // status = GetRemovableProperty( pDevice, pVolumeContext );
        //ASSERT( NT_SUCCESS( status ) );

        status = GetMediaSerialNumber( pDevice );

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
