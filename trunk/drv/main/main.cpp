//!
//    \author - Andrey Sobko (andrey.sobko@gmail.com)
//    \date - 06.07.2009 - 02.09.2010
//    \description - sample driver
//!

/// \todo check necessary headers
#include "../inc/commonkrnl.h"
#include "../inc/memmgr.h"
#include "../inc/filemgr.h"
#include "../inc/fltsystem.h"
#include "../inc/osspec.h"
#include "../inc/channel.h"

#include "../../inc/accessch.h"

#include "main.h"

#include "proclist.h"

Globals GlobalData;

extern "C"
{
    NTSTATUS
    DriverEntry (
        __in PDRIVER_OBJECT DriverObject,
        __in PUNICODE_STRING RegistryPath
        );
}

void
NTAPI
DriverUnload (
    )
{
    ChannelDestroyPort();
    FREE_OBJECT( GlobalData.m_FilteringSystem );
    ProcList::Destroy();
}

NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    UNREFERENCED_PARAMETER( RegistryPath );

    RtlZeroMemory( &GlobalData, sizeof( GlobalData) );

    GlobalData.m_FilterDriverObject = DriverObject;
    
    ProcList::Initialize();

    __try
    {
        GlobalData.m_FilteringSystem = new (
            PagedPool,
            FilteringSystem::m_AllocTag
            ) FilteringSystem;

        if ( !GlobalData.m_FilteringSystem )
        {
            __leave;
        }

        status = GetPreviousModeOffset();
        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        status = FileMgrInit (
            DriverObject,
            DriverUnload
            );

        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        status = ChannelInitPort();
        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        status = FileMgrStart();
        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        ASSERT( NT_SUCCESS( status ) );
    }
    __finally
    {
        if ( !NT_SUCCESS( status ) )
        {
            FREE_OBJECT( GlobalData.m_FilteringSystem );

            ChannelDestroyPort();

            ProcList::Destroy();
        }
    }

    return status;
}

NTSTATUS
RegisterExitProcessCb (
    __in _tpProcessExitCb CbFunc,
    __in_opt PVOID Opaque
    )
{
    NTSTATUS status = ProcList::RegisterExitProcessCb (
        CbFunc,
        Opaque
        );

    return status;
}

void
UnregisterExitProcessCb ( 
    __in _tpProcessExitCb CbFunc
    )
{
    ProcList::UnregisterExitProcessCb( CbFunc );
}

FilteringSystem*
GetFltSystem (
    )
{
    return GlobalData.m_FilteringSystem;
}