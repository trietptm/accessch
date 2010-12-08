#pragma once

#include "../../inc/fltcommon.h"
#include "../inc/fltevents.h"

class FilterBox;

typedef struct _FltCheckData
{
    ULONG               m_DataSize;
    ULONG               m_Count;
    UCHAR               m_Data[1];
} FltCheckData, *PFltCheckData;

#define PosListItemType  ULONG

enum CheckEntryType
{
    CheckEntryInvalid   = 0,
    CheckEntryGeneric   = 1,
    CheckEntryBox       = 2
};

class ParamCheckEntry
{
private:
    static ULONG m_AllocTag;
public:
    ParamCheckEntry();
    ~ParamCheckEntry();

    __checkReturn
    NTSTATUS
    Attach (
        __in ULONG DataSize,
        __in ULONG Count,
        __in PUCHAR Data
        );

public:
    LIST_ENTRY          m_List;
    ULONG               m_Flags;    // _PARAM_ENTRY_FLAG_XXX
    ULONG               m_PosCount;
    PosListItemType*    m_FilterPosList;
    
    CheckEntryType      m_Type;
    union
    {
        struct {
            ULONG               m_Parameter;
            FltOperation        m_Operation;
            FltCheckData*       m_CheckData;
        } Generic;

        struct {
            FilterBox*          m_Box;
            PRTL_BITMAP         m_Affecting;
        } Container;
    };
};

NTSTATUS
CheckEntry (
    __in ParamCheckEntry* Entry,
    __in EventData *Event
    );