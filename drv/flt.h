#ifndef __flt_h
#define __flt_h

typedef
__checkReturn
NTSTATUS
( *PFN_QUERY_EVENT_PARAM ) (
    __in PVOID Opaque,
    __in_opt Parameters ParameterId,
    __deref_out_opt PVOID* Data,
    __deref_out_opt PULONG DataSize
    );

class EventData
{
private:

    PVOID                   m_Opaque;
    PFN_QUERY_EVENT_PARAM   m_QueryFunc;
    Interceptors            m_InterceptorId;
    ULONG                   m_Major;
    ULONG                   m_Minor;
    ULONG                   m_ParamsCount;
    PParameters             m_Params;

public:
    EventData (
        __in PVOID Opaque,
        __in PFN_QUERY_EVENT_PARAM QueryFunc,
        __in Interceptors InterceptorId,
        __in ULONG Major,
        __in ULONG Minor,
        __in ULONG ParamsCount,
        __in PParameters Params
        ) : m_Opaque( Opaque ),
        m_QueryFunc( QueryFunc ),
        m_InterceptorId( InterceptorId ),
        m_Major( Major ),
        m_Minor( Minor ),
        m_ParamsCount( ParamsCount ),
        m_Params( Params )
    {
        ASSERT( Opaque );
        ASSERT( QueryFunc );
    }

    ULONG
    GetParametersCount (
        )
    {
        return m_ParamsCount;
    }

    Parameters
    GetParameterId (
        __in_opt ULONG ParameterNumber
        )
    {
        ASSERT( ParameterNumber <= m_ParamsCount );

        return m_Params[ ParameterNumber ];
    }

    NTSTATUS
    QueryParameter (
        __in Parameters PrarameterId,
        __deref_out_opt PVOID* Data,
        __deref_out_opt PULONG DataSize
        )
    {
        return m_QueryFunc( m_Opaque, PrarameterId, Data, DataSize ); 
    }

    NTSTATUS
    QueryParameterAsUnicodeString (
        __in Parameters PrarameterId,
        __in PUNICODE_STRING String
        )
    {
        ULONG lenght;
        PVOID buffer;
        NTSTATUS status = QueryParameter (
            PrarameterId,
            &buffer,
            &lenght
            );

        if ( NT_SUCCESS( status ) )
        {
            String->Buffer = (PWSTR) buffer;
            String->Length = String->MaximumLength = (USHORT) lenght;
        }

        return status;
    }
};

NTSTATUS
FilterEvent (
    __in EventData *Event,
    __inout PVERDICT Verdict
    );

#endif //__flt_h
