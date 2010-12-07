#pragma once

typedef struct _AggregationItem
{
    ULONG       m_FilterId;
    VERDICT     m_Verdict;
} AggregationItem, *PAggregationItem;

class Aggregation
{
private:
    static ULONG        m_AllocTag;

public:
    Aggregation();
    ~Aggregation();

    ULONG
    GetCount();

    ULONG
    GetFilterId (
        __in_opt ULONG Position
        );

    VERDICT
    GetVerdict (
        __in_opt ULONG Position
        );

    __checkReturn
    NTSTATUS
    Allocate (
        __in ULONG ItemsCount
        );

    __checkReturn
    NTSTATUS
    PlaceValue (
        __in ULONG Position,
        __in ULONG FilterId,
        __in_opt VERDICT Verdict
        );

private:
    ULONG               m_Count;
    PAggregationItem    m_Items;
};

class EventData
{
public:
    Aggregation m_Aggregator;

    EventData (
        __in ULONG InterceptorId,
        __in ULONG Major,
        __in ULONG Minor,
        __in ULONG OperationType
        );

    ~EventData();

    ULONG
    GetInterceptorId();

    ULONG
    GetOperationId();

    ULONG
    GetMinor();

    ULONG
    GetOperationType();

    __checkReturn
    virtual
    NTSTATUS
    QueryParameter (
        __in_opt ULONG ParameterId,
        __drv_when(return==0, __deref_out_opt __drv_valueIs(!=0)) PVOID* Data,
        __deref_out_opt PULONG DataSize
        );

    __checkReturn
    virtual
    NTSTATUS
    ObjectRequest (
        __in ULONG Command,
        __out_opt PVOID OutputBuffer,
        __inout_opt PULONG OutputBufferSize
        );

protected:
    ULONG   m_InterceptorId;
    ULONG   m_Major;
    ULONG   m_Minor;
    ULONG   m_OperationType;
};

#define PEventData EventData*
