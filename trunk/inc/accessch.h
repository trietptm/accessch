#ifndef __accesscheck_h
#define __accesscheck_h

#define ACCESSCH_PORT_NAME          L"\\AccessCheckPort"
#define ACCESSCH_MAX_CONNECTIONS    1

#define DRV_EVENT_CONTENT_SIZE      0x1000

#define _STREAM_FLAGS_DIRECTORY     0x00000001

typedef enum Interceptors
{
    FILE_MINIFILTER                 = 0,
};

typedef enum Parameters
{
	// bit position
	PARAMETER_FILE_NAME             = 1,
    PARAMETER_VOLUME_NAME           = 2,
    PARAMETER_REQUESTOR_PROCESS_ID  = 3,
    PARAMETER_CURRENT_THREAD_ID     = 4,
    PARAMETER_LUID                  = 5,
    PARAMETER_SID                   = 6,
    PARAMETER_ACCESS_MODE           = 7,
    PARAMETER_CREATE_OPTIONS        = 8,
} *PParameters;

#define _PARAMS_COUNT ( sizeof( PARAMS_MASK ) * sizeof( CHAR ) )

typedef ULONG VERDICT, *PVERDICT;
#define VERDICT_NOT_FILTERED        0x0000
#define VERDICT_ALLOW               0x0001
#define VERDICT_DENY                0x0002
#define VERDICT_ASK                 0x0004

#define Id2Bit( _id ) ( 1 << _id )
#define PARAMS_MASK ULONG

#include <pshpack8.h>

// send message communication  
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

// notify strutures
typedef enum _NOTIFY_COMMANDS
{
    // common commands
    ntfcom_Connect       = 0x0001,
    
    // object's commands
    ntfcom_PrepareIO     = 0x0100 // result struct
} NOTIFY_COMMANDS;

typedef struct _NOTIFY_COMMAND
{
    ULONG               m_Reserved;
    NOTIFY_COMMANDS     m_Command;
    ULONG               m_EventId;
} NOTIFY_COMMAND, *PNOTIFY_COMMAND;

// ntfcom_PrepareIO
typedef struct _NC_IOPREPARE
{
    HANDLE              m_Section;
    LARGE_INTEGER       m_IoSize;
} NC_IOPREPARE, *PNC_IOPREPARE;

#include <poppack.h>

#endif // __accesscheck_h 