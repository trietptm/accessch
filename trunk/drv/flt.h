#ifndef __flt_h
#define __flt_h

typedef enum Interceptors
{
    FILE_MINIFILTER = 0,
};

typedef enum Parameters
{
    PARAMETER_FILE_NAME             = 0,
    PARAMETER_VOLUME_NAME           = 1,
    PARAMETER_REQUESTOR_PROCESS_ID  = 2,
    PARAMETER_CURRENT_THREAD_ID     = 3,
} *PParameters;

typedef ULONG VERDICT, *PVERDICT;
#define VERDICT_NOT_FILTERED    0x0000
#define VERDICT_ALLOW           0x0001
#define VERDICT_NOT_DENY        0x0002
#define VERDICT_ASK             0x0004

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
public:

    PVOID                   m_Opaque;
    PFN_QUERY_EVENT_PARAM   m_QueryFunc;
    Interceptors            m_InterceptorId;
    ULONG                   m_Major;
    ULONG                   m_Minor;
    ULONG                   m_ParamsCount;
    PParameters              m_Params;

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