#ifndef __busata_h
#define __busata_h

NTSTATUS
GetAtaDiskSignature (
    __in PDEVICE_OBJECT pDevice
    );

#endif // __busata_h