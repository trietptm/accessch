#pragma once

#include "../inc/fltevents.h"
#include "../inc/fltsystem.h"
#include "../inc/processhelper.h"

__checkReturn
NTSTATUS
ChannelInitPort (
    __in ProcessHelper* ProcessHlp,
    __in FilteringSystem* FltSystem
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
