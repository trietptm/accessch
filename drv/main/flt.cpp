#include "pch.h"
#include "flt.h"
#include "fltstore.h"
#include "excludes.h"

ULONG FilteringSystem::m_AllocTag = 'sfSA';

EX_PUSH_LOCK FilteringSystem::m_AccessLock;
LIST_ENTRY FilteringSystem::m_FltObjects;
LONG FilteringSystem::m_ActiveCount = 0;

FilteringSystem::FilteringSystem (
    )
{
    ExInitializeRundownProtection( &m_Ref );
}

FilteringSystem::~FilteringSystem (
    )
{
    ExWaitForRundownProtectionRelease( &m_Ref );

    FiltersTree::DeleteAllFilters();

    ExRundownCompleted( &m_Ref );
}

void
FilteringSystem::Initialize (
    )
{
    FltInitializePushLock( &m_AccessLock );
    InitializeListHead( &m_FltObjects );
}

void FilteringSystem::Destroy (
    )
{
    ASSERT( IsListEmpty( &m_FltObjects ) );
    FltDeletePushLock( &m_AccessLock );
}

void
FilteringSystem::Attach (
    __in FilteringSystem* FltObject
    )
{
    NTSTATUS status = FltObject->AddRef();
    ASSERT( NT_SUCCESS( status ) );

    FltAcquirePushLockExclusive( &m_AccessLock );

    InsertTailList( &m_FltObjects, &FltObject->m_List );

    FltReleasePushLock( &m_AccessLock );
}

void
FilteringSystem::Detach (
    __in FilteringSystem* FltObject
    )
{
    FltAcquirePushLockExclusive( &m_AccessLock );

    RemoveEntryList( &FltObject->m_List );

    FltReleasePushLock( &m_AccessLock );

    FltObject->Release();
}

__checkReturn
BOOLEAN
FilteringSystem::IsExistFilters (
    )
{
    if ( m_ActiveCount )
    {
        return TRUE;
    }

    return FALSE;
}

__checkReturn
NTSTATUS
FilteringSystem::FilterEvent (
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

    if( !pRequestorProcess )
    {
        ASSERT( pRequestorProcess );
        return STATUS_UNSUCCESSFUL;
    }

    if ( IsInvisibleProcess( *pRequestorProcess ) )
    {
        return STATUS_NOT_SUPPORTED;
    }

    FilteringSystem* pFltObject = NULL;
    
    FltAcquirePushLockShared( &m_AccessLock );

    /// \todo реализовать продолжение поиска при регистрации нескольких клиентов
    if ( !IsListEmpty( &m_FltObjects ) )
    {
        PLIST_ENTRY Flink = m_FltObjects.Flink;
        while ( Flink != &m_FltObjects )
        {
            pFltObject = CONTAINING_RECORD (
                Flink,
                FilteringSystem,
                m_List
                );

            Flink = Flink->Flink;

            if ( pFltObject->IsActive() )
            {
                status = pFltObject->AddRef();
                if ( NT_SUCCESS( status ) )
                {
                    break;
                }
            }

            pFltObject = NULL;
        }

    }

    FltReleasePushLock( &m_AccessLock );

    if ( pFltObject )
    {
        status = pFltObject->SubFilterEvent (
            Event,
            Verdict,
            ParamsMask
            );

        pFltObject->Release();
    }
    else
    {
        status = STATUS_NOT_FOUND;
    }

    return status;
}

NTSTATUS
FilteringSystem::AddRef (
    )
{
    return STATUS_SUCCESS;
}

void
FilteringSystem::Release (
    )
{
}

BOOLEAN
FilteringSystem::IsActive (
    )
{
    return FiltersTree::IsActive();
}

void
FilteringSystem::RemoveAllFilters (
    )
{
    FiltersTree::DeleteAllFilters();
}

__checkReturn
NTSTATUS
FilteringSystem::ProceedChainGeneric (
    __in PCHAIN_ENTRY pEntry,
    __out PULONG FilterId
    )
{
    Filters* pFilters = FiltersTree::GetOrCreateFiltersBy (
        pEntry->m_Filter[0].m_Interceptor,
        pEntry->m_Filter[0].m_OperationId,
        pEntry->m_Filter[0].m_FunctionMi,
        pEntry->m_Filter[0].m_OperationType
        );

    if ( !pFilters )
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ULONG id = GetNextFilterid();

    NTSTATUS status = pFilters->AddFilter (
        pEntry->m_Filter->m_GroupId,
        pEntry->m_Filter->m_Verdict,
        UlongToHandle( pEntry->m_Filter->m_CleanupProcessId ),
        pEntry->m_Filter->m_RequestTimeout,
        pEntry->m_Filter->m_WishMask,
        pEntry->m_Filter->m_ParamsCount,
        pEntry->m_Filter->m_Params,
        &m_BoxList,
        id
        );

    if ( FilterId )
    {
        *FilterId = id;
    }

    pFilters->Release();

    return status;
}

__checkReturn
NTSTATUS
FilteringSystem::ProceedChainBox (
    __in PCHAIN_ENTRY pEntry,
    __out PULONG FilterId
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PFLTBOX pBox = pEntry[0].m_Box;

    FilterBox* pFltBox = NULL;

    switch ( pBox->m_Operation )
    {
    case _fltbox_add:
        status = m_BoxList.GetOrCreateBox( &pEntry->m_Box->m_Guid, &pFltBox );
        if ( !NT_SUCCESS( status ) )
        {
            pFltBox = NULL;
            break;
        }
        
        if ( !pFltBox )
        {
            ASSERT( pFltBox );
            status = STATUS_UNSUCCESSFUL;
            
            break;
        }
        
        status = pFltBox->AddParams (
            pEntry->m_Box->Items.m_ParamsCount,
            pEntry->m_Box->Items.m_Params,
            FilterId
            );

        break;

    default:
        __debugbreak();
    }

    if ( pFltBox )
    {
        pFltBox->Release();
    }

    return status;
}

__checkReturn
NTSTATUS
FilteringSystem::ProceedChain (
    __in PFILTERS_CHAIN Chain,
    __in ULONG ChainSize,
    __out PULONG FilterId
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    
    ASSERT( ARGUMENT_PRESENT( Chain ) );

    if ( Chain->m_Count != 1 )
    {
        __debugbreak();     // поправить возвращаемые типы
        return STATUS_NOT_SUPPORTED;
    }

   __try
   {
        PCHAIN_ENTRY pEntry = Chain->m_Entry;

        for ( ULONG item = 0; item < Chain->m_Count; item++ )
        {
            switch( pEntry->m_Operation )
            {
            case _fltchain_add:
                status = ProceedChainGeneric( pEntry, FilterId );
                break;

            case _fltchain_del:
                __debugbreak();
                
                break;

            case _fltbox_create:
                status = ProceedChainBox( pEntry, FilterId );

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
FilteringSystem::ChangeState (
    BOOLEAN Activate
    )
{
    if ( Activate )
    {
        InterlockedIncrement( &m_ActiveCount );
    }
    else
    {
        InterlockedDecrement( &m_ActiveCount );
    }

    return FiltersTree::ChangeState( Activate );
}

__checkReturn
NTSTATUS
FilteringSystem::SubFilterEvent (
    __in EventData *Event,
    __inout PVERDICT Verdict,
    __out PARAMS_MASK *ParamsMask
    )
{
    Filters* pFilters = GetFiltersBy (
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
