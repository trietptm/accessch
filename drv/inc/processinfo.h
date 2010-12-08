#pragma once

typedef void ( *_tpProcessExitCb )( HANDLE ProcessId, PVOID Opaque );

NTSTATUS
RegisterExitProcessCb (
    __in _tpProcessExitCb CbFunc,
    __in_opt PVOID Opaque
    );

void
UnregisterExitProcessCb ( 
    __in _tpProcessExitCb CbFunc
    );
