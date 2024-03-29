/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    hidusbfx2.h

Abstract:

    common header file

Author:


Environment:

    kernel mode only

Notes:


Revision History:


--*/
#ifndef _HIDUSBFX2_H_

#define _HIDUSBFX2_H_

#pragma warning(disable:4200)  // suppress nameless struct/union warning
#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <initguid.h>
#include <wdm.h>
#include "usbdi.h"
#include "usbdlib.h"

#pragma warning(default:4200)
#pragma warning(default:4201)
#pragma warning(default:4214)
#include <wdf.h>
#include "wdfusb.h"

#pragma warning(disable:4201)  // suppress nameless struct/union warning
#pragma warning(disable:4214)  // suppress bit field types other than int warning
#include <hidport.h>

#define NTSTRSAFE_LIB
#include <ntstrsafe.h>

#include "trace.h"

#define _DRIVER_NAME_                 "HIDUSBFX2: "
#define DO_NOTHING
#define INPUT_REPORT_BYTES            1

#define CONSUMER_CONTROL_REPORT_ID    1
#define SYSTEM_CONTROL_REPORT_ID      2
#define FEATURE_COLLECTION_REPORT_ID  3
#define TEST_COLLECTION_REPORT_ID     4

#define CONSUMER_CONTROL_BUTTONS_BIT_MASK   ((UCHAR)0x7f)   // (first 7 bits)
#define SYSTEM_CONTROL_BUTTONS_BIT_MASK     ((UCHAR)0x80)

#define INTERRUPT_ENDPOINT_INDEX     (0)

#define SWICTHPACK_DEBOUNCE_TIME_IN_MS   10

typedef UCHAR HID_REPORT_DESCRIPTOR, *PHID_REPORT_DESCRIPTOR;

//
// Define the vendor commands supported by device
//
#define HIDFX2_READ_SWITCH_STATE          0xD6
#define HIDFX2_READ_7SEGMENT_DISPLAY      0xD4
#define HIDFX2_READ_BARGRAPH_DISPLAY      0xD7
#define HIDFX2_SET_BARGRAPH_DISPLAY       0xD8
#define HIDFX2_IS_HIGH_SPEED              0xD9
#define HIDFX2_REENUMERATE                0xDA
#define HIDFX2_SET_7SEGMENT_DISPLAY       0xDB

//
// Bit masks for 7 segment display to be used with vendor command
//
#define SEGMENT_BIT_1   (UCHAR)0x1
#define SEGMENT_BIT_2   (UCHAR)0x2
#define SEGMENT_BIT_3   (UCHAR)0x4
#define SEGMENT_BIT_4   (UCHAR)0x8
#define SEGMENT_BIT_5   (UCHAR)0x10
#define SEGMENT_BIT_6   (UCHAR)0x20
#define SEGMENT_BIT_7   (UCHAR)0x40
#define SEGMENT_BIT_8   (UCHAR)0x80

#define SEGMENT_DISPLAY_1 \
        SEGMENT_BIT_2     \
     |  SEGMENT_BIT_3

#define SEGMENT_DISPLAY_2 \
        SEGMENT_BIT_1     \
     |  SEGMENT_BIT_2     \
     |  SEGMENT_BIT_6     \
     |  SEGMENT_BIT_5     \
     |  SEGMENT_BIT_8

#define SEGMENT_DISPLAY_3 \
        SEGMENT_BIT_1     \
     |  SEGMENT_BIT_2     \
     |  SEGMENT_BIT_6     \
     |  SEGMENT_BIT_3     \
     |  SEGMENT_BIT_8

#define SEGMENT_DISPLAY_4 \
        SEGMENT_BIT_2     \
     |  SEGMENT_BIT_3     \
     |  SEGMENT_BIT_6     \
     |  SEGMENT_BIT_7

#define SEGMENT_DISPLAY_5 \
        SEGMENT_BIT_1     \
     |  SEGMENT_BIT_3     \
     |  SEGMENT_BIT_6     \
     |  SEGMENT_BIT_7     \
     |  SEGMENT_BIT_8

#define SEGMENT_DISPLAY_6 \
        SEGMENT_BIT_1     \
     |  SEGMENT_BIT_3     \
     |  SEGMENT_BIT_5     \
     |  SEGMENT_BIT_6     \
     |  SEGMENT_BIT_7     \
     |  SEGMENT_BIT_8

#define SEGMENT_DISPLAY_7 \
        SEGMENT_BIT_1     \
     |  SEGMENT_BIT_2     \
     |  SEGMENT_BIT_3

#define SEGMENT_DISPLAY_8 \
        SEGMENT_BIT_1     \
     |  SEGMENT_BIT_2     \
     |  SEGMENT_BIT_3     \
     |  SEGMENT_BIT_5     \
     |  SEGMENT_BIT_6     \
     |  SEGMENT_BIT_7     \
     |  SEGMENT_BIT_8

#define BARGRAPH_LED_1_ON       0x1
#define BARGRAPH_LED_2_ON       0x2
#define BARGRAPH_LED_3_ON       0x4
#define BARGRAPH_LED_4_ON       0x8
#define BARGRAPH_LED_5_ON       0x10
#define BARGRAPH_LED_6_ON       0x20
#define BARGRAPH_LED_7_ON       0x40
#define BARGRAPH_LED_8_ON       0x80
#define BARGRAPH_LED_ALL_ON     0xff
#define BARGRAPH_LED_ALL_OFF    0x00

//
// This is the default report descriptor for the Hid device provided
// by the mini driver in response to IOCTL_HID_GET_REPORT_DESCRIPTOR.
//
// AC: Aplication Control
// AL: Application Launch
#ifdef USE_HARDCODED_HID_DESCRIPTORS

CONST  HID_REPORT_DESCRIPTOR           G_DefaultReportDescriptor[] = {
    // Consumer control collection
    0x05,0x0C,                      // USAGE_PAGE (Consumer Page)
    0x09,0x01,                      // USAGE (Consumer Control Usage 0x01)
    0xA1,0x01,                      // COLLECTION (Application)
    0x85,CONSUMER_CONTROL_REPORT_ID,//   REPORT_ID
    0x0A, 0x23, 0x02,               //   USAGE (Usage Browser)
    0x0A, 0x24, 0x02,               //   USAGE (Usage AC Back)
    0x0A, 0x25, 0x02,               //   USAGE (Usage AC Forward)
    0x0A, 0x27, 0x02,               //   USAGE (Usage AC Refresh)
    0x0A, 0x2A, 0x02,               //   USAGE (Usage AC BookMarks)
    0x0A, 0x8A, 0x01,               //   USAGE (Usage AL Mail)
    0x0A, 0x92, 0x01,               //   USAGE (Usage AL Calculator )
    0x15, 0x00,                     //   LOGICAL_MINIMUM(0)
    0x25, 0x01,                     //   LOGICAL_MAXIMUM(1)
    0x75, 0x01,                     //   REPORT_SIZE
    0x95, 0x07,                     //   REPORT_COUNT
    0x81, 0x02,                     //   INPUT (Data, Variable,Abs)
    0x75, 0x01,                     //   REPORT_SIZE
    0x95, 0x01,                     //   REPORT_COUNT
    0x81, 0x07,                     //   INPUT (const)
    0xC0,                           // END_COLLECTION
    // system control collection
    0x05, 0x01,                     // Usage Page (Generic Desktop)
    0x09, 0x80,                     // Usage (System Control)
    0xA1, 0x01,                     // Collection (Application)
    0x85, SYSTEM_CONTROL_REPORT_ID, //   Report ID
    0x95, 0x07,                     //   Report Count
    0x81, 0x07,                     //   Input (Constant)   PADDING
    0x09, 0x82,                     //   Usage (System Sleep)
    0x95, 0x01,                     //   Report Count (1)
    0x81, 0x06,                     //   Input (Data, Variable, Relative, Preferred)
    0xC0,                           // End Collection
    // Feature collection
    0x06,0x00, 0xFF,                // USAGE_PAGE (Vender Defined Usage Page)
    0x09,0x01,                      // USAGE (Vendor Usage 0x01)
    0xA1,0x01,                      // COLLECTION (Application)
    0x85,FEATURE_COLLECTION_REPORT_ID,// Report ID
    0x19,0x01,                      //   USAGE MINIMUM
    0x29,0x20,                      //   USAGE MAXIMUM
    0x15,0x00,                      //   LOGICAL_MINIMUM(1)
    0x26,0xff, 0x00,                //   LOGICAL_MAXIMUM(255)
    0x75,0x08,                      //   REPORT_SIZE
    0x95,0x01,                      //   REPORT_COUNT
    0xB1,0x00,                      //   Feature (Data,Ary,Abs)
    0xC0                            // END_COLLECTION
};

//
// This is the default HID descriptor returned by the mini driver
// in response to IOCTL_HID_GET_DEVICE_DESCRIPTOR. The size
// of report descriptor is currently the size of G_DefaultReportDescriptor.
//
CONST HID_DESCRIPTOR G_DefaultHidDescriptor = {
    0x09,   // length of HID descriptor
    0x21,   // descriptor type == HID  0x21
    0x0100, // hid spec release
    0x00,   // country code == Not Specified
    0x01,   // number of HID class descriptors
    { 0x22,   // descriptor type
    sizeof(G_DefaultReportDescriptor) }  // total length of report descriptor
};


CONST HID_COLLECTION_INFORMATION G_CollectionInformation = {
	sizeof(G_DefaultReportDescriptor),//ULONG  DescriptorSize;
	FALSE,//BOOLEAN  Polled;
	0,//UCHAR  Reserved1[ 1 ];
	0x054C,//USHORT  VendorID;
	0x0268,//USHORT  ProductID;
	0x0100//USHORT  VersionNumber;
};

UCHAR UsageToCommandDataMappingTable[] =
{
//  Vendor Command        Usage
    0,                    // N/A
    SEGMENT_DISPLAY_1,    // 1
    SEGMENT_DISPLAY_2,    // 2
    SEGMENT_DISPLAY_3,    // 3
    SEGMENT_DISPLAY_4,    // 4
    SEGMENT_DISPLAY_5,    // 5
    SEGMENT_DISPLAY_6,    // 6
    SEGMENT_DISPLAY_7,    // 7
    SEGMENT_DISPLAY_8,    // 8
    BARGRAPH_LED_1_ON,    // 9
    BARGRAPH_LED_2_ON,    // 10
    BARGRAPH_LED_3_ON,    // 11
    BARGRAPH_LED_4_ON,    // 12
    BARGRAPH_LED_5_ON,    // 13
    BARGRAPH_LED_6_ON,    // 14
    BARGRAPH_LED_7_ON,    // 15
    BARGRAPH_LED_8_ON,    // 16
    BARGRAPH_LED_ALL_ON,  // 17
    BARGRAPH_LED_ALL_OFF, // 18
};
#endif

#include <pshpack1.h>
typedef struct _HIDFX2_INPUT_REPORT {

    //
    //Report ID for the collection
    //
    BYTE ReportId;

    union {
        struct {
            //
            // Individual switches starting from the
            //  right of the set of switches
            //
            BYTE SwitchBrowser : 1;
            BYTE SwitchBack : 1;
            BYTE SwitchForward : 1;
            BYTE SwitchRefresh : 1;
            BYTE SwitchBookMarks : 1;
            BYTE SwitchMail : 1;
            BYTE SwitchCalculator : 1;
            BYTE Padding : 1;

        } ConsumerControlReport;

        struct {
            //
            // Individual switches starting from the
            //  right of the set of switches
            //
            BYTE Padding : 7;
            BYTE SwitchPowerSleep : 1;

        } SystemControlReport;

        //
        // The state of all the switches as a single
        // UCHAR
        //

        BYTE SwitchStateAsByte;
    };
}HIDFX2_INPUT_REPORT, *PHIDFX2_INPUT_REPORT;


typedef struct _HIDFX2_FEATURE_REPORT {
    //
    //Report ID for the collection
    //
    BYTE ReportId;

    //
    //one-byte feature data from 7-segment display or bar graph
    //
    BYTE FeatureData;

}HIDFX2_FEATURE_REPORT, *PHIDFX2_FEATURE_REPORT;
#include <poppack.h>


typedef struct _DEVICE_EXTENSION{

    //
    //WDF handles for USB Target
    //
    WDFUSBDEVICE      UsbDevice;
    WDFUSBINTERFACE   UsbInterface;
    WDFUSBPIPE        InterruptPipe;

    //
    //Device descriptor for the USB device
    //
    WDFMEMORY DeviceDescriptor;

    //
    // Switch state.
    //
    UCHAR    CurrentSwitchState;

    //
    // This variable stores state for the swicth that got toggled most recently
    // (the device returns the state of all the switches and not just the
    // one that got toggled).
    //
    UCHAR    LatestToggledSwitch;

    //
    // Interrupt endpoints sends switch state when first started
    // or when resuming from suspend. We need to ignore that data.
    //
    BOOLEAN  IsPowerUpSwitchState;

    //
    // WDF Queue for read IOCTLs from hidclass that get satisfied from
    // USB interrupt endpoint
    //
    WDFQUEUE   InterruptMsgQueue;

    //
    // Handle debouncing of switchpack
    //
    WDFTIMER DebounceTimer;

} DEVICE_EXTENSION, * PDEVICE_EXTENSION;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_EXTENSION, GetDeviceContext)

//
// driver routine declarations
//
// This type of function declaration is for Prefast for drivers.
// Because this declaration specifies the function type, PREfast for Drivers
// does not need to infer the type or to report an inference. The declaration
// also prevents PREfast for Drivers from misinterpreting the function type
// and applying inappropriate rules to the function. For example, PREfast for
// Drivers would not apply rules for completion routines to functions of type
// DRIVER_CANCEL. The preferred way to avoid Warning 28101 is to declare the
// function type explicitly. In the following example, the DriverEntry function
// is declared to be of type DRIVER_INITIALIZE.
//
DRIVER_INITIALIZE DriverEntry;

NTSTATUS
SixcdEvtDeviceAdd(
    IN WDFDRIVER       Driver,
    IN PWDFDEVICE_INIT DeviceInit
    );

VOID
SixcdEvtIoDefault(
    IN WDFQUEUE     Queue,
    IN WDFREQUEST   Request
    );

VOID
SixcdEvtInternalDeviceControl(
    IN WDFQUEUE     Queue,
    IN WDFREQUEST   Request,
    IN size_t       OutputBufferLength,
    IN size_t       InputBufferLength,
    IN ULONG        IoControlCode
    );

NTSTATUS
SixcdConfigContReaderForInterruptEndPoint(
    PDEVICE_EXTENSION DeviceContext
    );

NTSTATUS
SixcdGetHidDescriptor(
    IN WDFDEVICE Device,
    IN WDFREQUEST Request
    );

NTSTATUS
SixcdGetReportDescriptor(
    IN WDFDEVICE Device,
    IN WDFREQUEST Request
    );

VOID
SixcdEvtUsbInterruptPipeReadComplete(
    WDFUSBPIPE  Pipe,
    WDFMEMORY   Buffer,
    size_t      NumBytesTransferred,
    WDFCONTEXT  Context
    );

NTSTATUS
SixcdGetDeviceAttributes(
    IN WDFREQUEST Request
    );

NTSTATUS
SixcdEvtDevicePrepareHardware(
    IN WDFDEVICE    Device,
    IN WDFCMRESLIST ResourceList,
    IN WDFCMRESLIST ResourceListTranslated
	);

NTSTATUS
SixcdEvtDeviceD0Entry(
    IN  WDFDEVICE Device,
    IN  WDF_POWER_DEVICE_STATE PreviousState
    );

NTSTATUS
SixcdEvtDeviceD0Exit(
    IN  WDFDEVICE Device,
    IN  WDF_POWER_DEVICE_STATE TargetState
    );

PCHAR
GET_NTSTATUS_TEXT(
	NTSTATUS status
	);

PCHAR
GET_IOCTL_NAME(
	ULONG IoControlCode
	);

PCHAR
GET_INTERNAL_IOCTL_NAME(
	ULONG IoControlCode
	);
#endif   //_HIDUSBFX2_H_