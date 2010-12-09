#include "../inc/commonkrnl.h"
#include "../inc/fltsystem.h"

// ----------------------------------------------------------------------------
//

typedef struct _Globals
{
    PDRIVER_OBJECT          m_FilterDriverObject;
    PFLT_PORT               m_Port;
    EX_RUNDOWN_REF          m_RefClientPort;

    FilteringSystem*        m_FilteringSystem;
}Globals, *PGlobals;

extern Globals GlobalData;

