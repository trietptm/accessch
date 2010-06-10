#ifndef __flt_h
#define __flt_h

class InterceptorContext
{
public:
    InterceptorContext (
        __in OperationPoint OperationType
        ) :
        m_OperationType( OperationType )

    {

    };

    ~InterceptorContext()
    {

    }

    inline
    OperationPoint
    GetOperationType (
        )
    {
        return m_OperationType;
    }

    __checkReturn
    virtual
    NTSTATUS
    QueryParameter (
        __in_opt Parameters ParameterId,
        __deref_out_opt PVOID* Data,
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
    virtual
    NTSTATUS
    ObjectRequest (
        __in NOTIFY_ID Command,
        __in_opt PVOID OutputBuffer,
        __inout_opt PULONG OutputBufferSize
        )
    {
        UNREFERENCED_PARAMETER( Command );
        UNREFERENCED_PARAMETER( OutputBuffer );
        UNREFERENCED_PARAMETER( OutputBufferSize );

        ASSERT( FALSE );

        return STATUS_NOT_IMPLEMENTED;
    }
    
protected:
    OperationPoint  m_OperationType;
};

class EventData
{
private:

    InterceptorContext*     m_InterceptorContext;
    Interceptors            m_InterceptorId;
    DriverOperationId       m_Major;
    ULONG                   m_Minor;

public:
    EventData (
        __in InterceptorContext* InterceptorContext,
        __in Interceptors InterceptorId,
        __in DriverOperationId Major,
        __in ULONG Minor
        ) : m_InterceptorContext( InterceptorContext ),
        m_InterceptorId( InterceptorId ),
        m_Major( Major ),
        m_Minor( Minor )
    {
    }

    NTSTATUS
    QueryParameter (
        __in Parameters PrarameterId,
        __deref_out_opt PVOID* Data,
        __deref_out_opt PULONG DataSize
        )
    {
        NTSTATUS status = m_InterceptorContext->QueryParameter (
            PrarameterId,
            Data,
            DataSize
            );

        return status;
    }

    NTSTATUS
    QueryParameterAsUnicodeString (
        __in Parameters PrarameterId,
        __in PUNICODE_STRING String
        )
    {
        ULONG lenght;
        PVOID buffer;
        NTSTATUS status = m_InterceptorContext->QueryParameter (
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

    __checkReturn
    NTSTATUS
    ObjectRequst (
        __in NOTIFY_ID Command,
        __in_opt PVOID OutputBuffer,
        __inout_opt PULONG OutputBufferSize
        )
    {
        NTSTATUS status = m_InterceptorContext->ObjectRequest (
            Command,
            OutputBuffer,
            OutputBufferSize
            );

        return status;
    }

    inline
    Interceptors
    GetInterceptorId (
        )
    {
        return m_InterceptorId;
    };

    inline
    DriverOperationId
    GetOperationId (
        )
    {
        return m_Major;
    };

    inline
    ULONG
    GetMinor (
        )
    {
        return m_Minor;
    }

    OperationPoint
    GetOperationType (
        )
    {
        return m_InterceptorContext->GetOperationType();
    }
};

VOID
DeleteAllFilters (
    );

__checkReturn
NTSTATUS
FilterEvent (
    __in EventData *Event,
    __inout PVERDICT Verdict,
    __out PARAMS_MASK *ParamsMask
    );

__checkReturn
NTSTATUS
FilterProceedChain (
    __in PFILTERS_CHAIN Chain,
    __in ULONG ChainSize
    );

#endif //__flt_h
