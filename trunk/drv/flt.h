#ifndef __flt_h
#define __flt_h

typedef enum Interceptor
{
    FILE_MINIFILTER = 0,
};

typedef ULONG VERDICT, *PVERDICT;
#define VERDICT_NOT_FILTERED    0x0000
#define VERDICT_ALLOW           0x0001
#define VERDICT_NOT_DENY        0x0002
#define VERDICT_ASK             0x0004

typedef
__checkReturn
NTSTATUS
( *PFN_QUERY_EVENT_PARAM ) (
    __in PVOID Opaque,
    __deref_out_opt PVOID* Data,
    __deref_out_opt PULONG DataSize
    );

typedef struct EventData
{
    PVOID                   m_Opaque;
    PFN_QUERY_EVENT_PARAM   m_QueryFunc;
    Interceptor             InterceptorId;
    ULONG                   m_Major;
    ULONG                   m_Minor;
    ULONG                   m_ParamsCount;
    PULONG                  m_Params;
}*PEventData;

NTSTATUS
FilterEvent (
    __in PEventData Event,
    __inout PVERDICT Verdict
    );

#endif //__flt_h