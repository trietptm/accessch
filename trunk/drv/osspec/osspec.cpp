#include "../inc/commonkrnl.h"
#include "../inc/memmgr.h"
#include "../inc/osspec.h"

ULONG gPreviousModeOffset = 0;

__checkReturn
NTSTATUS
GetPreviousModeOffset (
    )
{
#if ( NTDDI_VERSION < NTDDI_WIN6 )
#pragma message ( "SetPreviousMode for XP or 2003" )

    /* Return relative offset from begin _KTHREAD */
#if defined (_WIN64)
#define MOVE_OFFSET 9
    if (*((ULONG*)ExGetPreviousMode) != (ULONG)0x048B4865) // mov eax,gs:[]
#else
#define MOVE_OFFSET 6
    if (*((USHORT*)ExGetPreviousMode) != (USHORT)0xA164) // mov eax,fs:[]
#endif
    {
        return STATUS_UNSUCCESSFUL;
    }

    if ( *( (USHORT*)( &( (char*) ExGetPreviousMode )[ MOVE_OFFSET ] ) )
        != (USHORT) 0x808A ) // mov al,[eax+0xXX]
    {
        return STATUS_UNSUCCESSFUL;
    }

    gPreviousModeOffset = (ULONG)( *( (USHORT*)( & ( 
        ( char* ) ExGetPreviousMode ) [ MOVE_OFFSET + 2 ] ) ) );

#endif // ( NTDDI_VERSION < NTDDI_WIN6 )
    
    return STATUS_SUCCESS;
}

/* Switch to disable NAPI hook protection
    return previous state of MODE */
MODE
SetPreviousMode (
    MODE OperationMode
    )
{
#if ( NTDDI_VERSION < NTDDI_WIN6 )

    char *pETO = NULL;
    MODE PreviousMode;
    
    ASSERT( gPreviousModeOffset );

    pETO = (char*) PsGetCurrentThread();

    PreviousMode = (MODE) pETO[ gPreviousModeOffset ];
    pETO[ gPreviousModeOffset ] = OperationMode;

    return PreviousMode;
#else
    
    UNREFERENCED_PARAMETER( OperationMode );
    MODE prevmode = (MODE) ExGetPreviousMode();
    
    return prevmode;

#endif // ( NTDDI_VERSION < NTDDI_WIN6 )
}
