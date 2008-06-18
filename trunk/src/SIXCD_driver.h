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

#include "wdm.h"
#include "SIXCD_hid.h"

#define PAGEDCODE code_seg("PAGE")
#define LOCKEDCODE code_seg()

#ifndef DBG
#define DBG 0
#endif
#define WIN98 1

ULONG _fltused = 0;

#if WIN98
	#include "usbioctl.h"
	#include "usbdi.h"
	#include "usbdlib.h"

	typedef struct _IO_REMOVE_LOCK_TRACKING_BLOCK * PIO_REMOVE_LOCK_TRACKING_BLOCK;

	typedef struct _IO_REMOVE_LOCK_COMMON_BLOCK {
		BOOLEAN     Removed;
		BOOLEAN     Reserved [3];
		LONG        IoCount;
		KEVENT      RemoveEvent;

	} IO_REMOVE_LOCK_COMMON_BLOCK;

	typedef struct _IO_REMOVE_LOCK_DBG_BLOCK {
		LONG        Signature;
		LONG        HighWatermark;
		LONGLONG    MaxLockedTicks;
		LONG        AllocateTag;
		LIST_ENTRY  LockList;
		KSPIN_LOCK  Spin;
		LONG        LowMemoryCount;
		ULONG       Reserved1[4];
		PVOID       Reserved2;
		PIO_REMOVE_LOCK_TRACKING_BLOCK Blocks;
	} IO_REMOVE_LOCK_DBG_BLOCK;

	typedef struct _IO_REMOVE_LOCK {
		IO_REMOVE_LOCK_COMMON_BLOCK Common;
	#if DBG
		IO_REMOVE_LOCK_DBG_BLOCK Dbg;
	#endif
	} IO_REMOVE_LOCK, *PIO_REMOVE_LOCK;

	#define InitializeRemoveLock(lock, tag, minutes, maxcount) IntInitializeRemoveLock(lock, tag, minutes, maxcount)
	#define AcquireRemoveLock(lock, tag) IntAcquireRemoveLock(lock, tag)
	#define ReleaseRemoveLock(lock, tag) IntReleaseRemoveLock(lock, tag)
	#define ReleaseRemoveLockAndWait(lock, tag) IntReleaseRemoveLockAndWait(lock, tag)
#else
	#include "usbdrivr.h"
	#define InitializeRemoveLock(Lock, Tag, Maxmin, HighWater) IoInitializeRemoveLock(Lock, Tag, Maxmin, HighWater)
	#define AcquireRemoveLock(RemoveLock, Tag) IoAcquireRemoveLock(RemoveLock, Tag)
	#define ReleaseRemoveLock(RemoveLock, Tag) IoReleaseRemoveLock(RemoveLock, Tag)
	#define ReleaseRemoveLockAndWait(RemoveLock, Tag) IoReleaseRemoveLockAndWait(RemoveLock, Tag)
#endif

//#define WIN32NAME  L"\\DosDevices\\SIXCD"
//#define DEVICENAME L"\\Device\\SIXCD"

UNICODE_STRING RegistryPath;

typedef struct _DEVICE_EXTENSION{
    PDEVICE_OBJECT pFdo;
    PDEVICE_OBJECT pLowerPdo;
    PDEVICE_OBJECT pPdo;
	BOOLEAN DeviceStarted;
	int iPOV;
	int iXYMov;
	int iSlider;
	int TThreshold;
	int BThreshold;
	int iCurrentConf;
	int iNumConf;
	int iConf[8];
	BOOLEAN StickSwitch;
	LONG RequestCount;
	BOOLEAN DeviceRemoved;
	BOOLEAN SurpriseRemoved;
	KEVENT RemoveEvent;
	BOOLEAN PowerDown;
	USB_DEVICE_DESCRIPTOR dd;
	USBD_CONFIGURATION_HANDLE hconfig;
	USBD_PIPE_HANDLE hintpipe;
	USBD_PIPE_HANDLE hintoutpipe;
	PUSB_CONFIGURATION_DESCRIPTOR pcd;
	PIRP pIrp;
	PIRP CurrentIrp;
	PURB pUrb;
	unsigned char intdata[20];
	unsigned char intoutdata[6];
	int buttons[12];
	int LStickDZ;
	int RStickDZ;
	int LAFactor;
	int RAFactor;
	BOOLEAN btnset;
	BOOLEAN bTThrottle;
	BOOLEAN bFPCalc;
	BOOLEAN bSSwitch;
	BOOLEAN bTShortcutTrigger;
	BOOLEAN bTShortcut;
	BOOLEAN bDPadButtons;
	BOOLEAN pollpending;
	KSPIN_LOCK polllock;
	//KSPIN_LOCK readlock;
	IO_REMOVE_LOCK RemoveLock;
	KDPC timeDPC;
	KTIMER timer;
	BOOLEAN timerEnabled;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//SIXCD_driver.c

NTSTATUS DriverEntry(IN PDRIVER_OBJECT pDriverObject, IN PUNICODE_STRING pRegistryPath);
NTSTATUS SIXCDCreate(IN PDEVICE_OBJECT pFdo, IN PIRP Irp);
NTSTATUS SIXCDClose(IN PDEVICE_OBJECT pFdo, IN PIRP Irp);
VOID SIXCDUnload(IN PDRIVER_OBJECT pDriverObject);
VOID SIXCDRemoveDevice(PDEVICE_OBJECT pFdo, PIRP pIrp);
VOID SIXCDStopDevice(PDEVICE_OBJECT pFdo, PIRP pIrp);
NTSTATUS SIXCDPnPComplete(PDEVICE_OBJECT pFdo, PIRP pIrp, PVOID Context);
NTSTATUS SIXCDIncRequestCount(PDEVICE_EXTENSION pDevExt);
VOID SIXCDDecRequestCount(PDEVICE_EXTENSION pDevExt);
NTSTATUS SIXCDDispatchPnp(IN PDEVICE_OBJECT pFdo, IN PIRP pIrp);
NTSTATUS SIXCDDispatchPower(IN PDEVICE_OBJECT pFdo, IN PIRP pIrp);
NTSTATUS SIXCDAddDevice(IN PDRIVER_OBJECT pDriverObject, IN PDEVICE_OBJECT pFdo);
NTSTATUS SIXCDStartDevice(PDEVICE_OBJECT pFdo, PIRP pIrp);
void SIXCDReadButtonConfig(IN PDEVICE_OBJECT pFdo);
int power2(int n);

//SIXCD_control.c

NTSTATUS SIXCDDispatchIntDevice(IN PDEVICE_OBJECT pFdo, IN PIRP pIrp);
NTSTATUS SIXCDDispatchDevice(IN PDEVICE_OBJECT pFdo, IN PIRP pIrp);
NTSTATUS SIXCDDispatchSystem(IN PDEVICE_OBJECT pFdo, IN PIRP pIrp);
NTSTATUS SIXCDReadData(PDEVICE_EXTENSION pDevExt, PIRP pIrp);
VOID timerDPCProc(IN PKDPC Dpc, IN PVOID DeferredContext, IN PVOID SystemArgument1, IN PVOID SystemArgument2);

//SIXCD_usb.c

NTSTATUS SendAwaitUrb(PDEVICE_OBJECT pFdo, PURB pUrb);
NTSTATUS CreateInterruptUrb(PDEVICE_OBJECT pFdo);
VOID DeleteInterruptUrb(PDEVICE_OBJECT pFdo);
NTSTATUS StartInterruptUrb(PDEVICE_EXTENSION pDevExt);
NTSTATUS SendInterruptUrb(PDEVICE_EXTENSION pDevExt);
NTSTATUS OnInterrupt(PDEVICE_OBJECT junk, PIRP pIrp, PDEVICE_EXTENSION pDevExt);
VOID StopInterruptUrb(PDEVICE_EXTENSION pDevExt);
//NTSTATUS ResetPipe(IN PDEVICE_OBJECT DeviceObject, IN USBD_PIPE_HANDLE *PipeHandle);

//RemoveLock.c

VOID IntInitializeRemoveLock(PIO_REMOVE_LOCK lock, ULONG tag, ULONG minutes, ULONG maxcount);
NTSTATUS IntAcquireRemoveLock(PIO_REMOVE_LOCK lock, PVOID tag);
VOID IntReleaseRemoveLock(PIO_REMOVE_LOCK lock, PVOID tag);
VOID IntReleaseRemoveLockAndWait(PIO_REMOVE_LOCK lock, PVOID tag);