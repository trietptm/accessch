#include "../inc/commonkrnl.h"
#include "../inc/memmgr.h"
#include "fltchecks.h"
#include "fltbox.h"

ULONG FilterBox::m_AllocTag = 'bfSA';

typedef struct _BoxFilterItem
{
    static ULONG        m_AllocTag;
    LIST_ENTRY          m_List;
    ULONG               m_Position;
    ParamCheckEntry*    m_Param;
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

        FREE_OBJECT( pEntry->m_Param );
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

    if ( refcount < 0 )
    {
        ASSERT( FALSE );
        /// \todo CRASH!!!
    }

    if ( !refcount )
    {
        /// \todo фоновое удаление
    }
}

__checkReturn
NTSTATUS
FilterBox::AddParams (
    __in ULONG ParamsCount,
    __in PFltParam Params,
    __out PULONG Position
    )
{
    if ( !ParamsCount || !Params )
    {
        return STATUS_INVALID_PARAMETER;
    }

    if ( ParamsCount != 1 ) /// \todo fix allocation size next lines when switch to multiple params
    {
        return STATUS_NOT_SUPPORTED;
    }

    NTSTATUS status = STATUS_UNSUCCESSFUL;

    PBoxFilterItem fltitem = NULL;

    __try
    {
        fltitem = ( PBoxFilterItem ) ExAllocatePoolWithTag (
            PagedPool,
            sizeof( BoxFilterItem ) + Params->m_Data.m_Size,
            BoxFilterItem::m_AllocTag
            );

        if ( !fltitem )
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            
            __leave;
        }

        fltitem->m_Param = new (
            PagedPool,
            m_AllocTag
            ) ParamCheckEntry;

        if ( !fltitem->m_Param )
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            
            __leave;
        }

        fltitem->m_Position = m_NextFreePosition++;
        *Position = fltitem->m_Position;

        fltitem->m_Param->m_Type = CheckEntryGeneric;
        fltitem->m_Param->m_Flags = Params->m_Flags;
    
        fltitem->m_Param->Generic.m_Parameter = Params->m_ParameterId;
        fltitem->m_Param->Generic.m_Operation = Params->m_Operation;
    
        status = fltitem->m_Param->Attach (
            Params->m_Data.m_Size,
            ParamsCount,
            Params->m_Data.m_Data
            );
    }
    __finally
    {
        if ( !NT_SUCCESS( status ) )
        {
            __debugbreak(); // nct

            if ( fltitem )
            {
                FREE_OBJECT( fltitem->m_Param );
                FREE_POOL( fltitem );
            }
        }
        else
        {
            InsertHeadList( &m_Items, &fltitem->m_List );
        }
    }

    return status;
}

NTSTATUS
FilterBox::MatchEvent (
    __in EventData *Event,
    __in PRTL_BITMAP Affecting
    )
{
    /// \todo добавить параметр - какие уже элементы проверялись и с каким результатом \
    // для оптимизации при продолжении сканирования по списку параметров
    if ( IsListEmpty( &m_Items ) )
    {
        return STATUS_SUCCESS;
    }

    NTSTATUS status = STATUS_SUCCESS;
    
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
        
        if ( pEntry->m_Position > Affecting->SizeOfBitMap )
        {
            __debugbreak(); //nct
            continue;
        }

        if ( !RtlCheckBit( Affecting, pEntry->m_Position ) )
        {
            continue;
        }

        status = CheckEntry( pEntry->m_Param, Event );
        if ( NT_SUCCESS( status ) )
        {
            break;
        }
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

__checkReturn
NTSTATUS
FilterBoxList::ReleaseBox (
    )
{
    FilterBox* box = LookupBoxp( Guid );
    if ( !box )
    {
        return STATUS_NOT_FOUND;
    }

    box->Release();

    return STATUS_SUCCESS;
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
