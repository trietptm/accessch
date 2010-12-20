#include "../inc/commonkrnl.h"
#include "../inc/memmgr.h"
#include "../inc/fltstorage.h"
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
    __in ProcessHelper* ProcessHlp
    )
{
    ASSERT( ProcessHlp );
    m_ProcessHelper = ProcessHlp;

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
    m_BoxList = NULL;

    m_ProcessHelper->RegisterExitProcessCb( ExitProcessCb, this );
}

FiltersStorage::~FiltersStorage (
    )
{
    m_ProcessHelper->UnregisterExitProcessCb( ExitProcessCb );
    m_ProcessHelper->Release();

    DeleteAllFilters();
    delete m_BoxList;
    FltDeletePushLock( &m_AccessLock );
}

void
FiltersStorage::ExitProcessCb (
    HANDLE ProcessId,
    PVOID Opaque
    )
{
    FiltersStorage* pThis = ( FiltersStorage* ) Opaque;
    pThis->CleanupFiltersByPidp( ProcessId );
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
FiltersStorage::Lock (
    )
{
    FltAcquirePushLockExclusive( &m_AccessLock );
}

void
FiltersStorage::UnLock (
    )
{
    FltReleasePushLock( &m_AccessLock );
}

__checkReturn
NTSTATUS
FiltersStorage::AddFilterUnsafe (
    __in ULONG Interceptor,
    __in ULONG OperationId,
    __in ULONG FunctionMi,
    __in ULONG OperationType,
    __in UCHAR GroupId,
    __in VERDICT Verdict,
    __in HANDLE ProcessId,
    __in_opt ULONG RequestTimeout,
    __in PARAMS_MASK WishMask,
    __in_opt ULONG ParamsCount,
    __in_opt PFltParam Params,
    __out PULONG FilterId
    )
{
    ASSERT( FilterId );
    NTSTATUS status = CreateBoxControlp();
    
    if ( !NT_SUCCESS( status ) )
    {
        return status;
    }

    Filters* pFilters = GetOrCreateFiltersByUnsafep (
        Interceptor,
        OperationId,
        FunctionMi,
        OperationType
        );

    if ( !pFilters )
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ULONG filterId = GetNextFilterid();

    status = pFilters->AddFilter (
        GroupId,
        Verdict,
        ProcessId,
        RequestTimeout,
        WishMask,
        ParamsCount,
        Params,
        m_BoxList,
        filterId
        );

    if ( NT_SUCCESS( status ) )
    {
        *FilterId = filterId;
    }

    pFilters->Release();

    return status;
}

__checkReturn
NTSTATUS
FiltersStorage::CreateBoxUnsafe (
    __in LPGUID Guid,
    __in ULONG ParamsCount,
    __in_opt PFltParam Params,
    __out PULONG FilterId
    )
{
    ASSERT( Guid );
    ASSERT( FilterId );

    NTSTATUS status = CreateBoxControlp();
    if ( !NT_SUCCESS( status ) )
    {
        return status;
    }

    PFilterBox pBox = NULL;
    status = m_BoxList->GetOrCreateBox( Guid, &pBox );

    if ( !NT_SUCCESS( status ) )
    {
        return status;
    }

    status = pBox->AddParams( ParamsCount, Params, FilterId );

    if ( !NT_SUCCESS( status ) )
    {
        return status;
    }

    pBox->Release();

    return status;
}

__checkReturn
NTSTATUS
FiltersStorage::ReleaseBoxUnsafe (
    __in LPGUID Guid
    )
{
    ASSERT( Guid );

    NTSTATUS status = m_BoxList->ReleaseBox( Guid );

    return status;
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

__checkReturn
NTSTATUS
FiltersStorage::CreateBoxControlp (
    )
{
    if ( m_BoxList )
    {
        return STATUS_SUCCESS;
    }

    m_BoxList = new ( PagedPool, m_AllocTag ) FilterBoxList;
    if ( m_BoxList )
    {
        return STATUS_SUCCESS;
    }

    return STATUS_INSUFFICIENT_RESOURCES;
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
            FREE_OBJECT( pItem->m_Filters );

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
NTSTATUS
FiltersStorage::FilterEvent (
    __in EventData* Event,
    __in PVERDICT Verdict,
    __in PPARAMS_MASK ParamsMask
    )
{
    Filters* pFilters = GetFiltersByp (
        Event->GetInterceptorId(),
        Event->GetOperationId(),
        Event->GetMinor(),
        Event->GetOperationType()
        );

    if ( !pFilters )
    {
        return STATUS_NOT_FOUND;
    }

    *Verdict = pFilters->GetVerdict (
        Event,
        ParamsMask
        );

    pFilters->Release();

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
FiltersStorage::GetOrCreateFiltersByUnsafep (
    __in ULONG Interceptor,
    __in ULONG Operation,
    __in_opt ULONG Minor,
    __in ULONG OperationType
    )
{
    FiltersItem item;
    item.m_Interceptor = Interceptor;
    item.m_Operation = Operation;
    item.m_Minor = Minor;
    item.m_OperationType = OperationType;
    
    BOOLEAN newElement = FALSE;
    
    PFiltersItem pItem = ( PFiltersItem ) RtlInsertElementGenericTableAvl (
        &m_Tree,
        &item,
        sizeof( item ),
        &newElement
        );
    
    if ( pItem && newElement )
    {
        pItem->m_Filters = new( PagedPool, m_AllocTag ) Filters;

        if ( !pItem->m_Filters )
        {
            __debugbreak(); //nct

            RtlDeleteElementGenericTableAvl (
                &m_Tree,
                &item
                );
            
            pItem = NULL;
        }
    }

    if ( pItem )
    {
        NTSTATUS status = pItem->m_Filters->AddRef();
        if ( NT_SUCCESS( status ) )
        {
            return pItem->m_Filters;
        }
    }

    return NULL;
}