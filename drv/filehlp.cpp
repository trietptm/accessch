#include "pch.h"
#include "filehlp.h"

FileInterceptorContext::FileInterceptorContext (
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects
    ) : m_Data( Data ), m_FltObjects( FltObjects )
{
    m_RequestorProcessId = 0;
    m_InstanceContext = 0;
    m_StreamContext = 0;
    m_FileNameInfo = 0;
    m_Sid = 0;
    RtlZeroMemory( &Luid.HighPart, sizeof(  Luid ) );
};

FileInterceptorContext::~FileInterceptorContext (
    )
{
    ReleaseContext( (PFLT_CONTEXT*) &m_InstanceContext );
    ReleaseContext( (PFLT_CONTEXT*) &m_StreamContext );
    ReleaseFileNameInfo( &m_FileNameInfo );
    SecurityFreeSid( &m_Sid );
};


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
        //! \todo: PARAMETER_LUID
        break;

    case PARAMETER_SID:
        //! \todo: PARAMETER_LUID
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

void
SecurityFreeSid (
    __in PSID* ppSid
    )
{
    if ( !*ppSid )
        return;

    ExFreePool( *ppSid );
    *ppSid = NULL;
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
        return status;
    }

    status = FltAllocateContext (
        Filter,
        FLT_STREAM_CONTEXT,
        sizeof( STREAM_CONTEXT ),
        NonPagedPool,
        (PFLT_CONTEXT*) ppStreamContext
        );

    if ( NT_SUCCESS( status ) )
    {
        RtlZeroMemory( *ppStreamContext, sizeof( STREAM_CONTEXT ) );

        status = FltGetInstanceContext (
            FltObjects->Instance,
            (PFLT_CONTEXT *) &InstanceContext
            );

        ASSERT( NT_SUCCESS( status ) );

        (*ppStreamContext)->m_InstanceContext = InstanceContext;

        BOOLEAN bIsDirectory;

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

        ReleaseContext( (PFLT_CONTEXT*) ppStreamContext );
    }

    return status;
}

