#ifndef __flt_h
#define __flt_h

#include "../../inc/filters.h"

//////////////////////////////////////////////////////////////////////////
#include "fltstore.h"

class FilteringSystem: private FiltersTree
{
public:
    static ULONG        m_AllocTag;

    static EX_PUSH_LOCK m_AccessLock;
    static LIST_ENTRY   m_FltObjects;
    static LONG         m_ActiveCount;

    FilteringSystem();
    ~FilteringSystem();

    static void Initialize();
    static void Destroy();

    static
    void
    Attach (
        __in FilteringSystem* FltObject
    );

    static
    void
    Detach (
        __in FilteringSystem* FltObject
        );

    static
    __checkReturn
    BOOLEAN
    IsExistFilters (
        );

    static
    __checkReturn
    NTSTATUS
    FilterEvent (
        __in EventData *Event,
        __inout PVERDICT Verdict,
        __out PARAMS_MASK *ParamsMask
        );

public:
    NTSTATUS
    AddRef();

    void
    Release();

    BOOLEAN
    IsActive (
        );

    void
    RemoveAllFilters (
        );

    __checkReturn
    NTSTATUS
    ProceedChain (
        __in PFILTERS_CHAIN Chain,
        __in ULONG ChainSize,
        __out PULONG FilterId
        );

    __checkReturn
    NTSTATUS
    ChangeState (
        BOOLEAN Activate
        );

    __checkReturn
    NTSTATUS
    SubFilterEvent (
        __in EventData *Event,
        __inout PVERDICT Verdict,
        __out PARAMS_MASK *ParamsMask
        );

private:
     EX_RUNDOWN_REF m_Ref;
     LIST_ENTRY     m_List;

    __checkReturn
    NTSTATUS
    ProceedChainGeneric (
        __in PCHAIN_ENTRY pEntry,
        __out PULONG FilterId
        );

    __checkReturn
    NTSTATUS
    ProceedChainBox (
        __in PCHAIN_ENTRY pEntry,
        __out PULONG FilterId
        );
};

#endif //__flt_h
