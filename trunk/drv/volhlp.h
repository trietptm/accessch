#ifndef __volhlp_h

__checkReturn
NTSTATUS
FillVolumeProperties (
     __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PVOLUME_CONTEXT pVolumeContext
    );
    
#endif // __volhlp_h