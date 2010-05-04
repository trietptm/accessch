#include "pch.h"
#include "../inc/accessch.h"
#include "flt.h"

#include "filehlp.h"
#include "security.h"

FileInterceptorContext::FileInterceptorContext (
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects
    ) : m_Data( Data ), m_FltObjects( FltObjects )
{
    m_SectionObject = NULL;
   
    m_RequestorProcessId = 0;
    m_RequestorThreadId = 0;
    m_InstanceContext = 0;
    m_StreamContext = 0;
    m_FileNameInfo = 0;
    m_Sid = 0;
    SecurityLuidReset( &m_Luid );
};

FileInterceptorContext::~FileInterceptorContext (
    )
{
    if ( m_SectionObject )
    {
        ObDereferenceObject( m_SectionObject );
    }

    ReleaseContext( (PFLT_CONTEXT*) &m_InstanceContext );
    ReleaseContext( (PFLT_CONTEXT*) &m_StreamContext );
    ReleaseFileNameInfo( &m_FileNameInfo );
    SecurityFreeSid( &m_Sid );
};

__checkReturn
NTSTATUS
FileInterceptorContext::CreateSectionForData (
    __deref_out PHANDLE Section,
    __out PLARGE_INTEGER Size
    )
{
    OBJECT_ATTRIBUTES oa;
                        
    InitializeObjectAttributes (
        &oa,
        NULL,
        OBJ_KERNEL_HANDLE,
        NULL,
        NULL
        );
    
    NTSTATUS status = FsRtlCreateSectionForDataScan (
        Section,
        &m_SectionObject,
        Size,
        m_FltObjects->FileObject,
        SECTION_MAP_READ | SECTION_QUERY,
        &oa,
        0,
        PAGE_READONLY,
        SEC_COMMIT,
        0
        );
            
    if ( !NT_SUCCESS( status ) )
    {
        m_SectionObject = NULL;
    }

    return status;
}

__checkReturn
NTSTATUS
FileInterceptorContext::QueryFileParameter (
    __in_opt Parameters ParameterId,
    __deref_out_opt PVOID* Data,
    __deref_out_opt PULONG DataSize
    )
{
    NTSTATUS status = STATUS_NOT_FOUND;

    ASSERT( ARGUMENT_PRESENT( Data ) );
    ASSERT( ARGUMENT_PRESENT( DataSize ) );

    switch ( ParameterId )
    {
    case PARAMETER_FILE_NAME:
        if ( !m_FileNameInfo )
        {
            status = QueryFileNameInfo (
                m_Data,
                &m_FileNameInfo
                );
            
            if ( !NT_SUCCESS( status ) )
            {
                break;
            }
        }
        
        *Data = m_FileNameInfo->Name.Buffer;
        *DataSize = m_FileNameInfo->Name.Length;
        status = STATUS_SUCCESS;

        break;

    case PARAMETER_VOLUME_NAME:
        if ( !m_FileNameInfo )
        {
            status = QueryFileNameInfo (
                m_Data,
                &m_FileNameInfo
                );

            if ( !NT_SUCCESS( status ) )
            {
                break;
            }
        }

        *Data = m_FileNameInfo->Volume.Buffer;
        *DataSize = m_FileNameInfo->Volume.Length;
        status = STATUS_SUCCESS;

        break;

    case PARAMETER_REQUESTOR_PROCESS_ID:
        if ( !m_RequestorProcessId )
        {
            m_RequestorProcessId = UlongToHandle ( 
                FltGetRequestorProcessId( m_Data )
                );
        }

        *Data = &m_RequestorProcessId;
        *DataSize = sizeof( m_RequestorProcessId );
        status = STATUS_SUCCESS;

        break;

    case PARAMETER_CURRENT_THREAD_ID:
        if ( !m_RequestorThreadId )
        {
            m_RequestorThreadId = PsGetCurrentThreadId();
        }

        *Data = &m_RequestorThreadId;
        *DataSize = sizeof( m_RequestorThreadId );
        status = STATUS_SUCCESS;

        break;

    case PARAMETER_LUID:
        if ( !SecurityIsLuidValid( &m_Luid ) )
        {
            status = SecurityGetLuid( &m_Luid );
            if ( !NT_SUCCESS( status ) )
            {
                SecurityLuidReset( &m_Luid );
                break;
            }
        }
        *Data = &m_Luid;
        *DataSize = sizeof( m_Luid );
        status = STATUS_SUCCESS;

        break;

    case PARAMETER_SID:
        if ( !m_Sid )
        {
            status = SecurityGetSid( m_Data, &m_Sid );
            if ( !NT_SUCCESS( status ) )
            {
                m_Sid = 0;
                break;
            }
        }

        *Data = m_Sid;
        *DataSize = RtlLengthSid( m_Sid );
        status = STATUS_SUCCESS;

        break;

    default:
        __debugbreak();
        break;
    }

    return status;
}

//////////////////////////////////////////////////////////////////////////
__checkReturn
NTSTATUS
QueryFileNameInfo (
    __in PFLT_CALLBACK_DATA Data,
    __deref_out_opt PFLT_FILE_NAME_INFORMATION* ppFileNameInfo
    )
{
    ULONG QueryNameFlags = FLT_FILE_NAME_NORMALIZED
        | FLT_FILE_NAME_QUERY_ALWAYS_ALLOW_CACHE_LOOKUP;

    NTSTATUS status = FltGetFileNameInformation (
        Data,
        QueryNameFlags,
        ppFileNameInfo
        );

    if ( !NT_SUCCESS( status ) )
    {
        return status;
    }

    status = FltParseFileNameInformation( *ppFileNameInfo );

    ASSERT( NT_SUCCESS( status ) ); //ignore unsuccessful parse

    return STATUS_SUCCESS;
}

void
ReleaseFileNameInfo (
    __in_opt PFLT_FILE_NAME_INFORMATION* ppFileNameInfo
    )
{
    ASSERT( ppFileNameInfo );

    if ( *ppFileNameInfo )
    {
        FltReleaseFileNameInformation( *ppFileNameInfo );
        *ppFileNameInfo = NULL;
    };
}

void
ReleaseContextImp (
    __in_opt PFLT_CONTEXT* ppContext
    )
{
    if ( !*ppContext )
    {
        return;
    }

    FltReleaseContext( *ppContext );
    *ppContext = NULL;
}

__checkReturn
NTSTATUS
QueryFileParameter (
    __in PVOID Opaque,
    __in_opt Parameters ParameterId,
    __deref_out_opt PVOID* Data,
    __deref_out_opt PULONG DataSize
    )
{
    FileInterceptorContext *fileContext = (FileInterceptorContext*) Opaque;
   
    NTSTATUS status = fileContext->QueryFileParameter (
        ParameterId,
        Data,
        DataSize
        );

    return status;
}

__checkReturn
NTSTATUS
GenerateStreamContext (
    __in PFLT_FILTER Filter,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __deref_out_opt PSTREAM_CONTEXT* ppStreamContext
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
        (PFLT_CONTEXT*) ppStreamContext
        );

    if ( NT_SUCCESS( status ) )
    {
        ASSERT( *ppStreamContext );
        return status;
    }

    status = FltAllocateContext (
        Filter,
        FLT_STREAM_CONTEXT,
        sizeof( STREAM_CONTEXT ),
        NonPagedPool,
        (PFLT_CONTEXT*) ppStreamContext
        );

    if ( !NT_SUCCESS( status ) )
    {
        *ppStreamContext = NULL;

        return status;
    }
    RtlZeroMemory( *ppStreamContext, sizeof( STREAM_CONTEXT ) );

    status = FltGetInstanceContext (
        FltObjects->Instance,
        (PFLT_CONTEXT *) &InstanceContext
        );

    ASSERT( NT_SUCCESS( status ) );

    (*ppStreamContext)->m_InstanceContext = InstanceContext;

    BOOLEAN bIsDirectory;

    //! \todo: safe call on Vista sp1 or higher!
    status = FltIsDirectory (
        FltObjects->FileObject,
        FltObjects->Instance,
        &bIsDirectory
        );

    if ( NT_SUCCESS( status ) )
    {
        if ( bIsDirectory )
        {
            InterlockedOr (
                &(*ppStreamContext)->m_Flags,
                _STREAM_FLAGS_DIRECTORY
                );
        }
    }

    status = FltSetStreamContext (
        FltObjects->Instance,
        FltObjects->FileObject,
        FLT_SET_CONTEXT_REPLACE_IF_EXISTS,
        *ppStreamContext,
        NULL
        );

    if ( !NT_SUCCESS( status ) )
    {
        ReleaseContext( (PFLT_CONTEXT*) ppStreamContext );
    }
    else
    {
        ASSERT( *ppStreamContext );
    }

    return status;
}

