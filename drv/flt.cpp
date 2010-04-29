#include "pch.h"
#include "../inc/accessch.h"
#include "flt.h"

//! \todo: описать типы для фильтрации!
#define FilterId            ULONG
#define OperationId         ULONG
#define FilterDataType      ULONG

struct FilterIdentity
{
    FilterId                m_Id;
    struct FilterIdentity*  m_Next;
};

struct FilterDataBinary
{
    ULONG                   m_Size;
    UCHAR                   m_Data[1];
};

struct FilterOperand
{
    OperationId             m_Operation;
    FilterDataType          m_Type;
    union
    {
        FilterDataBinary    m_Binary;
    } m_Data;
};

struct ParameterEntry
{
    Parameters              m_Parameter;
    struct ParameterEntry*  m_HorizontalNext;
    FilterOperand           m_Operand;
}*PParameterEntry;

struct ParametersArray
{
    struct ParametersArray* m_VerticalNext;
    ParameterEntry*         m_HorizontalNext;
    FilterIdentity*         m_FilterIdentity;
}*PParametersArray;

struct Filter
{
    ParametersArray*        m_Array;
    ParameterEntry          m_Start;
};

// ---


NTSTATUS
FilterEvent (
    __in EventData *Event,
    __inout PVERDICT Verdict
    )
{
    UNREFERENCED_PARAMETER( Event );
    UNREFERENCED_PARAMETER( Verdict );

    *Verdict = VERDICT_ASK;

    return STATUS_SUCCESS;
}