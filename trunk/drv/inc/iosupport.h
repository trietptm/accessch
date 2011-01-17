#pragma once

__checkReturn
NTSTATUS
IoSupportCommand (
    __in PIO_SUPPORT IoCommand,
    __in ULONG IoCommandBufferSize,
    __deref_out PIO_SUPPORT_RESULT IoSupportResult,
    __deref_out PULONG IoResultSize
    );

void
IoSupportCleanup (
    __in PIO_SUPPORT_RESULT IoSupportResult
    );
