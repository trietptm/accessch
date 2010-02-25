#include "main.h"
/*
http://msdn.microsoft.com/en-us/library/dd567984.aspx - формат device instance id
http://msdn.microsoft.com/en-us/library/dd567983.aspx -  здесь описано как генерируется ID если его нет в устройстве.

IRP_MN_QUERY_ID
IRP_MN_QUERY_CAPABILITIES
*/
__checkReturn
NTSTATUS
GetPnpInfo (
    __in PDEVICE_OBJECT pDevice,
    __out PVOID *pInformation
    )
{
    PIRP Irp;
    PIO_STACK_LOCATION irpSp;
    IO_STATUS_BLOCK statusBlock;
    KEVENT event;
    NTSTATUS status;
    PULONG_PTR returnInfo = (PULONG_PTR) pInformation;

    __try
    {
        Irp = IoBuildSynchronousFsdRequest(
            IRP_MJ_PNP,
            pDevice,
            NULL,
            0,
            NULL,
            &event,
            &statusBlock
            );

        if ( !Irp )
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
        }

        KeInitializeEvent (
            &event,
            SynchronizationEvent,
            FALSE
            );

        irpSp = IoGetNextIrpStackLocation( Irp );

        irpSp->MinorFunction = IRP_MN_QUERY_ID;
        //!\ todo check BusQueryDeviceSerialNumber on Vista
        irpSp->Parameters.QueryId.IdType = BusQueryInstanceID;

        status = IoCallDriver( pDevice, Irp );

        if ( STATUS_PENDING == status)
        {
            KeWaitForSingleObject (
                &event,
                Executive,
                KernelMode,
                FALSE,
                (PLARGE_INTEGER) NULL
                );
            
            status = statusBlock.Status;
        }
        
        if ( NT_SUCCESS( status ) )
        {
            if ( statusBlock.Information )
            {
                *returnInfo = (ULONG_PTR) statusBlock.Information;
            }
            else
            {
                status = STATUS_UNSUCCESSFUL;
            }
        }
    }
    __finally
    {
    }

    return status;
}

__checkReturn
NTSTATUS
GetPnpText (
    __in PDEVICE_OBJECT pDevice,
    __out PVOID *pInformation
    )
{
    PIRP Irp;
    PIO_STACK_LOCATION irpSp;
    IO_STATUS_BLOCK statusBlock;
    KEVENT event;
    NTSTATUS status;
    PULONG_PTR returnInfo = (PULONG_PTR) pInformation;

    __try
    {
        Irp = IoBuildSynchronousFsdRequest(
            IRP_MJ_PNP,
            pDevice,
            NULL,
            0,
            NULL,
            &event,
            &statusBlock
            );

        if ( !Irp )
        {
            status = STATUS_INSUFFICIENT_RESOURCES;
            __leave;
        }

        KeInitializeEvent (
            &event,
            SynchronizationEvent,
            FALSE
            );

        irpSp = IoGetNextIrpStackLocation( Irp );

        irpSp->MinorFunction = IRP_MN_QUERY_DEVICE_TEXT;
        irpSp->Parameters.QueryDeviceText.DeviceTextType = DeviceTextDescription;

        status = IoCallDriver( pDevice, Irp );

        if ( STATUS_PENDING == status)
        {
            KeWaitForSingleObject (
                &event,
                Executive,
                KernelMode,
                FALSE,
                (PLARGE_INTEGER) NULL
                );

            status = statusBlock.Status;
        }

        if ( NT_SUCCESS( status ) )
        {
            if ( statusBlock.Information )
            {
                *returnInfo = (ULONG_PTR) statusBlock.Information;
            }
            else
            {
                status = STATUS_UNSUCCESSFUL;
            }
        }
    }
    __finally
    {
    }

    return status;
}
