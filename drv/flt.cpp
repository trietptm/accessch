#include "pch.h"
#include "../inc/accessch.h"
#include "flt.h"
#include "fltstore.h"
#include "excludes.h"

// ---
__checkReturn
NTSTATUS
FilterEvent (
    __in EventData *Event,
    __inout PVERDICT Verdict,
    __out PARAMS_MASK *ParamsMask
    )
{
    PHANDLE pRequestorProcess;
    ULONG fieldSize;
    
    NTSTATUS status = Event->QueryParameter (
        PARAMETER_REQUESTOR_PROCESS_ID,
        (PVOID*) &pRequestorProcess,
        &fieldSize
        );
    
    if ( !NT_SUCCESS( status ) )
    {
        return STATUS_UNSUCCESSFUL;
    }

    if ( IsInvisibleProcess( *pRequestorProcess ) )
    {
        return STATUS_NOT_SUPPORTED;
    }

    Filters* pFilter = FiltersTree::GetFiltersByOperation (
        Event->GetInterceptorId(),
        Event->GetOperationId(),
        Event->GetMinor(),
        Event->GetOperationType()
        );
    
    if ( pFilter )
    {
        *Verdict = VERDICT_ASK;

        *ParamsMask =
            Id2Bit( PARAMETER_FILE_NAME )
            |
            Id2Bit ( PARAMETER_VOLUME_NAME )
            |
            Id2Bit ( PARAMETER_REQUESTOR_PROCESS_ID );

        pFilter->Release();
    }
    else
    {
        *Verdict = VERDICT_NOT_FILTERED;
    }
    
    return STATUS_SUCCESS;
}

__checkReturn
NTSTATUS
FilterAdd (
    __in FilterChain *Filters,
    __in ULONG ChainSize
    )
{
    ASSERT( ARGUMENT_PRESENT( Filters ) );

    return STATUS_NOT_SUPPORTED;
}