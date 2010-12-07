#pragma once

#include "../../inc/fltcommon.h"
#include "fltbox.h"

#define NumberOfBits 256
#define BitMapBufferSizeInUlong (NumberOfBits / 32)

class ParamCheckEntry;
struct FilterEntry;

class Filters
{
public:
    static ULONG        m_AllocTag;

public:
    Filters();
    ~Filters();

    __checkReturn
    NTSTATUS
    AddRef (
        );

    void
    Release();

    __checkReturn
    BOOLEAN
    IsEmpty();

    __checkReturn
    VERDICT
    GetVerdict (
        __in EventData *Event,
        __out PARAMS_MASK *ParamsMask
        );
    
    __checkReturn
    NTSTATUS
    AddFilter (
        __in UCHAR GroupId,
        __in VERDICT Verdict,
        __in HANDLE ProcessId,
        __in_opt ULONG RequestTimeout,
        __in PARAMS_MASK WishMask,
        __in_opt ULONG ParamsCount,
        __in_opt PFltParam Params,
        __in PFilterBoxList BoxList,
        __in ULONG FilterId
        );

    ULONG
    CleanupByProcess (
        __in HANDLE ProcessId
        );

private:
    __checkReturn
    NTSTATUS
    AddParamsUnsafe (
        __in ULONG Position,
        __in ULONG ParamsCount,
        __in PFltParam Params,
        __in PFilterBoxList BoxList
        );

    __checkReturn
    NTSTATUS
    GetFilterPosUnsafe (
    PULONG Position
        );

    __checkReturn
    NTSTATUS
    TryToFindExisting (
        __in PFltParam ParamEntry,
        __in ULONG Position,
        __deref_out_opt ParamCheckEntry** Entry
        );

    ParamCheckEntry*
    AddParameterWithFilterPos (
        __in PFltParam ParamEntry,
        __in ULONG Position,
        __in PFilterBoxList BoxList
        );

    void
    DeleteParamsByFilterPosUnsafe (
        __in_opt ULONG Position
        );

    void
    MoveFilterPosInParams (
        ULONG IdxFrom,
        ULONG IdxTo
        );

    NTSTATUS
    CheckEntryUnsafe (
        __in ParamCheckEntry* Entry,
        __in EventData *Event
        );

    NTSTATUS
    CheckGenericUnsafe (
        __in ParamCheckEntry* Entry,
        __in EventData *Event
        );

    NTSTATUS
    CheckContainerUnsafe (
        __in ParamCheckEntry* Entry,
        __in EventData *Event
        );

    NTSTATUS
    CheckParamsList (
        __in EventData *Event,
        __in PULONG Unmatched,
        __in PRTL_BITMAP Filtersbitmap
        );

private:
    EX_RUNDOWN_REF      m_Ref;
    EX_PUSH_LOCK        m_AccessLock;

    RTL_BITMAP          m_GroupsMap;
    ULONG               m_GroupsMapBuffer[ BitMapBufferSizeInUlong ];
    ULONG               m_GroupCount;

    RTL_BITMAP          m_ActiveFilters;
    ULONG               m_ActiveFiltersBuffer[ BitMapBufferSizeInUlong ];
    ULONG               m_FiltersCount;
    FilterEntry*        m_FiltersArray;
    LIST_ENTRY          m_ParamsCheckList;
};
