#include "pch.h"
#include "main.h"
#include "../inc/accessch.h"

#include "filehlp.h"
#include "security.h"

/// \todo FILE_OPEN_NO_RECALL

DriverOperationId
FileOperationSystemToInternal (
    ULONG OperationId
    )
{
    switch ( OperationId )
    {
    case IRP_MJ_CREATE:
        return OP_FILE_CREATE;
    
    case IRP_MJ_CLEANUP:
        return OP_FILE_CLEANUP;

    default:
        __debugbreak();
    }

    return OP_UNKNOWN;
}

//////////////////////////////////////////////////////////////////////////
__checkReturn
NTSTATUS
QueryFileNameInfo (
    __in PFLT_CALLBACK_DATA Data,
    __deref_out_opt PFLT_FILE_NAME_INFORMATION* FileNameInfo
    )
{
    ULONG QueryNameFlags = FLT_FILE_NAME_NORMALIZED
        | FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP;

    NTSTATUS status = FltGetFileNameInformation (
        Data,
        QueryNameFlags,
        FileNameInfo
        );

    if ( !NT_SUCCESS( status ) )
    {
        return status;
    }

    status = FltParseFileNameInformation( *FileNameInfo );

    ASSERT( NT_SUCCESS( status ) ); //ignore unsuccessful parse

    return STATUS_SUCCESS;
}

void
ReleaseFileNameInfo (
    __in_opt PFLT_FILE_NAME_INFORMATION* FileNameInfo
    )
{
    ASSERT( FileNameInfo );

    if ( *FileNameInfo )
    {
        FltReleaseFileNameInformation( *FileNameInfo );
        *FileNameInfo = NULL;
    };
}

void
ReleaseContextImp (
    __in_opt PFLT_CONTEXT* Context
    )
{
    if ( !*Context )
    {
        return;
    }

    FltReleaseContext( *Context );
    *Context = NULL;
}

NTSTATUS
IsDirectoryImp (
    __in PFLT_INSTANCE Instance,
    __in PFILE_OBJECT FileObject,
    __out PBOOLEAN IsDirectory
    )
{
    NTSTATUS status;

    if ( NtBuildNumber >= 6001 )
    {
        status = FltIsDirectory (
            FileObject,
            Instance,
            IsDirectory
            );
    }
    else
    {
        FILE_STANDARD_INFORMATION fsi = {};
        status = FltQueryInformationFile (
            Instance,
            FileObject,
            &fsi,
            sizeof( fsi ),
            FileStandardInformation,
            0
            );

        if ( NT_SUCCESS( status ) )
        {
            *IsDirectory = fsi.Directory;
        }
    }

    return status;
}


__checkReturn
NTSTATUS
GenerateStreamContext (
    __in PFLT_FILTER Filter,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PSTREAM_CONTEXT* StreamContext
    )
{
    ASSERT( ARGUMENT_PRESENT( Filter ) );
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    PINSTANCE_CONTEXT InstanceContext = NULL;

    if ( !FsRtlSupportsPerStreamContexts( FltObjects->FileObject ) )
    {
        return STATUS_NOT_SUPPORTED;
    }

    status = FltGetStreamContext (
        FltObjects->Instance,
        FltObjects->FileObject,
        (PFLT_CONTEXT*) StreamContext
        );

    if ( NT_SUCCESS( status ) )
    {
        ASSERT( *StreamContext );
        return status;
    }

    status = FltAllocateContext (
        Filter,
        FLT_STREAM_CONTEXT,
        sizeof( STREAM_CONTEXT ),
        NonPagedPool,
        (PFLT_CONTEXT*) StreamContext
        );

    if ( !NT_SUCCESS( status ) )
    {
        *StreamContext = NULL;

        return status;
    }
    RtlZeroMemory( *StreamContext, sizeof( STREAM_CONTEXT ) );

    status = FltGetInstanceContext (
        FltObjects->Instance,
        (PFLT_CONTEXT *) &InstanceContext
        );

    ASSERT( NT_SUCCESS( status ) );

    (*StreamContext)->m_InstanceContext = InstanceContext;

    BOOLEAN bIsDirectory;

    status = IsDirectoryImp (
        FltObjects->Instance,
        FltObjects->FileObject,
        &bIsDirectory
        );

    if ( NT_SUCCESS( status ) )
    {
        if ( bIsDirectory )
        {
            InterlockedOr (
                &(*StreamContext)->m_Flags,
                _STREAM_FLAGS_DIRECTORY
                );
        }
    }

    status = FltSetStreamContext (
        FltObjects->Instance,
        FltObjects->FileObject,
        FLT_SET_CONTEXT_REPLACE_IF_EXISTS,
        *StreamContext,
        NULL
        );

    if ( !NT_SUCCESS( status ) )
    {
        ReleaseContext( (PFLT_CONTEXT*) StreamContext );
    }
    else
    {
        ASSERT( *StreamContext );
    }

    return status;
}

