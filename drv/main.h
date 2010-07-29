#include "pch.h"

// ----------------------------------------------------------------------------
//

typedef struct _GLOBALS
{
    PDRIVER_OBJECT          m_FilterDriverObject;
    PFLT_FILTER             m_Filter;
    PFLT_PORT               m_Port;
    EX_PUSH_LOCK            m_ClientPortLock;
    PFLT_PORT               m_ClientPort;
}GLOBALS, *PGLOBALS;

extern GLOBALS Globals;

// ----------------------------------------------------------------------------
//

typedef struct _INSTANCE_CONTEXT
{
    DEVICE_TYPE             m_VolumeDeviceType;
    FLT_FILESYSTEM_TYPE     m_VolumeFilesystemType;
} INSTANCE_CONTEXT, *PINSTANCE_CONTEXT;

typedef struct _STREAM_CONTEXT
{
    PINSTANCE_CONTEXT       m_InstanceContext;
    LONG                    m_Flags;
    LONG                    m_WriteCount;
} STREAM_CONTEXT, *PSTREAM_CONTEXT;

typedef struct _STREAM_HANDLE_CONTEXT
{
    PSTREAM_CONTEXT         m_StreamContext;
    LUID                    m_Luid;
} STREAM_HANDLE_CONTEXT, *PSTREAM_HANDLE_CONTEXT;