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
        //! \todo: PARAMETER_FILE_NAME
        break;

    case PARAMETER_VOLUME_NAME:
        //! \todo: PARAMETER_VOLUME_NAME
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

    return STATUS_NOT_IMPLEMENTED;
}

//////////////////////////////////////////////////////////////////////////

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
ReleaseContext (
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
