#include "../inc/commonkrnl.h"
#include "../inc/memmgr.h"
#include "../../inc/accessch.h"
#include "../inc/iosupport.h"

__checkReturn
NTSTATUS
IoSupportCommand (
    __in PIO_SUPPORT IoCommand,
    __in ULONG IoCommandBufferSize,
    __deref_out_opt PVOID* OutputBuffer,
    __deref_out PULONG OutputSize
    )
{
    UNREFERENCED_PARAMETER( IoCommand );
    UNREFERENCED_PARAMETER( IoCommandBufferSize );
    UNREFERENCED_PARAMETER( OutputBuffer );
    UNREFERENCED_PARAMETER( OutputSize );

    /// \todo IoSupportCommand
    return STATUS_NOT_IMPLEMENTED;
}

void
IoSupportCleanup (
    __in PVOID InputBuffer
    )
{
    /// \todo IoSupportCleanup
    UNREFERENCED_PARAMETER( InputBuffer );
}
