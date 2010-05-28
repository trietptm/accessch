#ifndef __eventqueue_h
#define __eventqueue_h

struct QueuedItem
{
public:
        static
        VOID
        Initialize (
            );

        static
        VOID
        Destroy (
            );

        static
        NTSTATUS
        Add (
            __in PVOID Event,
            __deref_out_opt QueuedItem **Item
            );

        static
        NTSTATUS
            Lookup (
            __in ULONG EventId,
            __deref_out_opt QueuedItem **Item
            );

public:
        QueuedItem (
            __in PVOID Data
            );
        
        ~QueuedItem();

        VOID
        WaitAndDestroy (
            );

        ULONG
        GetId (
            );

        NTSTATUS
        Acquire (
            );

        VOID
        Release (
            );

        inline PVOID GetData (
            )
        {
            return m_Data;
        }

private:
    static LONG         m_EventId;
    static LIST_ENTRY   m_QueueItems;
    static EX_PUSH_LOCK m_QueueLock;
    
    LIST_ENTRY   m_List;
    EX_RUNDOWN_REF      m_Ref;
    ULONG               m_Id;
    PVOID               m_Data;

    VOID
    WaitForRelease();
};

#endif // __eventqueue_h