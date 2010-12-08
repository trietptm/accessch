#ifndef __proclist_h
#define __proclist_h

#include "../inc/processinfo.h"

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
        __in _tpProcessExitCb CbFunc,
        __in_opt PVOID Opaque
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
    RTL_AVL_COMPARE_ROUTINE
    Compare;

    static
    RTL_AVL_ALLOCATE_ROUTINE
    Allocate;

    static
    RTL_AVL_FREE_ROUTINE
    Free;

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