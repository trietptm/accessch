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

typedef
__checkReturn
NTSTATUS
( *PFN_OBJECT_REQUEST ) (
    __in PVOID Opaque,
    __in NOTIFY_ID Command,
    __in_opt PVOID OutputBuffer,
    __inout_opt PULONG OutputBufferSize
    );

enum EVENT_FLAGS
{
    _EVENT_FLAG_NONE     = 0x0000,
};

class EventData
{
private:

    PVOID                   m_Opaque;
    PFN_QUERY_EVENT_PARAM   m_QueryFunc;
    PFN_OBJECT_REQUEST      m_pfnObjectRequest;
    Interceptors            m_InterceptorId;
    ULONG                   m_Major;
    ULONG                   m_Minor;
    EVENT_FLAGS             m_Flags;

public:
    EventData (
        __in PVOID Opaque,
        __in PFN_QUERY_EVENT_PARAM QueryFunc,
        __in_opt PFN_OBJECT_REQUEST ObjectRequest,
        __in Interceptors InterceptorId,
        __in ULONG Major,
        __in ULONG Minor
        ) : m_Opaque( Opaque ),
        m_QueryFunc( QueryFunc ),
        m_pfnObjectRequest( ObjectRequest),
        m_InterceptorId( InterceptorId ),
        m_Major( Major ),
        m_Minor( Minor )
    {
        ASSERT( Opaque );
        ASSERT( QueryFunc );

        m_Flags  = _EVENT_FLAG_NONE;
    }

    EVENT_FLAGS
    EventFlagsSet( 
        __in EVENT_FLAGS Flag
        )
    {
        return (EVENT_FLAGS) FlagOn( m_Flags, Flag );
    }

    EVENT_FLAGS
    EventFlagsGet (
        )
    {
        return m_Flags;
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

    NTSTATUS
    ObjectRequst (
        __in NOTIFY_ID Command,
        __in_opt PVOID OutputBuffer,
        __inout_opt PULONG OutputBufferSize
        )
    {
        if ( m_pfnObjectRequest )
        {
            return m_pfnObjectRequest (
                m_Opaque,
                Command,
                OutputBuffer,
                OutputBufferSize
                );
        }

        return STATUS_NOT_SUPPORTED;
    }
};

__checkReturn
NTSTATUS
FilterEvent (
    __in EventData *Event,
    __inout PVERDICT Verdict,
    __out PARAMS_MASK *ParamsMask
    );

#endif //__flt_h
