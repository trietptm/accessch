#include "pch.h"
#include "main.h"
#include "../../inc/accessch.h"
#include "flt.h"
#include "volhlp.h"
#include "volumeflt.h"

VolumeInterceptorContext::VolumeInterceptorContext (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PInstanceContext InstanceCtx,
    __in PVolumeContext VolumeCtx,
    __in Interceptors InterceptorId,
    __in DriverOperationId Major,
    __in ULONG Minor,
    __in OperationPoint OperationType
    ) : EventData( InterceptorId, Major, Minor, OperationType ),
    m_FltObjects( FltObjects ),
    m_InstanceCtx( InstanceCtx ),
    m_VolumeCtx( VolumeCtx )
{
    ASSERT( InstanceCtx );
    ASSERT( VolumeCtx );

    m_RequestorPid = 0;
}

VolumeInterceptorContext::~VolumeInterceptorContext (
    )
{

}

__checkReturn
NTSTATUS
VolumeInterceptorContext::QueryParameter (
    __in_opt Parameters ParameterId,
    __drv_when(return==0, __deref_out_opt __drv_valueIs(!=0)) PVOID* Data,
    __deref_out_opt PULONG DataSize
    )
{
    NTSTATUS status = STATUS_NOT_FOUND;

    switch( ParameterId )
    {
    case PARAMETER_REQUESTOR_PROCESS_ID:
        *Data = &m_RequestorPid;
        *DataSize = sizeof( m_RequestorPid );
        status = STATUS_SUCCESS;

        break;

    case PARAMETER_DEVICE_TYPE:
        *Data = &m_InstanceCtx->m_VolumeDeviceType;
        *DataSize = sizeof( m_InstanceCtx->m_VolumeDeviceType );
        status = STATUS_SUCCESS;

        break;

    case PARAMETER_FILESYSTEM_TYPE:
        *Data = &m_InstanceCtx->m_VolumeDeviceType;
        *DataSize = sizeof( m_InstanceCtx->m_VolumeDeviceType );
        status = STATUS_SUCCESS;

        break;

    case PARAMETER_BUS_TYPE:
        *Data = &m_VolumeCtx->m_BusType;
        *DataSize = sizeof( m_VolumeCtx->m_BusType );
        status = STATUS_SUCCESS;

        break;

    case PARAMETER_DEVICE_ID:
        if ( m_VolumeCtx->m_DeviceId.Length )
        {
            *Data = m_VolumeCtx->m_DeviceId.Buffer;
            *DataSize = m_VolumeCtx->m_DeviceId.Length;
            status = STATUS_SUCCESS;
        }

        break;
    }

    return status;
}

__checkReturn
NTSTATUS
VolumeInterceptorContext::ObjectRequest (
    __in NOTIFY_ID Command,
    __out_opt PVOID OutputBuffer,
    __inout_opt PULONG OutputBufferSize
    )
{
    return STATUS_NOT_IMPLEMENTED;
}
