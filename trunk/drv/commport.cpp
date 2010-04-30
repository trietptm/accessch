#include "pch.h"
#include "commport.h"
#include "main.h"
#include "../inc/accessch.h"

typedef struct _PORT_CONTEXT
{
    PFLT_PORT               m_Connection;
}PORT_CONTEXT, *PPORT_CONTEXT;

// ----------------------------------------------------------------------------
// communications

__checkReturn
NTSTATUS
PortCreate (
    __in PFLT_FILTER pFilter,
    __deref_out_opt PFLT_PORT* ppPort
    )
{
    NTSTATUS status;

    PSECURITY_DESCRIPTOR sd;
    status = FltBuildDefaultSecurityDescriptor( &sd, FLT_PORT_ALL_ACCESS );

    if ( NT_SUCCESS( status ) )
    {
        UNICODE_STRING usName;
        OBJECT_ATTRIBUTES oa;

        RtlInitUnicodeString( &usName, ACCESSCH_PORT_NAME );
        InitializeObjectAttributes (
            &oa,
            &usName,
            OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
            NULL,
            sd
            );

        status = FltCreateCommunicationPort (
            pFilter,
            ppPort,
            &oa,
            NULL,
            PortConnect,
            PortDisconnect,
            PortMessageNotify,
            ACCESSCH_MAX_CONNECTIONS
            );

        FltFreeSecurityDescriptor( sd );

        if ( !NT_SUCCESS( status ) )
        {
            *ppPort = NULL;
        }
    }

    return status;
}

__checkReturn
NTSTATUS
PortConnect (
    __in PFLT_PORT ClientPort,
    __in_opt PVOID ServerPortCookie,
    __in_bcount_opt(SizeOfContext) PVOID ConnectionContext,
    __in ULONG SizeOfContext,
    __deref_out_opt PVOID *ConnectionCookie
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    PPORT_CONTEXT pPortContext = NULL;

    UNREFERENCED_PARAMETER( ServerPortCookie );
    UNREFERENCED_PARAMETER( ConnectionContext );
    UNREFERENCED_PARAMETER( SizeOfContext );

    FltAcquirePushLockExclusive( &Globals.m_ClientPortLock );
    __try
    {
        if ( Globals.m_ClientPort )
        {
            status = STATUS_ALREADY_REGISTERED;
            __leave;
        }

        pPortContext = (PPORT_CONTEXT) ExAllocatePoolWithTag (
            NonPagedPool,
            sizeof( PORT_CONTEXT ),
            _ALLOC_TAG
            );

        if ( !pPortContext )
        {
            status = STATUS_NO_MEMORY;
            __leave;
        }

        RtlZeroMemory( pPortContext, sizeof( PORT_CONTEXT ) );

        pPortContext->m_Connection = ClientPort;

        //! \todo  revise single port connection
        Globals.m_ClientPort = ClientPort;

        *ConnectionCookie = pPortContext;
    }
    __finally
    {
        FltReleasePushLock( &Globals.m_ClientPortLock );
    }

    return status;
}

void
PortDisconnect (
    __in PVOID ConnectionCookie
    )
{
    PPORT_CONTEXT pPortContext = (PPORT_CONTEXT) ConnectionCookie;

    ASSERT( ARGUMENT_PRESENT( pPortContext ) );

    FltAcquirePushLockExclusive( &Globals.m_ClientPortLock );
    Globals.m_ClientPort = NULL;
    FltReleasePushLock( &Globals.m_ClientPortLock );

    FltCloseClientPort( Globals.m_Filter, &pPortContext->m_Connection );

    FREE_POOL( pPortContext );
}

__checkReturn
NTSTATUS
PortMessageNotify (
    __in PVOID ConnectionCookie,
    __in_bcount_opt(InputBufferSize) PVOID InputBuffer,
    __in ULONG InputBufferSize,
    __out_bcount_part_opt(OutputBufferSize,*ReturnOutputBufferLength) PVOID OutputBuffer,
    __in ULONG OutputBufferSize,
    __out PULONG ReturnOutputBufferLength
    )
{
    NTSTATUS status = STATUS_INVALID_PARAMETER;

    UNREFERENCED_PARAMETER( InputBuffer );
    UNREFERENCED_PARAMETER( InputBufferSize );
    UNREFERENCED_PARAMETER( OutputBuffer );
    UNREFERENCED_PARAMETER( OutputBufferSize );

    PPORT_CONTEXT pPortContext = (PPORT_CONTEXT) ConnectionCookie;

    *ReturnOutputBufferLength = 0;

    ASSERT( pPortContext != NULL );
    if ( !pPortContext )
    {
        return STATUS_INVALID_PARAMETER;
    }

    return status;
}

__checkReturn
NTSTATUS
PortQueryConnected (
    __deref_out_opt PFLT_PORT* ppPort
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    FltAcquirePushLockShared( &Globals.m_ClientPortLock );
    if ( Globals.m_ClientPort)
    {
        *ppPort = Globals.m_ClientPort;
        status = STATUS_SUCCESS;
    }

    FltReleasePushLock( &Globals.m_ClientPortLock );

    return status;
}

void
PortRelease (
    __deref_in PFLT_PORT* ppPort
    )
{
    if ( *ppPort )
        *ppPort = NULL;
}
