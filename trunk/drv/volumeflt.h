#ifndef __volume_h
#define __volume_h

class VolumeInterceptorContext : public InterceptorContext
{
public:
    VolumeInterceptorContext (
        __in PCFLT_RELATED_OBJECTS FltObjects,
        __in PINSTANCE_CONTEXT InstanceContext,
        __in OperationPoint OperationType
        );

    ~VolumeInterceptorContext (
        );

    __checkReturn
    NTSTATUS
    QueryParameter (
        __in_opt Parameters ParameterId,
        __deref_out_opt PVOID* Data,
        __deref_out_opt PULONG DataSize
        );

    __checkReturn
    NTSTATUS
    ObjectRequest (
        __in NOTIFY_ID Command,
        __in_opt PVOID OutputBuffer,
        __inout_opt PULONG OutputBufferSize
        );

private:
    HANDLE                  m_RequestorPid;
    PCFLT_RELATED_OBJECTS   m_FltObjects;
    PINSTANCE_CONTEXT       m_InstanceContext;
};

#endif // __volume_h