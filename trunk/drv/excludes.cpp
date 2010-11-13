#include "pch.h"
#include "excludes.h"

HANDLE gAttachedProcess = NULL;

void
RegisterInvisibleProcess (
    HANDLE Process
    )
{
    ASSERT( ARGUMENT_PRESENT( Process ) );

    ASSERT( !gAttachedProcess );
    
    gAttachedProcess = Process;
}

void
UnregisterInvisibleProcess (
    HANDLE Process
    )
{
    UNREFERENCED_PARAMETER( Process );
    ASSERT( gAttachedProcess );

    gAttachedProcess = NULL;
}

__checkReturn
BOOLEAN
IsInvisibleProcess (
    HANDLE Process
    )
{
    if ( gAttachedProcess == Process )
    {
        return TRUE;
    }

    return FALSE;
}