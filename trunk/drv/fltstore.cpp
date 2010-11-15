#include "pch.h"
#include "../inc/accessch.h"
#include "flt.h"
#include "fltstore.h"
#include "proclist.h"

ULONG FiltersTree::m_AllocTag = 'tfSA';
RTL_AVL_TABLE FiltersTree::m_Tree;
EX_PUSH_LOCK FiltersTree::m_AccessLock;
LONG FiltersTree::m_Count;
LONG FiltersTree::m_Flags;
LONG FiltersTree::m_FilterIdCounter;

ULONG Filters::m_AllocTag = 'ifSA';


#define _FT_FLAGS_PAUSED 0x000
#define _FT_FLAGS_ACTIVE 0x001

//////////////////////////////////////////////////////////////////////////
typedef struct _FiltersItem
{
    Interceptors        m_Interceptor;
    DriverOperationId   m_Operation;
    ULONG               m_Minor;
    OperationPoint      m_OperationType;
    
    Filters*            m_Filters;
} FiltersItem, *pFiltersItem;
//////////////////////////////////////////////////////////////////////////
Filters::Filters (
    )
{
    ExInitializeRundownProtection( &m_Ref );
    FltInitializePushLock( &m_AccessLock );
    
    RtlInitializeBitMap (
        &m_GroupsMap,
        m_GroupsMapBuffer,
        NumberOfBits
        );

    RtlClearAllBits( &m_GroupsMap );
    m_GroupCount = 0;

    RtlInitializeBitMap (
        &m_ActiveFilters,
        m_ActiveFiltersBuffer,
        NumberOfBits
        );
    
    RtlClearAllBits( &m_ActiveFilters );

    m_FiltersCount = 0;
    m_FiltersArray = NULL;
    InitializeListHead( &m_ParamsCheckList );
}

Filters::~Filters (
    )
{
    ExWaitForRundownProtectionRelease( &m_Ref );

    ExRundownCompleted( &m_Ref );
    FltDeletePushLock( &m_AccessLock);

    ParamCheckEntry* pEntry = NULL;
    if ( !IsListEmpty( &m_ParamsCheckList ) )
    {
        PLIST_ENTRY Flink = m_ParamsCheckList.Flink;
        while ( Flink != &m_ParamsCheckList )
        {
            pEntry = CONTAINING_RECORD (
                Flink,
                ParamCheckEntry,
                m_List
                );
            
            Flink = Flink->Flink;

            FREE_POOL( pEntry->m_FilterPosList );
            FREE_POOL( pEntry );
        }
    }
    
    FREE_POOL( m_FiltersArray );
}

NTSTATUS
Filters::AddRef (
    )
{
    if ( ExAcquireRundownProtection( &m_Ref ) )
    {
        return STATUS_SUCCESS;
    }
    
    return STATUS_UNSUCCESSFUL;
}

void
Filters::Release (
    )
{
    ExReleaseRundownProtection( &m_Ref );
}

BOOLEAN
Filters::IsEmpty (
    )
{
    if ( m_FiltersCount )
    {
        return FALSE;
    }

    return TRUE;
}

NTSTATUS
Filters::CheckSingleEntryUnsafe (
    __in ParamCheckEntry* Entry,
    __in EventData *Event
    )
{
    PVOID pData;
    ULONG datasize;
    NTSTATUS status = Event->QueryParameter (
        Entry->m_Parameter,
        &pData,
        &datasize
        );

    if ( !NT_SUCCESS( status ) )
    {
        if ( FlagOn( Entry->m_Flags, _PARAM_ENTRY_FLAG_BE_PRESENT ) )
        {
            __debugbreak(); //nct

            return status;
        }
        
        return STATUS_SUCCESS;
    }
    
    status = STATUS_UNSUCCESSFUL;

    PVOID ptr = Entry->m_Data.m_Data;

    ULONG item;
    switch( Entry->m_Operation )
    {
    case _fltop_equ:
        if ( datasize != Entry->m_Data.m_DataSize / Entry->m_Data.m_Count )
        {
            break;
        }
        
        for ( item = 0; item < Entry->m_Data.m_Count; item++ )
        {
            if ( datasize == RtlCompareMemory (
                ptr,
                pData,
                datasize
                ) )
            {
                status = STATUS_SUCCESS;    
                break;
            }

            ptr = Add2Ptr( ptr, datasize );
        }

        break;

    case _fltop_and:
        ASSERT( Entry->m_Data.m_Count == 1 );

        if ( datasize != Entry->m_Data.m_DataSize / Entry->m_Data.m_Count )
        {
            break;
        }

        if ( datasize == sizeof( ULONG ) )
        {
            PULONG pu1 = (PULONG) ptr;
            PULONG pu2 = (PULONG) pData;
            if ( *pu1 & *pu2 )
            {
                status = STATUS_SUCCESS;
                break;
            }
        }
        else
        {
            __debugbreak(); //not impelemented
        }

        break;
    
    default:
        break;
    }

    if ( FlagOn( Entry->m_Flags, _PARAM_ENTRY_FLAG_NEGATION ) )
    {
        if ( NT_SUCCESS( status ) )
        {
            status = STATUS_UNSUCCESSFUL;
        }
        else
        {
            status = STATUS_SUCCESS;
        }
    }

    return status;
}

NTSTATUS
Filters::CheckParamsList (
    __in EventData *Event,
    __in PULONG Unmatched,
    __in PRTL_BITMAP Filtersbitmap
    )
{
    // must - at least one filter is active
    ASSERT( Event );
    ASSERT( Unmatched );
    ASSERT( Filtersbitmap );

    // check params in active filters
    if ( IsListEmpty( &m_ParamsCheckList ) )
    {
        return STATUS_SUCCESS;
    }

    PLIST_ENTRY Flink = m_ParamsCheckList.Flink;
    while ( Flink != &m_ParamsCheckList )
    {
        ParamCheckEntry* pEntry = CONTAINING_RECORD (
            Flink,
            ParamCheckEntry,
            m_List
            );

        Flink = Flink->Flink;

        // check necessary check param
        BOOLEAN bExistActiveFilter = FALSE;
        for ( ULONG cou = 0; cou < pEntry->m_PosCount; cou++ )
        {
            if ( !RtlCheckBit ( Filtersbitmap, pEntry->m_FilterPosList[cou] ) )
            {
                bExistActiveFilter = TRUE;
                break;
            }
        }

        if ( !bExistActiveFilter )
        {
            // this parameter used in already unmatcher filters
            __debugbreak(); /// nct
            continue;
        }

        // check data
        NTSTATUS status = CheckSingleEntryUnsafe( pEntry, Event );

        if ( NT_SUCCESS( status ) )
        {
            continue;
        }

        // set unmatched filters bit
        for ( ULONG cou = 0; cou < pEntry->m_PosCount; cou++ )
        {
            if ( RtlCheckBit( Filtersbitmap, pEntry->m_FilterPosList[cou] ) )
            {
                continue;
            }

            RtlSetBit( Filtersbitmap, pEntry->m_FilterPosList[cou] );

            (*Unmatched)++;
            if ( *Unmatched == m_FiltersCount )
            {
                // break circle - no filter left
                return STATUS_NOT_FOUND;
            }
        }
    }

    return STATUS_SUCCESS;
}

VERDICT
Filters::GetVerdict (
    __in EventData *Event,
    __out PARAMS_MASK *ParamsMask
    )
{
    VERDICT verdict = VERDICT_NOT_FILTERED;

    NTSTATUS status;

    RTL_BITMAP filtersbitmap;
    ULONG mapbuffer[ BitMapBufferSizeInUlong ];

    FltAcquirePushLockShared( &m_AccessLock );

    __try
    {
        ULONG unmatched = 0;

        RtlInitializeBitMap (
            &filtersbitmap,
            mapbuffer,
            NumberOfBits
            );

        RtlClearAllBits( &filtersbitmap );

        // set inactive filters
        for ( ULONG cou = 0; cou < m_FiltersCount; cou++ )
        {
            if ( !RtlCheckBit( &m_ActiveFilters, cou ) )
            {
                RtlSetBit( &filtersbitmap, cou );

                unmatched++;
                if ( unmatched == m_FiltersCount )
                {
                    __leave;
                }
            }
        }

        status = CheckParamsList( Event, &unmatched, &filtersbitmap );
        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        // at least 1 filter matched and bit not set
        *ParamsMask = 0;

        ULONG groupcount = m_GroupCount;

        RTL_BITMAP groupsmap;
        ULONG groupsmapbuffer[ BitMapBufferSizeInUlong ];
        
        RtlInitializeBitMap( &groupsmap, groupsmapbuffer, NumberOfBits );
        RtlClearAllBits( &groupsmap );

        ULONG position = -1;

        status = Event->m_Aggregator.Allocate( groupcount );
        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        while ( groupcount )
        {
            groupcount--;

            position = RtlFindClearBits( &filtersbitmap, 1, position + 1 );

            FilterEntry* pFilter = &m_FiltersArray[ position ];
            
            ASSERT( RtlCheckBit( &m_ActiveFilters, position ) );

            if ( RtlCheckBit( &groupsmap, pFilter->m_GroupId ) )
            {
                // already exist filter from this group
                continue;
            }

            status = Event->m_Aggregator.PlaceValue (
                groupcount,
                pFilter->m_FilterId,
                pFilter->m_Verdict
                );
            
            if ( !NT_SUCCESS( status ) )
            {
                // override existing verdict
                verdict = VERDICT_NOT_FILTERED;
                __leave;
            }

            RtlSetBit( &groupsmap, pFilter->m_GroupId );

            // integrated verdict and wish mask
            verdict |= pFilter->m_Verdict;
            *ParamsMask |= pFilter->m_WishMask;
        }

        ASSERT( *ParamsMask );
    }
    __finally
    {
    }

    FltReleasePushLock( &m_AccessLock );

    return verdict;
}

__checkReturn
NTSTATUS
Filters::TryToFindExisting (
    __in PPARAM_ENTRY ParamEntry,
    __in ULONG Position,
    __deref_out_opt ParamCheckEntry** Entry
    )
{
    if ( IsListEmpty( &m_ParamsCheckList ) )
    {
        return STATUS_NOT_FOUND;
    }

    NTSTATUS status = STATUS_NOT_FOUND;
    __try
    {
        PLIST_ENTRY Flink = m_ParamsCheckList.Flink;
        
        while ( Flink != &m_ParamsCheckList )
        {
            ParamCheckEntry* pEntry = CONTAINING_RECORD (
                Flink,
                ParamCheckEntry,
                m_List
                );

            Flink = Flink->Flink;

            if (
                pEntry->m_Operation != ParamEntry->m_Operation
                ||
                pEntry->m_Flags != ParamEntry->m_Flags
                ||
                pEntry->m_Data.m_DataSize != ParamEntry->m_FltData.m_Size
                ||
                pEntry->m_Data.m_DataSize != RtlCompareMemory (
                    pEntry->m_Data.m_Data,
                    ParamEntry->m_FltData.m_Data,
                    pEntry->m_Data.m_DataSize
                    )
                )
            {
                continue;
            }
            
            // the same ParamEntry, attach to existing
            __debugbreak(); //nct

            PULONG pPostListTmp = pEntry->m_FilterPosList;
            pEntry->m_FilterPosList = (PosListItemType*) ExAllocatePoolWithTag (
                PagedPool,
                sizeof( PosListItemType ) * ( pEntry->m_PosCount + 1 ),
                m_AllocTag
                );

            if ( !pEntry->m_FilterPosList )
            {
                pEntry->m_FilterPosList = pPostListTmp;

                status = STATUS_INSUFFICIENT_RESOURCES;
                __leave;
            }

            RtlCopyMemory (
                pEntry->m_FilterPosList,
                pPostListTmp,
                pEntry->m_PosCount * sizeof( PosListItemType )
                );

            pEntry->m_FilterPosList[ pEntry->m_PosCount ] = Position;
            pEntry->m_PosCount++;

            FREE_POOL( pPostListTmp );

            *Entry = pEntry;
             
            status = STATUS_SUCCESS;
            break;
        }
    }
    __finally
    {
    }
    
    return status;
}

ParamCheckEntry*
Filters::AddParameterWithFilterPos (
    __in PPARAM_ENTRY ParamEntry,
    __in ULONG Position
    )
{
    ParamCheckEntry* pEntry = NULL;

    // find existing
    NTSTATUS status = TryToFindExisting( ParamEntry, Position, &pEntry );
    if ( NT_SUCCESS( status ) )
    {
        return pEntry;
    }
    
    if ( STATUS_NOT_FOUND != status )
    {
        // other error - couldnt attach to existing
        return NULL;
    }

    // create new
    pEntry = (ParamCheckEntry*) ExAllocatePoolWithTag (
        PagedPool,
        sizeof( ParamCheckEntry ) + ParamEntry->m_FltData.m_Size, // byte m_Data
        m_AllocTag
        );
    
    if ( !pEntry )
    {
        return NULL;
    }

    pEntry->m_Operation = ParamEntry->m_Operation;
    pEntry->m_Flags = ParamEntry->m_Flags;
    pEntry->m_Parameter = ParamEntry->m_Id;
    pEntry->m_PosCount = 1;
    pEntry->m_FilterPosList = (PosListItemType*) ExAllocatePoolWithTag (
        PagedPool,
        sizeof( PosListItemType ),
        m_AllocTag
        );

    if ( !pEntry->m_FilterPosList )
    {
        FREE_POOL( pEntry );
        
        return NULL;
    }
    
    pEntry->m_FilterPosList[0] = Position;
    pEntry->m_Data.m_DataSize = ParamEntry->m_FltData.m_Size;
    pEntry->m_Data.m_Count = ParamEntry->m_FltData.m_Count;

    RtlCopyMemory (
        pEntry->m_Data.m_Data,
        ParamEntry->m_FltData.m_Data,
        ParamEntry->m_FltData.m_Size
        );

    InsertTailList( &m_ParamsCheckList, &pEntry->m_List );

    return pEntry;
}

void
Filters::DeleteCheckParamsByFilterPosUnsafe (
    __in_opt ULONG Position
    )
{
    if ( IsListEmpty( &m_ParamsCheckList ) )
    {
        return;
    }

    PLIST_ENTRY Flink = m_ParamsCheckList.Flink;

    while ( Flink != &m_ParamsCheckList )
    {
        ParamCheckEntry* pEntry = CONTAINING_RECORD (
            Flink,
            ParamCheckEntry,
            m_List
            );

        Flink = Flink->Flink;

        ULONG foundcount = 0;
        for ( ULONG idx = 0; idx < pEntry->m_PosCount; idx++ )
        {
            if ( pEntry->m_FilterPosList[ idx ] == Position )
            {
                foundcount++;
            }
        }

        if ( foundcount )
        {
            if ( foundcount == pEntry->m_PosCount )
            {
                RemoveEntryList( &pEntry->m_List );

                FREE_POOL( pEntry->m_FilterPosList );
                FREE_POOL( pEntry );

                continue;
            }
           
            __debugbreak(); //nct

            PosListItemType* pTmp = (PosListItemType*) ExAllocatePoolWithTag (
                PagedPool,
                sizeof( PosListItemType ) * pEntry->m_PosCount - foundcount,
                m_AllocTag
                );

            if ( pTmp )
            {
                ULONG idxto = 0;
                for ( ULONG idx = 0; idx < pEntry->m_PosCount; idx++ )
                {
                    if ( pEntry->m_FilterPosList[ idx ] != Position )
                    {
                        pTmp[ idxto++ ] = pEntry->m_FilterPosList[ idx ];
                    }
                }
                
                FREE_POOL( pEntry->m_FilterPosList );
                pEntry->m_FilterPosList = pTmp;
            }
            else
            {
                // low resources
                KeBugCheck( 0x7d ); // install more memory or reuse old buffer
            }
            
            pEntry->m_PosCount = pEntry->m_PosCount - foundcount;
        }
    };
}

__checkReturn
NTSTATUS
Filters::ParseParamsUnsafe (
    __in ULONG Position,
    __in ULONG ParamsCount,
    __in PPARAM_ENTRY Params
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    PPARAM_ENTRY params = Params;

    for ( ULONG cou = 0; cou < ParamsCount; cou++ )
    {
        ASSERT( params->m_FltData.m_Count );

        ParamCheckEntry* pEntry = AddParameterWithFilterPos (
            params,
            Position
            );

        if ( !pEntry )
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            DeleteCheckParamsByFilterPosUnsafe( Position );
            break;
        }

        params = (PARAM_ENTRY*) Add2Ptr (
            params,
            _sizeof_param_entry + params->m_FltData.m_Size
            );
    }
    
    return status;
}

__checkReturn
NTSTATUS
Filters::GetFilterPosUnsafe (
    PULONG Position
    )
{
    FilterEntry* pFiltersArray = (FilterEntry*) ExAllocatePoolWithTag (
        PagedPool,
        sizeof( FilterEntry ) * ( m_FiltersCount + 1 ),
        m_AllocTag
        );

    if ( !pFiltersArray )
    {   
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    if ( m_FiltersCount )
    {
        RtlCopyMemory( 
            pFiltersArray,
            m_FiltersArray,
            sizeof( FilterEntry ) * m_FiltersCount
            );

        FREE_POOL( m_FiltersArray );
    }

    m_FiltersArray = pFiltersArray;

    RtlZeroMemory( &m_FiltersArray[ m_FiltersCount ], sizeof( FilterEntry ) );
    *Position = m_FiltersCount;

    return STATUS_SUCCESS;
}

__checkReturn
NTSTATUS
Filters::AddFilter (
    __in UCHAR GroupId,
    __in VERDICT Verdict,
    __in HANDLE ProcessId,
    __in_opt ULONG RequestTimeout,
    __in PARAMS_MASK WishMask,
    __in_opt ULONG ParamsCount,
    __in PPARAM_ENTRY Params,
    __out PULONG FilterId
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    ASSERT( GroupId );
    ASSERT( Verdict );
    ASSERT( WishMask );

    FltAcquirePushLockExclusive( &m_AccessLock );

    __try
    {
        ULONG position;
        status = GetFilterPosUnsafe( &position );
        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        FilterEntry* pEntry = &m_FiltersArray[ position ];

        status = ParseParamsUnsafe (
            position,
            ParamsCount,
            Params
            );

        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }
        
        pEntry->m_Verdict = Verdict;
        pEntry->m_ProcessId = ProcessId;
        pEntry->m_RequestTimeout = RequestTimeout;
        pEntry->m_WishMask = WishMask;
        pEntry->m_FilterId = FiltersTree::GetNextFilterid();
        pEntry->m_GroupId = GroupId;
        
        RtlSetBit( &m_ActiveFilters, position );

        if ( !RtlCheckBit( &m_GroupsMap, GroupId ) )
        {
            RtlSetBit( &m_GroupsMap, GroupId );
            m_GroupCount++;        
        }

        m_FiltersCount++;
        *FilterId = pEntry->m_FilterId;

        status = STATUS_SUCCESS;
    }
    __finally
    {
    }

    FltReleasePushLock( &m_AccessLock );

    return status;
}

ULONG
Filters::CleanupByProcess (
    __in HANDLE ProcessId
    )
{
    ULONG removed = 0;

    FltAcquirePushLockExclusive( &m_AccessLock );

    for ( ULONG idx = 0; idx < m_FiltersCount; idx++ )
    {
        FilterEntry* pEntry = &m_FiltersArray[ idx ];

        if ( pEntry->m_ProcessId != ProcessId )
        {
            continue;
        }

        removed++;

        RtlClearBit( &m_ActiveFilters, idx );
        DeleteCheckParamsByFilterPosUnsafe( idx );

        m_FiltersCount--;

        if ( !m_FiltersCount )
        {
            FREE_POOL( m_FiltersArray );
            
            break;
        }

        __debugbreak();
        /// \todo
        // если не последний - удалить текущий фильтр и
        // сдвинуть прочие фильтры на одну позицию и
        // изменить в параметрах позицию фильтра и
        // продолжить проверку фильтров по pid-у
    }

    if ( !m_FiltersCount)
    {
        ASSERT( IsListEmpty( &m_ParamsCheckList ) );
    }


    FltReleasePushLock( &m_AccessLock );

    return removed;
}

//////////////////////////////////////////////////////////////////////////

void
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

    m_Count = 0;
    m_Flags = _FT_FLAGS_PAUSED;
    m_FilterIdCounter = 0;

    NTSTATUS status = ProcList::RegisterExitProcessCb( CleanupFiltersByPid );
 
    ASSERT( NT_SUCCESS( status ) );
}

void
FiltersTree::Destroy (
    )
{
    ProcList::UnregisterExitProcessCb( CleanupFiltersByPid );

    FltDeletePushLock( &m_AccessLock );
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

    pFiltersItem Struct1 = (pFiltersItem) FirstStruct;
    pFiltersItem Struct2 = (pFiltersItem) SecondStruct;
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
FiltersTree::Allocate (
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
    /// \todo free tree items
}

void
FiltersTree::DeleteAllFilters (
    )
{
    pFiltersItem pItem;

    FltAcquirePushLockExclusive( &m_AccessLock );

    do 
    {
        pItem = (pFiltersItem) RtlEnumerateGenericTableAvl (
            &m_Tree,
            TRUE
            );

        if ( pItem )
        {
            ASSERT( pItem->m_Filters );
            pItem->m_Filters->Filters::~Filters();
            FREE_POOL( pItem->m_Filters );

            RtlDeleteElementGenericTableAvl( &m_Tree, pItem );
        }

    } while ( pItem );

    m_Count = 0;
    m_FilterIdCounter = 0;

    FltReleasePushLock( &m_AccessLock );
}

void
FiltersTree::CleanupFiltersByPid (
    __in HANDLE ProcessId
    )
{
    UNREFERENCED_PARAMETER( ProcessId );

    pFiltersItem pItem;
    
    FltAcquirePushLockExclusive( &m_AccessLock );

    pItem = (pFiltersItem) RtlEnumerateGenericTableAvl (
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

            m_Count -= removed;

            pItem = (pFiltersItem) RtlEnumerateGenericTableAvl (
                &m_Tree,
                TRUE
                );
        }
        else
        {
            pItem = (pFiltersItem) RtlEnumerateGenericTableAvl (
                &m_Tree,
                FALSE
                );
        }
    };

    FltReleasePushLock( &m_AccessLock );
}

LONG
FiltersTree::GetNextFilterid (
    )
{
    LONG result = InterlockedIncrement( &m_FilterIdCounter );

    return result;
}

LONG
FiltersTree::GetCount (
    )
{
    return m_Count;
}

BOOLEAN
FiltersTree::IsActive (
    )
{
    if ( FlagOn( FiltersTree::m_Flags, _FT_FLAGS_ACTIVE ) )
    {
        return TRUE;
    }

    return FALSE;
}

NTSTATUS
FiltersTree::ChangeState (
    __in_opt BOOLEAN Activate
    )
{
    //! \\todo - switch to interlocked operations. fix return code....
    if ( Activate )
    {
        FiltersTree::m_Flags = _FT_FLAGS_ACTIVE;
    }
    else
    {
        FiltersTree::m_Flags = _FT_FLAGS_PAUSED;
    }

    return STATUS_SUCCESS;
}

__checkReturn
Filters*
FiltersTree::GetFiltersBy (
    __in Interceptors Interceptor,
    __in DriverOperationId Operation,
    __in_opt ULONG Minor,
    __in OperationPoint OperationType
    )
{
    Filters* pFilters = NULL;

    FiltersItem item;
    item.m_Interceptor = Interceptor;
    item.m_Operation = Operation;
    item.m_Minor = Minor;
    item.m_OperationType = OperationType;

    FltAcquirePushLockShared( &m_AccessLock );

    pFiltersItem pItem = (pFiltersItem) RtlLookupElementGenericTableAvl (
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
FiltersTree::GetOrCreateFiltersBy (
    __in Interceptors Interceptor,
    __in DriverOperationId Operation,
    __in_opt ULONG Minor,
    __in OperationPoint OperationType
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

    pFiltersItem pItem = (pFiltersItem) RtlInsertElementGenericTableAvl (
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