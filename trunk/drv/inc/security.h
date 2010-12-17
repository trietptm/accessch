#pragma once

//////////////////////////////////////////////////////////////////////////
// LUID

__checkReturn
NTSTATUS
SecurityGetLuid (
    __inout PLUID Luid
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
    __drv_when(return==0, __deref_out_opt __drv_valueIs(!=0)) PSID *Sid
    );

void
SecurityFreeSid (
    __deref_out_opt PSID* Sid
    );

//////////////////////////////////////////////////////////////////////////
// IMPERSONALIZATION

__checkReturn
NTSTATUS
Security_CaptureContext (
    __in PETHREAD OrigTh,
    __in PSECURITY_CLIENT_CONTEXT SecurityContext
    );

void
Security_ReleaseContext (
    __in PSECURITY_CLIENT_CONTEXT SecurityContext
    );

__checkReturn
NTSTATUS
Security_ImpersonateClient (
    __in PSECURITY_CLIENT_CONTEXT SecurityContext,
    __in_opt PETHREAD ServerThread
    );