#include "main.h"

#include <ntddcdrm.h>

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
