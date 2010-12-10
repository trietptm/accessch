#include "../inc/commonkrnl.h"
#include "../inc/fltsystem.h"

// ----------------------------------------------------------------------------
//

typedef struct _Globals
{
    PDRIVER_OBJECT          m_FilterDriverObject;
    FilteringSystem*        m_FilteringSystem;
}Globals, *PGlobals;

extern Globals GlobalData;

