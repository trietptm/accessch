#include "../inc/commonkrnl.h"
#include "../inc/memmgr.h"
#include "fltfilters.h"
#include "fltchecks.h"

#include "fltfilters.tmh"

ULONG Filters::m_AllocTag = 'ifSA';

struct FilterEntry
{
    ULONG               m_Flags;
    ULONG               m_FilterId;
    UCHAR               m_GroupId;
    VERDICT             m_Verdict;
    HANDLE              m_ProcessId;
    PARAMS_MASK         m_WishMask;
    ULONG               m_RequestTimeout;
};

//////////////////////////////////////////////////////////////////////////

Filters::Filters (
    )
{
    ExInitializeRundownProtection( &m_Ref );
    FltInitializePushLock( &m_AccessLock );
    
    RtlInitializeBitMap(
        &m_GroupsMap,
        m_GroupsMapBuffer,
        NumberOfBits
        );

    RtlClearAllBits( &m_GroupsMap );
    m_GroupCount = 0;

    RtlInitializeBitMap(
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
            pEntry = CONTAINING_RECORD(
                Flink,
                ParamCheckEntry,
                m_List
                );
            
            Flink = Flink->Flink;

            FREE_OBJECT( pEntry );
        }
    }
    
    FREE_POOL( m_FiltersArray );
}

__checkReturn
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

__checkReturn
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
        ParamCheckEntry* pEntry = CONTAINING_RECORD(
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
            // this parameter used in already unmatched filters
            __debugbreak(); /// nct
            continue;
        }

        // check data
        NTSTATUS status = CheckEntry( pEntry, Event );
        
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
    if ( !m_FiltersCount )
    {
        return VERDICT_NOT_FILTERED;
    }

    VERDICT verdict = VERDICT_NOT_FILTERED;

    NTSTATUS status;

    RTL_BITMAP filtersbitmap;
    ULONG mapbuffer[ BitMapBufferSizeInUlong ] = { 0 };

    RtlInitializeBitMap(
        &filtersbitmap,
        mapbuffer,
        NumberOfBits
        );

    RtlClearAllBits( &filtersbitmap );

    FltAcquirePushLockShared( &m_AccessLock );

    __try
    {
        ULONG unmatched = 0;

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

        // scan finished, lookup matched filters
        // at least 1 filter matched and bit not set
        *ParamsMask = 0;

        ULONG groupcount = m_GroupCount;

        RTL_BITMAP groupsmap;
        ULONG groupsmapbuffer[ BitMapBufferSizeInUlong ] = { 0 };
        
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

            status = Event->m_Aggregator.PlaceValue(
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
    __in PFltParam ParamEntry,
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
            ParamCheckEntry* pEntry = CONTAINING_RECORD(
                Flink,
                ParamCheckEntry,
                m_List
                );

            Flink = Flink->Flink;

            if (
                pEntry->Generic.m_Operation != ParamEntry->m_Operation
                ||
                pEntry->m_Flags != ParamEntry->m_Flags
                ||
                pEntry->Generic.m_CheckData->m_DataSize != ParamEntry->m_Data.m_Size
                ||
                pEntry->Generic.m_CheckData->m_DataSize != RtlCompareMemory(
                    pEntry->Generic.m_CheckData->m_Data,
                    ParamEntry->m_Data.m_Data,
                    pEntry->Generic.m_CheckData->m_DataSize
                    )
                )
            {
                continue;
            }
            
            // the same ParamEntry, attach to existing

            PULONG pPostListTmp = pEntry->m_FilterPosList;
            pEntry->m_FilterPosList = (PosListItemType*) ExAllocatePoolWithTag(
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

            RtlCopyMemory(
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
    __in PFltParam ParamEntry,
    __in ULONG Position,
    __in PFilterBoxList BoxList
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
    status = STATUS_SUCCESS;

    __try
    {
        pEntry = (ParamCheckEntry*) ExAllocatePoolWithTag(
            PagedPool,
            sizeof( ParamCheckEntry ) + ParamEntry->m_Data.m_Size, // byte m_Data
            m_AllocTag
            );

        if ( !pEntry )
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            
            __leave;
        }

        pEntry->ParamCheckEntry::ParamCheckEntry();

        pEntry->m_Flags = ParamEntry->m_Flags;
        pEntry->m_PosCount = 1;

        pEntry->m_FilterPosList = (PosListItemType*) ExAllocatePoolWithTag(
            PagedPool,
            sizeof( PosListItemType ),
            m_AllocTag
            );

        if ( !pEntry->m_FilterPosList )
        {
            status = STATUS_INSUFFICIENT_RESOURCES;

            __leave;
        }

        pEntry->m_FilterPosList[0] = Position;

        // types of entry
        if ( !ParamEntry->m_ParameterId )
        {
            ULONG bitcount = ParamEntry->m_Data.m_Box->m_BitCount;

            if (
                !bitcount
                ||
                bitcount % 32
                )
            {
                ASSERT( FALSE );
                status = STATUS_INVALID_PARAMETER_2;

                __leave;
            }

            pEntry->m_Type = CheckEntryBox;
            pEntry->Container.m_Affecting = NULL;

            pEntry->Container.m_Box = BoxList->LookupBox( &ParamEntry->m_Data.m_Box->m_Guid );

            if ( !pEntry->Container.m_Box )
            {
                status = STATUS_INSUFFICIENT_RESOURCES;
               
                __leave;
            }

            ULONG sizecb = ParamEntry->m_Data.m_Box->m_BitCount / 8;
            pEntry->Container.m_Affecting = ( PRTL_BITMAP ) ExAllocatePoolWithTag(
                PagedPool,
                sizeof( RTL_BITMAP ) + sizecb,
                m_AllocTag
                );

            if ( !pEntry->Container.m_Affecting )
            {
                status = STATUS_INSUFFICIENT_RESOURCES;

                __leave;
            }

            RtlInitializeBitMap(
                pEntry->Container.m_Affecting,
                (PULONG) Add2Ptr( pEntry->Container.m_Affecting, sizeof( RTL_BITMAP ) ),
                ParamEntry->m_Data.m_Box->m_BitCount
                );

            RtlCopyMemory(
                pEntry->Container.m_Affecting->Buffer,
                &ParamEntry->m_Data.m_Box->m_BitMask[0],
                sizecb
                );
        }
        else
        {
            pEntry->m_Type = CheckEntryGeneric;

            pEntry->Generic.m_Operation = ParamEntry->m_Operation;
            pEntry->Generic.m_Parameter = ParamEntry->m_ParameterId;
            
            status = pEntry->Attach(
                ParamEntry->m_Data.m_Size,
                ParamEntry->m_Data.m_Count,
                ParamEntry->m_Data.m_Data
                );

            if ( !NT_SUCCESS( status ) )
            {
                __leave;
            }
        }

        InsertTailList( &m_ParamsCheckList, &pEntry->m_List );
    }
    __finally
    {
        if ( !NT_SUCCESS( status ) )
        {
            if ( pEntry )
            {
                FREE_OBJECT( pEntry );
            }
        }
    }

    return pEntry;
}

void
Filters::DeleteParamsByFilterPosUnsafe (
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
        ParamCheckEntry* pEntry = CONTAINING_RECORD(
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

                pEntry->ParamCheckEntry::~ParamCheckEntry();
                FREE_POOL( pEntry );

                continue;
            }
           
            PosListItemType* pTmp = (PosListItemType*) ExAllocatePoolWithTag(
                PagedPool,
                sizeof( PosListItemType ) * pEntry->m_PosCount - foundcount,
                m_AllocTag
                );

            ULONG idxto = 0;

            if ( pTmp )
            {
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
                pTmp = pEntry->m_FilterPosList;
                for ( ULONG idx = 0; idx < pEntry->m_PosCount; idx++ )
                {
                    if ( pTmp[ idx ] != Position )
                    {
                        pTmp[ idxto++ ] = pTmp[ idx ];
                    }
                }
            }
            
            pEntry->m_PosCount = pEntry->m_PosCount - foundcount;
        }
    };
}

void
Filters::MoveFilterPosInParams (
    ULONG IdxFrom,
    ULONG IdxTo
    )
{
    if ( IsListEmpty( &m_ParamsCheckList ) )
    {
        return;
    }

    PLIST_ENTRY Flink = m_ParamsCheckList.Flink;

    while ( Flink != &m_ParamsCheckList )
    {
        ParamCheckEntry* pEntry = CONTAINING_RECORD(
            Flink,
            ParamCheckEntry,
            m_List
            );

        Flink = Flink->Flink;

        for ( ULONG idx = 0; idx < pEntry->m_PosCount; idx++ )
        {
            if ( pEntry->m_FilterPosList[ idx ] == IdxFrom )
            {
                pEntry->m_FilterPosList[ idx ] = IdxTo;
            }
        }
    }
}

__checkReturn
NTSTATUS
Filters::AddParamsUnsafe (
    __in ULONG Position,
    __in ULONG ParamsCount,
    __in PFltParam Params,
    __in PFilterBoxList BoxList
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    PFltParam params = Params;

    for ( ULONG cou = 0; cou < ParamsCount; cou++ )
    {
        ASSERT( params->m_Data.m_Count );

        ParamCheckEntry* pEntry = AddParameterWithFilterPos(
            params,
            Position,
            BoxList
            );

        if ( !pEntry )
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            DeleteParamsByFilterPosUnsafe( Position );
            break;
        }

        params = ( PFltParam ) Add2Ptr(
            params,
            sizeof( FltParam ) + params->m_Data.m_Size
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
    FilterEntry* pFiltersArray = (FilterEntry*) ExAllocatePoolWithTag(
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
    __in_opt PFltParam Params,
    __in PFilterBoxList BoxList,
    __in ULONG FilterId
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

        status = AddParamsUnsafe(
            position,
            ParamsCount,
            Params,
            BoxList
            );

        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }
        
        pEntry->m_Verdict = Verdict;
        pEntry->m_ProcessId = ProcessId;
        pEntry->m_RequestTimeout = RequestTimeout;
        pEntry->m_WishMask = WishMask;
        pEntry->m_FilterId = FilterId;
        pEntry->m_GroupId = GroupId;
        
        RtlSetBit( &m_ActiveFilters, position );

        if ( !RtlCheckBit( &m_GroupsMap, GroupId ) )
        {
            RtlSetBit( &m_GroupsMap, GroupId );
            m_GroupCount++;        
        }

        m_FiltersCount++;

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
    ULONG removedcount = 0;

    FltAcquirePushLockExclusive( &m_AccessLock );

    ULONG idx = 0;
    while ( TRUE )
    {
        if ( idx == m_FiltersCount )
        {
            break;
        }

        FilterEntry* pEntry = &m_FiltersArray[ idx ];

        if ( pEntry->m_ProcessId != ProcessId )
        {
            idx++;
            continue;
        }

        removedcount++;

        RtlClearBit( &m_ActiveFilters, idx );
        DeleteParamsByFilterPosUnsafe( idx );

        FilterEntry* pFiltersArrayNew = NULL;
        
        if ( m_FiltersCount > 1 )
        {
            pFiltersArrayNew = ( FilterEntry* ) ExAllocatePoolWithTag(
                PagedPool,
                sizeof( FilterEntry ) * ( m_FiltersCount - 1 ),
                m_AllocTag
                );

            ULONG idxto = 0;
            if ( !pFiltersArrayNew )
            {
                pFiltersArrayNew = m_FiltersArray;
                for ( ULONG idx2 = 0; idx2 < m_FiltersCount; idx2++ )
                {
                    if ( idx2 == idx )
                    {
                        continue;
                    }

                    pFiltersArrayNew[ idxto ] = m_FiltersArray[ idx2 ];
                    MoveFilterPosInParams( idx2, idxto );
                    idxto++;
                }
            }
            else
            {
                for ( ULONG idx2 = 0; idx2 < m_FiltersCount; idx2++ )
                {
                    if ( idx2 == idx )
                    {
                        continue;
                    }

                    pFiltersArrayNew[ idxto ] = m_FiltersArray[ idx2 ];
                    MoveFilterPosInParams( idx2, idxto );
                    idxto++;
                }

                FREE_POOL( m_FiltersArray );
                m_FiltersArray = pFiltersArrayNew;
            }
        }

        m_FiltersCount--;
    }

    if ( !m_FiltersCount)
    {
        ASSERT( IsListEmpty( &m_ParamsCheckList ) );
    }

    FltReleasePushLock( &m_AccessLock );

    return removedcount;
}
