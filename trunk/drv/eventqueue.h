#ifndef __eventqueue_h
#define __eventqueue_h

struct QueuedItem
{
public:

        QueuedItem();
        ~QueuedItem();

        ULONG
        GetId();

        VOID
        WaitForRelease();

        VOID
        InsertItem (
            PLIST_ENTRY List
            );

        VOID
        RemoveItem();

private:
    LIST_ENTRY      m_List;
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

#endif // __eventqueue_h