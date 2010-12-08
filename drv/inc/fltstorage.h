#pragma once

#include "../../inc/fltcommon.h"
#include "../inc/fltevents.h"

class Filters;
class FilterBoxList;

class FiltersStorage
{
public:
    static ULONG     m_AllocTag;
    static RTL_AVL_COMPARE_ROUTINE Compare;
    static RTL_AVL_ALLOCATE_ROUTINE Allocate;
    static RTL_AVL_FREE_ROUTINE Free;

public:
    FiltersStorage (
        );

    ~FiltersStorage (
        );

    void
    Lock();

    void
    UnLock();

    __checkReturn
    NTSTATUS
    AddFilterUnsafe (
        __in ULONG Interceptor,
        __in ULONG OperationId,
        __in ULONG FunctionMi,
        __in ULONG OperationType,
        __in UCHAR GroupId,
        __in VERDICT Verdict,
        __in HANDLE ProcessId,
        __in_opt ULONG RequestTimeout,
        __in PARAMS_MASK WishMask,
        __in_opt ULONG ParamsCount,
        __in_opt PFltParam Params,
        __out PULONG FilterId
        );

    __checkReturn
    NTSTATUS
    CreateBoxUnsafe (
        __in LPGUID Guid,
        __in ULONG ParamsCount,
        __in_opt PFltParam Params,
        __out PULONG FilterId
        );
  
    void
    DeleteAllFilters (
        );

    void
    CleanupFiltersByPid (
        __in HANDLE ProcessId,
        __in PVOID Context
        );

    BOOLEAN
    IsActive();
    
    __checkReturn
    NTSTATUS
    ChangeState (
        __in_opt BOOLEAN Activate
        );

    __checkReturn
    NTSTATUS
    FilterEvent (
        __in EventData* Event,
        __in PVERDICT Verdict,
        __in PPARAMS_MASK ParamsMask
        );
  
private:
    LONG
    GetNextFilterid();

    __checkReturn
    NTSTATUS
    CreateBoxControlp (
        );

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
    GetOrCreateFiltersByUnsafep (
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
