#ifndef __eventqueue_h
#define __eventqueue_h

struct QueuedItem
{
public:
        LIST_ENTRY      m_List;
public:

        QueuedItem();
        ~QueuedItem();

        ULONG
        GetId();

        VOID
        WaitForRelease();

        VOID
        Release (
            );

        VOID
        InsertItem (
            PLIST_ENTRY List
            );

        VOID
        RemoveItem();

private:
    EX_RUNDOWN_REF  m_Ref;
    ULONG           m_Id;
};

// QUEUE procedures

VOID
EventQueue_Init (
    );

VOID
EventQueue_Done (
    );

NTSTATUS
EventQueue_Add (
    __in PVOID Event,
    __deref_out_opt QueuedItem **Item
    );

VOID
EventQueue_WaitAndDestroy (
    __in QueuedItem **Item
    );

NTSTATUS
EventQueue_Lookup (
    __in ULONG EventId,
    __deref_out_opt QueuedItem **Item,
    __out_opt PVOID *Event
    );

#endif // __eventqueue_h