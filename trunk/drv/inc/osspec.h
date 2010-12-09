#pragma once

extern "C" NTSYSAPI USHORT NtBuildNumber;

#define KERNEL_HANDLE_MASK ((ULONG_PTR)((LONG)0x80000000))

#define IsKernelHandle( _handle ) \
    ((( KERNEL_HANDLE_MASK & (ULONG_PTR)( _handle )) == KERNEL_HANDLE_MASK) && \
    (( _handle ) != NtCurrentThread()) && \
    (( _handle ) != NtCurrentProcess()) )

__checkReturn
NTSTATUS
GetPreviousModeOffset (
    );

MODE
SetPreviousMode (
    MODE OperationMode
    );
