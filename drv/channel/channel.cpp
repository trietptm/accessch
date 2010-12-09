#include "../inc/commonkrnl.h"
#include "../inc/channel.h"
#include "commport.h" 

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
