#ifndef __volume_h
#define __volume_h

class VolumeInterceptorContext : public EventData
{
public:
    VolumeInterceptorContext (
        __in PCFLT_RELATED_OBJECTS FltObjects,
        __in PINSTANCE_CONTEXT InstanceContext,
        __in PVOLUME_CONTEXT pVolumeContext,
        __in Interceptors InterceptorId,
        __in DriverOperationId Major,
        __in ULONG Minor,
        __in OperationPoint OperationType
        );

    ~VolumeInterceptorContext (
        );

    __checkReturn
    NTSTATUS
    QueryParameter (
        __in_opt Parameters ParameterId,
        __drv_when(return==0, __deref_out_opt __drv_valueIs(!=0)) PVOID* Data,
        __deref_out_opt PULONG DataSize
        );

    __checkReturn
    NTSTATUS
    ObjectRequest (
        __in NOTIFY_ID Command,
        __out_opt PVOID OutputBuffer,
        __inout_opt PULONG OutputBufferSize
        );

private:
    HANDLE                  m_RequestorPid;
    PCFLT_RELATED_OBJECTS   m_FltObjects;
    PINSTANCE_CONTEXT       m_InstanceContext;
    PVOLUME_CONTEXT         m_VolumeContext;
};

#endif // __volume_h