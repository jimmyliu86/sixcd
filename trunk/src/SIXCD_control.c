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
#include "SIXCD_report.h"

#pragma LOCKEDCODE

NTSTATUS SIXCDDispatchIntDevice(IN PDEVICE_OBJECT pFdo, IN PIRP pIrp)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
    NTSTATUS Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION pDevExt = GET_MINIDRIVER_DEVICE_EXTENSION(pFdo);

		switch (stack->Parameters.DeviceIoControl.IoControlCode)
		{
		case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
			{
				PHID_DEVICE_ATTRIBUTES pHidAttributes;
				pHidAttributes = (PHID_DEVICE_ATTRIBUTES) pIrp->UserBuffer;

				KdPrint(("SIXCDDispatchIntDevice - sending device attributes"));
				if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(HID_DEVICE_ATTRIBUTES))
				{
					Status = STATUS_BUFFER_TOO_SMALL;
					KdPrint(("Buffer for Device Attributes is too small"));
				}
				else
				{
					RtlZeroMemory(pHidAttributes, sizeof(HID_DEVICE_ATTRIBUTES));

					pHidAttributes->Size = sizeof(HID_DEVICE_ATTRIBUTES);

//TODO: Replace this with something that reports VendorID & ProductID correctly
//					pHidAttributes->VendorID = 0x0738;//pDevExt->dd.idVendor;
//					pHidAttributes->ProductID = 0x4516;//pDevExt->dd.idProduct;
//Temporary Fix: Report the PS3 vendor/product ID
					pHidAttributes->VendorID = 0x054C;//pDevExt->dd.idVendor;
					pHidAttributes->ProductID = 0x0268;//pDevExt->dd.idProduct;
					pHidAttributes->VersionNumber = 0x0001;//pDevExt->dd.bcdDevice;

					pIrp->IoStatus.Information = sizeof(HID_DEVICE_ATTRIBUTES);

					KdPrint(("SIXCDDispatchIntDevice - Sent Device Attributes"));
				}
				break;
			}
		case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
			{
				PHID_DESCRIPTOR pHidDescriptor;
				pHidDescriptor = (PHID_DESCRIPTOR) pIrp->UserBuffer;

				KdPrint(("SIXCDDispatchIntDevice - sending device descriptor"));
				if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(HID_DESCRIPTOR))
				{
					Status = STATUS_BUFFER_TOO_SMALL;
					KdPrint(("Buffer for Device Descriptor is too small"));
				}
				else
				{
					RtlZeroMemory(pHidDescriptor, sizeof(HID_DESCRIPTOR));

					pHidDescriptor->bLength = sizeof(HID_DESCRIPTOR);
					pHidDescriptor->bDescriptorType = HID_HID_DESCRIPTOR_TYPE;
					pHidDescriptor->bcdHID = HID_REVISION;
					pHidDescriptor->bCountry = 0;
					pHidDescriptor->bNumDescriptors = 1;
					pHidDescriptor->DescriptorList[0].bReportType = HID_REPORT_DESCRIPTOR_TYPE;
					pHidDescriptor->DescriptorList[0].wReportLength = sizeof(ReportDescriptor);

					pIrp->IoStatus.Information = sizeof(HID_DESCRIPTOR);
					KdPrint(("SIXCDDispatchIntDevice - Sent Device Descriptor"));
				}
				break;
			}
		case IOCTL_HID_GET_REPORT_DESCRIPTOR:
			{
				KdPrint(("SIXCDDispatchIntDevice - sending report descriptor"));
				if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(ReportDescriptor))
				{
					Status = STATUS_BUFFER_TOO_SMALL;
					KdPrint(("Buffer for Report Descriptor is too small"));
				}
				else
				{
					RtlCopyMemory(pIrp->UserBuffer, ReportDescriptor, sizeof(ReportDescriptor));

					pIrp->IoStatus.Information = sizeof(ReportDescriptor);

					KdPrint(("SIXCDDispatchIntDevice - Sent Report Descriptor"));
				}
				break;
			}
		case IOCTL_HID_READ_REPORT:
			{
				KIRQL oldirql;
				LARGE_INTEGER timeout;

				KdPrint(("SIXCDDispatchIntDevice - IOCTL_HID_READ_REPORT entry"));

				Status = SIXCDReadData(pDevExt, pIrp);

				//Set a timer to keep reading data for another 5 seconds
				//Fixes problems with button configuration in games
				timeout.QuadPart = -50000000;
				KeSetTimer(&pDevExt->timer, timeout, &pDevExt->timeDPC);
				pDevExt->timerEnabled = TRUE;

				Status = StartInterruptUrb(pDevExt);

				KdPrint(("SIXCDDispatchIntDevice - IOCTL_HID_READ_REPORT exit"));
				return Status;
			}
		case IOCTL_HID_WRITE_REPORT:
			{
				PHID_XFER_PACKET temp = (PHID_XFER_PACKET)pIrp->UserBuffer;

				KdPrint(("SIXCDDispatchIntDevice - IOCTL_HID_WRITE_REPORT"));

				if (temp->reportBuffer && temp->reportBufferLen)
				{
					int iTemp;
					pDevExt->intoutdata[0] = 0x00;
					pDevExt->intoutdata[1] = 0x06;
					pDevExt->intoutdata[2] = 0x00;
					iTemp = temp->reportBuffer[1];
					KdPrint(("iTemp = %d", iTemp));
					iTemp = iTemp * pDevExt->LAFactor/100;
					KdPrint(("iTemp = %d", iTemp));
					pDevExt->intoutdata[3] = iTemp;
					pDevExt->intoutdata[4] = 0x00;
					iTemp = temp->reportBuffer[2];
					KdPrint(("iTemp = %d", iTemp));
					iTemp = iTemp * pDevExt->RAFactor/100;
					KdPrint(("iTemp = %d", iTemp));
					pDevExt->intoutdata[5] = iTemp;

					Status = SendInterruptUrb(pDevExt);

					if(temp->reportBuffer[0] == 1)
					{
						SIXCDReadButtonConfig(pFdo);
					}
				}
				else
				{
					KdPrint(("HIDMINI.SYS: Write report ID 0x%x w/o buffer\n",
							  (unsigned)temp->reportId ));
				}

				Status = STATUS_SUCCESS;

				pIrp->IoStatus.Information = 0;

				break;
			}
		default:
			{
				KdPrint(("SIXCDDispatchIntDevice - Irp not implemented"));
				Status = STATUS_NOT_SUPPORTED;
				break;
			}
		}

		pIrp->IoStatus.Status = Status;

	if(Status != STATUS_PENDING)
	{
		KdPrint(("SIXCDDispatchIntDevice - irp status not pending"));
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		Status = STATUS_SUCCESS;
	}
	else
	{
		KdPrint(("SIXCDDispatchIntDevice - mark the irp as pending"));
		IoMarkIrpPending(pIrp);
	}

	KdPrint(("SIXCDDispatchIntDevice - returning"));
    return Status;
}

#pragma PAGEDCODE

NTSTATUS SIXCDDispatchDevice(IN PDEVICE_OBJECT pFdo, IN PIRP pIrp)
{
	PDEVICE_EXTENSION pDevExt = GET_MINIDRIVER_DEVICE_EXTENSION(pFdo);
	NTSTATUS status;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);

	IoSkipCurrentIrpStackLocation(pIrp);
	status = IoCallDriver(pDevExt->pLowerPdo, pIrp);
	return status;
}

#pragma PAGEDCODE

NTSTATUS SIXCDDispatchSystem(IN PDEVICE_OBJECT pFdo, IN PIRP pIrp)
{
	PDEVICE_EXTENSION pDevExt = GET_MINIDRIVER_DEVICE_EXTENSION(pFdo);
	NTSTATUS status;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);

	IoSkipCurrentIrpStackLocation(pIrp);
	status = IoCallDriver(pDevExt->pLowerPdo, pIrp);
	return status;
}

#pragma LOCKEDCODE

double sqrt2(double x)
{							// sqrt
	double result;

	_asm
		{
		fld x
		fsqrt
		fstp result
		}

	return result;
}

int ftoi(float f)
{
   int rv;
	_asm {
		fld f
		fistp rv
		//fwait /* not sure if this is necessary on newer processors... */
		}
	return rv ;
}

void CalcAxes(short *xdatain, short *ydatain, int iDeadZone, PDEVICE_EXTENSION pDevExt)
{
	short xdata = *xdatain;
	short ydata = *ydatain;

	if(!pDevExt->bFPCalc)
	{
		//Floating Point disabled
		//Use simple deadzone calculations

		if(xdata >= 0)
		{
			if(xdata < iDeadZone)
			{
				xdata = 0;
			}
			else
			{
				xdata = 32767 * (xdata - iDeadZone)/(32767 - iDeadZone);
			}
		}
		else
		{
			if(xdata > -(iDeadZone))
			{
				xdata = 0;
			}
			else
			{
				xdata = -32767 * (xdata + iDeadZone)/(-32768 + iDeadZone);
			}
		}

		if(ydata >= 0)
		{
			if(ydata < iDeadZone)
			{
				ydata = 0;
			}
			else
			{
				ydata = 32767 * (ydata - iDeadZone)/(32767 - iDeadZone);
			}
		}
		else
		{
			if(ydata > -(iDeadZone))
			{
				ydata = 0;
			}
			else
			{
				ydata = -32767 * (ydata + iDeadZone)/(-32768 + iDeadZone);
			}
		}
	}
	else
	{
		//Floating Point enabled

		NTSTATUS Status;
		KFLOATING_SAVE saveData;
		double radius;
		double xdata2;
		double ydata2;

		Status = KeSaveFloatingPointState(&saveData);

		if(NT_SUCCESS(Status))
		{
			radius = (double)(xdata * xdata) + (double)(ydata * ydata);
			radius = sqrt2(radius);
			if(radius > 32767)
				radius = 32767;

			if(radius >= iDeadZone)
			{
				radius = 32767 * (radius - iDeadZone)/(32767 - iDeadZone);
				xdata2 = xdata/(radius * 0.7071);
				ydata2 = ydata/(radius * 0.7071);
				if(xdata2 > 1)
					xdata2 = 1;
				if(xdata2 < -1)
					xdata2 = -1;
				if(ydata2 > 1)
					ydata2 = 1;
				if(ydata2 < -1)
					ydata2 = -1;
				xdata2 = xdata2 * radius;
				ydata2 = ydata2 * radius;
				xdata = ftoi(xdata2);
				ydata = ftoi(ydata2);
			}
			else
			{
				xdata = 0;
				ydata = 0;
			}

			KeRestoreFloatingPointState(&saveData);
		}
	}

	*xdatain = xdata;
	*ydatain = ydata;
}

NTSTATUS SIXCDReadData(PDEVICE_EXTENSION pDevExt, PIRP pIrp)
{
	unsigned char xbdata[11]; //Holds output data
	short btndata;
	short xdata;
	short ydata;
	int iTBuffer; //Temporary buffer for analog button data
	int iTest;
	NTSTATUS Status;

	RtlZeroMemory(&btndata, sizeof(btndata));
	RtlZeroMemory(&xbdata, sizeof(xbdata));
	RtlZeroMemory(&xdata, sizeof(xdata));
	RtlZeroMemory(&ydata, sizeof(ydata));

	if(pDevExt->btnset)
	{

		//Button A
		iTBuffer = pDevExt->intdata[4] & 255; //Pass the value of button A to iTBuffer
		//If value of button is higher than the threshold value then button is active
		if (iTBuffer >= pDevExt->BThreshold)
		{
			btndata |= pDevExt->buttons[0];
		}

		//Button B
		iTBuffer = pDevExt->intdata[5] & 255;
		if (iTBuffer >= pDevExt->BThreshold)
		{
			btndata |= pDevExt->buttons[1];
		}

		//Button X
		iTBuffer = pDevExt->intdata[6] & 255;
		if (iTBuffer >= pDevExt->BThreshold)
		{
			btndata |= pDevExt->buttons[2];
		}

		//Button Y
		iTBuffer = pDevExt->intdata[7] & 255;
		if (iTBuffer >= pDevExt->BThreshold)
		{
			btndata |= pDevExt->buttons[3];
		}

		//Black Button
		iTBuffer = pDevExt->intdata[8] & 255;
		if (iTBuffer >= pDevExt->BThreshold)
		{
			btndata |= pDevExt->buttons[4];
		}

		//White Button
		iTBuffer = pDevExt->intdata[9] & 255;
		if (iTBuffer >= pDevExt->BThreshold)
		{
			btndata |= pDevExt->buttons[5];


		if((pDevExt->intdata[2] & 16) && (pDevExt->intdata[2] & 32) && pDevExt->bTShortcut)
		{
			if(!pDevExt->bTShortcutTrigger)
			{
				pDevExt->bTShortcutTrigger = TRUE;
				pDevExt->bTThrottle = !pDevExt->bTThrottle;
			}
		}
		else
		{
			pDevExt->bTShortcutTrigger = FALSE;

			//Start Button
			if (pDevExt->intdata[2] & 16)
			{
				btndata |= pDevExt->buttons[6];
			}

			//Back Button
			if (pDevExt->intdata[2] & 32)
			{
				btndata |= pDevExt->buttons[7];
			}
		}

		//L-Stick Press
		if (pDevExt->intdata[2] & 64)
		{
			btndata |= pDevExt->buttons[8];
		}

		//R-Stick Press
		if (pDevExt->intdata[2] & 128)
		{
			btndata |= pDevExt->buttons[9];
		}

		if(!pDevExt->bTThrottle)
		{
			//L-Trigger Button
			iTBuffer = pDevExt->intdata[10] & 255;
			if (iTBuffer >= pDevExt->TThreshold)
			{
				btndata |= pDevExt->buttons[10];
			}

			//R-Trigger Button
			iTBuffer = pDevExt->intdata[11] & 255;
			if (iTBuffer >= pDevExt->TThreshold)
			{
				btndata |= pDevExt->buttons[11];
			}
		}
	}
	else
	{
		//Button A
		iTBuffer = pDevExt->intdata[4] & 255; //Pass the value of button A to iTBuffer
		//If value of button is higher than the threshold value then button is active
		if (iTBuffer >= pDevExt->BThreshold)
		{
			xbdata[0] |= 1;
		}

		//Button B
		iTBuffer = pDevExt->intdata[5] & 255;
		if (iTBuffer >= pDevExt->BThreshold)
		{
			xbdata[0] |= 2;
		}

		//Button X
		iTBuffer = pDevExt->intdata[6] & 255;
		if (iTBuffer >= pDevExt->BThreshold)
		{
			xbdata[0] |= 4;
		}

		//Button Y
		iTBuffer = pDevExt->intdata[7] & 255;
		if (iTBuffer >= pDevExt->BThreshold)
		{
			xbdata[0] |= 8;
		}

		//Black Button
		iTBuffer = pDevExt->intdata[8] & 255;
		if (iTBuffer >= pDevExt->BThreshold)
		{
			xbdata[0] |= 16;
		}

		//White Button
		iTBuffer = pDevExt->intdata[9] & 255;
		if (iTBuffer >= pDevExt->BThreshold)
		{
			xbdata[0] |= 32;
		}

		if((pDevExt->intdata[2] & 16) && (pDevExt->intdata[2] & 32) && pDevExt->bTShortcut)
		{
			if(!pDevExt->bTShortcutTrigger)
			{
				pDevExt->bTShortcutTrigger = TRUE;
				pDevExt->bTThrottle = !pDevExt->bTThrottle;
			}
		}
		else
		{
			pDevExt->bTShortcutTrigger = FALSE;

			//Start Button
			if (pDevExt->intdata[2] & 16)
			{
				xbdata[0] |= 64;
			}

			//Back Button
			if (pDevExt->intdata[2] & 32)
			{
				xbdata[0] |= 128;
			}
		}

		//L-Stick Press
		if (pDevExt->intdata[2] & 64)
		{
			xbdata[1] |= 1;
		}

		//R-Stick Press
		if (pDevExt->intdata[2] & 128)
		{
			xbdata[1] |= 2;
		}

		if(!pDevExt->bTThrottle)
		{
			//L-Trigger Button
			iTBuffer = pDevExt->intdata[10] & 255;
			if (iTBuffer >= pDevExt->TThreshold)
			{
				xbdata[1] |= 4;
			}

			//R-Trigger Button
			iTBuffer = pDevExt->intdata[11] & 255;
			if (iTBuffer >= pDevExt->TThreshold)
			{
				xbdata[1] |= 8;
			}
		}

	}

	if (pDevExt->bDPadButtons)
	{
		if(pDevExt->intdata[2] & 1)
		{
			KdPrint(("Up"));
			xbdata[10] |= 16;
		}
		if(pDevExt->intdata[2] & 8)
		{
			KdPrint(("Right"));
			xbdata[10] |= 32;
		}
		if(pDevExt->intdata[2] & 2)
		{
			KdPrint(("Down"));
			xbdata[10] |= 64;
		}
		if(pDevExt->intdata[2] & 4)
		{
			KdPrint(("Left"));
			xbdata[10] |= 128;
		}
	}

	// If both analog sticks are pressed then switch control between D-pad, L-stick, and R-stick
	if ((pDevExt->intdata[2] & 64) && (pDevExt->intdata[2] & 128))
	{
		if (!pDevExt->StickSwitch)
		{
			pDevExt->StickSwitch = TRUE;

			pDevExt->iCurrentConf += 1;

			if(pDevExt->iCurrentConf > (pDevExt->iNumConf - 1))
			{
				pDevExt->iCurrentConf = 0;
			}

			switch(pDevExt->iConf[pDevExt->iCurrentConf])
			{
			case 1:
				{
					pDevExt->iXYMov = 2;
					pDevExt->iPOV = 1;
					pDevExt->iSlider = 4;
					break;
				}
			case 2:
				{
					pDevExt->iXYMov = 1;
					pDevExt->iPOV = 4;
					pDevExt->iSlider = 2;
					break;
				}
			case 3:
				{
					pDevExt->iXYMov = 4;
					pDevExt->iPOV = 2;
					pDevExt->iSlider = 1;
					break;
				}
			case 4:
				{
					pDevExt->iXYMov = 2;
					pDevExt->iPOV = 4;
					pDevExt->iSlider = 1;
					break;
				}
			case 5:
				{
					pDevExt->iXYMov = 4;
					pDevExt->iPOV = 1;
					pDevExt->iSlider = 2;
					break;
				}
			case 6:
				{
					pDevExt->iXYMov = 1;
					pDevExt->iPOV = 2;
					pDevExt->iSlider = 4;
					break;
				}
			case 7:
				{
					pDevExt->iXYMov = 1;
					pDevExt->iXYMov |= 2;
					pDevExt->iPOV = 0;
					pDevExt->iSlider = 4;
					break;
				}
			case 8:
				{
					pDevExt->iXYMov = 1;
					pDevExt->iXYMov |= 2;
					pDevExt->iPOV = 4;
					pDevExt->iSlider = 0;
					break;
				}
			}
		}
	}
	else
	{
		pDevExt->StickSwitch = FALSE;
	}

	// X/Y Movement
	xdata = 0;
	ydata = 0;
	if((pDevExt->iXYMov & 2) || (pDevExt->iXYMov & 4))
	{
		int iDeadZone;
		int iXYControl;
		if(pDevExt->iXYMov & 2)
		{
			iDeadZone = pDevExt->LStickDZ;
			iXYControl = 12;
		}
		else
		{
			if(pDevExt->iXYMov & 4)
			{
				iDeadZone = pDevExt->RStickDZ;
				iXYControl = 16;
			}
		}

		xbdata[2] = pDevExt->intdata[iXYControl] & 255;
		xbdata[3] = pDevExt->intdata[iXYControl + 1] & 255;

		xdata = xbdata[3] << 8;
		xdata |= xbdata[2];

		xbdata[4] = (pDevExt->intdata[iXYControl + 2] & 255) ^ 255;
		xbdata[5] = (pDevExt->intdata[iXYControl + 3] & 255) ^ 255;

		ydata = xbdata[5] << 8;
		ydata |= xbdata[4];

		CalcAxes(&xdata, &ydata, iDeadZone, pDevExt);
	}

	if ((pDevExt->iXYMov & 1) && !pDevExt->bDPadButtons)
	{
		if(pDevExt->intdata[2] & 8)
		{
			KdPrint(("Right"));
			xdata = 32767;
		}
		else
		{
			if(pDevExt->intdata[2] & 4)
			{
				KdPrint(("Left"));
				xdata = -32767;
			}
		}

		if(pDevExt->intdata[2] & 2)
		{
			KdPrint(("Down"));
			ydata = 32767;
		}
		else
		{
			if(pDevExt->intdata[2] & 1)
			{
				KdPrint(("Up"));
				ydata = -32767;
			}
		}
	}

	RtlCopyMemory(&xbdata[2], &xdata, sizeof(xdata));
	RtlCopyMemory(&xbdata[4], &ydata, sizeof(ydata));

	// POV Hat Switch
	xbdata[10] |= 8;
	if((pDevExt->iPOV & 2) || (pDevExt->iPOV & 4))
	{
		int iPOVControl;
		if(pDevExt->iPOV & 2)
		{
			iPOVControl = 12;
		}
		else
		{
			if(pDevExt->iPOV & 4)
			{
				iPOVControl = 16;
			}
		}

		if((pDevExt->intdata[iPOVControl + 1] & 255) == 127)
		{
			KdPrint(("Right"));
			xbdata[10] = 2;
		}

		if((pDevExt->intdata[iPOVControl + 1] & 255) == 128)
		{
			KdPrint(("Left"));
			xbdata[10] = 6;
		}

		if((pDevExt->intdata[iPOVControl + 3] & 255) == 128)
		{
			KdPrint(("Down"));
			xbdata[10] = 4;
		}

		if((pDevExt->intdata[iPOVControl + 3] & 255) == 127)
		{
			KdPrint(("Up"));
			xbdata[10] = 0;
		}


		if((((pDevExt->intdata[iPOVControl + 3] & 255) < 127) && ((pDevExt->intdata[iPOVControl + 3] & 255) > 50)) && (((pDevExt->intdata[iPOVControl + 1] & 255) < 127) && (pDevExt->intdata[iPOVControl + 1] & 255) > 50))
		{
			xbdata[10] = 1;
		}

		if((((pDevExt->intdata[iPOVControl + 1] & 255) < 127) && ((pDevExt->intdata[iPOVControl + 1] & 255) > 50)) && (((pDevExt->intdata[iPOVControl + 3] & 255) > 128) && (pDevExt->intdata[iPOVControl + 3] & 255) < 205))
		{
			xbdata[10] = 3;
		}

		if((((pDevExt->intdata[iPOVControl + 3] & 255) > 128) && ((pDevExt->intdata[iPOVControl + 3] & 255) < 205)) && (((pDevExt->intdata[iPOVControl + 1] & 255) > 128) && (pDevExt->intdata[iPOVControl + 1] & 255) < 205))
		{
			xbdata[10] = 5;
		}

		if((((pDevExt->intdata[iPOVControl + 1] & 255) > 128) && ((pDevExt->intdata[iPOVControl + 1] & 255) < 205)) && (((pDevExt->intdata[iPOVControl + 3] & 255) < 127) && (pDevExt->intdata[iPOVControl + 1] & 255) > 50))
		{
			xbdata[10] = 7;
		}
	}

	if ((pDevExt->iPOV & 1) && !pDevExt->bDPadButtons)
	{
		if(pDevExt->intdata[2] & 8)
		{
			KdPrint(("Right"));
			xbdata[10] = 2;
		}

		if(pDevExt->intdata[2] & 4)
		{
			KdPrint(("Left"));
			xbdata[10] = 6;
		}

		if(pDevExt->intdata[2] & 2)
		{
			KdPrint(("Down"));
			xbdata[10] = 4;
		}

		if(pDevExt->intdata[2] & 1)
		{
			KdPrint(("Up"));
			xbdata[10] = 0;
		}

		if((pDevExt->intdata[2] & 1) && (pDevExt->intdata[2] & 8))
		{
			xbdata[10] = 1;
		}

		if((pDevExt->intdata[2] & 8) && (pDevExt->intdata[2] & 2))
		{
			xbdata[10] = 3;
		}

		if((pDevExt->intdata[2] & 2) && (pDevExt->intdata[2] & 4))
		{
			xbdata[10] = 5;
		}

		if((pDevExt->intdata[2] & 4) && (pDevExt->intdata[2] & 1))
		{
			xbdata[10] = 7;
		}
	}

	// Throttle and Rudder
	xdata = 0;
	ydata = 0;
	if((pDevExt->iSlider & 2) || (pDevExt->iSlider & 4))
	{
		int iDeadZone;
		int iSliderControl;
		if(pDevExt->iSlider & 2)
		{
			iDeadZone = pDevExt->LStickDZ;
			iSliderControl = 12;
		}
		else
		{
			if(pDevExt->iSlider & 4)
			{
				iDeadZone = pDevExt->RStickDZ;
				iSliderControl = 16;
			}
		}

		if(!pDevExt->bSSwitch)
		{
			xbdata[6] = pDevExt->intdata[iSliderControl] & 255;
			xbdata[7] = pDevExt->intdata[iSliderControl + 1] & 255;

			xdata = xbdata[7] << 8;
			xdata |= xbdata[6];

			xbdata[8] = (pDevExt->intdata[iSliderControl + 2] & 255) ^ 255;
			xbdata[9] = (pDevExt->intdata[iSliderControl + 3] & 255) ^ 255;

			ydata = xbdata[9] << 8;
			ydata |= xbdata[8];

			if(!pDevExt->bTThrottle)
			{
				CalcAxes(&xdata, &ydata, iDeadZone, pDevExt);
			}
			else
			{
				/*if(pDevExt->btnset)
				{
					if (ydata < -32500)
					{
						btndata |= pDevExt->buttons[10];
					}

					if (ydata > 32500)
					{
						btndata |= pDevExt->buttons[11];
					}
				}
				else
				{
					if (ydata < -32500)
					{
						xbdata[1] |= 4;
					}

					if (ydata > 32500)
					{
						xbdata[1] |= 8;
					}
				}*/

				ydata = 0;
				CalcAxes(&xdata, &ydata, iDeadZone, pDevExt);

				//L-Trigger Button - R-Trigger Button
				iTBuffer = (pDevExt->intdata[10] & 255) - (pDevExt->intdata[11] & 255);

				ydata = 32767 * iTBuffer/255;
			}
		}
		else
		{
			xbdata[6] = pDevExt->intdata[iSliderControl] & 255;
			xbdata[7] = pDevExt->intdata[iSliderControl + 1] & 255;

			ydata = xbdata[7] << 8;
			ydata |= xbdata[6];

			xbdata[8] = (pDevExt->intdata[iSliderControl + 2] & 255) ^ 255;
			xbdata[9] = (pDevExt->intdata[iSliderControl + 3] & 255) ^ 255;

			xdata = xbdata[9] << 8;
			xdata |= xbdata[8];

			if(!pDevExt->bTThrottle)
			{
				CalcAxes(&xdata, &ydata, iDeadZone, pDevExt);
			}
			else
			{
				/*if(pDevExt->btnset)
				{
					if (ydata < -32500)
					{
						btndata |= pDevExt->buttons[10];
					}

					if (ydata > 32500)
					{
						btndata |= pDevExt->buttons[11];
					}
				}
				else
				{
					if (ydata < -32500)
					{
						xbdata[1] |= 4;
					}

					if (ydata > 32500)
					{
						xbdata[1] |= 8;
					}
				}*/

				xdata = 0;
				CalcAxes(&xdata, &ydata, iDeadZone, pDevExt);

				//L-Trigger Button - R-Trigger Button
				iTBuffer = (pDevExt->intdata[10] & 255) - (pDevExt->intdata[11] & 255);

				xdata = 32767 * iTBuffer/255;
			}
		}
	}

	if ((pDevExt->iSlider & 1) && !pDevExt->bDPadButtons)
	{
		if(!pDevExt->bSSwitch)
		{
			if(pDevExt->intdata[2] & 8)
			{
				KdPrint(("Right"));
				xdata = 32767;
			}
			else
			{
				if(pDevExt->intdata[2] & 4)
				{
					KdPrint(("Left"));
					xdata = -32767;
				}
			}

			if(!pDevExt->bTThrottle)
			{
				if(pDevExt->intdata[2] & 2)
				{
					KdPrint(("Down"));
					ydata = 32767;
				}
				else
				{
					if(pDevExt->intdata[2] & 1)
					{
						KdPrint(("Up"));
						ydata = -32767;
					}
				}
			}
			else
			{
				short tempThrottle;

				RtlZeroMemory(&tempThrottle, sizeof(tempThrottle));

				//L-Trigger Button - R-Trigger Button
				iTBuffer = (pDevExt->intdata[10] & 255) - (pDevExt->intdata[11] & 255);

				tempThrottle = 32767 * iTBuffer/255;

				ydata = tempThrottle;

				if(pDevExt->btnset)
				{
					if (pDevExt->intdata[2] & 1)
					{
						btndata |= pDevExt->buttons[10];
					}

					if (pDevExt->intdata[2] & 2)
					{
						btndata |= pDevExt->buttons[11];
					}
				}
				else
				{
					if (pDevExt->intdata[2] & 1)
					{
						xbdata[1] |= 4;
					}

					if (pDevExt->intdata[2] & 2)
					{
						xbdata[1] |= 8;
					}
				}
			}
		}
		else
		{
			if(pDevExt->intdata[2] & 8)
			{
				KdPrint(("Right"));
				ydata = 32767;
			}
			else
			{
				if(pDevExt->intdata[2] & 4)
				{
					KdPrint(("Left"));
					ydata = -32767;
				}
			}

			if(!pDevExt->bTThrottle)
			{
				if(pDevExt->intdata[2] & 2)
				{
					KdPrint(("Down"));
					xdata = 32767;
				}
				else
				{
					if(pDevExt->intdata[2] & 1)
					{
						KdPrint(("Up"));
						xdata = -32767;
					}
				}
			}
			else
			{
				short tempThrottle;

				RtlZeroMemory(&tempThrottle, sizeof(tempThrottle));

				//L-Trigger Button - R-Trigger Button
				iTBuffer = (pDevExt->intdata[10] & 255) - (pDevExt->intdata[11] & 255);

				tempThrottle = 32767 * iTBuffer/255;

				xdata = tempThrottle;

				if(pDevExt->btnset)
				{
					if (pDevExt->intdata[2] & 1)
					{
						btndata |= pDevExt->buttons[10];
					}

					if (pDevExt->intdata[2] & 2)
					{
						btndata |= pDevExt->buttons[11];
					}
				}
				else
				{
					if (pDevExt->intdata[2] & 1)
					{
						xbdata[1] |= 4;
					}

					if (pDevExt->intdata[2] & 2)
					{
						xbdata[1] |= 8;
					}
				}
			}
		}
	}

	if(pDevExt->btnset)
		RtlCopyMemory(&xbdata, &btndata, sizeof(btndata));

	RtlCopyMemory(&xbdata[6], &xdata, sizeof(xdata));
	RtlCopyMemory(&xbdata[8], &ydata, sizeof(ydata));

	KdPrint(("SIXCDReadData - IOCTL_HID_READ_REPORT, report obtained - %xh %xh %xh %xh", pDevExt->intdata[12], pDevExt->intdata[13], pDevExt->intdata[14], pDevExt->intdata[15]));
	RtlCopyMemory(pIrp->UserBuffer, xbdata, sizeof(xbdata));
	pIrp->IoStatus.Information = sizeof(xbdata);
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

VOID timerDPCProc(IN PKDPC Dpc, IN PDEVICE_EXTENSION pDevExt, IN PVOID SystemArgument1, IN PVOID SystemArgument2)
{
	pDevExt->timerEnabled = FALSE;
}

//+static void hid_fixup_ps3(struct usb_device * dev, int ifnum)
//+{
//+	char * buf;
//+	int ret;
//+
//+	buf = kmalloc(18, GFP_KERNEL);
//+	if (!buf)
//+		return;
//+	
//+	ret = usb_control_msg(dev, /*usb_dev_handle *dev*/
//+			      usb_rcvctrlpipe(dev, 0), /*unsigned int pipe*/
//+			      0x01 /*HID_REQ_GET_REPORT*/, /*__u8 request*/
//+			      0xA1 /*USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE*/, /*__u8 requesttype*/
//+			      0x03F2 /*(3 << 8) | 0xf2*/, /*__u16 value*/
//+			      ifnum, /*__u16 index*/
//+			      buf, /*void * data*/
//+			      17, /*__u16 size*/
//+			      5000 /*USB_CTRL_GET_TIMEOUT*/); /*int timeout*/
//+	if (ret < 0)
//+		printk(KERN_ERR "%s: ret=%d\n", __FUNCTION__, ret);
//+
//+	kfree(buf);
//+}

//#define HID_REQ_GET_REPORT              0x01

//#define USB_DIR_IN                      0x80			b'10000000
//#define USB_TYPE_CLASS                  (0x01 << 5)	b'00100000
//#define USB_RECIP_INTERFACE             0x01			b'00000001
// USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE =	b'10100001
//
// bmRequestType
// D[7]: Data Xfer Dir
// D[6:5]: Type
// D[4:0]: Recipient

//#define USB_CTRL_GET_TIMEOUT    5000

//UsbBuildVendorRequest(urb, \
//                              cmd, \
//                              length, \
//                              transferFlags, \
//                              reservedbits, \
//                              request, \
//                              value, \
//                              index, \
//                              transferBuffer, \
//                              transferBufferMDL, \
//                              transferBufferLength, \
//                              link) { \
//            (urb)->UrbHeader.Function =  cmd; \
//            (urb)->UrbHeader.Length = (length); \
//            (urb)->UrbControlVendorClassRequest.TransferBufferLength = (transferBufferLength); \
//            (urb)->UrbControlVendorClassRequest.TransferBufferMDL = (transferBufferMDL); \
//            (urb)->UrbControlVendorClassRequest.TransferBuffer = (transferBuffer); \
//            (urb)->UrbControlVendorClassRequest.RequestTypeReservedBits = (reservedbits); \
//            (urb)->UrbControlVendorClassRequest.Request = (request); \
//            (urb)->UrbControlVendorClassRequest.Value = (value); \
//            (urb)->UrbControlVendorClassRequest.Index = (index); \
//            (urb)->UrbControlVendorClassRequest.TransferFlags = (transferFlags); \
//            (urb)->UrbControlVendorClassRequest.UrbLink = (link); }
//
//b'0000 0001 1010 0001
//#define URB_FUNCTION_CLASS_INTERFACE                 0x001B 'b0000 0000 0001 1011
//#define URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE      0x000B 'b0000 0000 0000 1011
NTSTATUS EnableSixaxis(PDEVICE_EXTENSION pDevExt)
	{

	NTSTATUS status;
	PIRP pIrp;
	IO_STATUS_BLOCK iostatus;
	PIO_STACK_LOCATION stack;
	KEVENT event;
	char * buf;
	PURB urb = (PURB)ExAllocatePool(NonPagedPool, sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST));
	char * buf = (char *)ExAllocatePool(NonPagedPool, 18);


	KdPrint(("EnableSixaxis - Building urb"));
	UsbBuildVendorRequest(urb, 
		URB_FUNCTION_CLASS_DEVICE, 
		sizeof(struct _URB_CONTROL_VENDOR_OR_CLASS_REQUEST),
		0,
		0, 
		0x01, 
		(3 << 8) | 0xf2, 
		0x01, 
		buf, 
		NULL, 
		17);

	KeInitializeEvent(&event, NotificationEvent, FALSE);

	pIrp = IoBuildDeviceIoControlRequest(IOCTL_INTERNAL_USB_SUBMIT_URB,
		pDevExt->pLowerPdo, NULL, 0, NULL, 0, TRUE, &event, &iostatus);

	if (!pIrp)
		{
			KdPrint(("EnableSixaxis - Unable to allocate IRP for sending URB"));
			return STATUS_INSUFFICIENT_RESOURCES;
		}

	stack = IoGetNextIrpStackLocation(pIrp);
	stack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
	stack->Parameters.Others.Argument1 = urb;
	status = IoCallDriver(pDevExt->pLowerPdo, pIrp);
	if (status == STATUS_PENDING)
		{
			KdPrint(("EnableSixaxis - status_pending"));
			KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
			status = iostatus.Status;
		}
	if(NT_SUCCESS(status))
	{
		KdPrint(("EnableSixaxis - status success"));
	}
	else
	{
		KdPrint(("EnableSixaxis - error %d", status));
	}
	KdPrint(("EnableSixaxis - returning"));

	return status;
}
void enable_sixaxis()
{
	char msg[] = { 0x53 /*HIDP_TRANS_SET_REPORT | HIDP_DATA_RTYPE_FEATURE*/,
		0xf4,  0x42, 0x03, 0x00, 0x00 };\
	PURB Urb;

	UsbBuildSelectInterfaceRequest(
}

#pragma LOCKEDCODE
