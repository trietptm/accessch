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
