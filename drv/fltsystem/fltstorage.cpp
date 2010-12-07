#include "../inc/commonkrnl.h"
#include "../inc/memmgr.h"
#include "../inc/fltstorage.h"
#include "fltbox.h"
#include "fltfilters.h"

ULONG FiltersStorage::m_AllocTag = 'sfSA';

#define _FT_FLAGS_PAUSED 0x000
#define _FT_FLAGS_ACTIVE 0x001

typedef struct _FiltersItem
{
    ULONG       m_Interceptor;
    ULONG       m_Operation;
    ULONG       m_Minor;
    ULONG       m_OperationType;
    
    Filters*    m_Filters;
} FiltersItem, *PFiltersItem;

//////////////////////////////////////////////////////////////////////////

FiltersStorage::FiltersStorage (
    )
{
    FltInitializePushLock( &m_AccessLock );

    RtlInitializeGenericTableAvl (
        &m_Tree,
        FiltersStorage::Compare,
        FiltersStorage::Allocate,
        FiltersStorage::Free,
        NULL
        );

    m_Flags = _FT_FLAGS_PAUSED;
    m_FilterIdCounter = 0;
}

FiltersStorage::~FiltersStorage (
    )
{
    FltDeletePushLock( &m_AccessLock );
}

RTL_GENERIC_COMPARE_RESULTS
NTAPI
FiltersStorage::Compare (
    __in struct _RTL_AVL_TABLE *Table,
    __in PVOID FirstStruct,
    __in PVOID SecondStruct
    )
{
    UNREFERENCED_PARAMETER( Table );

    RTL_GENERIC_COMPARE_RESULTS result;

    PFiltersItem Struct1 = ( PFiltersItem ) FirstStruct;
    PFiltersItem Struct2 = ( PFiltersItem ) SecondStruct;
    ULONG comparesize = FIELD_OFFSET( FiltersItem, m_Filters );

    int ires = memcmp (
        Struct1,
        Struct2,
        comparesize
        );

    switch ( ires )
    {
    case 0:
        result = GenericEqual;
        break;

    case 1:
        result = GenericGreaterThan;
        break;

    case -1:
        result = GenericLessThan;
        break;

    default:
        __debugbreak();
        break;
    }

    return result;
}

PVOID
NTAPI
FiltersStorage::Allocate (
    __in struct _RTL_AVL_TABLE *Table,
    __in CLONG ByteSize
    )
{
    UNREFERENCED_PARAMETER( Table );

    PVOID ptr = ExAllocatePoolWithTag (
        PagedPool,
        ByteSize,
        m_AllocTag
        );
    
    return ptr;
}

void
NTAPI
FiltersStorage::Free (
    __in struct _RTL_AVL_TABLE *Table,
    __in __drv_freesMem(Mem) __post_invalid PVOID Buffer
    )
{
    UNREFERENCED_PARAMETER( Table );

    FREE_POOL( Buffer );
}

void
FiltersStorage::DeleteAllFilters (
    )
{
    PFiltersItem pItem;

    FltAcquirePushLockExclusive( &m_AccessLock );

    do 
    {
        pItem = ( PFiltersItem ) RtlEnumerateGenericTableAvl (
            &m_Tree,
            TRUE
            );

        if ( pItem )
        {
            ASSERT( pItem->m_Filters );
            FREE_OBJECT( pItem->m_Filters );

            RtlDeleteElementGenericTableAvl( &m_Tree, pItem );
        }

    } while ( pItem );

    m_FilterIdCounter = 0;

    FltReleasePushLock( &m_AccessLock );
}

void
FiltersStorage::CleanupFiltersByPid (
    __in HANDLE ProcessId,
    __in PVOID Opaque
    )
{
    FiltersStorage* pThis = ( FiltersStorage* ) Opaque;
    
    pThis->CleanupFiltersByPidp( ProcessId );
}

void
FiltersStorage::CleanupFiltersByPidp (
    __in HANDLE ProcessId
    )
{
    UNREFERENCED_PARAMETER( ProcessId );

    PFiltersItem pItem;
    
    FltAcquirePushLockExclusive( &m_AccessLock );

    pItem = ( PFiltersItem ) RtlEnumerateGenericTableAvl (
        &m_Tree,
        TRUE
        );

    while ( pItem )
    {
        ASSERT( pItem->m_Filters );
        ULONG removed = pItem->m_Filters->CleanupByProcess( ProcessId );

        if ( pItem->m_Filters->IsEmpty() )
        {
            pItem->m_Filters->Filters::~Filters();
            FREE_POOL( pItem->m_Filters );

            RtlDeleteElementGenericTableAvl( &m_Tree, pItem );

            pItem = ( PFiltersItem ) RtlEnumerateGenericTableAvl (
                &m_Tree,
                TRUE
                );
        }
        else
        {
            pItem = ( PFiltersItem ) RtlEnumerateGenericTableAvl (
                &m_Tree,
                FALSE
                );
        }
    };

    FltReleasePushLock( &m_AccessLock );
}

LONG
FiltersStorage::GetNextFilterid (
    )
{
    LONG result = InterlockedIncrement( &m_FilterIdCounter );

    return result;
}

BOOLEAN
FiltersStorage::IsActive (
    )
{
    if ( FlagOn( FiltersStorage::m_Flags, _FT_FLAGS_ACTIVE ) )
    {
        return TRUE;
    }

    return FALSE;
}

NTSTATUS
FiltersStorage::ChangeState (
    __in_opt BOOLEAN Activate
    )
{
    //! \\todo - switch to interlocked operations. fix return code....
    if ( Activate )
    {
        FiltersStorage::m_Flags = _FT_FLAGS_ACTIVE;
    }
    else
    {
        FiltersStorage::m_Flags = _FT_FLAGS_PAUSED;
    }

    return STATUS_SUCCESS;
}

__checkReturn
Filters*
FiltersStorage::GetFiltersByp (
    __in ULONG Interceptor,
    __in ULONG Operation,
    __in_opt ULONG Minor,
    __in ULONG OperationType
    )
{
    Filters* pFilters = NULL;

    FiltersItem item;
    item.m_Interceptor = Interceptor;
    item.m_Operation = Operation;
    item.m_Minor = Minor;
    item.m_OperationType = OperationType;

    FltAcquirePushLockShared( &m_AccessLock );

    PFiltersItem pItem = ( PFiltersItem ) RtlLookupElementGenericTableAvl (
        &m_Tree,
        &item
        );

    if ( pItem)
    {
        pFilters = pItem->m_Filters;

        NTSTATUS status = pFilters->AddRef();
        if ( !NT_SUCCESS( status ) )
        {
            pFilters = NULL;
        }

    }

    FltReleasePushLock( &m_AccessLock );

    return pFilters;
}

__checkReturn
Filters*
FiltersStorage::GetOrCreateFiltersByp (
    __in ULONG Interceptor,
    __in ULONG Operation,
    __in_opt ULONG Minor,
    __in ULONG OperationType
    )
{
    Filters* pFilters = NULL;

    FiltersItem item;
    item.m_Interceptor = Interceptor;
    item.m_Operation = Operation;
    item.m_Minor = Minor;
    item.m_OperationType = OperationType;
    
    BOOLEAN newElement = FALSE;
    
    FltAcquirePushLockExclusive( &m_AccessLock );

    PFiltersItem pItem = ( PFiltersItem ) RtlInsertElementGenericTableAvl (
        &m_Tree,
        &item,
        sizeof( item ),
        &newElement
        );
    
    if ( pItem )
    {
        if ( newElement )
        {
            pItem->m_Filters = (Filters*) ExAllocatePoolWithTag (
                PagedPool,
                sizeof( Filters ),
                m_AllocTag
                );
        
            if ( pItem->m_Filters )
            {
                pItem->m_Filters->Filters::Filters();
            }
        }

        pFilters = pItem->m_Filters;
    }

    if ( pFilters )
    {
        NTSTATUS status = pFilters->AddRef();
        if ( !NT_SUCCESS( status ) )
        {
            if ( newElement )
            {
                pFilters->Filters::~Filters();
                FREE_POOL( pFilters );
                pItem->m_Filters = NULL;
            }
            
            pFilters = NULL;
        }
    }

    FltReleasePushLock( &m_AccessLock );

    return pFilters;
}