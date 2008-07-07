/*
    SIXaxis Control Driver implements a KMDF windows filter.
    Copyright (C) 2008  Mythgarr

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "sixcd.h"

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
    WDF_DRIVER_CONFIG params;
    NTSTATUS  status;

    DbgPrint("SIXCD: ---> DriverEntry\n");

    WDF_DRIVER_CONFIG_INIT(
                        &params,
                        SixcdEvtDeviceAdd
                        );

    //
    // Create the framework WDFDRIVER object, with the handle
    // to it returned in Driver.
    //
    status = WdfDriverCreate(DriverObject,
                             RegistryPath,
                             WDF_NO_OBJECT_ATTRIBUTES,
                             &params,
                             WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        //
        // Framework will automatically cleanup on error Status return
        //
        DbgPrint("SIXCD: Error Creating WDFDRIVER 0x%x\n", status);
    }

	DbgPrint("SIXCD: <--- DriverEntry\n");
    return status;
}

NTSTATUS
SixcdEvtDeviceAdd(
    IN WDFDRIVER       Driver,
    IN PWDFDEVICE_INIT DeviceInit
    )
/*++

Arguments:

    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.

Return Value:

    NTSTATUS

--*/
{
	NTSTATUS                      status = STATUS_SUCCESS;
	WDF_OBJECT_ATTRIBUTES         attributes;
	WDF_IO_QUEUE_CONFIG           queueConfig;
	WDFDEVICE                     hDevice;
	PDEVICE_EXTENSION             devContext = NULL;
	WDF_PNPPOWER_EVENT_CALLBACKS  pnpPowerCallbacks;
	WDF_TIMER_CONFIG              timerConfig;
	WDFTIMER                      timerHandle;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	DbgPrint("SIXCD: ---> SixcdEvtDeviceAdd");

	// Tell the framework that this is a filter driver. This saves us some extra work coding unused functions.
	WdfFdoInitSetFilter(DeviceInit);


	// Initialize PnP power callbacks
	WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

	// PrepareHardware is used to do any special initialization or configuration for the device.
	// This is the likely future home of rumble & LED initialization code.
//	pnpPowerCallbacks.EvtDevicePrepareHardware = SixcdEvtDevicePrepareHardware;

	// Set the callback functions for entering & exiting the D0 state
	pnpPowerCallbacks.EvtDeviceD0Entry = SixcdEvtDeviceD0Entry;
	pnpPowerCallbacks.EvtDeviceD0Exit  = SixcdEvtDeviceD0Exit;

	WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);


	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_EXTENSION);

	// Create a framework device object.
	status = WdfDeviceCreate(&DeviceInit, &attributes, &hDevice);
	if (!NT_SUCCESS(status)) {
		DbgPrint("SIXCD: WdfDeviceCreate failed with status code 0x%x\n", status);
		return status;
	}

	devContext = GetDeviceContext(hDevice);

	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig,
							WdfIoQueueDispatchParallel);
	
	queueConfig.EvtIoDefault  = SixcdEvtIoDefault;

	// Set up our Device Control function - this is the real meat & potatoes for this filter
	queueConfig.EvtIoInternalDeviceControl  = SixcdEvtInternalDeviceControl;

	status = WdfIoQueueCreate(hDevice,
							  &queueConfig,
							  WDF_NO_OBJECT_ATTRIBUTES,
							  WDF_NO_HANDLE
							  );
	if (!NT_SUCCESS (status)) {
		DbgPrint("SIXCD: WdfIoQueueCreate failed 0x%x\n", status);
		return status;
	}

	DbgPrint("SIXCD: <--- SixcdEvtDeviceAdd");
	return status;
}



