#ifndef __eventqueue_h
#define __eventqueue_h

struct QueuedItem
{
public:
        LIST_ENTRY      m_List;
public:

        QueuedItem (
            __in PVOID Data
            );
        
        ~QueuedItem();

        ULONG
        GetId();

        NTSTATUS
        Acquire (
            );

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

        PVOID GetData (
            )
        {
            return m_Data;
        }

private:
    EX_RUNDOWN_REF  m_Ref;
    ULONG           m_Id;
    PVOID           m_Data;
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
    __deref_out_opt QueuedItem **Item
    );

#endif // __eventqueue_h