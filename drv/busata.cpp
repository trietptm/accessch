#include "main.h"

#include <ntdddisk.h>
#include <ntddscsi.h>
#include <storport.h>
#include <ata.h>

#define  SENDIDLENGTH  sizeof (SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE

__checkReturn
NTSTATUS
GetIdentifyInfo (
    __in PDEVICE_OBJECT pDevice,
    __in UCHAR bCommandReg
    )
{
    PIRP Irp;
    KEVENT Event;
    NTSTATUS status;
    IO_STATUS_BLOCK Iosb;

    UCHAR buffer[ sizeof( SRB_IO_CONTROL ) + SENDIDLENGTH ];
    SRB_IO_CONTROL *pSrbIoCntrl = (PSRB_IO_CONTROL) buffer;
    SENDCMDINPARAMS *pin = (PSENDCMDINPARAMS)(buffer + sizeof( SRB_IO_CONTROL ));
    SENDCMDOUTPARAMS *pOut = (PSENDCMDOUTPARAMS)(buffer + sizeof( SRB_IO_CONTROL ));

    RtlZeroMemory( buffer, sizeof( buffer ) );
    pSrbIoCntrl->HeaderLength = sizeof( SRB_IO_CONTROL );
    pSrbIoCntrl->Timeout = 10000;
    pSrbIoCntrl->Length = SENDIDLENGTH;
    pSrbIoCntrl->ControlCode = IOCTL_SCSI_MINIPORT_IDENTIFY;
    RtlCopyMemory( (PUCHAR) pSrbIoCntrl->Signature, "SCSIDISK", 8 );

    pin->irDriveRegs.bCommandReg = bCommandReg; //ATA-ID_CMD; ATAPI-ATAPI_ID_CMD 
    //pin->bDriveNumber = 0; // wdk: member is opaque. Do not use it.

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    __try
    {
        Irp = IoBuildDeviceIoControlRequest (
            IOCTL_ATA_MINIPORT, //IOCTL_SCSI_MINIPORT
            pDevice,
            buffer,
            sizeof( SRB_IO_CONTROL ) + sizeof (SENDCMDINPARAMS) - 1,
            buffer,
            sizeof( SRB_IO_CONTROL ) + SENDIDLENGTH,
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

            PIDENTIFY_DEVICE_DATA pId = (PIDENTIFY_DEVICE_DATA) (pOut->bBuffer);

            __debugbreak();
        }
    }
    __finally
    {
    }

    return status;
}


