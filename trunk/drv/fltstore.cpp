#include "pch.h"
#include "../inc/accessch.h"
#include "flt.h"
#include "fltstore.h"

RTL_AVL_TABLE FiltersTree::m_Tree;
EX_PUSH_LOCK FiltersTree::m_AccessLock;

//////////////////////////////////////////////////////////////////////////
typedef struct _ITEM_FILTERS
{
    Interceptors        m_Interceptor;
    DriverOperationId   m_Operation;
    ULONG               m_Minor;
    OperationPoint      m_OperationType;
    
    Filters*            m_Filters;
} ITEM_FILTERS, *PITEM_FILTERS;
//////////////////////////////////////////////////////////////////////////

Filters::Filters (
    )
{
    ExInitializeRundownProtection( &m_Ref );
    FltInitializePushLock( &m_AccessLock );
    
    RtlInitializeBitMap (
        &m_ActiveFilters,
        m_ActiveFiltersBuffer,
        NumberOfBits
        );
    
    RtlClearAllBits( &m_ActiveFilters );

    InitializeListHead( &m_FilteringHead );
}

Filters::~Filters (
    )
{
    ExWaitForRundownProtectionRelease( &m_Ref );

    ExRundownCompleted( &m_Ref );
    FltDeletePushLock( &m_AccessLock);
}

void
Filters::Release (
    )
{
    ExReleaseRundownProtection( &m_Ref );
}

VERDICT
Filters::GetVerdict (
    __in EventData *Event,
    __out PARAMS_MASK *ParamsMask
    )
{
    ASSERT( ARGUMENT_PRESENT( Event ) );
    ASSERT( ARGUMENT_PRESENT( ParamsMask ) );
    
    FltAcquirePushLockShared( &m_AccessLock );

    if ( !IsListEmpty( &m_FilteringHead ) )
    {
        PLIST_ENTRY Flink = m_FilteringHead.Flink;
        while ( Flink != &m_FilteringHead )
        {
            FilterEntry* pEntry = CONTAINING_RECORD (
                Flink,
                FilterEntry,
                m_List
                );

            Flink = Flink->Flink;

            // \todo pEntry->m_NumbersCount
        }

    }

    // \todo enum in m_FilteringHead and gather filters matched event

    FltReleasePushLock( &m_AccessLock );

    return VERDICT_NOT_FILTERED;
}

//////////////////////////////////////////////////////////////////////////

VOID
FiltersTree::Initialize (
    )
{
    FltInitializePushLock( &m_AccessLock );
    RtlInitializeGenericTableAvl (
        &m_Tree,
        FiltersTree::Compare,
        FiltersTree::Allocate,
        FiltersTree::Free,
        NULL
        );
}


VOID
FiltersTree::Destroy (
    )
{
    FltDeletePushLock( &m_AccessLock);
}

RTL_GENERIC_COMPARE_RESULTS
NTAPI
FiltersTree::Compare (
    __in struct _RTL_AVL_TABLE *Table,
    __in PVOID FirstStruct,
    __in PVOID SecondStruct
    )
{
    UNREFERENCED_PARAMETER( Table );

    RTL_GENERIC_COMPARE_RESULTS result;

    PITEM_FILTERS Struct1 = (PITEM_FILTERS) FirstStruct;
    PITEM_FILTERS Struct2 = (PITEM_FILTERS) SecondStruct;

    int ires = memcmp (
        Struct1,
        Struct2,
        sizeof( ITEM_FILTERS ) - FIELD_OFFSET( ITEM_FILTERS, m_Filters )
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
FiltersTree::Allocate (
    __in struct _RTL_AVL_TABLE *Table,
    __in CLONG ByteSize
    )
{
    UNREFERENCED_PARAMETER( Table );

    PVOID ptr = ExAllocatePoolWithTag (
        PagedPool,
        ByteSize,
        _ALLOC_TAG
        );
    
    return ptr;
}

VOID
NTAPI
FiltersTree::Free (
    __in struct _RTL_AVL_TABLE *Table,
    __in __drv_freesMem(Mem) __post_invalid PVOID Buffer
    )
{
    UNREFERENCED_PARAMETER( Table );

    FREE_POOL( Buffer );
}

FiltersTree::FiltersTree (
    )
{

}

FiltersTree::~FiltersTree (
    )
{
    // \todo free tree items
}

__checkReturn
Filters*
FiltersTree::GetFiltersByOperation (
    __in Interceptors Interceptor,
    __in DriverOperationId Operation,
    __in_opt ULONG Minor,
    __in OperationPoint OperationType
    )
{
    return NULL;
}