#include "../inc/commonkrnl.h"
#include "../inc/channel.h"
#include "../inc/filemgr.h"
#include "commport.h" 

PFLT_PORT           gPort = NULL;
EX_RUNDOWN_REF      gRefClientPort;
PFLT_PORT           gClientPort = NULL;

__checkReturn
NTSTATUS
ChannelInitPort (
    )
{
    QueuedItem::Initialize();

    ExInitializeRundownProtection( &gRefClientPort );
    ExWaitForRundownProtectionRelease( &gRefClientPort );
    ExRundownCompleted( &gRefClientPort );

    /// \todo reference filtering system

    NTSTATUS status = PortCreate (
        FileMgrGetFltFilter(),
        &gPort
        );

    if ( !NT_SUCCESS( status ) )
    {
        gPort = NULL;
    }

    return status;
}

void
ChannelDestroyPort (
    )
{
    /// \todo dereference filtering system

    if ( gPort )
    {
        FltCloseCommunicationPort( gPort );
    }

    QueuedItem::Destroy();
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
