#ifndef __filehlp_h
#define __filehlp_h

typedef struct _INSTANCE_CONTEXT
{
    DEVICE_TYPE             m_VolumeDeviceType;
    FLT_FILESYSTEM_TYPE     m_VolumeFilesystemType;
} INSTANCE_CONTEXT, *PINSTANCE_CONTEXT;

typedef struct _STREAM_CONTEXT
{
    PINSTANCE_CONTEXT       m_InstanceContext;
    LONG                    m_Flags;
} STREAM_CONTEXT, *PSTREAM_CONTEXT;

typedef struct _STREAM_HANDLE_CONTEXT
{
    PSTREAM_CONTEXT         m_StreamContext;
    LUID                    m_Luid;
} STREAM_HANDLE_CONTEXT, *PSTREAM_HANDLE_CONTEXT;

// ----------------------------------------------------------------------------

class FileInterceptorContext
{
private:
    // intercepted data
    PFLT_CALLBACK_DATA          m_Data;
    PCFLT_RELATED_OBJECTS       m_FltObjects;
    
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

    ULONG                       m_Dummy;

private:
    __checkReturn
    NTSTATUS
    CheckAccessContext (
        );

    __checkReturn
    NTSTATUS
    CreateSectionForData (
        __deref_out PHANDLE Section,
        __out PLARGE_INTEGER Size
        );

public:
    FileInterceptorContext (
        PFLT_CALLBACK_DATA Data,
        PCFLT_RELATED_OBJECTS FltObjects
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
        __in NOTIFY_COMMANDS Command,
        __in_opt PVOID OutputBuffer,
        __inout_opt PULONG OutputBufferSize
        );
};

//////////////////////////////////////////////////////////////////////////
__checkReturn
NTSTATUS
QueryFileNameInfo (
    __in PFLT_CALLBACK_DATA Data,
    __deref_out_opt PFLT_FILE_NAME_INFORMATION* ppFileNameInfo
    );

void
ReleaseFileNameInfo (
    __in_opt PFLT_FILE_NAME_INFORMATION* ppFileNameInfo
    );

#define ReleaseContext( _context ) ReleaseContextImp( (PFLT_CONTEXT*) _context )

void
ReleaseContextImp (
    __in_opt PFLT_CONTEXT* ppContext
    );

void
SecurityFreeSid (
    __in PSID* ppSid
    );

__checkReturn
NTSTATUS
FileQueryParameter (
    __in PVOID Opaque,
    __in_opt Parameters ParameterId,
    __deref_out_opt PVOID* Data,
    __deref_out_opt PULONG DataSize
    );

__checkReturn
NTSTATUS
FileObjectRequest (
    __in PVOID Opaque,
    __in NOTIFY_COMMANDS Command,
    __in_opt PVOID OutputBuffer,
    __inout_opt PULONG OutputBufferSize
    );

__checkReturn
NTSTATUS
GenerateStreamContext (
    __in PFLT_FILTER Filter,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PSTREAM_CONTEXT* ppStreamContext
    );

#endif // __filehlp_h