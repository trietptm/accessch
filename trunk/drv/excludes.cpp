#include "pch.h"
#include "excludes.h"

HANDLE gAttachedProcess = NULL;

VOID
RegisterInvisibleProcess (
    HANDLE Process
    )
{
    ASSERT( ARGUMENT_PRESENT( Process ) );

    ASSERT( !gAttachedProcess );
    
    gAttachedProcess = Process;
}

VOID
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