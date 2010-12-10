#pragma once

typedef void (NTAPI *_tpOnOnload)();

#include "../inc/fltsystem.h"

__checkReturn
NTSTATUS
FileMgrInit (
    __in PDRIVER_OBJECT DriverObject,
    __in _tpOnOnload UnloadCb
    );

__checkReturn
NTSTATUS
FileMgrStart (
    );

PFLT_FILTER
FileMgrGetFltFilter (
    );