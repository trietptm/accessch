#include "main.h"
#include "flt.h"

NTSTATUS
FilterEvent (
    __in PEventData Event,
    __inout PVERDICT Verdict
    )
{
    UNREFERENCED_PARAMETER( Event );
    UNREFERENCED_PARAMETER( Verdict );

    return STATUS_NOT_IMPLEMENTED;
}