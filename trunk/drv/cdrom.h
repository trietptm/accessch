#ifndef __cdrom_h
#define __cdrom_h

__checkReturn
NTSTATUS
GetMediaATIP (
    __in PDEVICE_OBJECT pDevice
    );

#endif // __cdrom_h
