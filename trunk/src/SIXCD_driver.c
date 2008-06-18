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

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath)
{
    NTSTATUS status = STATUS_SUCCESS;
	HID_MINIDRIVER_REGISTRATION hidMinidriverRegistration;

    pDriverObject->MajorFunction[IRP_MJ_CREATE]		= SIXCDCreate;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE]		= SIXCDClose;
    pDriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL]	= SIXCDDispatchIntDevice;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]				= SIXCDDispatchDevice;
	pDriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL]				= SIXCDDispatchSystem;
    pDriverObject->MajorFunction[IRP_MJ_POWER]		= SIXCDDispatchPower;
    pDriverObject->MajorFunction[IRP_MJ_PNP]		= SIXCDDispatchPnp;
	pDriverObject->DriverUnload						= SIXCDUnload;
    pDriverObject->DriverExtension->AddDevice		= SIXCDAddDevice;

	RtlZeroMemory(&hidMinidriverRegistration, sizeof(HID_MINIDRIVER_REGISTRATION));

	hidMinidriverRegistration.Revision				= HID_REVISION;
	hidMinidriverRegistration.DriverObject			= pDriverObject;
	hidMinidriverRegistration.RegistryPath			= pRegistryPath;
	hidMinidriverRegistration.DeviceExtensionSize	= sizeof(DEVICE_EXTENSION);
	hidMinidriverRegistration.DevicesArePolled		= TRUE;

	status = HidRegisterMinidriver(&hidMinidriverRegistration);

	if (NT_SUCCESS(status))
	{
		KdPrint(("Minidriver Registration Worked"));
		RegistryPath.Buffer = (PWSTR) ExAllocatePool(PagedPool, pRegistryPath->Length + sizeof(WCHAR));
		RegistryPath.MaximumLength = pRegistryPath->Length + sizeof(WCHAR);
		RtlCopyUnicodeString(&RegistryPath, pRegistryPath);
		RegistryPath.Buffer[pRegistryPath->Length/sizeof(WCHAR)] = 0;
		KdPrint(("%ws", RegistryPath.Buffer));
	}
	else
	{
		KdPrint(("Minidriver Registration Failed"));
	}
    return status;
}

NTSTATUS SIXCDCreate(IN PDEVICE_OBJECT pFdo, IN PIRP pIrp)
{
    NTSTATUS status = STATUS_SUCCESS;

    pIrp->IoStatus.Status = status;
    pIrp->IoStatus.Information = 0;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	KdPrint(("SIXCDCreate"));

    return status;
}

NTSTATUS SIXCDClose(IN PDEVICE_OBJECT pFdo, IN PIRP pIrp)
{
    pIrp->IoStatus.Status = STATUS_SUCCESS;
    pIrp->IoStatus.Information = 0;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	KdPrint(("SIXCDClose"));

    return STATUS_SUCCESS;
}

#pragma PAGEDCODE

NTSTATUS SIXCDAddDevice(IN PDRIVER_OBJECT pDriverObject, IN PDEVICE_OBJECT pPdo)
{
    NTSTATUS status = STATUS_SUCCESS;
	NTSTATUS ntstatus;
    PDEVICE_OBJECT pFdo = pPdo;
    PDEVICE_EXTENSION pDevExt = GET_MINIDRIVER_DEVICE_EXTENSION(pFdo);

	PAGED_CODE();

	pDevExt->pPdo = pPdo;

	KeInitializeSpinLock(&pDevExt->polllock);
	//KeInitializeSpinLock(&pDevExt->readlock);

	InitializeRemoveLock(&pDevExt->RemoveLock,'SIXCD',0,0);

    // Set power management flags in the device object
    pFdo->Flags |= DO_POWER_PAGABLE | DO_DIRECT_IO;

	pDevExt->pLowerPdo = GET_LOWER_DEVICE_OBJECT(pFdo);

	if(!NT_SUCCESS(CreateInterruptUrb(pFdo)))
	{
		KdPrint(("SIXCDAddDevice - Could not create interrupt urb"));
	}

    // Clear the "initializing" flag so that we can get IRPs
    pFdo->Flags &= ~DO_DEVICE_INITIALIZING;

	KdPrint(("SIXCDAddDevice"));

    return status;
}

#pragma PAGEDCODE

VOID SIXCDUnload(IN PDRIVER_OBJECT pDriverObject)
{
	KdPrint(("SIXCDUnload"));
	if (RegistryPath.Buffer != NULL)
	{
		RtlFreeUnicodeString(&RegistryPath);
	}
	return;
}

#pragma PAGEDCODE

NTSTATUS SIXCDDispatchPnp(IN PDEVICE_OBJECT pFdo, IN PIRP pIrp)
{
    PDEVICE_EXTENSION pDevExt = GET_MINIDRIVER_DEVICE_EXTENSION(pFdo);
    PIO_STACK_LOCATION stack;
    NTSTATUS status = STATUS_SUCCESS;
	KEVENT event;

	PAGED_CODE();

	status = AcquireRemoveLock(&pDevExt->RemoveLock, pIrp);

	if (!NT_SUCCESS(status))
	{
		pIrp->IoStatus.Information = 0;
		pIrp->IoStatus.Status = status;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		return status;
	}

	status = SIXCDIncRequestCount(pDevExt);
	if (!NT_SUCCESS(status))
	{
		pIrp->IoStatus.Information = 0;
		pIrp->IoStatus.Status = status;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		return status;
	}

	stack = IoGetCurrentIrpStackLocation(pIrp);

	switch (stack->MinorFunction)
	{
	case IRP_MN_START_DEVICE:
		{
			KdPrint(("SIXCDDispatchPnp - IRP_MN_START_DEVICE entry"));

			KeInitializeEvent(&event, NotificationEvent, FALSE);

			IoCopyCurrentIrpStackLocationToNext(pIrp);
			IoSetCompletionRoutine(pIrp, SIXCDPnPComplete, &event, TRUE, TRUE, TRUE);
			status = IoCallDriver(pDevExt->pLowerPdo, pIrp);

			if (status == STATUS_PENDING)
			{
				status = KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
			}

			if(NT_SUCCESS(status))
			{
				status = SIXCDStartDevice(pFdo, pIrp);
			}

			pIrp->IoStatus.Information = 0;
			pIrp->IoStatus.Status = status;
			IoCompleteRequest(pIrp, IO_NO_INCREMENT);

			ReleaseRemoveLock(&pDevExt->RemoveLock, pIrp);

			KdPrint(("SIXCDDispatchPnp - IRP_MN_START_DEVICE exit"));

			break;
		}
	case IRP_MN_REMOVE_DEVICE:
		{
			KdPrint(("SIXCDDispatchPnp - IRP_MN_REMOVE_DEVICE entry"));

			pDevExt->DeviceRemoved = TRUE;

			IoCancelIrp(pDevExt->pIrp);

			if (!pDevExt->SurpriseRemoved)
			{
				ReleaseRemoveLockAndWait(&pDevExt->RemoveLock, pIrp);

				KdPrint(("SIXCDDispatchPnp - Stop device"));

				SIXCDStopDevice(pFdo, pIrp);
			}

			KdPrint(("SIXCDDispatchPnp - release and wait removelock"));

			KdPrint(("SIXCDDispatchPnp - Call removedevice"));

			SIXCDRemoveDevice(pFdo, pIrp);

			pIrp->IoStatus.Status = STATUS_SUCCESS;

			KdPrint(("SIXCDDispatchPnp - Pass irp down"));

			IoSkipCurrentIrpStackLocation(pIrp);
			status = IoCallDriver(pDevExt->pLowerPdo, pIrp);

			if (InterlockedDecrement(&pDevExt->RequestCount) > 0)
			{
				KeWaitForSingleObject(&pDevExt->RemoveEvent, Executive, KernelMode, FALSE, NULL );
			}

			status = STATUS_SUCCESS;

			KdPrint(("SIXCDDispatchPnp - IRP_MN_REMOVE_DEVICE exit"));

			return status;
		}
	case IRP_MN_STOP_DEVICE:
		{
			KdPrint(("SIXCDDispatchPnp - IRP_MN_STOP_DEVICE"));

			IoCancelIrp(pDevExt->pIrp);

			ReleaseRemoveLockAndWait(&pDevExt->RemoveLock, pIrp);

			SIXCDStopDevice(pFdo, pIrp);

			pIrp->IoStatus.Status = STATUS_SUCCESS;
			IoSkipCurrentIrpStackLocation(pIrp);
			status = IoCallDriver(pDevExt->pLowerPdo, pIrp);

			break;
		}
	case IRP_MN_QUERY_CAPABILITIES:
		{
			KdPrint(("SIXCDDispatchPnp - IRP_MN_QUERY_CAPABILITIES"));

			stack->Parameters.DeviceCapabilities.Capabilities->SurpriseRemovalOK = TRUE;
			stack->Parameters.DeviceCapabilities.Capabilities->EjectSupported = FALSE;
			stack->Parameters.DeviceCapabilities.Capabilities->Removable = TRUE;
			stack->Parameters.DeviceCapabilities.Capabilities->DockDevice = FALSE;
			stack->Parameters.DeviceCapabilities.Capabilities->LockSupported = FALSE;
			stack->Parameters.DeviceCapabilities.Capabilities->D1Latency = 0;
			stack->Parameters.DeviceCapabilities.Capabilities->D2Latency = 0;
			stack->Parameters.DeviceCapabilities.Capabilities->D3Latency = 0;

			pIrp->IoStatus.Information = 0;
            pIrp->IoStatus.Status = STATUS_SUCCESS;
            IoCompleteRequest(pIrp, IO_NO_INCREMENT);

			ReleaseRemoveLock(&pDevExt->RemoveLock, pIrp);

			break;
		}
	case IRP_MN_SURPRISE_REMOVAL:
		{
			KdPrint(("SIXCDDispatchPnp - IRP_MN_SURPRISE_REMOVAL entry"));

			pDevExt->SurpriseRemoved = TRUE;

			IoCancelIrp(pDevExt->pIrp);

			ReleaseRemoveLockAndWait(&pDevExt->RemoveLock, pIrp);

			SIXCDStopDevice(pFdo, pIrp);

			pIrp->IoStatus.Status = STATUS_SUCCESS;
			IoSkipCurrentIrpStackLocation(pIrp);
			status = IoCallDriver(pDevExt->pLowerPdo, pIrp);

			KdPrint(("SIXCDDispatchPnp - IRP_MN_SURPRISE_REMOVAL exit"));

			break;
		}
	default:
		{
			KdPrint(("SIXCDDispatchPnp - Irp %d not supported", stack->MinorFunction));

			IoSkipCurrentIrpStackLocation (pIrp);
			status = IoCallDriver(pDevExt->pLowerPdo, pIrp);
			ReleaseRemoveLock(&pDevExt->RemoveLock, pIrp);

			break;
		}
	}

		SIXCDDecRequestCount(pDevExt);

	return status;
}

#pragma PAGEDCODE

NTSTATUS SIXCDPnPComplete(PDEVICE_OBJECT pFdo, PIRP pIrp, PVOID Context)
{
    NTSTATUS status = STATUS_MORE_PROCESSING_REQUIRED;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(pFdo);
    KeSetEvent((PKEVENT) Context, 0, FALSE);

	// If the lower driver returned PENDING, mark our stack location as
	// pending also. This prevents the IRP's thread from being freed if
	// the client's call returns pending.
    if(pIrp->PendingReturned)
    {
        IoMarkIrpPending(pIrp);
    }

    return status;
}

#pragma LOCKEDCODE

NTSTATUS SIXCDIncRequestCount(PDEVICE_EXTENSION pDevExt)
{
    NTSTATUS Status;

    InterlockedIncrement( &pDevExt->RequestCount );
    ASSERT(pDevExt->RequestCount > 0);

    if (pDevExt->DeviceRemoved)
    {
		// PnP has already told us to remove the device so fail and make
		// sure that the event has been set.
        if (0 == InterlockedDecrement(&pDevExt->RequestCount))
        {
            KeSetEvent(&pDevExt->RemoveEvent, IO_NO_INCREMENT, FALSE);
        }
        Status = STATUS_DELETE_PENDING;
    }
    else
    {
        Status = STATUS_SUCCESS;
    }

    return Status;
}

VOID SIXCDDecRequestCount(PDEVICE_EXTENSION pDevExt)
{
    LONG LocalCount;

    LocalCount = InterlockedDecrement(&pDevExt->RequestCount);

    ASSERT(pDevExt->RequestCount >= 0);

    if (LocalCount == 0)
    {
		// PnP has already told us to remove the device so the PnP remove
		// code should have set device as removed and should be waiting on
		// the event.
		if(pDevExt->DeviceRemoved)
		{
			KeSetEvent(&pDevExt->RemoveEvent, IO_NO_INCREMENT, FALSE);
		}
    }

    return;
}

#pragma PAGEDCODE

NTSTATUS SIXCDStartDevice(PDEVICE_OBJECT pFdo, PIRP pIrp)
{
	NTSTATUS status;
	PDEVICE_EXTENSION pDevExt = GET_MINIDRIVER_DEVICE_EXTENSION(pFdo);
	ULONG size;
	URB urb; // URB for use in this subroutine
	USB_CONFIGURATION_DESCRIPTOR tcd;
	PUSB_CONFIGURATION_DESCRIPTOR pcd;
	PUSB_INTERFACE_DESCRIPTOR pid;
	USBD_INTERFACE_LIST_ENTRY interfaces[2] = {{NULL, NULL},{NULL,NULL}};
	PURB selurb;
	PUSBD_INTERFACE_INFORMATION pii;

	//PAGED_CODE();

	// Read our device descriptor. The only real purpose to this would be to find out how many
	// configurations there are so we can read their descriptors. There's only one configuration.

	KdPrint(("SIXCDStartDevice - getting device descriptor"));
	UsbBuildGetDescriptorRequest(&urb, sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST), USB_DEVICE_DESCRIPTOR_TYPE,
		0, 0, &pDevExt->dd, NULL, sizeof(pDevExt->dd), NULL);
	status = SendAwaitUrb(pFdo, &urb);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("SIXCDStartDevice - Error %X trying to read device descriptor", status));
		return status;
	}

	// Read the descriptor of the first configuration. This requires two steps. The first step
	// reads the fixed-size configuration descriptor alone. The second step reads the
	// configuration descriptor plus all imbedded interface and endpoint descriptors.

	KdPrint(("SIXCDStartDevice - getting configuration descriptor"));
	UsbBuildGetDescriptorRequest(&urb, sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST), USB_CONFIGURATION_DESCRIPTOR_TYPE,
		0, 0, &tcd, NULL, sizeof(tcd), NULL);
	status = SendAwaitUrb(pFdo, &urb);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("SIXCDStartDevice - Error %X trying to read configuration descriptor 1", status));
		return status;
	}

	size = tcd.wTotalLength;
	pcd = (PUSB_CONFIGURATION_DESCRIPTOR) ExAllocatePool(NonPagedPool, size);
	if (!pcd)
	{
		KdPrint(("SIXCDStartDevice - Unable to allocate %X bytes for configuration descriptor", size));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	KdPrint(("SIXCDStartDevice - Getting second part of configuration descriptor"));
	UsbBuildGetDescriptorRequest(&urb, sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST), USB_CONFIGURATION_DESCRIPTOR_TYPE,
		0, 0, pcd, NULL, size, NULL);
	status = SendAwaitUrb(pFdo, &urb);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("SIXCDStartDevice - Error %X trying to read configuration descriptor 1", status));
		return status;
	}

	// Locate the descriptor for the one and only interface we expect to find
	pid = USBD_ParseConfigurationDescriptorEx(pcd, pcd, -1, -1, -1, -1, -1);

	// Create a URB to use in selecting a configuration.
	interfaces[0].InterfaceDescriptor = pid;
	interfaces[0].Interface = NULL;
	interfaces[1].InterfaceDescriptor = NULL;
	interfaces[1].Interface = NULL;

	KdPrint(("SIXCDStartDevice - selecting the configuration"));
	selurb = USBD_CreateConfigurationRequestEx(pcd, interfaces);
	if (!selurb)
	{
		KdPrint(("SIXCDStartDevice - Unable to create configuration request"));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Verify that the interface describes exactly the endpoints we expect
	if (pid->bNumEndpoints != 2)
	{
		KdPrint(("SIXCDStartDevice - %d is the wrong number of endpoints", pid->bNumEndpoints));
		return STATUS_DEVICE_CONFIGURATION_ERROR;
	}

	pii = interfaces[0].Interface;
	if (pii->NumberOfPipes != pid->bNumEndpoints)
	{
		KdPrint(("SIXCDStartDevice - NumberOfPipes %d does not match bNumEndpoints %d",pii->NumberOfPipes, pid->bNumEndpoints));
		return STATUS_DEVICE_CONFIGURATION_ERROR;
	}

	KdPrint(("Pipe 0 : MaxTransfer %d, MaxPckSize %d, PipeType %d, Interval %d, Handle %d Address %d", pii->Pipes[0].MaximumTransferSize, pii->Pipes[0].MaximumPacketSize, pii->Pipes[0].PipeType, pii->Pipes[0].Interval, pii->Pipes[0].PipeHandle, pii->Pipes[0].EndpointAddress));

	//pii->Pipes[0].MaximumTransferSize = 0x0020;
	//pii->Pipes[0].MaximumPacketSize = 0x0020;
	//pii->Pipes[0].PipeType = UsbdPipeTypeInterrupt;
	//pii->Pipes[0].Interval = 0x04;

	KdPrint(("Pipe 1 : MaxTransfer %d, MaxPckSize %d, PipeType %d, Interval %d, Handle %d Address %d", pii->Pipes[1].MaximumTransferSize, pii->Pipes[1].MaximumPacketSize, pii->Pipes[1].PipeType, pii->Pipes[1].Interval, pii->Pipes[1].PipeHandle, pii->Pipes[1].EndpointAddress));

	//pii->Pipes[1].MaximumTransferSize = 0x0006;
	//pii->Pipes[1].MaximumPacketSize = 0x0020;
	//pii->Pipes[1].PipeType = UsbdPipeTypeInterrupt;
	//pii->Pipes[1].Interval = 0x04;

	// Submit the set-configuration request
	status = SendAwaitUrb(pFdo, selurb);
	if (!NT_SUCCESS(status))
	{
		KdPrint(("SIXCDStartDevice - Error %X trying to select configuration", status));
		return status;
	}

	// Save the configuration and pipe handles
	pDevExt->hconfig = selurb->UrbSelectConfiguration.ConfigurationHandle;
	pDevExt->hintpipe = pii->Pipes[0].PipeHandle;
	pDevExt->hintoutpipe = pii->Pipes[1].PipeHandle;

	//ResetPipe(pFdo,pii->Pipes[0].PipeHandle);
	//ResetPipe(pFdo,pii->Pipes[1].PipeHandle);

	// Transfer ownership of the configuration descriptor to the device extension
	pDevExt->pcd = pcd;
	pcd = NULL;

	// Initialize the variable that will contain the data read from device
	RtlZeroMemory(&pDevExt->intdata, sizeof(pDevExt->intdata));
	RtlZeroMemory(&pDevExt->intoutdata, sizeof(pDevExt->intoutdata));
	RtlZeroMemory(&pDevExt->buttons, sizeof(pDevExt->buttons));

	ExFreePool(selurb);

	if (pcd)
	{
		ExFreePool(pcd);
	}

	KeInitializeDpc(&pDevExt->timeDPC, timerDPCProc, pDevExt);
	KeInitializeTimer(&pDevExt->timer);

	SIXCDReadButtonConfig(pFdo);

	pDevExt->DeviceStarted = TRUE;
    return STATUS_SUCCESS;
}

#pragma PAGEDCODE

VOID SIXCDRemoveDevice(PDEVICE_OBJECT pFdo, PIRP pIrp)
{
	PDEVICE_EXTENSION pDevExt = GET_MINIDRIVER_DEVICE_EXTENSION(pFdo);
	NTSTATUS status;

	pDevExt->PowerDown = TRUE;

	if(pDevExt->pIrp)
	{
		KdPrint(("SIXCDRemoveDevice - Trying to delete the interrupt urb"));
		DeleteInterruptUrb(pFdo);
	}
	return;
}

VOID SIXCDStopDevice(PDEVICE_OBJECT pFdo, PIRP pIrp)
{
	NTSTATUS status;
	PDEVICE_EXTENSION pDevExt = GET_MINIDRIVER_DEVICE_EXTENSION(pFdo);
	URB urb;

	// Cancel the URB in case it's currently active
	KdPrint(("SIXCDStopDevice - About to stop the interrupt urb"));
	StopInterruptUrb(pDevExt);
	KdPrint(("SIXCDStopDevice - Stopped the interrupt urb"));

	KeCancelTimer(&pDevExt->timer);

	if(pDevExt->DeviceStarted)
	{
		pDevExt->DeviceStarted = FALSE;

		KdPrint(("SIXCDStopDevice - Starting to deconfigure device"));
		UsbBuildSelectConfigurationRequest(&urb, sizeof(struct _URB_SELECT_CONFIGURATION), NULL);
		status = SendAwaitUrb(pFdo, &urb);
		KdPrint(("SIXCDStopDevice - Deconfiguring device"));
		if(!NT_SUCCESS(status))
		{
			KdPrint(("SIXCDStopDevice - Error %X trying to deconfigure device", status));
		}
	}

	KdPrint(("SIXCDStopDevice - freeing pcd"));
	if(pDevExt->pcd)
		ExFreePool(pDevExt->pcd);
	pDevExt->pcd = NULL;

	KdPrint(("SIXCDStopDevice - passing irp down"));
	KdPrint(("SIXCDStopDevice - returning"));
	return;
}

#pragma PAGEDCODE

NTSTATUS SIXCDDispatchPower(IN PDEVICE_OBJECT pFdo, IN PIRP pIrp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
    PDEVICE_EXTENSION pDevExt = GET_MINIDRIVER_DEVICE_EXTENSION(pFdo);
    /*switch (stack->MinorFunction)
    {
    case IRP_MN_WAIT_WAKE:
		KdPrint(("SIXCDDispatchPower - IRP_MN_WAIT_WAKE"));
        break;
    case IRP_MN_POWER_SEQUENCE:
		KdPrint(("SIXCDDispatchPower - IRP_MN_POWER_SEQUENCE"));
        break;
    case IRP_MN_SET_POWER:
		KdPrint(("SIXCDDispatchPower - IRP_MN_SET_POWER"));
        break;
    case IRP_MN_QUERY_POWER:
		KdPrint(("SIXCDDispatchPower - IRP_MN_QUERY_POWER"));
        break;
    }*/
	IoSkipCurrentIrpStackLocation(pIrp);
	PoStartNextPowerIrp(pIrp);
	KdPrint(("SIXCDDispatchPower"));
    return PoCallDriver(pDevExt->pLowerPdo, pIrp);
}

#pragma PAGEDCODE

int ReadRegistry(HANDLE hKey, PCWSTR entry, int defValue)
{
	NTSTATUS ntstatus;
	PKEY_VALUE_PARTIAL_INFORMATION vpi;
	ULONG size = 0;
	int iReceived = 0;
	UNICODE_STRING valname;

	RtlInitUnicodeString(&valname, entry);

	size = sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(valname) + sizeof(int);
	vpi = (PKEY_VALUE_PARTIAL_INFORMATION) ExAllocatePool(PagedPool, size);
	ntstatus = ZwQueryValueKey(hKey, &valname, KeyValuePartialInformation, vpi, size, &size);

	if(NT_SUCCESS(ntstatus))
	{
		RtlCopyMemory(&iReceived, &vpi->Data, sizeof(iReceived));
	}
	else
	{
		iReceived = defValue;
	}
	ExFreePool(vpi);

	return iReceived;
}

void SIXCDReadButtonConfig(IN PDEVICE_OBJECT pFdo)
{
	PDEVICE_EXTENSION pDevExt = GET_MINIDRIVER_DEVICE_EXTENSION(pFdo);
	HANDLE hKey;
	NTSTATUS ntstatus;
	int iReceived;

	pDevExt->LStickDZ = 0;
	pDevExt->RStickDZ = 0;

	ntstatus = IoOpenDeviceRegistryKey(GET_PHYSICAL_DEVICE_OBJECT(pFdo), PLUGPLAY_REGKEY_DEVICE, KEY_ALL_ACCESS, &hKey);
	if (NT_SUCCESS(ntstatus))
	{
		int count;
		KdPrint(("IoOpenDeviceRegistryKey - success"));
		pDevExt->btnset = TRUE;
		for(count=1; count<=12; count++)
		{
			if(count == 1)
				iReceived = ReadRegistry(hKey, L"1", 0);
			if(count == 2)
				iReceived = ReadRegistry(hKey, L"2", 0);
			if(count == 3)
				iReceived = ReadRegistry(hKey, L"3", 0);
			if(count == 4)
				iReceived = ReadRegistry(hKey, L"4", 0);
			if(count == 5)
				iReceived = ReadRegistry(hKey, L"5", 0);
			if(count == 6)
				iReceived = ReadRegistry(hKey, L"6", 0);
			if(count == 7)
				iReceived = ReadRegistry(hKey, L"7", 0);
			if(count == 8)
				iReceived = ReadRegistry(hKey, L"8", 0);
			if(count == 9)
				iReceived = ReadRegistry(hKey, L"9", 0);
			if(count == 10)
				iReceived = ReadRegistry(hKey, L"10", 0);
			if(count == 11)
				iReceived = ReadRegistry(hKey, L"11", 0);
			if(count == 12)
				iReceived = ReadRegistry(hKey, L"12", 0);

			KdPrint(("Button %d - %d",count, iReceived));
			if((iReceived < 1) || (iReceived > 12))
			{
				pDevExt->btnset = FALSE;
			}
			else
			{
				pDevExt->buttons[count - 1] = power2(iReceived - 1);
			}

		}

		iReceived = ReadRegistry(hKey, L"LStickDZ", 0);
		if(iReceived < 0)
		{
			pDevExt->LStickDZ = 0;
		}
		else
		{
			if(iReceived >= 100)
			{
				pDevExt->LStickDZ = 32766;
			}
			else
			{
				pDevExt->LStickDZ = 32767 * iReceived/100;
			}
		}
		KdPrint(("%d", iReceived));

		iReceived = ReadRegistry(hKey, L"RStickDZ", 0);
		if(iReceived < 0)
		{
			pDevExt->RStickDZ = 0;
		}
		else
		{
			if(iReceived >= 100)
			{
				pDevExt->RStickDZ = 32766;
			}
			else
			{
				pDevExt->RStickDZ = 32767 * iReceived/100;
			}
		}
		KdPrint(("%d", iReceived));

		iReceived = ReadRegistry(hKey, L"LAFactor", 100);
		if((iReceived < 0) || (iReceived > 100))
		{
			pDevExt->LAFactor = 100;
		}
		else
		{
			pDevExt->LAFactor = iReceived;
		}
		KdPrint(("LAFactor = %d", pDevExt->LAFactor));

		iReceived = ReadRegistry(hKey, L"RAFactor", 100);
		if((iReceived < 0) || (iReceived > 100))
		{
			pDevExt->RAFactor = 100;
		}
		else
		{
			pDevExt->RAFactor = iReceived;
		}
		KdPrint(("RAFactor = %d", pDevExt->RAFactor));

		pDevExt->iNumConf = 0;
		pDevExt->iCurrentConf = 0;

		for(count=1; count<=8; count++)
		{
			if(count == 1)
				iReceived = ReadRegistry(hKey, L"Conf1", 0);
			if(count == 2)
				iReceived = ReadRegistry(hKey, L"Conf2", 0);
			if(count == 3)
				iReceived = ReadRegistry(hKey, L"Conf3", 0);
			if(count == 4)
				iReceived = ReadRegistry(hKey, L"Conf4", 0);
			if(count == 5)
				iReceived = ReadRegistry(hKey, L"Conf5", 0);
			if(count == 6)
				iReceived = ReadRegistry(hKey, L"Conf6", 0);
			if(count == 7)
				iReceived = ReadRegistry(hKey, L"Conf7", 0);
			if(count == 8)
				iReceived = ReadRegistry(hKey, L"Conf8", 0);

			switch(iReceived)
			{
			case 1:
				{
					pDevExt->iConf[count - 1] = 1;
					pDevExt->iNumConf += 1;

					if(count == 1)
					{
						pDevExt->iXYMov = 2;
						pDevExt->iPOV = 1;
						pDevExt->iSlider = 4;
					}
					break;
				}
			case 2:
				{
					pDevExt->iConf[count - 1] = 2;
					pDevExt->iNumConf += 1;

					if(count == 1)
					{
						pDevExt->iXYMov = 1;
						pDevExt->iPOV = 4;
						pDevExt->iSlider = 2;
					}
					break;
				}
			case 3:
				{
					pDevExt->iConf[count - 1] = 3;
					pDevExt->iNumConf += 1;

					if(count == 1)
					{
						pDevExt->iXYMov = 4;
						pDevExt->iPOV = 2;
						pDevExt->iSlider = 1;
					}
					break;
				}
			case 4:
				{
					pDevExt->iConf[count - 1] = 4;
					pDevExt->iNumConf += 1;

					if(count == 1)
					{
						pDevExt->iXYMov = 2;
						pDevExt->iPOV = 4;
						pDevExt->iSlider = 1;
					}
					break;
				}
			case 5:
				{
					pDevExt->iConf[count - 1] = 5;
					pDevExt->iNumConf += 1;

					if(count == 1)
					{
						pDevExt->iXYMov = 4;
						pDevExt->iPOV = 1;
						pDevExt->iSlider = 2;
					}
					break;
				}
			case 6:
				{
					pDevExt->iConf[count - 1] = 6;
					pDevExt->iNumConf += 1;

					if(count == 1)
					{
						pDevExt->iXYMov = 1;
						pDevExt->iPOV = 2;
						pDevExt->iSlider = 4;
					}
					break;
				}
			case 7:
				{
					pDevExt->iConf[count - 1] = 7;
					pDevExt->iNumConf += 1;

					if(count == 1)
					{
						pDevExt->iXYMov = 1;
						pDevExt->iXYMov |= 2;
						pDevExt->iPOV = 0;
						pDevExt->iSlider = 4;
					}
					break;
				}
			case 8:
				{
					pDevExt->iConf[count - 1] = 8;
					pDevExt->iNumConf += 1;

					if(count == 1)
					{
						pDevExt->iXYMov = 1;
						pDevExt->iXYMov |= 2;
						pDevExt->iPOV = 4;
						pDevExt->iSlider = 0;
					}
					break;
				}
			default:
				{
					pDevExt->iConf[count - 1] = 0;
					break;
				}

			}
		}

		if (pDevExt->iNumConf == 0)
		{
			pDevExt->iNumConf = 1;
			pDevExt->iConf[0] = 1;

			pDevExt->iXYMov = 2;
			pDevExt->iPOV = 1;
			pDevExt->iSlider = 4;
		}

		iReceived = ReadRegistry(hKey, L"BThreshold", 0);
		if((iReceived < 1) || (iReceived > 255))
		{
			pDevExt->BThreshold = 10;
		}
		else
		{
			pDevExt->BThreshold = iReceived;
		}

		iReceived = ReadRegistry(hKey, L"TThreshold", 0);
		if((iReceived < 1) || (iReceived > 255))
		{
			pDevExt->TThreshold = 10;
		}
		else
		{
			pDevExt->TThreshold = iReceived;
		}

		iReceived = ReadRegistry(hKey, L"TThrottle", 0);
		if((iReceived < 0) || (iReceived > 1))
		{
			pDevExt->bTThrottle = 0;
		}
		else
		{
			pDevExt->bTThrottle = iReceived;
		}

		iReceived = ReadRegistry(hKey, L"TShortcut", 0);
		if((iReceived < 0) || (iReceived > 1))
		{
			pDevExt->bTShortcut = 0;
		}
		else
		{
			pDevExt->bTShortcut = iReceived;
		}

		iReceived = ReadRegistry(hKey, L"SSwitch", 0);
		if((iReceived < 0) || (iReceived > 1))
		{
			pDevExt->bSSwitch = 0;
		}
		else
		{
			pDevExt->bSSwitch = iReceived;
		}

		iReceived = ReadRegistry(hKey, L"FPCalc", 0);
		if((iReceived < 0) || (iReceived > 1))
		{
			pDevExt->bFPCalc = 0;
		}
		else
		{
			pDevExt->bFPCalc = iReceived;
		}

		iReceived = ReadRegistry(hKey, L"DPadButtons", 0);
		if((iReceived < 0) || (iReceived > 1))
		{
			pDevExt->bDPadButtons = 0;
		}
		else
		{
			pDevExt->bDPadButtons = iReceived;
		}

	}
	ZwClose(hKey);
}

int power2(int n)
{
	int vTotal;
	int Count;
	for(Count=0; !(Count > n); Count++)
	{
		if(Count==0)
		{
			vTotal = 1;
		}
		else
		{
			vTotal= vTotal * 2;
		}
	}
	return vTotal;
}

#pragma LOCKEDCODE