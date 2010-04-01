#include "pch.h"
#include "../inc/accessch.h"
#include "flt.h"

NTSTATUS
FilterEvent (
    __in EventData *Event,
    __inout PVERDICT Verdict
    )
{
    UNREFERENCED_PARAMETER( Event );
    UNREFERENCED_PARAMETER( Verdict );

    *Verdict = VERDICT_ASK;

    return STATUS_SUCCESS;
}