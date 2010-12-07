#include "pch.h"
#include "flt.h"
#include "fltstore.h"
#include "excludes.h"


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
