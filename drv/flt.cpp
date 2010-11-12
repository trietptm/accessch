#include "pch.h"
#include "../inc/accessch.h"
#include "flt.h"
#include "fltstore.h"
#include "excludes.h"

ULONG Aggregation::m_AllocTag = 'gaSA';

VOID
RemoveAllFilters (
    )
{
    FiltersTree::DeleteAllFilters();
}

__checkReturn
BOOLEAN
FilterIsExistAny (
    )
{
    if ( !FiltersTree::IsActive()
        ||
        !FiltersTree::GetCount()
        )
    {
        return FALSE;
    }
    
    return TRUE;
}

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

    Filters* pFilters = FiltersTree::GetFiltersBy (
        Event->GetInterceptorId(),
        Event->GetOperationId(),
        Event->GetMinor(),
        Event->GetOperationType()
        );
    
    if ( pFilters )
    {
        *Verdict = pFilters->GetVerdict( Event, ParamsMask );

        pFilters->Release();
    }
    else
    {
        *Verdict = VERDICT_NOT_FILTERED;
    }
    
    return STATUS_SUCCESS;
}

__checkReturn
NTSTATUS
FilterProceedChain (
    __in PFILTERS_CHAIN Chain,
    __in ULONG ChainSize,
    __out PULONG FilterId
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    
    ASSERT( ARGUMENT_PRESENT( Chain ) );

   __try
   {
        PCHAIN_ENTRY pEntry = Chain->m_Entry;

        for ( ULONG item = 0; item < Chain->m_Count; item++ )
        {
            switch( pEntry->m_Operation )
            {
            case _fltchain_add:
                {
                    Filters* pFilters = FiltersTree::GetOrCreateFiltersBy (
                        pEntry->m_Filter[0].m_Interceptor,
                        pEntry->m_Filter[0].m_OperationId,
                        pEntry->m_Filter[0].m_FunctionMi,
                        pEntry->m_Filter[0].m_OperationType
                        );

                    if ( pFilters )
                    {
                        ULONG id;
                        status = pFilters->AddFilter (
                            pEntry->m_Filter->m_GroupId,
                            pEntry->m_Filter->m_Verdict,
                            pEntry->m_Filter->m_RequestTimeout,
                            pEntry->m_Filter->m_WishMask,
                            pEntry->m_Filter->m_ParamsCount,
                            pEntry->m_Filter->m_Params,
                            &id
                            );

                        if ( FilterId )
                        {
                            *FilterId = id;
                        }

                        pFilters->Release();

                        if ( NT_SUCCESS( status ) )
                        {
                            InterlockedIncrement( &FiltersTree::m_Count );
                        }
                    }
                    else
                    {
                        status = STATUS_UNSUCCESSFUL;
                        break;
                    }
                }

                break;
            }
        }
   }
   __finally
   {
   }

    return status;
}

__checkReturn
NTSTATUS
FilterChangeState (
    BOOLEAN Activate
    )
{
    return FiltersTree::ChangeState( Activate );
}
