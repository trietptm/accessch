#pragma once

typedef void ( *_tpProcessExitCb )( HANDLE ProcessId, PVOID Opaque );

void
RegisterProcess (
    HANDLE ProcessId
    );

void
UnregisterProcess (
    HANDLE ProcessId
    );

//////////////////////////////////////////////////////////////////////////

class ProcessHelper
{
public:
    static ULONG            m_AllocTag;

public:

    ProcessHelper (
        );

    ~ProcessHelper (
        );

    /// \todo перевести на наследование от базового IInterface с AddRef\Release функциями
    NTSTATUS
    AddRef (
        );

    void
    Release (
        );

    __checkReturn
    NTSTATUS
    RegisterProcessItem (
        HANDLE ProcessId
        );

    void
    UnregisterProcessItem (
        HANDLE ProcessId
        );

    NTSTATUS
    RegisterExitProcessCb (
        __in _tpProcessExitCb CbFunc,
        __in_opt PVOID Opaque
        );

    void
    UnregisterExitProcessCb ( 
        __in _tpProcessExitCb CbFunc
        );

private:
    LONG            m_RefCount;
    EX_PUSH_LOCK    m_TreeAccessLock;
    RTL_AVL_TABLE   m_Tree;

    EX_PUSH_LOCK    m_CbAccessLock;
    LIST_ENTRY      m_ExitProcessCbList;

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
};
