#ifndef __filehlp_h
#define __filehlp_h

DriverOperationId
FileOperationSystemToInternal (
    ULONG OperationId
    );

__checkReturn
BOOLEAN
IsPrefetchEcpPresent (
    __in PFLT_FILTER Filter,
    __in PFLT_CALLBACK_DATA Data
    );

__checkReturn
NTSTATUS
QueryFileNameInfo (
    __in PFLT_CALLBACK_DATA Data,
    __drv_when(return==0, __out_opt __drv_valueIs(!=0))
    PFLT_FILE_NAME_INFORMATION* FileNameInfo
    );

void
ReleaseFileNameInfo (
    __in_opt PFLT_FILE_NAME_INFORMATION* FileNameInfo
    );

void
ReleaseContext (
    __in_opt PFLT_CONTEXT* Context
    );

void
SecurityFreeSid (
    __in PSID* Sid
    );

__checkReturn
 NTSTATUS
FileIsMarkedForDelete (
    __in PFLT_INSTANCE Instance,
    __in PFILE_OBJECT FileObject,
    __out PBOOLEAN IsMarked
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
GenerateStreamContext (
    __in PFLT_FILTER Filter,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __drv_when(return==0, __out_opt __drv_valueIs(!=0))
    PSTREAM_CONTEXT* StreamContext
    );

__checkReturn
NTSTATUS
GetStreamHandleContext (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PSTREAMHANDLE_CONTEXT* StreamHandleContext
    );

__checkReturn
NTSTATUS
GenerateStreamHandleContext (
    __in PFLT_FILTER Filter,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PSTREAMHANDLE_CONTEXT* StreamHandleContext
    );

#endif // __filehlp_h