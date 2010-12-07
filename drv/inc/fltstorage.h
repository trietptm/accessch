#pragma once

class Filters;
class FilterBoxList;

class FiltersStorage
{
public:
    static ULONG     m_AllocTag;

public:
    FiltersStorage (
        );

    ~FiltersStorage (
        );

    void
    DeleteAllFilters (
        );

    void
    CleanupFiltersByPid (
        __in HANDLE ProcessId,
        __in PVOID Context
        );

    LONG
    GetNextFilterid();

    BOOLEAN
    IsActive();
    
    __checkReturn
    NTSTATUS
    ChangeState (
        __in_opt BOOLEAN Activate
        );

    static RTL_AVL_COMPARE_ROUTINE Compare;
    static RTL_AVL_ALLOCATE_ROUTINE Allocate;
    static RTL_AVL_FREE_ROUTINE Free;
  
private:
    void
    CleanupFiltersByPidp (
        __in HANDLE ProcessId
        );

    __checkReturn
    Filters*
    GetFiltersByp (
        __in ULONG Interceptor,
        __in ULONG Operation,
        __in_opt ULONG Minor,
        __in ULONG OperationType
        );
    
    __checkReturn
    Filters*
    GetOrCreateFiltersByp (
        __in ULONG Interceptor,
        __in ULONG Operation,
        __in_opt ULONG Minor,
        __in ULONG OperationType
        );

private:
    RTL_AVL_TABLE    m_Tree;
    EX_PUSH_LOCK     m_AccessLock;
    LONG             m_FilterIdCounter;
    LONG             m_Flags;
    FilterBoxList*   m_BoxList;
};
