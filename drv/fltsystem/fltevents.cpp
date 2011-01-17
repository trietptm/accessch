#include "../inc/commonkrnl.h"
#include "../inc/memmgr.h"
#include "../inc/fltevents.h"

ULONG Aggregation::m_AllocTag = 'gaSA';

Aggregation::Aggregation (
    )
{
    m_Count = 0;
    m_Items = NULL;
}
    
Aggregation::~Aggregation (
    )
{
    if ( m_Items )
    {
        FREE_POOL( m_Items );
    }
}

ULONG
Aggregation::GetCount (
    )
{
    return m_Count;
}

ULONG
Aggregation::GetFilterId (
    __in_opt ULONG Position
    )
{
    ASSERT( Position < m_Count );

    return m_Items[ Position ].m_FilterId;
}

VERDICT
Aggregation::GetVerdict (
    __in_opt ULONG Position
    )
{
    ASSERT( Position < m_Count );
        
    return m_Items[ Position ].m_Verdict;
}

__checkReturn
NTSTATUS
Aggregation::Allocate (
    __in ULONG ItemsCount
    )
{
    ASSERT( ItemsCount );
        
    if ( !ItemsCount )
    {
        return STATUS_INVALID_PARAMETER;
    }

    ULONG size = sizeof( AggregationItem ) * ItemsCount;
    m_Items = (PAggregationItem) ExAllocatePoolWithTag (
        PagedPool,
        size,
        m_AllocTag
        );

    if ( !m_Items )
    {
        return STATUS_INSUFF_SERVER_RESOURCES;
    }

    RtlZeroMemory( m_Items, size );
    m_Count = ItemsCount;

    return STATUS_SUCCESS;
}

__checkReturn
NTSTATUS
Aggregation::PlaceValue (
    __in ULONG Position,
    __in ULONG FilterId,
    __in_opt VERDICT Verdict
    )
{
    ASSERT( Position < m_Count );

    if ( Position >= m_Count )
    {
        return STATUS_INVALID_PARAMETER;
    }

    m_Items[ Position].m_FilterId = FilterId;
    m_Items[ Position].m_Verdict = Verdict;
        
    return STATUS_SUCCESS;
}

EventData::EventData (
    __in ULONG InterceptorId,
    __in ULONG Major,
    __in ULONG Minor,
    __in ULONG OperationType
    ) :
    m_InterceptorId( InterceptorId ),
    m_Major( Major ),
    m_Minor( Minor ),
    m_OperationType( OperationType )
{

};

EventData::~EventData (
    )
{

}
ULONG
EventData::GetInterceptorId (
    )
{
    return m_InterceptorId;
};

ULONG
EventData::GetOperationId (
    )
{
    return m_Major;
};

ULONG
EventData::GetMinor (
    )
{
    return m_Minor;
}

ULONG
EventData::GetOperationType (
    )
{
    return m_OperationType;
}

__checkReturn
NTSTATUS
EventData::QueryParameter (
    __in_opt ULONG ParameterId,
    __drv_when(return==0, __deref_out_opt __drv_valueIs(!=0)) PVOID* Data,
    __deref_out_opt PULONG DataSize
    )
{
    UNREFERENCED_PARAMETER( ParameterId );
    UNREFERENCED_PARAMETER( Data );
    UNREFERENCED_PARAMETER( DataSize );

    ASSERT( FALSE );

    return STATUS_NOT_IMPLEMENTED;
}

__checkReturn
NTSTATUS
EventData::ObjectRequest (
    __in ULONG Command,
    __out_opt PVOID OutputBuffer,
    __inout_opt PULONG OutputBufferSize
    )
{
    UNREFERENCED_PARAMETER( Command );
    UNREFERENCED_PARAMETER( OutputBuffer );
    UNREFERENCED_PARAMETER( OutputBufferSize );

    ASSERT( FALSE );

    return STATUS_NOT_IMPLEMENTED;
}