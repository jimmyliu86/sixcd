#define USE_HARDCODED_HID_DESCRIPTORS
#include "sixcd.h"

NTSTATUS
SixcdEvtDevicePrepareHardware(
    IN WDFDEVICE    Device,
    IN WDFCMRESLIST ResourceList,
    IN WDFCMRESLIST ResourceListTranslated
	)
{
    NTSTATUS                            status = STATUS_SUCCESS;
    PDEVICE_EXTENSION                   devContext = NULL;
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS configParams;
    WDF_OBJECT_ATTRIBUTES               attributes;
    PUSB_DEVICE_DESCRIPTOR              usbDeviceDescriptor = NULL;

    UNREFERENCED_PARAMETER(ResourceList);
    UNREFERENCED_PARAMETER(ResourceListTranslated);

    PAGED_CODE ();

    DbgPrint("SIXCD: --->SixcdEvtDevicePrepareHardware\n");

    devContext = GetDeviceContext(Device);

    //
    // Create a WDFUSBDEVICE object. WdfUsbTargetDeviceCreate obtains the
    // USB device descriptor and the first USB configuration descriptor from
    // the device and stores them. It also creates a framework USB interface
    // object for each interface in the device's first configuration.
    //
    // The parent of each USB device object is the driver's framework driver
    // object. The driver cannot change this parent, and the ParentObject
    // member or the WDF_OBJECT_ATTRIBUTES structure must be NULL.
    //

    status = WdfUsbTargetDeviceCreate(Device,
                                WDF_NO_OBJECT_ATTRIBUTES,
                                &devContext->UsbDevice);
    if (!NT_SUCCESS(status)) {
		DbgPrint("SIXCD: WdfUsbTargetDeviceCreate failed (Device:0x%x,WDF_NO_OBJECT_ATTRIBUTES:0x%x,status:%s)\n", Device, WDF_NO_OBJECT_ATTRIBUTES, GET_NTSTATUS_TEXT(status) );
        return status;
    }

    //
    // Select a device configuration by using a
    // WDF_USB_DEVICE_SELECT_CONFIG_PARAMS structure to specify USB
    // descriptors, a URB, or handles to framework USB interface objects.
    //
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE( &configParams);

    status = WdfUsbTargetDeviceSelectConfig(devContext->UsbDevice,
                                        WDF_NO_OBJECT_ATTRIBUTES,
                                        &configParams);
    if(!NT_SUCCESS(status)) {
        DbgPrint("SIXCD: WdfUsbTargetDeviceSelectConfig failed %!STATUS!\n",
            status);
        return status;
    }

    devContext->UsbInterface =
                configParams.Types.SingleInterface.ConfiguredUsbInterface;

    //
    // Get the device descriptor and store it in device context
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = Device;
    status = WdfMemoryCreate(
                             &attributes,
                             NonPagedPool,
                             0,
                             sizeof(USB_DEVICE_DESCRIPTOR),
                             &devContext->DeviceDescriptor,
                             &usbDeviceDescriptor
                             );

    if(!NT_SUCCESS(status)) {
        DbgPrint("SIXCD: WdfMemoryCreate for Device Descriptor failed %!STATUS!\n",
            status);
        return status;
    }

    WdfUsbTargetDeviceGetDeviceDescriptor(
          devContext->UsbDevice,
          usbDeviceDescriptor
          );

    //
    // Get the Interrupt pipe. There are other endpoints but we are only
    // interested in interrupt endpoint since our HID data comes from that
    // endpoint. Another way to get the interrupt endpoint is by enumerating
    // through all the pipes in a loop and looking for pipe of Interrupt type.
    //
    devContext->InterruptPipe = WdfUsbInterfaceGetConfiguredPipe(
                                                  devContext->UsbInterface,
                                                  INTERRUPT_ENDPOINT_INDEX,
                                                  NULL);// pipeInfo

    if (NULL == devContext->InterruptPipe) {
        DbgPrint("SIXCD: Failed to get interrupt pipe info\n");
        status = STATUS_INVALID_DEVICE_STATE;
        return status;
    }

    //
    // Tell the framework that it's okay to read less than
    // MaximumPacketSize
    //
    WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(devContext->InterruptPipe);

    //
    //configure continuous reader
    //

//	status = SixcdConfigContReaderForInterruptEndPoint(devContext);

    DbgPrint("SIXCD: <--- SixcdEvtDevicePrepareHardware(status=0x%x)\n",status);

    return status;
}

VOID
SixcdEvtIoDefault(
    IN WDFQUEUE     Queue,
    IN WDFREQUEST   Request
    )
{
    WDF_REQUEST_SEND_OPTIONS options;
    NTSTATUS status;
    WDF_REQUEST_PARAMETERS params;
    BOOLEAN ret;

	DbgPrint("SIXCD: ---> SixcdEvtIoDefault");

    WDF_REQUEST_PARAMETERS_INIT(&params);

    WdfRequestGetParameters(
                            Request,
                            &params
                            );

    WdfRequestFormatRequestUsingCurrentType(Request);

    WDF_REQUEST_SEND_OPTIONS_INIT(
                                  &options,
                                  WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET
                                  );

    ret = WdfRequestSend (
                          Request,
                          WdfDeviceGetIoTarget(WdfIoQueueGetDevice(Queue)),
                          &options
                          );
    if (!ret) {
        status = WdfRequestGetStatus(Request);
		DbgPrint("SIXCD: Failed to forward request (status:0x%x)",status);
        WdfRequestComplete(
                           Request,
                           status
                           );
    }

	DbgPrint("SIXCD: <--- SixcdEvtIoDefault");
    return;
}

VOID
SixcdEvtInternalDeviceControl(
    IN WDFQUEUE     Queue,
    IN WDFREQUEST   Request,
    IN size_t       OutputBufferLength,
    IN size_t       InputBufferLength,
    IN ULONG        IoControlCode
    )
/*++

Routine Description:

    This event is called when the framework receives
    IRP_MJ_INTERNAL DEVICE_CONTROL requests from the system.

Arguments:

    Queue - Handle to the framework queue object that is associated
            with the I/O request.
    Request - Handle to a framework request object.

    OutputBufferLength - length of the request's output buffer,
                        if an output buffer is available.
    InputBufferLength - length of the request's input buffer,
                        if an input buffer is available.

    IoControlCode - the driver-defined or system-defined I/O control code
                    (IOCTL) that is associated with the request.
Return Value:

    VOID

--*/
{
    NTSTATUS            status = STATUS_SUCCESS;
    WDFDEVICE           device;
    PDEVICE_EXTENSION   devContext = NULL;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

	DbgPrint("SIXCD: ---> SixcdEvtInternalDeviceControl(IOCTL=%s,0x%x)\n",GET_INTERNAL_IOCTL_NAME(IoControlCode),IoControlCode);

    device = WdfIoQueueGetDevice(Queue);
/*	devContext = GetDeviceContext(device);

    //
    // Please note that HIDCLASS provides the buffer in the Irp->UserBuffer
    // field irrespective of the ioctl buffer type. However, framework is very
    // strict about type checking. You cannot get Irp->UserBuffer by using
    // WdfRequestRetrieveOutputMemory if the ioctl is not a METHOD_NEITHER
    // internal ioctl. So depending on the ioctl code, we will either
    // use retreive function or escape to WDM to get the UserBuffer.
    //

    switch(IoControlCode) {

		case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
			//
			// Retrieves the device's HID descriptor.
			//
			status = SixcdGetHidDescriptor(device, Request);
			break;

		case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
			//
			//Retrieves a device's attributes in a HID_DEVICE_ATTRIBUTES structure.
			//
			status = SixcdGetDeviceAttributes(Request);
			break;

		case IOCTL_HID_GET_REPORT_DESCRIPTOR:
			//
			//Obtains the report descriptor for the HID device.
			//
			status = SixcdGetReportDescriptor(device, Request);
			break;

		case IOCTL_HID_READ_REPORT:

			//
			// Returns a report from the device into a class driver-supplied buffer.
			// For now queue the request to the manual queue. The request will
			// be retrived and completd when continuous reader reads new data
			// from the device.
			//
			status = WdfRequestForwardToIoQueue(Request, devContext->InterruptMsgQueue);

			if(!NT_SUCCESS(status)){
				DbgPrint("SIXCD: WdfRequestForwardToIoQueue failed with status: 0x%x\n", status);

				WdfRequestComplete(Request, status);
			}

			return;

		default:
			status = STATUS_NOT_SUPPORTED;
			break;
    }
*/
    WdfRequestComplete(Request, status);

    DbgPrint("SIXCD: <--- SixcdEvtInternalDeviceControl(status=0x%x)\n",status);
    return;
}

NTSTATUS
SixcdGetHidDescriptor(
    IN WDFDEVICE Device,
    IN WDFREQUEST Request
	)
{
    NTSTATUS            status = STATUS_SUCCESS;
    size_t              bytesToCopy = 0;
    WDFMEMORY           memory;

    UNREFERENCED_PARAMETER(Device);

    DbgPrint("SIXCD: ---> SixcdGetHidDescriptor\n");

    //
    // This IOCTL is METHOD_NEITHER so WdfRequestRetrieveOutputMemory
    // will correctly retrieve buffer from Irp->UserBuffer.
    // Remember that HIDCLASS provides the buffer in the Irp->UserBuffer
    // field irrespective of the ioctl buffer type. However, framework is very
    // strict about type checking. You cannot get Irp->UserBuffer by using
    // WdfRequestRetrieveOutputMemory if the ioctl is not a METHOD_NEITHER
    // internal ioctl.
    //
    status = WdfRequestRetrieveOutputMemory(Request, &memory);
    if (!NT_SUCCESS(status)) {
        DbgPrint("WdfRequestRetrieveOutputMemory failed 0x%x\n", status);
        return status;
    }

    //
    // Use hardcoded "HID Descriptor"
    //
    bytesToCopy = G_DefaultHidDescriptor.bLength;
    status = WdfMemoryCopyFromBuffer(memory,
                            0, // Offset
                            (PVOID) &G_DefaultHidDescriptor,
                            bytesToCopy);
    if (!NT_SUCCESS(status)) {
        DbgPrint("WdfMemoryCopyFromBuffer failed 0x%x\n", status);
        return status;
    }

    //
    // Report how many bytes were copied
    //
    WdfRequestSetInformation(Request, bytesToCopy);

    DbgPrint("SIXCD: <--- SixcdGetHidDescriptor\n");
    return status;
}

NTSTATUS
SixcdGetReportDescriptor(
    IN WDFDEVICE Device,
    IN WDFREQUEST Request
	)
{
    NTSTATUS            status = STATUS_SUCCESS;
    ULONG_PTR           bytesToCopy;
    WDFMEMORY           memory;

    UNREFERENCED_PARAMETER(Device);

    DbgPrint("SIXCD: ---> SixcdGetReportDescriptor\n");

    //
    // This IOCTL is METHOD_NEITHER so WdfRequestRetrieveOutputMemory
    // will correctly retrieve buffer from Irp->UserBuffer.
    // Remember that HIDCLASS provides the buffer in the Irp->UserBuffer
    // field irrespective of the ioctl buffer type. However, framework is very
    // strict about type checking. You cannot get Irp->UserBuffer by using
    // WdfRequestRetrieveOutputMemory if the ioctl is not a METHOD_NEITHER
    // internal ioctl.
    //
    status = WdfRequestRetrieveOutputMemory(Request, &memory);
    if (!NT_SUCCESS(status)) {
        DbgPrint("WdfRequestRetrieveOutputMemory failed 0x%x\n", status);
        return status;
    }

    //
    // Use hardcoded Report descriptor
    //
    bytesToCopy = G_DefaultHidDescriptor.DescriptorList[0].wReportLength;
    status = WdfMemoryCopyFromBuffer(memory,
                            0,
                            (PVOID) G_DefaultReportDescriptor,
                            bytesToCopy);
    if (!NT_SUCCESS(status)) {
        DbgPrint("SIXCD: WdfMemoryCopyFromBuffer failed 0x%x\n", status);
        return status;
    }

    //
    // Report how many bytes were copied
    //
    WdfRequestSetInformation(Request, bytesToCopy);

    DbgPrint("SIXCD: <--- SixcdGetReportDescriptor\n");
    return status;
}


NTSTATUS
SixcdGetDeviceAttributes(
    IN WDFREQUEST Request
	)
{
    NTSTATUS                 status = STATUS_SUCCESS;
    PHID_DEVICE_ATTRIBUTES   deviceAttributes = NULL;
    PUSB_DEVICE_DESCRIPTOR   usbDeviceDescriptor = NULL;
    PDEVICE_EXTENSION        deviceInfo = NULL;

    DbgPrint("SIXCD: --->SixcdGetDeviceAttributes\n");

    deviceInfo = GetDeviceContext(WdfIoQueueGetDevice(WdfRequestGetIoQueue(Request)));

    //
    // This IOCTL is METHOD_NEITHER so WdfRequestRetrieveOutputMemory
    // will correctly retrieve buffer from Irp->UserBuffer.
    // Remember that HIDCLASS provides the buffer in the Irp->UserBuffer
    // field irrespective of the ioctl buffer type. However, framework is very
    // strict about type checking. You cannot get Irp->UserBuffer by using
    // WdfRequestRetrieveOutputMemory if the ioctl is not a METHOD_NEITHER
    // internal ioctl.
    //
    status = WdfRequestRetrieveOutputBuffer(Request,
                                            sizeof (HID_DEVICE_ATTRIBUTES),
                                            &deviceAttributes,
                                            NULL);
    if (!NT_SUCCESS(status)) {
        DbgPrint("WdfRequestRetrieveOutputBuffer failed 0x%x\n", status);
        return status;
    }

    //
    // Retrieve USB device descriptor saved in device context
    //
    usbDeviceDescriptor = WdfMemoryGetBuffer(deviceInfo->DeviceDescriptor, NULL);

    deviceAttributes->Size = sizeof (HID_DEVICE_ATTRIBUTES);
    deviceAttributes->VendorID = usbDeviceDescriptor->idVendor;
    deviceAttributes->ProductID = usbDeviceDescriptor->idProduct;;
    deviceAttributes->VersionNumber = usbDeviceDescriptor->bcdDevice;

    //
    // Report how many bytes were copied
    //
    WdfRequestSetInformation(Request, sizeof (HID_DEVICE_ATTRIBUTES));

    DbgPrint("SIXCD: --->SixcdGetDeviceAttributes(status=0x%x)\n",status);
    return status;
}

NTSTATUS
SixcdConfigContReaderForInterruptEndPoint(
    PDEVICE_EXTENSION DeviceContext
    )
/*++

Routine Description:

    This routine configures a continuous reader on the
    interrupt endpoint. It's called from the PrepareHarware event.

Arguments:

    DeviceContext - Pointer to device context structure

Return Value:

    NT status value

--*/
{
    WDF_USB_CONTINUOUS_READER_CONFIG contReaderConfig;
    NTSTATUS status = STATUS_SUCCESS;

    PAGED_CODE ();

    DbgPrint("SIXCD: ---> SixcdConfigContReaderForInterruptEndPoint\n");

    WDF_USB_CONTINUOUS_READER_CONFIG_INIT(&contReaderConfig,
                                          SixcdEvtUsbInterruptPipeReadComplete,
                                          DeviceContext,    // Context
                                          sizeof(UCHAR));   // TransferLength
    //
    // Reader requests are not posted to the target automatically.
    // Driver must explictly call WdfIoTargetStart to kick start the
    // reader.  In this sample, it's done in D0Entry.
    // By defaut, framework queues two requests to the target
    // endpoint. Driver can configure up to 10 requests with CONFIG macro.
    //
    status = WdfUsbTargetPipeConfigContinuousReader(DeviceContext->InterruptPipe,
                                                    &contReaderConfig);

    if (!NT_SUCCESS(status)) {
        DbgPrint("SIXCD: SixcdConfigContReaderForInterruptEndPoint failed %x\n",
                    status);
        return status;
    }

    DbgPrint("SIXCD: <--- SixcdConfigContReaderForInterruptEndPoint(status=0x%x)\n", status);

    return status;
}

VOID
SixcdEvtUsbInterruptPipeReadComplete(
    WDFUSBPIPE  Pipe,
    WDFMEMORY   Buffer,
    size_t      NumBytesTransferred,
    WDFCONTEXT  Context
    )
/*++

Routine Description:

    This the completion routine of the continuous reader. This can
    called concurrently on multiprocessor system if there are
    more than one readers configured. So make sure to protect
    access to global resources.

Arguments:

    Pipe - Handle to WDF USB pipe object

    Buffer - This buffer is freed when this call returns.
             If the driver wants to delay processing of the buffer, it
             can take an additional referrence.

    NumBytesTransferred - number of bytes of data that are in the read buffer.

    Context - Provided in the WDF_USB_CONTINUOUS_READER_CONFIG_INIT macro

Return Value:

    NT status value

--*/
{
    WDFDEVICE          device;
    PDEVICE_EXTENSION  devContext = Context;
    UCHAR              toggledSwitch = 0;
    PUCHAR             switchState = NULL;
    UCHAR              currentSwitchState = 0;
    UCHAR              previousSwitchState = 0;

    UNREFERENCED_PARAMETER(NumBytesTransferred);
    UNREFERENCED_PARAMETER(Pipe);

	DbgPrint("SIXCD: ---> SixcdEvtUsbInterruptPipeReadComplete Enter\n");

    //
    // Interrupt endpoints sends switch state when first started
    // or when resuming from suspend. We need to ignore that data since
    // user did not change the switch state.
    //
    if (devContext->IsPowerUpSwitchState) {
        devContext->IsPowerUpSwitchState = FALSE;
        
		DbgPrint("SIXCD: Dropping interrupt message since received during powerup/resume\n");
        return;
    }

    device = WdfObjectContextGetObject(devContext);
    switchState = WdfMemoryGetBuffer(Buffer, NULL);

    currentSwitchState = *switchState;
    previousSwitchState = devContext->CurrentSwitchState;

    //
    // we want to know which switch got toggled from 0 to 1
    // Since the device returns the state of all the swicthes and not just the
    // one that got toggled, we need to store previous state and xor
    // it with current state to know whcih one swicth got toggled.
    // Further, the toggle is considered "on" only when it changes from 0 to 1
    // (and not when it changes from 1 to 0).
    //
    toggledSwitch = (previousSwitchState ^ currentSwitchState) & currentSwitchState;

	DbgPrint("SIXCD: SixcdEvtUsbInterruptPipeReadComplete SwitchState %x, " 
                "prevSwitch:0x%x, x0R:0x%x\n",
                currentSwitchState, 
                previousSwitchState, 
                toggledSwitch
                );

    //
    // Store switch state in device context
    //
    devContext->CurrentSwitchState = *switchState;
    //if (toggledSwitch != 0) {
        devContext->LatestToggledSwitch = toggledSwitch;
    //}

    //
    // Complete pending Read requests if there is at least one switch toggled
    // to on position.
    //
    if (toggledSwitch != 0) {
        BOOLEAN inTimerQueue;

        //
        // Debounce the switchpack. A simple logic is used for debouncing.
        // A timer is started for 10 ms everytime there is a switch toggled on. 
        // If within 10 ms same or another switch gets toggled, the timer gets
        // reset for another 10 ms. The HID read request is completed in timer
        // function if there is still a switch in toggled-on state. Note that
        // debouncing happens at the whole switch pack level (not individual
        // switches) which means if two different switches are toggled-on within
        // 10 ms only one of them (later one in this case) will get accepted and
        // sent to hidclass driver
        //
        inTimerQueue = WdfTimerStart(
            devContext->DebounceTimer,
            WDF_REL_TIMEOUT_IN_MS(SWICTHPACK_DEBOUNCE_TIME_IN_MS)
            );
        
        DbgPrint("Debounce Timer started with timeout of %d ms"
            " (TimerReturnValue:%d)\n",
            SWICTHPACK_DEBOUNCE_TIME_IN_MS, inTimerQueue);
    }

	DbgPrint("SIXCD: <--- SixcdEvtUsbInterruptPipeReadComplete Exit\n");
}

NTSTATUS
SixcdEvtDeviceD0Entry(
    IN  WDFDEVICE Device,
    IN  WDF_POWER_DEVICE_STATE PreviousState
	)
{
    PDEVICE_EXTENSION   devContext = NULL;
    NTSTATUS            status = STATUS_SUCCESS;
    UCHAR               switchState = 0;

    devContext = GetDeviceContext(Device);

    DbgPrint("SIXCD: --->SixcdEvtDeviceD0Entry\n");

    DbgPrint("SIXCD: <--- SixcdEvtDeviceD0Entry(status=0x%x)\n", status);

    return status;
}

NTSTATUS
SixcdEvtDeviceD0Exit(
    IN  WDFDEVICE Device,
    IN  WDF_POWER_DEVICE_STATE TargetState
	)
{
    PDEVICE_EXTENSION         devContext;

    PAGED_CODE();

    DbgPrint("SIXCD: ---> SixcdEvtDeviceD0Exit Enter\n");

    DbgPrint("SIXCD: <--- SixcdEvtDeviceD0Exit\n");

    return STATUS_SUCCESS;
}

PCHAR
GET_NTSTATUS_TEXT(
	NTSTATUS status
	)
{
	switch (status)
	{
	case STATUS_INVALID_PARAMETER:
		return "STATUS_INVALID_PARAMETER";
	case STATUS_INSUFFICIENT_RESOURCES:
		return "STATUS_INSUFFICIENT_RESOURCES";
	case STATUS_UNSUCCESSFUL:
		return "STATUS_UNSUCCESSFUL";
	default:
		return "UNKNOWN STATUS";
	}
}

PCHAR
GET_IOCTL_NAME(
	ULONG IoControlCode
	)
{
    switch (IoControlCode)
    {
	case IOCTL_HID_GET_DRIVER_CONFIG:
		return "IOCTL_HID_GET_DRIVER_CONFIG";
	case IOCTL_HID_SET_DRIVER_CONFIG:
		return "IOCTL_HID_SET_DRIVER_CONFIG";
	case IOCTL_HID_GET_POLL_FREQUENCY_MSEC:
		return "IOCTL_HID_GET_POLL_FREQUENCY_MSEC";
	case IOCTL_HID_SET_POLL_FREQUENCY_MSEC:
		return "IOCTL_HID_SET_POLL_FREQUENCY_MSEC";
	case IOCTL_GET_NUM_DEVICE_INPUT_BUFFERS:
		return "IOCTL_GET_NUM_DEVICE_INPUT_BUFFERS";
	case IOCTL_SET_NUM_DEVICE_INPUT_BUFFERS:
		return "IOCTL_SET_NUM_DEVICE_INPUT_BUFFERS";
	case IOCTL_HID_GET_COLLECTION_INFORMATION:
		return "IOCTL_HID_GET_COLLECTION_INFORMATION";

	case IOCTL_HID_GET_COLLECTION_DESCRIPTOR:
		return "IOCTL_HID_GET_COLLECTION_DESCRIPTOR";
	case IOCTL_HID_FLUSH_QUEUE:
		return "IOCTL_HID_FLUSH_QUEUE";

	case IOCTL_HID_SET_FEATURE:
		return "IOCTL_HID_SET_FEATURE";
	#if (NTDDI_VERSION >= NTDDI_WINXP) 
	case IOCTL_HID_SET_OUTPUT_REPORT:
		return "IOCTL_HID_SET_OUTPUT_REPORT";
	#endif

	case IOCTL_HID_GET_FEATURE:
		return "IOCTL_HID_GET_FEATURE";
	case IOCTL_GET_PHYSICAL_DESCRIPTOR:
		return "IOCTL_GET_PHYSICAL_DESCRIPTOR";
	case IOCTL_HID_GET_HARDWARE_ID:
		return "IOCTL_HID_GET_HARDWARE_ID";
	#if (NTDDI_VERSION >= NTDDI_WINXP) 
	case IOCTL_HID_GET_INPUT_REPORT:
		return "IOCTL_HID_GET_INPUT_REPORT";
	#endif

	/*
	 *  No more IOCTL_HID_GET_FRIENDLY_NAME - use one of the following:
	 */
	case IOCTL_HID_GET_MANUFACTURER_STRING:
		return "IOCTL_HID_GET_MANUFACTURER_STRING";
	case IOCTL_HID_GET_PRODUCT_STRING:
		return "IOCTL_HID_GET_PRODUCT_STRING";
	case IOCTL_HID_GET_SERIALNUMBER_STRING:
		return "IOCTL_HID_GET_SERIALNUMBER_STRING";

	case IOCTL_HID_GET_INDEXED_STRING:
		return "IOCTL_HID_GET_INDEXED_STRING";
	#if (NTDDI_VERSION >= NTDDI_WINXP) 
	case IOCTL_HID_GET_MS_GENRE_DESCRIPTOR:
		return "IOCTL_HID_GET_MS_GENRE_DESCRIPTOR";

	case IOCTL_HID_ENABLE_SECURE_READ:
		return "IOCTL_HID_ENABLE_SECURE_READ";      
	case IOCTL_HID_DISABLE_SECURE_READ:
		return "IOCTL_HID_DISABLE_SECURE_READ";
	#endif

    default:
        return "Unknown IOCTL";
    }
}

PCHAR
GET_INTERNAL_IOCTL_NAME(
	ULONG IoControlCode
	)
{
    switch (IoControlCode)
    {
	
    case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
        return "IOCTL_HID_GET_DEVICE_DESCRIPTOR";
    case IOCTL_HID_GET_REPORT_DESCRIPTOR:
        return "IOCTL_HID_GET_REPORT_DESCRIPTOR";
    case IOCTL_HID_READ_REPORT:
        return "IOCTL_HID_READ_REPORT";
    case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
        return "IOCTL_HID_GET_DEVICE_ATTRIBUTES";
    case IOCTL_HID_WRITE_REPORT:
        return "IOCTL_HID_WRITE_REPORT";

    case IOCTL_HID_SET_FEATURE:
        return "IOCTL_HID_SET_FEATURE";
    case IOCTL_HID_GET_FEATURE:
        return "IOCTL_HID_GET_FEATURE";

    case IOCTL_HID_GET_STRING:
        return "IOCTL_HID_GET_STRING";
    case IOCTL_HID_ACTIVATE_DEVICE:
        return "IOCTL_HID_ACTIVATE_DEVICE";
    case IOCTL_HID_DEACTIVATE_DEVICE:
        return "IOCTL_HID_DEACTIVATE_DEVICE";
    case IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST:
        return "IOCTL_HID_SEND_IDLE_NOTIFICATION_REQUEST";
    default:
        return "Unknown IOCTL";
    }
}
