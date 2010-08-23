#include "pch.h"
#include "../inc/accessch.h"
#include "flt.h"
#include "fltstore.h"

RTL_AVL_TABLE FiltersTree::m_Tree;
EX_PUSH_LOCK FiltersTree::m_AccessLock;
LONG FiltersTree::m_Count;
LONG FiltersTree::m_FilterIdCounter;

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

    m_FilterCount = 0;
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

    m_FilterCount = 0;
}

NTSTATUS
Filters::AddRef (
    )
{
    NTSTATUS status = ExAcquireRundownProtection( &m_Ref );
    
    return status;
}

void
Filters::Release (
    )
{
    ExReleaseRundownProtection( &m_Ref );
}

NTSTATUS
Filters::CheckSingleEntryUnsafe (
    __in ParamCheckEntry* Entry,
    __in EventData *Event,
    __out PARAMS_MASK *ParamsMask
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
        ASSERT( FALSE );

        return VERDICT_NOT_FILTERED;
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
            __debugbreak();
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

VERDICT
Filters::GetVerdict (
    __in EventData *Event,
    __out PARAMS_MASK *ParamsMask
    )
{
    /// \todo refactor
    VERDICT verdict = VERDICT_NOT_FILTERED;

    RTL_BITMAP filtersbitmap;
    ULONG mapbuffer[ BitMapBufferSizeInUlong ];

    FltAcquirePushLockShared( &m_AccessLock );

    __try
    {
        if ( IsListEmpty( &m_ParamsCheckList ) )
        {
            __leave;
        }
        
        ULONG unmatched = 0;

        RtlInitializeBitMap (
            &filtersbitmap,
            mapbuffer,
            NumberOfBits
            );

        RtlClearAllBits( &filtersbitmap );

        // set inactive filters
        for ( ULONG cou = 0; cou < m_FilterCount; cou++ )
        {
            if ( !RtlCheckBit( &m_ActiveFilters, cou ) )
            {
                RtlSetBit( &filtersbitmap, cou );

                unmatched++;
                if ( unmatched == m_FilterCount )
                {
                    __leave;
                }
            }
        }

        // check params in active filters
        PLIST_ENTRY Flink = m_ParamsCheckList.Flink;
        while ( Flink != &m_ParamsCheckList )
        {
            /// \todo dont check entry with inactive filters only
            ParamCheckEntry* pEntry = CONTAINING_RECORD (
                Flink,
                ParamCheckEntry,
                m_List
                );
            
            Flink = Flink->Flink;

            NTSTATUS status = CheckSingleEntryUnsafe (
                pEntry,
                Event,
                ParamsMask
                );

            if ( NT_SUCCESS( status ) )
            {
                // nothing
            }
            else
            {
                // set unmatched filters bit
                for ( ULONG cou = 0; cou < pEntry->m_PosCount; cou++ )
                {
                    ULONG isset = RtlCheckBit (
                        &filtersbitmap,
                        pEntry->m_FilterPosList[cou]
                    );

                    if ( !isset )
                    {
                        RtlSetBit (
                            &filtersbitmap,
                            pEntry->m_FilterPosList[cou]
                        );
                        
                        unmatched++;
                        if ( unmatched == m_FilterCount )
                        {
                            __leave;
                        }
                    }
                }
            }
        }

        // at least 1 filter matched and bit not set
        ULONG position = RtlFindClearBits( &filtersbitmap, 1, 0 );

        ASSERT( FlagOn (
            m_FiltersArray[ position ].m_Flags,
            FLT_POSITION_BISY
            ) );

        ASSERT( RtlCheckBit( &m_ActiveFilters, position ) );

        verdict = m_FiltersArray[ position ].m_Verdict;
        *ParamsMask = m_FiltersArray[ position ].m_WishMask;
    }
    __finally
    {
    }

    FltReleasePushLock( &m_AccessLock );

    return verdict;
}

ParamCheckEntry*
Filters::AddParameterWithFilterPos (
    __in PPARAM_ENTRY ParamEntry,
    __in ULONG FilterPos
    )
{
    ParamCheckEntry* pEntry = NULL;

    // find existing
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

            if (
                pEntry->m_Operation == ParamEntry->m_Operation
                &&
                pEntry->m_Flags == ParamEntry->m_Flags
                &&
                pEntry->m_Data.m_DataSize == ParamEntry->m_FltData.m_Size
                &&
                pEntry->m_Data.m_DataSize == RtlCompareMemory (
                    pEntry->m_Data.m_Data,
                    ParamEntry->m_FltData.m_Data,
                    pEntry->m_Data.m_DataSize
                    )
                )
            {
                // the same ParamEntry, attach to existing
                __debugbreak();

                PULONG pPostListTmp = pEntry->m_FilterPosList;
                pEntry->m_FilterPosList = (PosListItemType*) ExAllocatePoolWithTag (
                    PagedPool,
                    sizeof( PosListItemType ) * ( pEntry->m_PosCount + 1 ),
                    _ALLOC_TAG
                    );

                if ( !pEntry->m_FilterPosList )
                {
                    pEntry->m_FilterPosList = pPostListTmp;

                    return NULL;
                }

                RtlCopyMemory (
                    pEntry->m_FilterPosList,
                    pPostListTmp,
                    pEntry->m_PosCount * sizeof( PosListItemType )
                    );

                pEntry->m_FilterPosList[ pEntry->m_PosCount ] = FilterPos;
                pEntry->m_PosCount++;

                ExFreePool( pPostListTmp );

                return pEntry;
            }

            Flink = Flink->Flink;
        }
    }

    // create new
    pEntry = (ParamCheckEntry*) ExAllocatePoolWithTag (
        PagedPool,
        sizeof( ParamCheckEntry ) + ParamEntry->m_FltData.m_Size, // byte m_Data
        _ALLOC_TAG
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
        _ALLOC_TAG
        );

    if ( !pEntry->m_FilterPosList )
    {
        FREE_POOL( pEntry );
        
        return NULL;
    }
    
    pEntry->m_FilterPosList[0] = FilterPos;
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

VOID
Filters::DeleteCheckParamsByFilterPosUnsafe (
    __in_opt ULONG Posittion
    )
{
    __debugbreak();
}

__checkReturn
NTSTATUS
Filters::ParseParamsUnsafe (
    __in ULONG FilterPos,
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
            FilterPos
            );

        if ( !pEntry )
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            DeleteCheckParamsByFilterPosUnsafe( FilterPos );
            break;
        }

        params = (PARAM_ENTRY*) Add2Ptr (
            params,
            sizeof( PARAM_ENTRY ) + params->m_FltData.m_Size
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
    for ( ULONG cou = 0; cou < m_FilterCount; cou++ )
    {
        if  ( !FlagOn( m_FiltersArray[ cou ].m_Flags, FLT_POSITION_BISY ) )
        {
            *Position = cou;
            
            return STATUS_SUCCESS;
        }
    }

    FilterEntry* pFiltersArray = (FilterEntry*) ExAllocatePoolWithTag (
        PagedPool,
        sizeof( FilterEntry ) * ( m_FilterCount + 1 ),
        _ALLOC_TAG
        );

    if ( !pFiltersArray )
    {   
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    
    if ( m_FilterCount )
    {
        RtlCopyMemory( 
            pFiltersArray,
            m_FiltersArray,
            sizeof( FilterEntry ) * m_FilterCount
            );

        FREE_POOL( m_FiltersArray );
    }

    m_FiltersArray = pFiltersArray;

    RtlZeroMemory( &m_FiltersArray[ m_FilterCount ], sizeof( FilterEntry ) );
    *Position = m_FilterCount;

    return STATUS_SUCCESS;
}

__checkReturn
NTSTATUS
Filters::AddFilter (
    __in VERDICT Verdict,
    __in_opt ULONG RequestTimeout,
    __in PARAMS_MASK WishMask,
    __in_opt ULONG ParamsCount,
    __in_opt PPARAM_ENTRY Params,
    __out PULONG FilterId
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    ASSERT( Verdict );

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
        pEntry->m_RequestTimeout = RequestTimeout;
        pEntry->m_WishMask = WishMask;
        pEntry->m_FilterId = FiltersTree::GetNextFilterid();

        SetFlag (
            m_FiltersArray[ position ].m_Flags,
            FLT_POSITION_BISY
            );

        RtlSetBit( &m_ActiveFilters, position );

        m_FilterCount++;
        *FilterId = pEntry->m_FilterId;

        status = STATUS_SUCCESS;
    }
    __finally
    {
    }

    FltReleasePushLock( &m_AccessLock );

    return status;
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

    m_Count = 0;
    m_FilterIdCounter = 0;
}


VOID
FiltersTree::Destroy (
    )
{
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

    PITEM_FILTERS Struct1 = (PITEM_FILTERS) FirstStruct;
    PITEM_FILTERS Struct2 = (PITEM_FILTERS) SecondStruct;
    ULONG comparesize = FIELD_OFFSET( ITEM_FILTERS, m_Filters );

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
    /// \todo free tree items
}

VOID
FiltersTree::DeleteAllFilters (
    )
{
    PITEM_FILTERS pItem;

    FltAcquirePushLockExclusive( &m_AccessLock );

    do 
    {
        pItem = (PITEM_FILTERS) RtlEnumerateGenericTableAvl (
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

    ITEM_FILTERS item;
    item.m_Interceptor = Interceptor;
    item.m_Operation = Operation;
    item.m_Minor = Minor;
    item.m_OperationType = OperationType;

    FltAcquirePushLockShared( &m_AccessLock );

    PITEM_FILTERS pItem = (PITEM_FILTERS) RtlLookupElementGenericTableAvl (
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

    ITEM_FILTERS item;
    item.m_Interceptor = Interceptor;
    item.m_Operation = Operation;
    item.m_Minor = Minor;
    item.m_OperationType = OperationType;
    
    BOOLEAN newElement = FALSE;
    
    FltAcquirePushLockExclusive( &m_AccessLock );

    PITEM_FILTERS pItem = (PITEM_FILTERS) RtlInsertElementGenericTableAvl (
        &m_Tree,
        &item,
        sizeof( item ),
        &newElement
        );
    
    if ( pItem)
    {
        if ( newElement )
        {
            pItem->m_Filters = (Filters*) ExAllocatePoolWithTag (
                PagedPool,
                sizeof(Filters),
                _ALLOC_TAG
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