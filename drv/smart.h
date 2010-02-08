#ifndef __smart_h
#define __smart_h

__checkReturn
NTSTATUS
GetSmartInfo (
    __in PDEVICE_OBJECT pDevice
    );

__checkReturn
NTSTATUS
GetIdentifyInfo (
    __in PDEVICE_OBJECT pDevice,
    __in UCHAR bCommandReg
    );

#endif // __smart_h
