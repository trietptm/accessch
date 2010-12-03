#include "pch.h"
#include "flt.h"
#include "fltbox.h"

ULONG FilterBox::m_AllocTag = 'bfSA';

typedef struct _BoxFilterItem
{
    static ULONG    m_AllocTag;
    LIST_ENTRY      m_List;
    ULONG           m_Position;
    PARAM_ENTRY     m_Data;
} BoxFilterItem, *PBoxFilterItem;

ULONG BoxFilterItem::m_AllocTag = 'ibSA';

FilterBox::FilterBox (
    __in LPGUID Guid
    )
{
    m_RefCount = 0;
    m_Guid = *Guid;

    m_NextFreePosition = 0;
    
    InitializeListHead( &m_Items );
}

FilterBox::~FilterBox (
    )
{
    PBoxFilterItem pEntry = NULL;

    PLIST_ENTRY Flink = m_Items.Flink;
    while ( Flink != &m_Items )
    {
        pEntry = CONTAINING_RECORD (
            Flink,
            BoxFilterItem,
            m_List
            );

        Flink = Flink->Flink;
        RemoveEntryList( &pEntry->m_List );

        FREE_POOL( pEntry );
    }
}

__checkReturn
NTSTATUS
FilterBox::AddRef (
    )
{
    InterlockedIncrement( &m_RefCount );

    return STATUS_SUCCESS;
}

void
FilterBox::Release (
    )
{
    LONG refcount = InterlockedDecrement( &m_RefCount );
    if ( !refcount )
    {
        /// \todo фоновое удаление
    }
}

__checkReturn
NTSTATUS
FilterBox::AddParams (
    __in ULONG ParamsCount,
    __in PPARAM_ENTRY Params,
    __out PULONG Position
    )
{
    if ( !ParamsCount || !Params )
    {
        return STATUS_INVALID_PARAMETER;
    }

    if ( ParamsCount != 1 )
    {
        return STATUS_NOT_SUPPORTED;
    }

    PBoxFilterItem fltitem = ( PBoxFilterItem ) ExAllocatePoolWithTag (
        PagedPool,
        sizeof( BoxFilterItem ) + Params->m_FltData.m_Size,
        BoxFilterItem::m_AllocTag
        );

    if ( !fltitem )
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    fltitem->m_Position = m_NextFreePosition++;
    *Position = fltitem->m_Position;

    RtlCopyMemory (
        &fltitem->m_Data,
        Params,  
        sizeof( PARAM_ENTRY ) + Params->m_FltData.m_Size
        );

    InsertHeadList( &m_Items, &fltitem->m_List );

    return STATUS_SUCCESS;
}

NTSTATUS
FilterBox::MatchEvent (
    __in EventData *Event,
    __in PRTL_BITMAP Affecting
    )
{
    NTSTATUS status = STATUS_NOT_FOUND;
    
    PBoxFilterItem pEntry = NULL;

    PLIST_ENTRY Flink = m_Items.Flink;
    while ( Flink != &m_Items )
    {
        pEntry = CONTAINING_RECORD (
            Flink,
            BoxFilterItem,
            m_List
            );

        Flink = Flink->Flink;
    }

    return status;
}


//////////////////////////////////////////////////////////////////////////

FilterBoxList::FilterBoxList (
    )
{
    FltInitializePushLock( &m_AccessLock );
    InitializeListHead( &m_List );
}

FilterBoxList::~FilterBoxList (
    )
{
    FltDeletePushLock( &m_AccessLock );

    FilterBox* pEntry = NULL;

    PLIST_ENTRY Flink = m_List.Flink;
    while ( Flink != &m_List )
    {
        pEntry = CONTAINING_RECORD (
            Flink,
            FilterBox,
            m_List
            );

        Flink = Flink->Flink;
        RemoveEntryList( &pEntry->m_List );

        ASSERT( !pEntry->m_RefCount );
        pEntry->FilterBox::~FilterBox();
        FREE_POOL( pEntry );
    }
}

__checkReturn
NTSTATUS
FilterBoxList::GetOrCreateBox (
    __in LPGUID Guid,
    __deref_out_opt PFilterBox* FltBox
    )
{
    ASSERT( Guid );

    NTSTATUS status = STATUS_INSUFFICIENT_RESOURCES;
    FilterBox* fltbox = NULL;

    FltAcquirePushLockExclusive( &m_AccessLock );

    fltbox = LookupBoxp( Guid );
    if ( !fltbox )
    {
        fltbox = CreateNewp( Guid );
    }

    FltReleasePushLock( &m_AccessLock );

    if ( fltbox )
    {
        fltbox->AddRef();

        *FltBox = fltbox;
        status = STATUS_SUCCESS;
    }
    
    return status;
}

FilterBox*
FilterBoxList::LookupBox (
    __in LPGUID Guid
    )
{
    FilterBox* box = LookupBoxp( Guid );
    if ( !box )
    {
        return NULL;
    }

    box->AddRef();
    
    return box;
}

FilterBox*
FilterBoxList::CreateNewp (
    __in LPGUID Guid
    )
{
    ASSERT( Guid );

    FilterBox* pFltBox = ( FilterBox* ) ExAllocatePoolWithTag (
        PagedPool,
        sizeof( FilterBox ),
        FilterBox::m_AllocTag
        );

    if ( !pFltBox )
    {
        return NULL;
    }

    pFltBox->FilterBox::FilterBox( Guid );
    pFltBox->AddRef();

    InsertHeadList( &m_List, &pFltBox->m_List );

    return pFltBox;
}

FilterBox*
FilterBoxList::LookupBoxp (
    __in LPGUID Guid
    )
{
    ASSERT( Guid );

    if ( IsListEmpty( &m_List ) )
    {
        return NULL;
    }

    FilterBox* pEntry = NULL;

    PLIST_ENTRY Flink = m_List.Flink;
    while ( Flink != &m_List )
    {
        pEntry = CONTAINING_RECORD (
            Flink,
            FilterBox,
            m_List
            );

        Flink = Flink->Flink;

        if ( IsEqualGUID( *Guid, pEntry->m_Guid ) )
        {
            return pEntry;
        }
    }

    return NULL;
}
