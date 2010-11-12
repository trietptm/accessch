#ifndef __eventqueue_h
#define __eventqueue_h

class QueuedItem
{
public:
    static
    void
    Initialize (
        );

    static
    void
    Destroy (
        );

    static
    __checkReturn
    NTSTATUS
    Add (
        __in PVOID Event,
        __drv_when(return==0, __deref_out_opt __drv_valueIs(!=0)) QueuedItem **Item
        );

    static
    __checkReturn
    NTSTATUS
    Lookup (
        __in ULONG EventId,
        __drv_when(return==0, __deref_out_opt __drv_valueIs(!=0)) QueuedItem **Item
        );

public:
    QueuedItem (
        __in PVOID Data
        );

    ~QueuedItem();

    void
    WaitAndDestroy (
        );

    ULONG
    GetId (
        );

    NTSTATUS
    Acquire (
        );

    void
    Release (
        );

    inline
    PVOID
    GetData (
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

    void
    WaitForRelease();
};

#endif // __eventqueue_h