#include "main.h"

#include <ntdddisk.h>
#include <ntddscsi.h>
#include <storport.h>
#include <ata.h>

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
