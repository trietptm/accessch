#ifndef __flt_h
#define __flt_h

class EventData
{
public:
    EventData (
        __in Interceptors InterceptorId,
        __in DriverOperationId Major,
        __in ULONG Minor,
        __in OperationPoint OperationType
        ) :
        m_InterceptorId( InterceptorId ),
        m_Major( Major ),
        m_Minor( Minor ),
        m_OperationType( OperationType )
    {

    };

    ~EventData()
    {

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
        return m_OperationType;
    }

    __checkReturn
    virtual
    NTSTATUS
    QueryParameter (
        __in_opt Parameters ParameterId,
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
    virtual
    NTSTATUS
    ObjectRequest (
        __in NOTIFY_ID Command,
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
    
protected:
    Interceptors            m_InterceptorId;
    DriverOperationId       m_Major;
    ULONG                   m_Minor;
    OperationPoint          m_OperationType;
};

 /*   NTSTATUS
    QueryParameterAsUnicodeString (
        __in Parameters PrarameterId,
        __out PUNICODE_STRING String
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
    }*/

VOID
RemoveAllFilters (
    );

__checkReturn
 BOOLEAN
 FilterIsExistAny (
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
    __in ULONG ChainSize,
    __out PULONG FilterId
    );

__checkReturn
NTSTATUS
FilterChangeState (
    BOOLEAN Activate
    );

#endif //__flt_h
