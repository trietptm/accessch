#pragma once

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

extern PFLT_FILTER gFileFilter;
