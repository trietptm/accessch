#pragma prefast(disable:28252) // sal annotation compare h -> cpp
#pragma prefast(disable:28253) // sal annotation compare h -> cpp
#pragma prefast(disable:28107) // critical region checks
#pragma prefast(disable:28175) // access to DEVICE_OBJECT members

#include <fltKernel.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <ntdddisk.h>

extern "C" NTSYSAPI USHORT NtBuildNumber;

#define KERNEL_HANDLE_MASK ((ULONG_PTR)((LONG)0x80000000))

#define IsKernelHandle( _handle ) \
    ((( KERNEL_HANDLE_MASK & (ULONG_PTR)( _handle )) == KERNEL_HANDLE_MASK) && \
    (( _handle ) != NtCurrentThread()) && \
    (( _handle ) != NtCurrentProcess()) )

#if ( NTDDI_VERSION < NTDDI_WIN6 )
MODE
SetPreviousMode (
    MODE OperationMode
    );

#endif // ( NTDDI_VERSION < NTDDI_WIN6 )

#include "../inc/memmgr.h"
