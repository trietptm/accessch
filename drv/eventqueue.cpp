#include "pch.h"
#include "eventqueue.h"

LONG            gEventId = 0;
LIST_ENTRY      gQueueItems = {0};
EX_PUSH_LOCK    gQueueLock;
//

QueuedItem::QueuedItem (
    __in PVOID Data
    )
{
    m_Id = InterlockedIncrement( &gEventId );
    if ( !m_Id )
        m_Id = InterlockedIncrement( &gEventId );

    ExInitializeRundownProtection( &m_Ref );
    m_Data = Data;
}

QueuedItem::~QueuedItem (
    )
{
    ExRundownCompleted( &m_Ref );
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
    return ExAcquireRundownProtection( &m_Ref );
}

VOID
QueuedItem::WaitForRelease (
    )
{
    ExWaitForRundownProtectionRelease( &m_Ref );
}

VOID
QueuedItem::Release (
    )
{
    ExReleaseRundownProtection( &m_Ref );
}

VOID
QueuedItem::InsertItem (
    PLIST_ENTRY List
    )
{
    InsertTailList( List, &m_List );
}

VOID
QueuedItem::RemoveItem (
    )
{
    RemoveEntryList( &m_List ); 
}

// QUEUE procedures

VOID
EventQueue_Init (
    )
{
    InitializeListHead( &gQueueItems );
    FltInitializePushLock( &gQueueLock );
}

VOID
EventQueue_Done (
    )
{
    ASSERT( IsListEmpty( &gQueueItems ) );
}

NTSTATUS
EventQueue_Add (
    __in PVOID Event,
    __deref_out_opt QueuedItem **Item
    )
{
    ASSERT( ARGUMENT_PRESENT( Event ) );
    ASSERT( ARGUMENT_PRESENT( Item ) );

    QueuedItem *pItem = (QueuedItem*) ExAllocatePoolWithTag (
        PagedPool,
        sizeof( QueuedItem ),
        _ALLOC_TAG
        );

    if ( !pItem )
    {
        return STATUS_NO_MEMORY;
    }
    
    pItem->QueuedItem::QueuedItem( Event );

    FltAcquirePushLockExclusive( &gQueueLock );
    pItem->InsertItem( &gQueueItems );
    FltReleasePushLock( &gQueueLock );

    *Item = pItem;

    return STATUS_SUCCESS;
}

VOID
EventQueue_WaitAndDestroy (
    __in QueuedItem **Item
    )
{
    ASSERT( ARGUMENT_PRESENT( Item ) );
    ASSERT( ARGUMENT_PRESENT( *Item ) );

    QueuedItem *pItem = *Item;
    *Item = NULL;
    
    FltAcquirePushLockExclusive( &gQueueLock );
    pItem->RemoveItem();
    FltReleasePushLock( &gQueueLock );

    pItem->WaitForRelease();

    FREE_POOL( pItem );
}

NTSTATUS
EventQueue_Lookup (
    __in ULONG EventId,
    __deref_out_opt QueuedItem **Item
    )
{
    NTSTATUS status = STATUS_NOT_FOUND;
    QueuedItem *pItem = NULL;

    ASSERT( ARGUMENT_PRESENT( Item ) );

    FltAcquirePushLockShared( &gQueueLock );

    PLIST_ENTRY Flink;

    Flink = gQueueItems.Flink;

    while ( Flink != &gQueueItems )
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

    FltReleasePushLock( &gQueueLock );
    
    return status;
}
