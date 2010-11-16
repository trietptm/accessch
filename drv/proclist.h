#ifndef __proclist_h
#define __proclist_h

typedef void ( *_tpProcessExitCb )( HANDLE ProcessId );

class ProcList
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
    NTSTATUS
    RegisterExitProcessCb (
        __in _tpProcessExitCb CbFunc
        );

    static
    void
    UnregisterExitProcessCb ( 
        __in _tpProcessExitCb CbFunc
        );

private:
    static ULONG            m_AllocTag;
    static EX_PUSH_LOCK     m_TreeAccessLock;
    static RTL_AVL_TABLE    m_Tree;

    static EX_PUSH_LOCK     m_CbAccessLock;
    static LIST_ENTRY       m_ExitProcessCbList;

//////////////////////////////////////////////////////////////////////////
    
    static
    RTL_GENERIC_COMPARE_RESULTS
    NTAPI
    Compare (
        __in struct _RTL_AVL_TABLE *Table,
        __in PVOID FirstStruct,
        __in PVOID SecondStruct
        );

    static
    PVOID
    NTAPI
    Allocate (
        __in struct _RTL_AVL_TABLE *Table,
        __in CLONG ByteSize
        );

    static
    void
    NTAPI
    Free (
        __in struct _RTL_AVL_TABLE *Table,
        __in __drv_freesMem(Mem) __post_invalid PVOID Buffer
        );

    //////////////////////////////////////////////////////////////////////////

    static
    void
    NTAPI
    CreateProcessNotifyExCb (
        __inout PVOID Process,
        __in HANDLE ProcessId,
        __in_opt PVOID CreateInfo
        );

    static
    void
    NTAPI
    CreateProcessNotifyCb (
        __in HANDLE ParentId,
        __in HANDLE ProcessId,
        __in BOOLEAN Create
        );

    static
    __checkReturn
    NTSTATUS
    RegisterNewProcess (
        HANDLE ProcessId
        );

    static
    void
    UnregisterProcess (
        HANDLE ProcessId
        );
};

#endif //__proclist_h