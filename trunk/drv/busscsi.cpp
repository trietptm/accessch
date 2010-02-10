#include "main.h"

#include <ntdddisk.h>
#include <ntddscsi.h>
#include <storport.h>
#include <ata.h>

#define  SENDIDLENGTH  sizeof (SENDCMDOUTPARAMS) + IDENTIFY_BUFFER_SIZE

__checkReturn
NTSTATUS
GetSCSIIdentifyInfo (
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
    RtlMoveMemory( (PUCHAR) pSrbIoCntrl->Signature, "SCSIDISK", 8 );

    pin->irDriveRegs.bCommandReg = bCommandReg; //ATA-ID_CMD; ATAPI-ATAPI_ID_CMD 
    //pin->bDriveNumber = 0; // wdk: member is opaque. Do not use it.

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    __try
    {
        Irp = IoBuildDeviceIoControlRequest (
            IOCTL_SCSI_MINIPORT,
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
        }
    }
    __finally
    {
    }

    return status;
}

__checkReturn
NTSTATUS
GetSCSISmartInfo (
    __in PDEVICE_OBJECT pDevice
    )
{
    PIRP Irp;
    KEVENT Event;
    NTSTATUS status;
    IO_STATUS_BLOCK Iosb;

    PSRB_IO_CONTROL srbControl;
    ULONG srbSize = sizeof( SRB_IO_CONTROL ) + sizeof( GETVERSIONINPARAMS );
    PUCHAR buffer;
    PGETVERSIONINPARAMS versionParams;

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    __try
    {
        srbControl = (PSRB_IO_CONTROL) ExAllocatePoolWithTag (
            NonPagedPool,
            srbSize,
            _ALLOC_TAG
            );

        if ( !srbControl )
        {
            __leave;
        }

        RtlZeroMemory( srbControl, srbSize );

        srbControl->HeaderLength = sizeof( SRB_IO_CONTROL );
        RtlMoveMemory( srbControl->Signature, "SCSIDISK", 8 );
        srbControl->Timeout = 10000;
        srbControl->Length = sizeof(GETVERSIONINPARAMS);
        srbControl->ControlCode = IOCTL_SCSI_MINIPORT_SMART_VERSION;

        buffer = (PUCHAR)srbControl + srbControl->HeaderLength;

        versionParams = (PGETVERSIONINPARAMS)buffer;
        versionParams->bIDEDeviceMap = 0; //diskData->ScsiAddress.TargetId;

        Irp = IoBuildDeviceIoControlRequest (
            IOCTL_SCSI_MINIPORT,
            pDevice,
            srbControl,
            srbSize,
            srbControl,
            srbSize,
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
        }
    }
    __finally
    {
        if ( srbControl )
        {
            ExFreePool( srbControl );
            srbControl = NULL;
        }
    }

    return status;
}

__checkReturn
NTSTATUS
GetSCSIDiskInfo (
    __in PDEVICE_OBJECT pDevice
    )
{
    PIRP Irp;
    KEVENT Event;
    NTSTATUS status;
    IO_STATUS_BLOCK Iosb;

    UCHAR Buffer[sizeof( SCSI_PASS_THROUGH ) + 24 + sizeof( DISK_INFORMATION )];
    PSCSI_PASS_THROUGH scsiData = (PSCSI_PASS_THROUGH) Buffer;

    RtlZeroMemory( Buffer, sizeof( Buffer ) );

    scsiData->Length = sizeof( SCSI_PASS_THROUGH );
    scsiData->SenseInfoLength = 24; //if set 32 - will not filled by device
    scsiData->CdbLength = CDB6GENERIC_LENGTH;
    scsiData->DataIn = SCSI_IOCTL_DATA_IN; 
    scsiData->DataTransferLength = sizeof(CDB::_READ_DISK_INFORMATION); 
    scsiData->TimeOutValue = 10; //sec
    scsiData->DataBufferOffset = scsiData->Length + scsiData->SenseInfoLength;
    scsiData->SenseInfoOffset = scsiData->Length;

    scsiData->CdbLength = sizeof(CDB::_READ_DISK_INFORMATION);
    CDB::_READ_DISK_INFORMATION* pRead = (CDB::_READ_DISK_INFORMATION*) scsiData->Cdb;
    pRead->OperationCode = SCSIOP_READ_DISK_INFORMATION;
    pRead->AllocationLength[0] = sizeof( DISK_INFORMATION ) >> 8;
    pRead->AllocationLength[1] = sizeof( DISK_INFORMATION ) & 0xff;

    KeInitializeEvent( &Event, NotificationEvent, FALSE );

    PVOID PtrOut = Add2Ptr( scsiData, scsiData->DataBufferOffset );

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
            
            if ( STATUS_IO_TIMEOUT == status )
            {
                //! \todo раскрутить диск
            }

            if ( !NT_SUCCESS ( status ) )
            {
                __leave;
            }
            
            PDISK_INFORMATION pDiskInfo = (PDISK_INFORMATION) PtrOut;
        }
    }
    __finally
    {
    }

    return status;
} 

__checkReturn
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

    PVOID PtrOut = Add2Ptr( scsiData, scsiData->DataBufferOffset );

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

            PINQUIRYDATA pInquiryData = (PINQUIRYDATA) PtrOut;

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
