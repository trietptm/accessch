#include "pch.h"
#include "main.h"
#include "../inc/accessch.h"
#include "flt.h"
#include "volhlp.h"
#include "volumeflt.h"

VolumeInterceptorContext::VolumeInterceptorContext (
    __in PCFLT_RELATED_OBJECTS FltObjects,
    __in PINSTANCE_CONTEXT InstanceContext,
    __in OperationPoint OperationType
    ) : InterceptorContext( OperationType ),
    m_FltObjects( FltObjects ),
    m_InstanceContext( InstanceContext )
{

}

VolumeInterceptorContext::~VolumeInterceptorContext (
    )
{

}

__checkReturn
NTSTATUS
VolumeInterceptorContext::QueryParameter (
    __in_opt Parameters ParameterId,
    __deref_out_opt PVOID* Data,
    __deref_out_opt PULONG DataSize
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

__checkReturn
NTSTATUS
VolumeInterceptorContext::ObjectRequest (
    __in NOTIFY_ID Command,
    __in_opt PVOID OutputBuffer,
    __inout_opt PULONG OutputBufferSize
    )
{
    return STATUS_NOT_IMPLEMENTED;
}
