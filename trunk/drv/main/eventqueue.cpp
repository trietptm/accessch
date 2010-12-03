#include "pch.h"
#include "eventqueue.h"

// static block
ULONG           QueuedItem::m_AllocTag = 'iqSA';
LONG            QueuedItem::m_EventId;
LIST_ENTRY      QueuedItem::m_QueueItems;
EX_PUSH_LOCK    QueuedItem::m_QueueLock;

void
QueuedItem::Initialize (
    )
{
    m_EventId = 0;
    InitializeListHead( &m_QueueItems );
    FltInitializePushLock( &m_QueueLock );
};

void
QueuedItem::Destroy (
    )
{
    ASSERT( IsListEmpty( &m_QueueItems ) );
}

__checkReturn
NTSTATUS
QueuedItem::Add (
    __in PVOID Event,
    __drv_when(return==0, __deref_out_opt __drv_valueIs(!=0)) QueuedItem **Item
    )
{
    ASSERT( ARGUMENT_PRESENT( Event ) );
    ASSERT( ARGUMENT_PRESENT( Item ) );

    QueuedItem *pItem = (QueuedItem*) ExAllocatePoolWithTag (
        PagedPool,
        sizeof( QueuedItem ),
        m_AllocTag
        );

    if ( !pItem )
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    pItem->QueuedItem::QueuedItem( Event );

    FltAcquirePushLockExclusive( &m_QueueLock );
    InsertTailList( &m_QueueItems, &pItem->m_List );
    FltReleasePushLock( &m_QueueLock );

    *Item = pItem;

    return STATUS_SUCCESS;
}

__checkReturn
NTSTATUS
QueuedItem::Lookup (
    __in ULONG EventId,
    __drv_when(return==0, __deref_out_opt __drv_valueIs(!=0)) QueuedItem **Item
    )
{
    NTSTATUS status = STATUS_NOT_FOUND;
    QueuedItem *pItem = NULL;

    ASSERT( ARGUMENT_PRESENT( Item ) );

    FltAcquirePushLockShared( &m_QueueLock );

    if ( !IsListEmpty( &m_QueueItems ) )
    {
        PLIST_ENTRY Flink;

        Flink = m_QueueItems.Flink;

        while ( Flink != &m_QueueItems )
        {
            pItem = CONTAINING_RECORD( Flink, QueuedItem, m_List );
            Flink = Flink->Flink;

            if ( pItem->GetId() == EventId )
            {
                *Item = pItem;
                status = pItem->Acquire();

                break;
            }
        }
    }

    FltReleasePushLock( &m_QueueLock );

    return status;
}

// end static block

QueuedItem::QueuedItem (
    __in PVOID Data
    )
{
    m_Id = InterlockedIncrement( &m_EventId );
    if ( !m_Id )
    {
        m_Id = InterlockedIncrement( &m_EventId );
    }

    ExInitializeRundownProtection( &m_Ref );
    m_Data = Data;
}

QueuedItem::~QueuedItem (
    )
{
    ExRundownCompleted( &m_Ref );
}

void
QueuedItem::WaitAndDestroy (
    )
{
    FltAcquirePushLockExclusive( &m_QueueLock );
    RemoveEntryList( &m_List ); 
    FltReleasePushLock( &m_QueueLock );

    WaitForRelease();

    PVOID ptr = this;
    FREE_POOL( ptr );
}

ULONG
QueuedItem::GetId (
    )
{
    return m_Id;
};

NTSTATUS
QueuedItem::Acquire (
    )
{
    if ( ExAcquireRundownProtection( &m_Ref ) )
    {
        return STATUS_SUCCESS;
    }

    return STATUS_UNSUCCESSFUL;
}

void
QueuedItem::WaitForRelease (
    )
{
    ExWaitForRundownProtectionRelease( &m_Ref );
}

void
QueuedItem::Release (
    )
{
    ExReleaseRundownProtection( &m_Ref );
}