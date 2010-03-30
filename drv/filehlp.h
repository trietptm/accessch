#ifndef __filehlp_h
#define __filehlp_h

#include "flt.h"

typedef struct _INSTANCE_CONTEXT
{
    DEVICE_TYPE             m_VolumeDeviceType;
    FLT_FILESYSTEM_TYPE     m_VolumeFilesystemType;
} INSTANCE_CONTEXT, *PINSTANCE_CONTEXT;

typedef struct _STREAM_CONTEXT
{
    LUID                    m_Luid;
    LONG                    m_Flags;
} STREAM_CONTEXT, *PSTREAM_CONTEXT;

typedef struct _STREAM_HANDLE_CONTEXT
{
} STREAM_HANDLE_CONTEXT, *PSTREAM_HANDLE_CONTEXT;

// ----------------------------------------------------------------------------

#define _VOLUME_DESCRIPTION_LENGTH  0x20
typedef struct _VOLUME_CONTEXT
{
    PFLT_INSTANCE           m_Instance;
    STORAGE_BUS_TYPE        m_BusType;
    DEVICE_REMOVAL_POLICY   m_RemovablePolicy;
    UCHAR                   m_VendorId[_VOLUME_DESCRIPTION_LENGTH];
    UCHAR                   m_ProductId[_VOLUME_DESCRIPTION_LENGTH];
    UCHAR                   m_ProductRevisionLevel[_VOLUME_DESCRIPTION_LENGTH];
    UCHAR                   m_VendorSpecific[_VOLUME_DESCRIPTION_LENGTH];
} VOLUME_CONTEXT, *PVOLUME_CONTEXT;

class FileInterceptorContext
{
private:
    PFLT_CALLBACK_DATA          m_Data;
    PCFLT_RELATED_OBJECTS       m_FltObjects;

    HANDLE                      m_RequestorProcessId;
    HANDLE                      m_RequestorThreadId;
    PINSTANCE_CONTEXT           m_InstanceContext;
    PSTREAM_CONTEXT             m_StreamContext;
    PFLT_FILE_NAME_INFORMATION  m_FileNameInfo;
    PSID                        m_Sid;
    LUID                        Luid;

public:
    FileInterceptorContext (
        PFLT_CALLBACK_DATA Data,
        PCFLT_RELATED_OBJECTS FltObjects
        );

    ~FileInterceptorContext (
        );

    __checkReturn
    NTSTATUS
    QueryFileParameter (
        __in_opt Parameters ParameterId,
        __deref_out_opt PVOID* Data,
        __deref_out_opt PULONG DataSize
        );
};

void
ReleaseFileNameInfo (
    __in_opt PFLT_FILE_NAME_INFORMATION* ppFileNameInfo
    );

void
ReleaseContext (
    __in_opt PFLT_CONTEXT* ppContext
    );

void
SecurityFreeSid (
    __in PSID* ppSid
    );

__checkReturn
NTSTATUS
QueryFileParameter (
    __in PVOID Opaque,
    __in_opt Parameters ParameterId,
    __deref_out_opt PVOID* Data,
    __deref_out_opt PULONG DataSize
    );

#endif // __filehlp_h