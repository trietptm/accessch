#include "../inc/commonkrnl.h"
#include "../inc/memmgr.h"
#include "security.h"

void
SecurityFreeSid (
    __deref_out_opt PSID* Sid
    )
{
    if ( !*Sid )
        return;

    FREE_POOL( *Sid );
}

__checkReturn
NTSTATUS
SecurityGetLuid (
    __out PLUID Luid
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PACCESS_TOKEN pToken = 0;

    SECURITY_SUBJECT_CONTEXT SubjectCtx;

    SeCaptureSubjectContext( &SubjectCtx );

    pToken = SeQuerySubjectContextToken( &SubjectCtx );

    if ( pToken )
    {
        status = SeQueryAuthenticationIdToken( pToken, Luid );
    }

    SeReleaseSubjectContext( &SubjectCtx );

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
    __in PSID SidSrc,
    __deref_out_opt PSID* Sid
    )
{
    NTSTATUS status;
    ASSERT( RtlValidSid( SidSrc ) );

    ULONG SidLength = RtlLengthSid( SidSrc );

    *Sid = ExAllocatePoolWithTag( PagedPool, SidLength, 'isSA' );
    if ( !*Sid )
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    status = RtlCopySid( SidLength, *Sid, SidSrc );
    if ( !NT_SUCCESS( status ) )
    {
        FREE_POOL( *Sid );
    }

    return status;
}

__checkReturn
NTSTATUS
SecurityGetSid (
    __in_opt PFLT_CALLBACK_DATA Data,
    __drv_when(return==0, __deref_out_opt __drv_valueIs(!=0)) PSID *Sid
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
        {
            __leave;
        }

        if ( Data )
        {
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

        *Sid = pSid;
        pSid = NULL;
    }
    __finally
    {
        if ( pTokenUser )
        {
            FREE_POOL( pTokenUser );
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

__checkReturn
NTSTATUS
Security_CaptureContext (
    __in PETHREAD OrigTh,
    __in PSECURITY_CLIENT_CONTEXT SecurityContext
    )
{
    SECURITY_QUALITY_OF_SERVICE SecurityQos;

    SecurityQos.Length = sizeof( SECURITY_QUALITY_OF_SERVICE );
    SecurityQos.ImpersonationLevel = SecurityImpersonation;
    SecurityQos.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
    SecurityQos.EffectiveOnly = FALSE;

    NTSTATUS status = SeCreateClientSecurity (
        OrigTh,
        &SecurityQos,
        FALSE,
        SecurityContext
        );

    return status;
}

void
Security_ReleaseContext (
    __in PSECURITY_CLIENT_CONTEXT SecurityContext
    )
{
    ASSERT( SecurityContext );

    SeDeleteClientSecurity( SecurityContext );
}

__checkReturn
NTSTATUS
Security_ImpersonateClient (
    __in PSECURITY_CLIENT_CONTEXT SecurityContext,
    __in_opt PETHREAD ServerThread
    )
{
    NTSTATUS status = SeImpersonateClientEx( SecurityContext, ServerThread );

    return status;
}