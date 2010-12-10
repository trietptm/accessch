#pragma once

#include "../inc/fltevents.h"

__checkReturn
NTSTATUS
ChannelInitPort (
    );

void
ChannelDestroyPort (
    );

__checkReturn
NTSTATUS
ChannelAskUser (
    __in EventData *Event,
    __in PARAMS_MASK ParamsMask,
    __inout VERDICT* Verdict
    );
