#include "pch.h"

// ----------------------------------------------------------------------------
//

typedef struct _GLOBALS
{
    PDRIVER_OBJECT          m_FilterDriverObject;
    PFLT_FILTER             m_Filter;
    PFLT_PORT               m_Port;
    EX_PUSH_LOCK            m_ClientPortLock;
    PFLT_PORT               m_ClientPort;
}GLOBALS, *PGLOBALS;

extern GLOBALS Globals;
