#include "pch.h"
#include "eventqueue.h"

LONG            gEventId = 0;
LIST_ENTRY      gQueueItems = {0};
EX_PUSH_LOCK    gQueueLock;
//

QueuedItem::QueuedItem (
    )
{
    m_Id = InterlockedIncrement( &gEventId );
    if ( !m_Id )
        m_Id = InterlockedIncrement( &gEventId );

    ExInitializeRundownProtection( &m_Ref );
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

VOID
QueuedItem::WaitForRelease (
    )
{
    ExWaitForRundownProtectionRelease( &m_Ref );
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
    ExInitializePushLock( &gQueueLock );
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
        sizeof(QueuedItem),
        _ALLOC_TAG
        );

    if ( pItem )
    {
        return STATUS_NO_MEMORY;
    }

    pItem->QueuedItem::QueuedItem();

    FltAcquirePushLockExclusive( &gQueueLock );
    pItem->InsertItem( &gQueueItems );
    FltReleasePushLock( &gQueueLock );
    
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


