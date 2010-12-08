#include "../inc/commonkrnl.h"
#include "main.h"
#include "../../inc/accessch.h"
#include "filehlp.h"
#include "fileflt.h"
#include "security.h"

FileInterceptorContext::FileInterceptorContext (
    __in PFLT_CALLBACK_DATA Data,
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in_opt PStreamContext StreamCtx,
    __in Interceptors InterceptorId,
    __in DriverOperationId Major,
    __in ULONG Minor,
    __in OperationPoint OperationType
    ) :
    EventData( InterceptorId, Major, Minor, OperationType ),
    m_Data( Data ),
    m_FltObjects( FltObjects ),
    m_StreamCtx( StreamCtx )
{
    m_VolumeCtx = 0;

    m_CacheSyncronizer = 0;
    m_StreamFlagsTemp = 0;

    if ( m_StreamCtx )
    {
        m_StreamFlagsTemp = m_StreamCtx->m_Flags;
    
        if ( !FlagOn( m_StreamFlagsTemp, _STREAM_FLAGS_DIRECTORY ) )
        {
            BOOLEAN isMarkedForDelete;
            if ( NT_SUCCESS ( FileIsMarkedForDelete  (
                m_FltObjects->Instance,
                m_FltObjects->FileObject,
                &isMarkedForDelete
                ) ) )
            {
                if ( isMarkedForDelete )
                {
                    m_StreamFlagsTemp |= _STREAM_FLAGS_DELONCLOSE;
                }
            }
        }

        m_CacheSyncronizer = m_StreamCtx->m_WriteCount;
    }

    m_Section = NULL;
    m_SectionObject = NULL;

    m_RequestorProcessId = 0;
    m_RequestorThreadId = 0;
    m_InstanceCtxt = 0;
    m_FileNameInfo = 0;
    m_Sid = 0;
    SecurityLuidReset( &m_Luid );

    m_DesiredAccess = 0;
    m_CreateOptions = 0;
    m_CreateMode = 0;

    if (
        IRP_MJ_CREATE == m_Data->Iopb->MajorFunction
        &&
        PreProcessing == m_OperationType
        )
    {
        m_PreCreate = TRUE;
    }
    else
    {
        m_PreCreate = FALSE;
    }
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

    ReleaseContext( (PFLT_CONTEXT*) &m_VolumeCtx );
    ReleaseContext( (PFLT_CONTEXT*) &m_InstanceCtxt );
    ReleaseFileNameInfo( &m_FileNameInfo );
    SecurityFreeSid( &m_Sid );
};

__checkReturn
NTSTATUS
FileInterceptorContext::CheckAccessToVolumeContext (
    )
{
    if ( m_VolumeCtx )
    {
        return STATUS_SUCCESS;
    }

    NTSTATUS status = FltGetVolumeContext (
        GlobalData.m_Filter,
        m_FltObjects->Volume, 
        (PFLT_CONTEXT*) &m_VolumeCtx
        );

    if ( !NT_SUCCESS( status ) )
    {
        m_VolumeCtx = 0;
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
    if ( !m_StreamCtx )
    {
        return STATUS_NOT_SUPPORTED;
    }

    if ( FlagOn( m_StreamCtx->m_Flags, _STREAM_FLAGS_DIRECTORY ) )
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

    NTSTATUS status = FsRtlCreateSectionForDataScan (
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
    __in_opt ULONG ParameterId,
    __drv_when(return==0, __deref_out_opt __drv_valueIs(!=0)) PVOID* Data,
    __deref_out_opt PULONG DataSize
    )
{
    NTSTATUS status = STATUS_SUCCESS;

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
                m_PreCreate,
                &m_FileNameInfo
                );

            if ( !NT_SUCCESS( status ) )
            {
                break;
            }
        }

        if ( !m_FileNameInfo )
        {
            ASSERT( m_FileNameInfo );
            status = STATUS_UNSUCCESSFUL;
            break;
        }

        *Data = m_FileNameInfo->Name.Buffer;
        *DataSize = m_FileNameInfo->Name.Length;

        break;

    case PARAMETER_VOLUME_NAME:
        if ( !m_FileNameInfo )
        {
            status = QueryFileNameInfo (
                m_Data,
                m_PreCreate,
                &m_FileNameInfo
                );

            if ( !NT_SUCCESS( status ) )
            {
                break;
            }
            
            if ( !m_FileNameInfo )
            {
                ASSERT( m_FileNameInfo );
                status = STATUS_UNSUCCESSFUL;
                
                break;
            }
        }

        *Data = m_FileNameInfo->Volume.Buffer;
        *DataSize = m_FileNameInfo->Volume.Length;

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

        break;

    case PARAMETER_CURRENT_THREAD_ID:
        if ( !m_RequestorThreadId )
        {
            m_RequestorThreadId = PsGetCurrentThreadId();
        }

        *Data = &m_RequestorThreadId;
        *DataSize = sizeof( m_RequestorThreadId );

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

        break;

    case PARAMETER_OBJECT_STREAM_FLAGS:
        if ( !m_StreamCtx )
        {
            status = STATUS_NOT_SUPPORTED;
            break;
        }

        *Data = &m_StreamFlagsTemp;
        *DataSize = sizeof( m_StreamFlagsTemp );

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

        break;

    case PARAMETER_RESULT_STATUS:
        if ( PreProcessing == m_OperationType )
        {
            status = STATUS_NOT_SUPPORTED;
            break;
        }

        *Data = &m_Data->IoStatus.Status;
        *DataSize = sizeof( m_Data->IoStatus.Status );

        break;

    case PARAMETER_RESULT_INFORMATION:
        if ( PreProcessing == m_OperationType )
        {
            status = STATUS_NOT_SUPPORTED;
            break;
        }

        *Data = &m_Data->IoStatus.Information;
        *DataSize = sizeof( m_Data->IoStatus.Information );

        break;

    case PARAMETER_DEVICE_ID:
        status = CheckAccessToVolumeContext();
        if ( !NT_SUCCESS( status ) )
        {
            status = STATUS_UNSUCCESSFUL;
            break;
        }

        if ( !m_VolumeCtx->m_DeviceId.Buffer )
        {
            status = STATUS_UNSUCCESSFUL;
            break;
        }

        *Data = m_VolumeCtx->m_DeviceId.Buffer;
        *DataSize = m_VolumeCtx->m_DeviceId.Length;

        break;

    default:
        __debugbreak();
        status = STATUS_NOT_FOUND;

        break;
    }

    return status;
}

__checkReturn
NTSTATUS
FileInterceptorContext::ObjectRequest (
    __in NOTIFY_ID Command,
    __out_opt PVOID OutputBuffer,
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

void
FileInterceptorContext::SetCache1 (
    )
{
    if ( !m_StreamCtx )
    {
        return;
    }

    if ( m_CacheSyncronizer == m_StreamCtx->m_WriteCount )
    {
        InterlockedOr( &m_StreamCtx->m_Flags, _STREAM_FLAGS_CASHE1 );
    }
}
