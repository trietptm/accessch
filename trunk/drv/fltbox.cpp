#include "pch.h"
#include "fltbox.h"

FilterBox::FilterBox (
    )
{
    ExInitializeRundownProtection( &m_Ref );
}

FilterBox::~FilterBox (
    )
{
    ExWaitForRundownProtectionRelease( &m_Ref );
    ExRundownCompleted( &m_Ref );
}

__checkReturn
NTSTATUS
FilterBox::AddRef (
    )
{
    if ( ExAcquireRundownProtection( &m_Ref ) )
    {
        return STATUS_SUCCESS;
    }

    return STATUS_UNSUCCESSFUL;
}

void
FilterBox::Release (
    )
{
    ExReleaseRundownProtection( &m_Ref );
}

//////////////////////////////////////////////////////////////////////////

FilterBoxList::FilterBoxList (
    )
{
    FltInitializePushLock( &m_AccessLock );
}

FilterBoxList::~FilterBoxList (
    )
{
    FltDeletePushLock( &m_AccessLock );
}

__checkReturn
NTSTATUS
FilterBoxListGetOrCreateBox (
    LPGUID Guid
    )
{
    ASSERT( Guid );

    UNREFERENCED_PARAMETER( Guid );
    
    return STATUS_NOT_IMPLEMENTED;
}
