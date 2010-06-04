#include "pch.h"
#include "main.h"
#include "../inc/accessch.h"
#include "flt.h"
#include "filehlp.h"
#include "fileflt.h"
#include "security.h"

FileInterceptorContext::FileInterceptorContext (
    __in PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in OperationPoint OperationType
    ) : InterceptorContext( OperationType ),
    m_Data( Data ),
    m_FltObjects( FltObjects ),
    m_OperationType( OperationType )
{
    m_StreamContext = NULL;

    m_Section = NULL;
    m_SectionObject = NULL;

    m_RequestorProcessId = 0;
    m_RequestorThreadId = 0;
    m_InstanceContext = 0;
    m_FileNameInfo = 0;
    m_Sid = 0;
    SecurityLuidReset( &m_Luid );

    m_DesiredAccess = 0;
    m_CreateOptions = 0;
    m_CreateMode = 0;
};

FileInterceptorContext::~FileInterceptorContext (
    )
{
    if ( m_Section )
    {
        if ( IsKernelHandle( m_Section ) )
        {
            ZwClose( m_Section );
        }
    }

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
    FileInterceptorContext::CheckAccessToStreamContext (
    )
{
    if ( m_StreamContext )
    {
        return STATUS_SUCCESS;
    }

    NTSTATUS status = GenerateStreamContext (
        Globals.m_Filter,
        m_FltObjects,
        &m_StreamContext
        );

    if ( !NT_SUCCESS( status ) )
    {
        m_StreamContext = NULL;
    }

    return status;
}

__checkReturn
    NTSTATUS
    FileInterceptorContext::CreateSectionForData (
    __deref_out PHANDLE Section,
    __out PLARGE_INTEGER Size
    )
{
    NTSTATUS status = CheckAccessToStreamContext(); 

    if ( !NT_SUCCESS( status ) )
    {
        return STATUS_NOT_SUPPORTED;
    }

    if ( FlagOn( m_StreamContext->m_Flags, _STREAM_FLAGS_DIRECTORY ) )
    {
        return STATUS_NOT_SUPPORTED;
    }

    OBJECT_ATTRIBUTES oa;

    InitializeObjectAttributes (
        &oa,
        NULL,
        OBJ_KERNEL_HANDLE,
        NULL,
        NULL
        );

#if ( NTDDI_VERSION < NTDDI_WIN6 )
    KPROCESSOR_MODE prevmode = ExGetPreviousMode();

    if ( prevmode == UserMode )
    {
        SetPreviousMode( KernelMode );
    }
#endif // ( NTDDI_VERSION < NTDDI_WIN6 )

    status = FsRtlCreateSectionForDataScan (
        &m_Section,
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

#if ( NTDDI_VERSION < NTDDI_WIN6 )
    if ( prevmode == UserMode )
    {
        SetPreviousMode( UserMode );
    }
#endif // ( NTDDI_VERSION < NTDDI_WIN6 )

    if ( NT_SUCCESS( status ) )
    {
        if ( IsKernelHandle( m_Section ) )
        {
            status = ObOpenObjectByPointer (
                m_SectionObject,
                0,
                NULL,
                GENERIC_READ,
                NULL,
                KernelMode,
                Section
                );

            if ( !NT_SUCCESS( status ) )
            {
                __debugbreak();
                *Section = 0;
                status = STATUS_SUCCESS; // read using kernel routine
            }
        }
        else
        {
            __debugbreak();
            *Section = m_Section;
        }
    }
    else
    {
        m_SectionObject = NULL;
    }

    return status;
}

__checkReturn
    NTSTATUS
    FileInterceptorContext::QueryParameter (
    __in_opt Parameters ParameterId,
    __deref_out_opt PVOID* Data,
    __deref_out_opt PULONG DataSize
    )
{
    NTSTATUS status = STATUS_NOT_FOUND;

    ASSERT( ARGUMENT_PRESENT( Data ) );
    ASSERT( ARGUMENT_PRESENT( DataSize ) );

    FLT_PARAMETERS *pFltParams = &m_Data->Iopb->Parameters;

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

    case PARAMETER_DESIRED_ACCESS:
        if ( IRP_MJ_CREATE == m_Data->Iopb->MajorFunction )
        {
            m_DesiredAccess = pFltParams->Create.SecurityContext->DesiredAccess;
        }
        else
        {
            // use stream handle context
            status = STATUS_NOT_SUPPORTED;
            break;
        }

        *Data = &m_DesiredAccess;
        *DataSize = sizeof( m_DesiredAccess );
        status = STATUS_SUCCESS;

        break;

    case PARAMETER_CREATE_OPTIONS:
        if ( IRP_MJ_CREATE == m_Data->Iopb->MajorFunction )
        {
            m_CreateOptions = pFltParams->Create.Options & FILE_VALID_OPTION_FLAGS;
        }
        else
        {
            // use stream handle context
            status = STATUS_NOT_SUPPORTED;
            break;
        }

        *Data = &m_CreateOptions;
        *DataSize = sizeof( m_CreateOptions );
        status = STATUS_SUCCESS;

        break;

    case PARAMETER_CREATE_MODE:
        if ( IRP_MJ_CREATE == m_Data->Iopb->MajorFunction )
        {
            m_CreateMode = (pFltParams->Create.Options >> 24) & 0xff;
        }
        else
        {
            // use stream handle context
            status = STATUS_NOT_SUPPORTED;

            break;
        }

        *Data = &m_CreateMode;
        *DataSize = sizeof( m_CreateMode );
        status = STATUS_SUCCESS;

        break;

    case PARAMETER_RESULT_STATUS:
        if ( PreProcessing == m_OperationType )
        {
            status = STATUS_NOT_SUPPORTED;
            break;
        }

        *Data = &m_Data->IoStatus.Status;
        *DataSize = sizeof( m_Data->IoStatus.Status );
        status = STATUS_SUCCESS;

        break;

    case PARAMETER_RESULT_INFORMATION:
        if ( PreProcessing == m_OperationType )
        {
            status = STATUS_NOT_SUPPORTED;
            break;
        }

        *Data = &m_Data->IoStatus.Information;
        *DataSize = sizeof( m_Data->IoStatus.Information );
        status = STATUS_SUCCESS;

        break;

    default:
        __debugbreak();
        break;
    }

    return status;
}

__checkReturn
    NTSTATUS
    FileInterceptorContext::ObjectRequest (
    __in NOTIFY_ID Command,
    __in_opt PVOID OutputBuffer,
    __inout_opt PULONG OutputBufferSize
    )
{
    NTSTATUS status = STATUS_NOT_SUPPORTED;

    switch( Command )
    {
    case ntfcom_PrepareIO:
        if (
            OutputBuffer
            &&
            OutputBufferSize
            &&
            *OutputBufferSize >= sizeof(NC_IOPREPARE)
            )
        {
            HANDLE hSection;
            LARGE_INTEGER size;
            status = CreateSectionForData( &hSection, &size );
            if ( NT_SUCCESS( status ) )
            {
                PNC_IOPREPARE prepare = (NC_IOPREPARE*) OutputBuffer;
                prepare->m_Section = hSection;
                prepare->m_IoSize = size;
            }
        }
        break;

    default:
        __debugbreak();
        break;
    }

    return status;
}
