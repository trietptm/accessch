#ifndef __accesscheck_h
#define __accesscheck_h

#define ACCESSCH_PORT_NAME          L"\\AccessCheckPort"
#define DRV_EVENT_CONTENT_SIZE      0x1000

#define _STREAM_FLAGS_DIRECTORY        0x00000001

typedef enum Interceptors
{
    FILE_MINIFILTER = 0,
};

typedef enum Parameters
{
    PARAMETER_FILE_NAME             = 0,
    PARAMETER_VOLUME_NAME           = 1,
    PARAMETER_REQUESTOR_PROCESS_ID  = 2,
    PARAMETER_CURRENT_THREAD_ID     = 3,
    PARAMETER_LUID                  = 4,
    PARAMETER_SID                   = 5,
} *PParameters;

typedef ULONG VERDICT, *PVERDICT;
#define VERDICT_NOT_FILTERED    0x0000
#define VERDICT_ALLOW           0x0001
#define VERDICT_NOT_DENY        0x0002
#define VERDICT_ASK             0x0004

#include <pshpack8.h>
typedef struct _REPLY_RESULT
{
    ULONG               m_Flags;
} REPLY_RESULT, *PREPLY_RESULT;

typedef struct _SINGLE_PARAMETER
{
    Parameters          m_Id;
    ULONG               m_Size;
    UCHAR               m_Data[1];
} SINGLE_PARAMETER, *PSINGLE_PARAMETER;

typedef struct _MESSAGE_DATA
{
    ULONG               m_EventId;
    ULONG               m_ParametersCount;
    SINGLE_PARAMETER    m_Parameters[1];
} MESSAGE_DATA, *PMESSAGE_DATA;

#include <poppack.h>

#endif // __accesscheck_h 