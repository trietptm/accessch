#ifndef __accesscheck_h
#define __accesscheck_h

#define ACCESSCH_PORT_NAME          L"\\AccessCheckPort"

#define _STREAM_FLAGS_DIRECTORY		0x00000001

#include <pshpack8.h>
typedef struct _REPLY_RESULT
{
    ULONG			m_Flags;
} REPLY_RESULT, *PREPLY_RESULT;

typedef struct _MESSAGE_DATA
{
    ULONG			m_ProcessId;
    ULONG			m_ThreadId;
    LUID			m_Luid;
    ULONG			m_FlagsStream;
    ULONG			m_FlagsHandle;
    ULONG			m_FileNameOffset;
    ULONG			m_FileNameLen;
    ULONG			m_VolumeNameOffset;
    ULONG			m_VolumeNameLen;
    ULONG			m_SidOffset;
    ULONG			m_SidLength;
    ULONG			m_Flags;
} MESSAGE_DATA, *PMESSAGE_DATA;

#include <poppack.h>

#endif // __accesscheck_h 