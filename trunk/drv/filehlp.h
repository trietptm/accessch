#ifndef __filehlp_h
#define __filehlp_h

void
ReleaseFileNameInfo (
    __in_opt PFLT_FILE_NAME_INFORMATION* ppFileNameInfo
    );

FORCEINLINE
void
ReleaseContext (
    __in_opt PFLT_CONTEXT* ppContext
    );

void
SecurityFreeSid (
    __in PSID* ppSid
    );

#endif // __filehlp_h