#include "main.h"

#include "ntdddisk.h"
#include "ntddvol.h"

__checkReturn
NTSTATUS
PDORequest (
    __in PDEVICE_OBJECT pDevice,
    __in ULONG IoControlCode,
    __in PVOID pBufferIn,
    __in ULONG BufferInSize,
    __in PVOID *ppBufferOut,
    __in PULONG pBufferOutSize
    );

__checkReturn
NTSTATUS
GetSmartInfo (
    __in PDEVICE_OBJECT pDevice
    )
{
    NTSTATUS status;
    PVOLUME_DISK_EXTENTS pvdExtents = NULL;
    PGETVERSIONINPARAMS pVersionParams = NULL;

    __try
    {
        ULONG BufferSize = 0;

        status = PDORequest (
            pDevice,
            SMART_GET_VERSION,
            NULL,
            0,
            (PVOID*) &pVersionParams,
            &BufferSize
            );

        if ( !NT_SUCCESS( status ) )
        {
            pVersionParams = NULL;
            __leave;
        }
        
        status = PDORequest (
            pDevice,
            IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
            NULL,
            0,
            (PVOID*) &pvdExtents,
            &BufferSize
            );

        if ( !NT_SUCCESS( status ) )
        {
            pvdExtents = NULL;
            __leave;
        }

        #define IDENTIFY_BUFFER_SIZE 512

        SENDCMDINPARAMS InParams = {
            IDENTIFY_BUFFER_SIZE, { 0, 1, 1, 0, 0,
            ((pvdExtents->Extents[0].DiskNumber & 1) ? 0xB0 : 0xA0),
            ((pVersionParams->bIDEDeviceMap >>
            pvdExtents->Extents[0].DiskNumber & 0x10) ? ATAPI_ID_CMD : ID_CMD) },
            (UCHAR)pvdExtents->Extents[0].DiskNumber
        };

        PSENDCMDOUTPARAMS pbtIDOutCmd = NULL;
        
          //Get SMART information
        status = PDORequest (
            pDevice,
            SMART_RCV_DRIVE_DATA,
            &InParams,
            sizeof(SENDCMDINPARAMS),
            (PVOID*) &pbtIDOutCmd, 
            &BufferSize
            );

        if ( !NT_SUCCESS( status ) )
        {
            __leave;
        }

        //RtlCopyMemory(lpszSerialNumber1, &pIDSector[10], 20);
    }
    __finally
    {
        if ( pVersionParams )
        {
            ExFreePool( pVersionParams );
            pVersionParams = NULL;
        }

        if ( pvdExtents )
        {
            ExFreePool( pvdExtents );
            pvdExtents = NULL;
        }
    }

    return status;
}
