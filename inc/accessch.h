#ifndef __accesscheck_h
#define __accesscheck_h

#define ACCESSCH_PORT_NAME          L"\\AccessCheckPort"
#define ACCESSCH_MAX_CONNECTIONS    1

#define DRV_EVENT_CONTENT_SIZE      0x1000

#define _STREAM_FLAGS_DIRECTORY     0x00000001
#define _STREAM_FLAGS_CASHE1        0x00000002
#define _STREAM_FLAGS_DELONCLOSE    0x00000004

typedef enum Interceptors
{
    FILE_MINIFILTER                 = 1,
    VOLUME_MINIFILTER               = 2,
};

typedef enum DriverOperationId
{
    OP_UNKNOWN                      = -1,
    OP_VOLUME_ATTACH                = 1,
    OP_FILE_CREATE                  = 2,
    OP_FILE_CLEANUP                 = 3,
};

typedef enum OperationPoint
{
    PreProcessing   = 0x001,
    PostProcessing  = 0x002,
};

typedef enum Parameters
{
    // bit position
    PARAMETER_RESERVED0             = 0,
    PARAMETER_RESERVED1             = 1,
    PARAMETER_FILE_NAME             = 2,
    PARAMETER_VOLUME_NAME           = 3,
    PARAMETER_REQUESTOR_PROCESS_ID  = 4,
    PARAMETER_CURRENT_THREAD_ID     = 5,
    PARAMETER_LUID                  = 6,
    PARAMETER_SID                   = 7,
    PARAMETER_DESIRED_ACCESS        = 8,
    PARAMETER_CREATE_OPTIONS        = 9,
    PARAMETER_OBJECT_STREAM_FLAGS   = 10,
    PARAMETER_CREATE_MODE           = 11,
    PARAMETER_RESULT_STATUS         = 20,
    PARAMETER_RESULT_INFORMATION    = 21,
    PARAMETER_DEVICE_TYPE           = 30,
    PARAMETER_FILESYSTEM_TYPE       = 31,
    PARAMETER_BUS_TYPE              = 32,
    PARAMETER_DEVICE_ID             = 33,

    //
    PARAMETER_MAXIMUM               = 63
} *PParameters;

#define Id2Bit( _id ) ( 1 << _id )

#define PARAMS_MASK __int64
#define _PARAMS_COUNT ( sizeof( PARAMS_MASK ) * 8 )

typedef ULONG VERDICT, *PVERDICT;
#define VERDICT_NOT_FILTERED        0x0000
#define VERDICT_ALLOW               0x0001
#define VERDICT_DENY                0x0002
#define VERDICT_ASK                 0x0004
#define VERDICT_CACHE1              0x0100

#include <pshpack8.h>

// send message communication  
typedef struct _REPLY_RESULT
{
    VERDICT             m_Flags;
} REPLY_RESULT, *PREPLY_RESULT;

typedef struct _EVENT_PARAMETER
{
    Parameters          m_Id;
    ULONG               m_Size;
    UCHAR               m_Data[1];
} EVENT_PARAMETER, *PEVENT_PARAMETER;

typedef struct _MESSAGE_DATA
{
    ULONG               m_EventId;
    Interceptors        m_Interceptor;
    DriverOperationId   m_OperationId;
    ULONG               m_FuncionMi;
    OperationPoint      m_OperationType;
    ULONG               m_ParametersCount;
    EVENT_PARAMETER     m_Parameters[1];
} MESSAGE_DATA, *PMESSAGE_DATA;

// notify structures
typedef enum _NOTIFY_ID
{
    // common commands
    ntfcom_Connect       = 0001,
    ntfcom_FiltersChain  = 0050,
    
    // object's commands
    ntfcom_PrepareIO     = 0100 // result struct
} NOTIFY_ID;

typedef struct _NOTIFY_COMMAND
{
    ULONG               m_Reserved;
    NOTIFY_ID           m_Command;
    union 
    {
        ULONG           m_EventId;
        UCHAR           m_Data[1];
    };
} NOTIFY_COMMAND, *PNOTIFY_COMMAND;

// ntfcom_PrepareIO
typedef struct _NC_IOPREPARE
{
    HANDLE              m_Section;
    LARGE_INTEGER       m_IoSize;
} NC_IOPREPARE, *PNC_IOPREPARE;

// filters structures
// основная сложность при работе с фильтрами в хелпере, который позволяет
// формировать сложные структуры в r3

typedef enum FltOperation
{
    _fltop_equ      = 0x0000,
    _fltop_and      = 0x0001,
};

typedef struct _FILTER_PARAMETER
{
    ULONG               m_Size;
    UCHAR               m_Data[1];
} FILTER_PARAMETER, *PFILTER_PARAMETER;

#define _PARAM_ENTRY_FLAG_NEGATION  0x0001

typedef struct _PARAM_ENTRY
{
    Parameters          m_Id;
    //ULONG               m_Count;
    FltOperation        m_Operation;
    ULONG               m_Flags;
    FILTER_PARAMETER    m_FltData;
}PARAM_ENTRY, *PPARAM_ENTRY;

typedef struct _FILTER
{
    Interceptors        m_Interceptor;
    DriverOperationId   m_FunctionMj;
    ULONG               m_FunctionMi;
    OperationPoint      m_OperationType;
    VERDICT             m_Verdict;
    ULONG               m_RequestTimeout;       //msec
    PARAMS_MASK         m_WishMask;
    ULONG               m_ParamsCount;
    PARAM_ENTRY         m_Params[1];
} FILTER,*PFILTER;

typedef enum ChainOperation
{
    _fltchain_add       = 0,
    _fltchain_del       = 1,
};

typedef struct _CHAIN_ENTRY
{
    ChainOperation      m_Operation;
    union
    {
        FILTER          m_Filter[1];    // add filters
        ULONG           m_Id[1];        // delete filters
    };

} CHAIN_ENTRY,*PCHAIN_ENTRY;

typedef struct _FILTERS_CHAIN
{
    ULONG               m_Count;
    CHAIN_ENTRY         m_Entry[1];

} FILTERS_CHAIN, *PFILTERS_CHAIN;
// end filters structures

#include <poppack.h>

#endif // __accesscheck_h 