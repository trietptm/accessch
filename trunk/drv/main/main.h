#include "../inc/commonkrnl.h"
#include "../inc/fltsystem.h"

// ----------------------------------------------------------------------------
//

typedef struct _Globals
{
    PDRIVER_OBJECT          m_FilterDriverObject;
    PFLT_FILTER             m_Filter;
    PFLT_PORT               m_Port;
    EX_RUNDOWN_REF          m_RefClientPort;
    PFLT_PORT               m_ClientPort;

    FilteringSystem*        m_FilteringSystem;
}Globals, *PGlobals;

extern Globals GlobalData;

// ----------------------------------------------------------------------------
//

typedef struct _InstanceContext
{
    DEVICE_TYPE             m_VolumeDeviceType;
    FLT_FILESYSTEM_TYPE     m_VolumeFilesystemType;
} InstanceContext, *PInstanceContext;

typedef struct _StreamContext
{
    PInstanceContext        m_InstanceCtx;
    LONG                    m_Flags;
    LONG                    m_WriteCount;
} StreamContext, *PStreamContext;

typedef struct _StreamHandleContext
{
    PStreamContext          m_StreamCtx;
    LUID                    m_Luid;
    LONG                    m_Flags;
} StreamHandleContext, *PStreamHandleContext;