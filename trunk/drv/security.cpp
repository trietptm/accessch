#include "security.h"

__checkReturn
NTSTATUS
SecurityGetLuid (
    __out PLUID pLuid
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PACCESS_TOKEN pToken = 0;

    SECURITY_SUBJECT_CONTEXT SubjectContext;

    SeCaptureSubjectContext( &SubjectContext );

    pToken = SeQuerySubjectContextToken( &SubjectContext );

    if ( pToken )
    {
        status = SeQueryAuthenticationIdToken( pToken, pLuid );
    }

    SeReleaseSubjectContext( &SubjectContext );

    return status;
}

BOOLEAN
SecurityIsLuidValid (
    __in PLUID Luid
    )
{
    if ( Luid->LowPart || Luid->HighPart )
    {
        return TRUE;
    }

    return FALSE;
}

void
SecurityLuidReset (
    __in PLUID Luid
    )
{
    ASSERT( ARGUMENT_PRESENT( Luid ) );
    
    RtlZeroMemory( Luid, sizeof( LUID ) );
}

__checkReturn
NTSTATUS
SecurityAllocateCopySid (
    __in PSID pSid,
    __deref_out_opt PSID* ppSid
    )
{
    NTSTATUS status;
    ASSERT( RtlValidSid( pSid ) );

    ULONG SidLength = RtlLengthSid( pSid );

    *ppSid = ExAllocatePoolWithTag( PagedPool, SidLength, _ALLOC_TAG );
    if ( !*ppSid )
    {
        return STATUS_NO_MEMORY;
    }

    status = RtlCopySid( SidLength, *ppSid, pSid );
    if ( !NT_SUCCESS( status ) )
    {
        ExFreePool( *ppSid );
    }

    return status;
}

__checkReturn
NTSTATUS
SecurityGetSid (
    __in_opt PFLT_CALLBACK_DATA Data,
    __deref_out_opt PSID *ppSid
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    PACCESS_TOKEN pAccessToken = NULL;
    PTOKEN_USER pTokenUser = NULL;

    BOOLEAN CopyOnOpen;
    BOOLEAN EffectiveOnly;
    SECURITY_IMPERSONATION_LEVEL ImpersonationLevel;

    PSID pSid = NULL;
    __try
    {
        if ( PsIsThreadTerminating( PsGetCurrentThread() ) )
            __leave;

        pAccessToken = PsReferenceImpersonationToken (
            Data->Thread,
            &CopyOnOpen,
            &EffectiveOnly,
            &ImpersonationLevel
            );

        if ( !pAccessToken )
        {
            pAccessToken = PsReferencePrimaryToken (
                FltGetRequestorProcess( Data )
                );
        }

        if ( !pAccessToken )
        {
            pAccessToken = PsReferencePrimaryToken( PsGetCurrentProcess() );
        }

        if ( !pAccessToken )
        {
            __leave;
        }

        status = SeQueryInformationToken (
            pAccessToken,
            TokenUser,
            (PVOID*) &pTokenUser
            );

        if( !NT_SUCCESS( status ) )
        {
            pTokenUser = NULL;
            __leave;
        }

        status = SecurityAllocateCopySid( pTokenUser->User.Sid, &pSid );
        if( !NT_SUCCESS( status ) )
        {
            pSid = NULL;
            __leave;
        }

        *ppSid = pSid;
        pSid = NULL;
    }
    __finally
    {
        if ( pTokenUser )
        {
            ExFreePool( pTokenUser );
            pTokenUser = NULL;
        }

        if ( pAccessToken )
        {
            PsDereferenceImpersonationToken( pAccessToken );
            pAccessToken = NULL;
        }

        SecurityFreeSid( &pSid );
    }

    return status;
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
