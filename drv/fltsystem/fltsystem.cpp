#include "../inc/commonkrnl.h"
#include "../inc/memmgr.h"
#include "../inc/fltsystem.h"
#include "fltbox.h"

ULONG FilteringSystem::m_AllocTag = 'sfSA';

typedef struct _FiltersStorageItem
{
    LIST_ENTRY          m_List;
    FiltersStorage*     m_Item;
} FiltersStorageItem, *PFiltersStorageItem;
 
FilteringSystem::FilteringSystem (
    )
{
    FltInitializePushLock( &m_AccessLock );
    InitializeListHead( &m_List );
}

FilteringSystem::~FilteringSystem (
    )
{
}

NTSTATUS
FilteringSystem::Attach (
    __in FiltersStorage* FltStorage
    )
{
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

__checkReturn
NTSTATUS
FilteringSystem::FilterEvent (
    __in PEventData Event,
    __in PVERDICT Verdict,
    __in PPARAMS_MASK ParamsMask
    )
{
    UNREFERENCED_PARAMETER( Event );
    UNREFERENCED_PARAMETER( Verdict );
    UNREFERENCED_PARAMETER( ParamsMask );

    return STATUS_NOT_IMPLEMENTED;
}