#include "pch.h"

void
ReleaseFileNameInfo (
    __in_opt PFLT_FILE_NAME_INFORMATION* ppFileNameInfo
    )
{
    ASSERT( ppFileNameInfo );

    if ( *ppFileNameInfo )
    {
        FltReleaseFileNameInformation( *ppFileNameInfo );
        *ppFileNameInfo = NULL;
    };
}

FORCEINLINE
void
ReleaseContext (
    __in_opt PFLT_CONTEXT* ppContext
    )
{
    if ( !*ppContext )
    {
        return;
    }

    FltReleaseContext( *ppContext );
    *ppContext = NULL;
}

void
SecurityFreeSid (
    __in PSID* ppSid
    )
{
    if ( !*ppSid )
        return;

    ExFreePool( *ppSid );
    *ppSid = NULL;
}