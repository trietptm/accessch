#ifndef __security_h
#define __security_h

#include "pch.h"

//////////////////////////////////////////////////////////////////////////
// LUID
__checkReturn
NTSTATUS
SecurityGetLuid (
    __inout PLUID pLuid
    );

BOOLEAN
SecurityIsLuidValid (
    __in PLUID Luid
    );

void
SecurityLuidReset (
    __in PLUID Luid
    );

//////////////////////////////////////////////////////////////////////////
// SID

__checkReturn
NTSTATUS
SecurityGetSid (
    __in_opt PFLT_CALLBACK_DATA Data,
    __deref_out_opt PSID *ppSid
    );

void
SecurityFreeSid (
    __in PSID* ppSid
    );

#endif // __security_h