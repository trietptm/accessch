#ifndef __pnphelper_h
#define __pnphelper_h

__checkReturn
NTSTATUS
GetPnpInfo (
    __in PDEVICE_OBJECT pDevice,
    __out PVOID *pInformation
    );

__checkReturn
NTSTATUS
GetPnpText (
    __in PDEVICE_OBJECT pDevice,
    __out PVOID *pInformation
    );

#endif //__pnphelper_h