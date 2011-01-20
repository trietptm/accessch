#include "../inc/commonkrnl.h"
#include "../inc/memmgr.h"
#include "../inc/fltsystem.h"
#include "../inc/excludes.h"
#include "../../inc/accessch.h"
#include "fltbox.h"

#include "fltsystem.tmh"

ULONG FilteringSystem::m_AllocTag = 'sfSA';

typedef struct _FiltersStorageItem
{
    LIST_ENTRY          m_List;
    FiltersStorage*     m_Item;
} FiltersStorageItem, *PFiltersStorageItem;
 
FilteringSystem::FilteringSystem (
    )
{
    DoTraceEx( TRACE_LEVEL_CRITICAL, TB_FILTERS, "init" );

    FltInitializePushLock( &m_AccessLock );
    InitializeListHead( &m_List );
    m_RefCount = 0;
}

FilteringSystem::~FilteringSystem (
    )
{
    DoTraceEx( TRACE_LEVEL_CRITICAL, TB_FILTERS, "done" );
    ASSERT( !m_RefCount );
}

__checkReturn
NTSTATUS
FilteringSystem::AddRef (
    )
{
    InterlockedIncrement( &m_RefCount );
    
    return STATUS_SUCCESS;
}

void
FilteringSystem::Release (
    )
{
    InterlockedDecrement( &m_RefCount );
}

NTSTATUS
FilteringSystem::Attach (
    __in FiltersStorage* FltStorage
    )
{
    DoTraceEx( TRACE_LEVEL_WARNING, TB_FILTERS, "Attaching %p", FltStorage );

    PFiltersStorageItem pItem = ( PFiltersStorageItem ) ExAllocatePoolWithTag (
        PagedPool,
        sizeof( FiltersStorageItem ),
        FilteringSystem::m_AllocTag
        );

    if ( !pItem )
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    pItem->m_Item = FltStorage;

    FltAcquirePushLockExclusive( &m_AccessLock );
    InsertHeadList( &m_List, &pItem->m_List );
    FltReleasePushLock( &m_AccessLock );

    return STATUS_SUCCESS;
}

void
FilteringSystem::Detach (
    __in FiltersStorage* FltStorage
    )
{

    DoTraceEx( TRACE_LEVEL_WARNING, TB_FILTERS, "detaching %p", FltStorage );

    PFiltersStorageItem pItem = NULL;

    FltAcquirePushLockExclusive( &m_AccessLock );

    if ( !IsListEmpty( &m_List ) )
    {
        PLIST_ENTRY Flink;

        Flink = m_List.Flink;

        while ( Flink != &m_List )
        {
            pItem = CONTAINING_RECORD( Flink, FiltersStorageItem, m_List );
            Flink = Flink->Flink;

            if ( pItem->m_Item == FltStorage )
            {
                RemoveEntryList( &pItem->m_List );
                FREE_POOL( pItem );

                break;
            }
        }
    }

    FltReleasePushLock( &m_AccessLock );
}

BOOLEAN
FilteringSystem::IsFiltersExist (
    )
{
    if ( IsListEmpty( &m_List ) )
    {
        return FALSE;
    }

    return TRUE;
}

__checkReturn
NTSTATUS
FilteringSystem::FilterEvent (
    __in EventData* Event,
    __in PVERDICT Verdict,
    __in PPARAMS_MASK ParamsMask
    )
{
    PHANDLE phProcess = NULL;
    ULONG hProcessSize;

    DoTraceEx (
        TRACE_LEVEL_INFORMATION,
        TB_FILTERS,
        "processing: %p (%d)%d:%d:%d",
        Event,
        Event->GetOperationType(),
        Event->GetInterceptorId(),
        Event->GetOperationId(),
        Event->GetMinor()
        );
    
    NTSTATUS status = Event->QueryParameter (
        PARAMETER_REQUESTOR_PROCESS_ID,
        ( PVOID*) &phProcess,
        &hProcessSize
        );

    if ( !NT_SUCCESS( status ) )
    {
        return STATUS_INVALID_PARAMETER;
    }

    if ( IsInvisibleProcess( *phProcess ) )
    {
        return STATUS_UNSUCCESSFUL;
    }

    NTSTATUS statusRet = STATUS_NOT_FOUND;

    FltAcquirePushLockShared( &m_AccessLock );

    if ( !IsListEmpty( &m_List ) )
    {
        PLIST_ENTRY Flink;

        Flink = m_List.Flink;

        PFiltersStorageItem pItem = NULL;

        while ( Flink != &m_List )
        {
            pItem = CONTAINING_RECORD( Flink, FiltersStorageItem, m_List );
            Flink = Flink->Flink;

            status = pItem->m_Item->FilterEvent (
                Event,
                Verdict,
                ParamsMask
                );

            /// \todo multiple clients
            if ( NT_SUCCESS( status ) )
            {
                statusRet = STATUS_SUCCESS;
                
                break;
            }
        }
    }

    FltReleasePushLock( &m_AccessLock );

     DoTraceEx (
        TRACE_LEVEL_INFORMATION,
        TB_FILTERS,
        "%p result 0x%x, mask 0x%I64x",
        Event,
        *Verdict,
        *ParamsMask
        );

    return statusRet;
}