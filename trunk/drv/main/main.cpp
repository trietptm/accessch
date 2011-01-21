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
#include "../inc/processhelper.h"

#include "../../inc/accessch.h"

#include "main.tmh"

typedef struct _Globals
{
    PDRIVER_OBJECT          m_FilterDriverObject;
    FilteringSystem*        m_FilteringSystem;
    ProcessHelper*          m_ProcessHelper;
}Globals, *PGlobals;

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
    DoTraceEx( TRACE_LEVEL_CRITICAL, TB_CORE, "DriverUnload..." );

    ChannelDestroyPort();

    FileMgrUnregister();
   
    FREE_OBJECT( GlobalData.m_FilteringSystem );
    FREE_OBJECT( GlobalData.m_ProcessHelper );

    DoTraceEx( TRACE_LEVEL_CRITICAL, TB_CORE, "DriverUnload complete" );

    WPP_CLEANUP( GlobalData.m_FilterDriverObject->DeviceObject );
}

NTSTATUS
DriverEntry (
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    UNREFERENCED_PARAMETER( RegistryPath );
    WPP_INIT_TRACING( DriverObject, RegistryPath );

    DoTraceEx( TRACE_LEVEL_CRITICAL, TB_CORE, "DriverEntry..." );

    RtlZeroMemory( &GlobalData, sizeof( GlobalData) );

    GlobalData.m_FilterDriverObject = DriverObject;
    
    __try
    {
        status = GetPreviousModeOffset();
        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        GlobalData.m_ProcessHelper = new (
            PagedPool,
            ProcessHelper::m_AllocTag
            ) ProcessHelper;

        if ( !GlobalData.m_ProcessHelper )
        {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        GlobalData.m_FilteringSystem = new (
            PagedPool,
            FilteringSystem::m_AllocTag
            ) FilteringSystem;

        if ( !GlobalData.m_FilteringSystem )
        {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        status = FileMgrInit (
            DriverObject,
            DriverUnload,
            GlobalData.m_FilteringSystem
            );

        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        status = ChannelInitPort(
            GlobalData.m_ProcessHelper,
            GlobalData.m_FilteringSystem
            );

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
        DoTraceEx( TRACE_LEVEL_CRITICAL, TB_CORE, "DriverEntry result %!STATUS!", status );

        if ( !NT_SUCCESS( status ) )
        {
            __debugbreak(); //nct
            DriverUnload();
        }
    }

    return status;
}

void
RegisterProcess (
    HANDLE ProcessId
    )
{
    NTSTATUS status = GlobalData.m_ProcessHelper->RegisterProcessItem( ProcessId );

    ASSERT( NT_SUCCESS( status ) );
}

void
UnregisterProcess (
    HANDLE ProcessId
    )
{
    GlobalData.m_ProcessHelper->UnregisterProcessItem( ProcessId );
}