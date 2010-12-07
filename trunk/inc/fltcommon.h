#ifndef __filters_h
#define __filters_h

typedef ULONG VERDICT, *PVERDICT;
#define VERDICT_NOT_FILTERED        0x0000
#define VERDICT_DENY                0x0001
#define VERDICT_ASK                 0x0002
#define VERDICT_CACHE1              0x0100

#define PARAMS_MASK __int64
#define PPARAMS_MASK PARAMS_MASK*
#define Id2Bit( _id ) ( (PARAMS_MASK) 1 << _id )
#define _PARAMS_COUNT ( sizeof( PARAMS_MASK ) * 8 )

typedef enum FltOperation
{
    FltOp_equ      = 0x0000,
    FltOp_and      = 0x0001,
    FltOp_pattern  = 0x0002,
};

#define FltFlags ULONG
#define FltFlags_None              0x0000
#define FltFlags_Negation          0x0001
#define FltFlags_BePresent         0x0002
#define FltFlags_CaseInsensitive   0x0010


#include <pshpack8.h>

typedef struct _FltBoxControl
{
    GUID        m_Guid;
    ULONG       m_BitCount;
    ULONG       m_BitMask[1];
} FltBoxControl, *PFltBoxControl;

typedef struct _FltData
{
    ULONG               m_Size;
    ULONG               m_Count;
    union
    {
        UCHAR           m_Data[1];
        FltBoxControl   m_Box[1];
    };

} FltData, *PFltData;

typedef struct _FltParam
{
    ULONG               m_ParameterId;
    FltOperation        m_Operation;
    FltFlags            m_Flags;
    FltData             m_Data;
} FltParam, *PFltParam;

// send message communication  
typedef struct _REPLY_RESULT
{
    VERDICT             m_Flags;
} REPLY_RESULT, *PREPLY_RESULT;

#include <poppack.h>

#endif // __filters_h