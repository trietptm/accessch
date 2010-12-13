#include "../inc/commonkrnl.h"
#include "../inc/channel.h"
#include "../inc/filemgr.h"
#include "commport.h" 

PortGlobals gPort = { 0 };

__checkReturn
NTSTATUS
ChannelInitPort (
    )
{
    QueuedItem::Initialize();

    gPort.m_FltSystem = GetFltSystem();
    ASSERT( gPort.m_FltSystem );

    ExInitializeRundownProtection( &gPort.m_RefClientPort );
    ExWaitForRundownProtectionRelease( &gPort.m_RefClientPort );
    ExRundownCompleted( &gPort.m_RefClientPort );

    NTSTATUS status = PortCreate (
        FileMgrGetFltFilter(),
        &gPort.m_Port
        );

    if ( !NT_SUCCESS( status ) )
    {
        gPort.m_Port = NULL;
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

    QueuedItem::Destroy();
    gPort.m_FltSystem->Release();
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

    return status;
}
