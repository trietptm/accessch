#ifndef __filehlp_h
#define __filehlp_h

#include "flt.h"

void
ReleaseFileNameInfo (
    __in_opt PFLT_FILE_NAME_INFORMATION* ppFileNameInfo
    );

void
ReleaseContext (
    __in_opt PFLT_CONTEXT* ppContext
    );

void
SecurityFreeSid (
    __in PSID* ppSid
    );

__checkReturn
NTSTATUS
QueryFileParameter (
    __in PVOID Opaque,
    __in_opt Parameters ParameterId,
    __deref_out_opt PVOID* Data,
    __deref_out_opt PULONG DataSize
    );

#endif // __filehlp_h