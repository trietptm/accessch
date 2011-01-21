#include "../inc/commonkrnl.h"
#include "../inc/channel.h"
#include "../inc/filemgr.h"
#include "commport.h" 

#include "channel.tmh" 

PortGlobals gPort = { 0 };

__checkReturn
NTSTATUS
ChannelInitPort (
    __in ProcessHelper* ProcessHlp,
    __in FilteringSystem* FltSystem
    )
{
    ASSERT( ProcessHlp );
    ASSERT( FltSystem );

    NTSTATUS status = ProcessHlp->AddRef();
    if ( !NT_SUCCESS( status ) )
    {
        return status;
    }

    status = FltSystem->AddRef();
    if ( !NT_SUCCESS( status ) )
    {
        ProcessHlp->Release();

        return status;
    }

    QueuedItem::Initialize();

    gPort.m_FltSystem = FltSystem;
    gPort.m_ProcessHelper = ProcessHlp;

    ExInitializeRundownProtection( &gPort.m_RefClientPort );
    ExWaitForRundownProtectionRelease( &gPort.m_RefClientPort );
    ExRundownCompleted( &gPort.m_RefClientPort );

    status = PortCreate (
        FileMgrGetFltFilter(),
        &gPort.m_Port
        );

    if ( !NT_SUCCESS( status ) )
    {
        gPort.m_Port = NULL;
        ProcessHlp->Release();
        FltSystem->Release();
    }

    return status;
}

void
ChannelDestroyPort (
    )
{
    if ( gPort.m_Port )
    {
        FltCloseCommunicationPort( gPort.m_Port );
    }

    gPort.m_ProcessHelper->Release();
    gPort.m_ProcessHelper = NULL;

    QueuedItem::Destroy();
    gPort.m_FltSystem->Release();
    gPort.m_FltSystem = NULL;
}

__checkReturn
NTSTATUS
ChannelAskUser (
    __in EventData *Event,
    __in PARAMS_MASK ParamsMask,
    __inout VERDICT* Verdict
    )
{
    NTSTATUS status = PortAskUser (
        Event,
        ParamsMask,
        Verdict
        );

    if ( NT_SUCCESS( status ) )
    {
        DoTraceEx (
            TRACE_LEVEL_INFORMATION,
            TB_CHANNEL,
            "processing: %p verdict 0x%x",
            Event,
            *Verdict
            );
    }
    else
    {
        DoTraceEx (
            TRACE_LEVEL_INFORMATION,
            TB_CHANNEL,
            "processing: %p error %!STATUS!",
            Event,
            status
            );
    }

    return status;
}
