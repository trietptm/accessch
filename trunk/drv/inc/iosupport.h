#pragma once

__checkReturn
NTSTATUS
IoSupportCommand (
    __in PIO_SUPPORT IoCommand,
    __in ULONG IoCommandBufferSize,
    __deref_out_opt PVOID* OutputBuffer,
    __deref_out PULONG OutputSize
    );

void
IoSupportCleanup (
    __in PVOID InputBuffer
    );
