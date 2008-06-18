/*
    Copyright 2004 Andrew Jones

    This file is part of SIXCD.

    SIXCD is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    SIXCD is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "SIXCD_driver.h"

#pragma PAGEDCODE

//Send URB to the lower device object
NTSTATUS SendAwaitUrb(PDEVICE_OBJECT pFdo, PURB pUrb)
	{
	PDEVICE_EXTENSION pDevExt = GET_MINIDRIVER_DEVICE_EXTENSION(pFdo);
	KEVENT event;
	PIRP pIrp;
	IO_STATUS_BLOCK iostatus;
	PIO_STACK_LOCATION stack;
	NTSTATUS status;

	//PAGED_CODE();
	//ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	KeInitializeEvent(&event, NotificationEvent, FALSE);

	pIrp = IoBuildDeviceIoControlRequest(IOCTL_INTERNAL_USB_SUBMIT_URB,
		pDevExt->pLowerPdo, NULL, 0, NULL, 0, TRUE, &event, &iostatus);

	if (!pIrp)
		{
			KdPrint(("SendAwaitUrb - Unable to allocate IRP for sending URB"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}

	stack = IoGetNextIrpStackLocation(pIrp);
	stack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
	stack->Parameters.Others.Argument1 = pUrb;
	status = IoCallDriver(pDevExt->pLowerPdo, pIrp);
	if (status == STATUS_PENDING)
		{
			KdPrint(("SendAwaitUrb - status_pending"));
			KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
			status = iostatus.Status;
		}
	if(NT_SUCCESS(status))
	{
		KdPrint(("SendAwaitUrb - status success"));
	}
	else
	{
		KdPrint(("SendAwaitUrb - error %d", status));
	}
	KdPrint(("SendAwaitUrb - returning"));
	return status;
}

#pragma LOCKEDCODE

NTSTATUS StartInterruptUrb(PDEVICE_EXTENSION pDevExt)
	{

	NTSTATUS status;
	PIRP Irp;
	PURB urb;
	PIO_STACK_LOCATION stack;
	BOOLEAN bStartIrp;
	KIRQL oldirql;

	KeAcquireSpinLock(&pDevExt->polllock, &oldirql);

	// If the IRP is currently running, don't try to start it again.
	if (pDevExt->pollpending)
		bStartIrp = FALSE;
	else
		bStartIrp = TRUE, pDevExt->pollpending = TRUE;

	KeReleaseSpinLock(&pDevExt->polllock, oldirql);

	if (!bStartIrp)
		return STATUS_DEVICE_BUSY;	// already pending

	Irp = pDevExt->pIrp;
	urb = pDevExt->pUrb;
	if(!(Irp && urb))
	{
		KdPrint(("StartInterruptUrb - Could not get Irp and urb from Device Extension"));
	}

	// Acquire the remove lock so we can't remove the lower device while the IRP
	// is still active.
	status = AcquireRemoveLock(&pDevExt->RemoveLock, Irp);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("StartInterruptUrb - Acquiring remove lock failed"));
		pDevExt->pollpending = FALSE;
		return status;
	}

	KdPrint(("StartInterruptUrb - Building urb"));
	UsbBuildInterruptOrBulkTransferRequest(urb, sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
		pDevExt->hintpipe, &pDevExt->intdata, NULL, 20, USBD_TRANSFER_DIRECTION_IN | USBD_SHORT_TRANSFER_OK, NULL);

	// Install "OnInterrupt" as the completion routine for the IRP.
	IoSetCompletionRoutine(Irp, (PIO_COMPLETION_ROUTINE) OnInterrupt, pDevExt, TRUE, TRUE, TRUE);

	// Initialize the IRP for an internal control request
	stack = IoGetNextIrpStackLocation(Irp);
	stack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
	stack->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;
	stack->Parameters.Others.Argument1 = urb;

	// Set the IRP Cancel flag to false.  It could have been canceled previously
	// and we need to reuse it.  IoReuseIrp is not available in Windows 9x.
	Irp->Cancel = FALSE;

	KdPrint(("StartInterruptUrb - Returning"));
	return IoCallDriver(pDevExt->pLowerPdo, Irp);
}

NTSTATUS SendInterruptUrb(PDEVICE_EXTENSION pDevExt)
	{

	NTSTATUS status;
	PIRP pIrp;
	IO_STATUS_BLOCK iostatus;
	PIO_STACK_LOCATION stack;
	KEVENT event;
	PURB urb = (PURB)ExAllocatePool(NonPagedPool, sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER));


	KdPrint(("SendInterruptUrb - Building urb"));
	UsbBuildInterruptOrBulkTransferRequest(urb, sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
		pDevExt->hintoutpipe, &pDevExt->intoutdata, NULL, 6, USBD_TRANSFER_DIRECTION_OUT | USBD_SHORT_TRANSFER_OK, NULL);

	//status = SendAwaitUrb(pDevExt->pPdo, urb);

	KeInitializeEvent(&event, NotificationEvent, FALSE);

	pIrp = IoBuildDeviceIoControlRequest(IOCTL_INTERNAL_USB_SUBMIT_URB,
		pDevExt->pLowerPdo, NULL, 0, NULL, 0, TRUE, &event, &iostatus);

	if (!pIrp)
		{
			KdPrint(("SendInterruptUrb - Unable to allocate IRP for sending URB"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}

	stack = IoGetNextIrpStackLocation(pIrp);
	stack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
	stack->Parameters.Others.Argument1 = urb;
	status = IoCallDriver(pDevExt->pLowerPdo, pIrp);
	if (status == STATUS_PENDING)
		{
			KdPrint(("SendAwaitUrb - status_pending"));
			KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
			status = iostatus.Status;
		}
	if(NT_SUCCESS(status))
	{
		KdPrint(("SendInterruptUrb - status success"));
	}
	else
	{
		KdPrint(("SendInterruptUrb - error %d", status));
	}
	KdPrint(("SendInterruptUrb - returning"));

	return status;
}

#pragma PAGEDCODE
//Create an Interrupt Urb
NTSTATUS CreateInterruptUrb(PDEVICE_OBJECT pFdo)
	{
	PDEVICE_EXTENSION pDevExt = GET_MINIDRIVER_DEVICE_EXTENSION(pFdo);
	PIRP pIrp;
	PURB pUrb;

	pIrp = IoAllocateIrp(pDevExt->pLowerPdo->StackSize, FALSE);
	if (!pIrp)
		{
		KdPrint(("CreateInterruptUrb - Unable to create IRP for interrupt polling"));
		return STATUS_INSUFFICIENT_RESOURCES;
		}

	pUrb = (PURB) ExAllocatePool(NonPagedPool, sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER));
	if (!pUrb)
		{
		KdPrint(("CreateInterruptUrb - Unable to allocate interrupt polling URB"));
		IoFreeIrp(pIrp);
		return STATUS_INSUFFICIENT_RESOURCES;
		}

	pDevExt->pIrp = pIrp;
	pDevExt->pUrb = pUrb;

	return STATUS_SUCCESS;
}

#pragma PAGEDCODE
//Delete the Interrup Urb
VOID DeleteInterruptUrb(PDEVICE_OBJECT pFdo)
{
	PDEVICE_EXTENSION pDevExt = GET_MINIDRIVER_DEVICE_EXTENSION(pFdo);

	//ASSERT(pDevExt->pIrp != NULL);
	//ASSERT(pDevExt->pUrb != NULL);

	ExFreePool(pDevExt->pUrb);
	IoFreeIrp(pDevExt->pIrp);
	pDevExt->pIrp = NULL;
	pDevExt->pUrb = NULL;
}

#pragma LOCKEDCODE

NTSTATUS OnInterrupt(PDEVICE_OBJECT junk, PIRP pIrp, PDEVICE_EXTENSION pDevExt)
	{
	KIRQL oldirql;
	NTSTATUS Status;

	KeAcquireSpinLock(&pDevExt->polllock, &oldirql);

	if (NT_SUCCESS(pIrp->IoStatus.Status))
	{
		//Status = SIXCDReadData(pDevExt, pDevExt->CurrentIrp);

		KdPrint(("OnInterrupt - Success reading report"));
	}
	else
	{
		KdPrint(("OnInterrupt - Failed to read report"));
	}

	pDevExt->pollpending = FALSE;	// allow another poll to be started
	KeReleaseSpinLock(&pDevExt->polllock, oldirql);

	KdPrint(("OnInterrupt - Releasing Removelock"));
	ReleaseRemoveLock(&pDevExt->RemoveLock, pDevExt->pIrp); // balances acquisition in StartInterruptUrb

	if(pDevExt->timerEnabled)
		Status = StartInterruptUrb(pDevExt);

	KdPrint(("OnInterrupt - Returning"));
	return STATUS_MORE_PROCESSING_REQUIRED;
}

#pragma LOCKEDCODE

VOID StopInterruptUrb(PDEVICE_EXTENSION pDevExt)
{
	KdPrint(("StopInterruptUrb - Entered"));
	if (pDevExt->pollpending)
	{
		KdPrint(("StopInterruptUrb - Canceling pIrp"));
		IoCancelIrp(pDevExt->pIrp);
	}
	return;
}

/*++

Routine Description:

    This routine synchronously submits a URB_FUNCTION_RESET_PIPE
    request down the stack.

Arguments:

    DeviceObject - pointer to device object
    PipeInfo - pointer to PipeInformation structure
               to retrieve the pipe handle

Return Value:

    NT status value

--*/

/*NTSTATUS ResetPipe(IN PDEVICE_OBJECT DeviceObject, IN USBD_PIPE_HANDLE *PipeHandle)
{
    PURB              urb;
    NTSTATUS          ntStatus;
    PDEVICE_EXTENSION deviceExtension;

    //
    // initialize variables
    //

    urb = NULL;
    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;


    urb = ExAllocatePool(NonPagedPool,
                         sizeof(struct _URB_PIPE_REQUEST));

    if(urb) {

        urb->UrbHeader.Length = (USHORT) sizeof(struct _URB_PIPE_REQUEST);
        urb->UrbHeader.Function = URB_FUNCTION_RESET_PIPE;
        urb->UrbPipeRequest.PipeHandle = PipeHandle;

        ntStatus = SendAwaitUrb(DeviceObject, urb);

        ExFreePool(urb);
    }
    else {

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    if(NT_SUCCESS(ntStatus)) {

        KdPrint(("ResetPipe - success\n"));
        ntStatus = STATUS_SUCCESS;
    }
    else {

        KdPrint(("ResetPipe - failed\n"));
    }

    return ntStatus;
}*/