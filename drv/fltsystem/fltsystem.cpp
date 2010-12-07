#include "../inc/commonkrnl.h"
#include "../inc/fltsystem.h"
#include "fltbox.h"

ULONG FilteringSystem::m_AllocTag = 'sfSA';
 
FilteringSystem::FilteringSystem (
    )
{
    FltInitializePushLock( &m_AccessLock );
    InitializeListHead( &m_List );
}

FilteringSystem::~FilteringSystem (
    )
{
}

NTSTATUS
FilteringSystem::Attach (
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

void
FilteringSystem::Detach (
    )
{
}

__checkReturn
NTSTATUS
FilteringSystem::FilterEvent (
    __in PEventData Event,
    __in PVERDICT Verdict,
    __in PPARAMS_MASK ParamsMask
    )
{
    UNREFERENCED_PARAMETER( Event );
    UNREFERENCED_PARAMETER( Verdict );
    UNREFERENCED_PARAMETER( ParamsMask );

    return STATUS_NOT_IMPLEMENTED;
}