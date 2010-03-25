#include "pch.h"
#include "flt.h"

NTSTATUS
FilterEvent (
    __in EventData *Event,
    __inout PVERDICT Verdict
    )
{
    UNREFERENCED_PARAMETER( Event );
    UNREFERENCED_PARAMETER( Verdict );

    return STATUS_SUCCESS;
}