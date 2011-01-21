#pragma once

typedef void (NTAPI *_tpOnOnload)();

#include "../inc/fltsystem.h"

__checkReturn
NTSTATUS
FileMgrInit (
    __in PDRIVER_OBJECT DriverObject,
    __in _tpOnOnload UnloadCb,
    __in FilteringSystem* FltSystem
    );

__checkReturn
NTSTATUS
FileMgrStart (
    );

void
FileMgrUnregister (
    );

PFLT_FILTER
FileMgrGetFltFilter (
    );