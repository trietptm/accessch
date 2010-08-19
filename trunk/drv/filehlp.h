#ifndef __filehlp_h
#define __filehlp_h

DriverOperationId
FileOperationSystemToInternal (
    ULONG OperationId
    );

__checkReturn
NTSTATUS
QueryFileNameInfo (
    __in PFLT_CALLBACK_DATA Data,
    __deref_out_opt PFLT_FILE_NAME_INFORMATION* FileNameInfo
    );

void
ReleaseFileNameInfo (
    __in_opt PFLT_FILE_NAME_INFORMATION* FileNameInfo
    );

#define ReleaseContext( _context ) ReleaseContextImp( (PFLT_CONTEXT*) _context )

void
ReleaseContextImp (
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
    __deref_out_opt PSTREAM_CONTEXT* StreamContext
    );

#endif // __filehlp_h