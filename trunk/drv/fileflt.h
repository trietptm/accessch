#ifndef __fileflt_h
#define __fileflt_h

class FileInterceptorContext : public InterceptorContext
{
private:
    // intercepted data
    PFLT_CALLBACK_DATA          m_Data;
    PCFLT_RELATED_OBJECTS       m_FltObjects;
    OperationPoint              m_OperationType;

    // service field
    PSTREAM_CONTEXT             m_StreamContext;

    // data access
    HANDLE                      m_Section;
    PVOID                       m_SectionObject;
    PVOID                       m_MappedBase;

    // queryed parameters
    HANDLE                      m_RequestorProcessId;
    HANDLE                      m_RequestorThreadId;
    PINSTANCE_CONTEXT           m_InstanceContext;
    PFLT_FILE_NAME_INFORMATION  m_FileNameInfo;
    PSID                        m_Sid;
    LUID                        m_Luid;

    ACCESS_MASK                 m_DesiredAccess;
    ULONG                       m_CreateOptions;
    ULONG                       m_CreateMode;

private:
    __checkReturn
        NTSTATUS
        CheckAccessToStreamContext (
        );

    __checkReturn
        NTSTATUS
        CreateSectionForData (
        __deref_out PHANDLE Section,
        __out PLARGE_INTEGER Size
        );

public:
    FileInterceptorContext (
        __in PFLT_CALLBACK_DATA Data,
        __in PCFLT_RELATED_OBJECTS FltObjects,
        __in OperationPoint OperationType
        );

    ~FileInterceptorContext (
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
};

#endif // __fileflt_h