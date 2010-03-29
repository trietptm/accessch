#include "pch.h"
#include "filehlp.h"

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
    status = STATUS_NOT_FOUND;

    ASSERT( ARGUMENT_PRESENT( Data ) );
    ASSERT( ARGUMENT_PRESENT( DataSize ) );

    FileInterceptorContext *fileContext = (FileInterceptorContext) Opaque;
    switch ( ParameterId )
    {
    case PARAMETER_FILE_NAME:
        //! \todo: PARAMETER_FILE_NAME
        break;

    case PARAMETER_VOLUME_NAME:
        //! \todo: PARAMETER_VOLUME_NAME
        break;

    case PARAMETER_REQUESTOR_PROCESS_ID:
        if ( !fileContext->m_RequestorProcessId )
        {
            fileContext->m_RequestorProcessId = FltGetRequestorProcessId( fileContext->m_Data );
        }

        *Data = &fileContext->m_RequestorProcessId;
        *DataSize = sizeof( fileContext->m_RequestorProcessId );
        status = STATUS_SUCCESS;

        break;

    case PARAMETER_CURRENT_THREAD_ID:
        if ( !fileContext->m_RequestorThreadId )
        {
            fileContext->m_RequestorThreadId = PsGetCurrentThreadId();
        }

        *Data = &fileContext->m_RequestorThreadId;
        *DataSize = sizeof( fileContext->m_RequestorThreadId );
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
