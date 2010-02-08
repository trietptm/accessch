#pragma warning(error:4100)     //  Enable-Unreferenced formal parameter
#pragma warning(error:4101)     //  Enable-Unreferenced local variable
#pragma warning(error:4061)     //  Eenable-missing enumeration in switch statement
#pragma warning(error:4505)     //  Enable-identify dead functions

#include <fltKernel.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#define _VOLUME_DESCRIPTION_LENGTH  0x20
typedef struct _VOLUME_CONTEXT
{
    PFLT_INSTANCE           m_Instance;
    ULONG                   m_BusType; //STORAGE_BUS_TYPE
    DEVICE_REMOVAL_POLICY   m_RemovablePolicy;
    UCHAR                   m_VendorId[_VOLUME_DESCRIPTION_LENGTH];
    UCHAR                   m_ProductId[_VOLUME_DESCRIPTION_LENGTH];
    UCHAR                   m_ProductRevisionLevel[_VOLUME_DESCRIPTION_LENGTH];
    UCHAR                   m_VendorSpecific[_VOLUME_DESCRIPTION_LENGTH];
} VOLUME_CONTEXT, *PVOLUME_CONTEXT;

#include "mm.h"
