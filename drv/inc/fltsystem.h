#pragma once
#include  "../../inc/fltcommon.h"
#include  "../inc/fltevents.h"
#include  "../inc/fltstorage.h"

class FilteringSystem
{
public:
    static ULONG        m_AllocTag;

public:
    FilteringSystem();
    ~FilteringSystem();

    __checkReturn
    NTSTATUS
    Attach (
        __in FiltersStorage* FltStorage
        );

    void
    Detach (
        __in FiltersStorage* FltStorage
        );

    __checkReturn
    NTSTATUS
    FilterEvent (
        __in EventData* Event,
        __in PVERDICT Verdict,
        __in PPARAMS_MASK ParamsMask
        );

private:
    EX_PUSH_LOCK        m_AccessLock;
    LIST_ENTRY          m_List;
};
