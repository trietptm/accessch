#pragma prefast(disable:28252) // sal annotation compare h -> cpp
#pragma prefast(disable:28253) // sal annotation compare h -> cpp
#pragma prefast(disable:28107) // critical region checks
#pragma prefast(disable:28175) // access to DEVICE_OBJECT members

#include <fltKernel.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <ntdddisk.h>

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

typedef struct _PORT_CONTEXT
{
    PFLT_PORT               m_Connection;
}PORT_CONTEXT, *PPORT_CONTEXT;

typedef struct _INSTANCE_CONTEXT
{
    DEVICE_TYPE             m_VolumeDeviceType;
    FLT_FILESYSTEM_TYPE     m_VolumeFilesystemType;
} INSTANCE_CONTEXT, *PINSTANCE_CONTEXT;

typedef struct _STREAM_CONTEXT
{
    LUID                    m_Luid;
    LONG                    m_Flags;
} STREAM_CONTEXT, *PSTREAM_CONTEXT;

typedef struct _STREAM_HANDLE_CONTEXT
{
} STREAM_HANDLE_CONTEXT, *PSTREAM_HANDLE_CONTEXT;

// ----------------------------------------------------------------------------

#define _VOLUME_DESCRIPTION_LENGTH  0x20
typedef struct _VOLUME_CONTEXT
{
    PFLT_INSTANCE           m_Instance;
    STORAGE_BUS_TYPE        m_BusType;
    DEVICE_REMOVAL_POLICY   m_RemovablePolicy;
    UCHAR                   m_VendorId[_VOLUME_DESCRIPTION_LENGTH];
    UCHAR                   m_ProductId[_VOLUME_DESCRIPTION_LENGTH];
    UCHAR                   m_ProductRevisionLevel[_VOLUME_DESCRIPTION_LENGTH];
    UCHAR                   m_VendorSpecific[_VOLUME_DESCRIPTION_LENGTH];
} VOLUME_CONTEXT, *PVOLUME_CONTEXT;

#include "mm.h"
