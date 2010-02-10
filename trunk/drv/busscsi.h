#ifndef __scsi_h
#define __scsi_h

__checkReturn
NTSTATUS
GetSCSIIdentifyInfo (
    __in PDEVICE_OBJECT pDevice,
    __in UCHAR bCommandReg
    );

__checkReturn
NTSTATUS
GetSCSISmartInfo (
    __in PDEVICE_OBJECT pDevice
    );

__checkReturn
NTSTATUS
GetSCSIInfo (
    __in PDEVICE_OBJECT pDevice,
    __in PVOLUME_CONTEXT pVolumeContext
    );

__checkReturn
NTSTATUS
GetSCSIDiskInfo (
    __in PDEVICE_OBJECT pDevice
    );

#endif // __scsi_h
