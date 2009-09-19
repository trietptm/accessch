#ifndef __common_h
#define __common_h

#include <pshpack8.h>
typedef struct _REPLY_RESULT
{
	ULONG			m_Flags;		// see \todo
} REPLY_RESULT, *PREPLY_RESULT;
#include <poppack.h>

#endif //__common_h