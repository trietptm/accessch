#include "pch.h"
#include "../inc/accessch.h"
#include "flt.h"

// \todo описать типы для фильтрации!
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
    __inout PVERDICT Verdict,
    __out PARAMS_MASK *ParamsMask
    )
{
    UNREFERENCED_PARAMETER( Event );
    UNREFERENCED_PARAMETER( Verdict );

    *Verdict = VERDICT_ASK;

    *ParamsMask =
        Id2Bit( PARAMETER_FILE_NAME )
        |
        Id2Bit ( PARAMETER_VOLUME_NAME )
        |
        Id2Bit ( PARAMETER_REQUESTOR_PROCESS_ID );
    
    return STATUS_SUCCESS;
}