    Copyright 2008 Andrew Jones

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

SixCD - Sixaxis Controller Driver
================================
Alpha Release 0.0.1
================================

-------------------
Source Code
-------------------

SIXCD_control.c 	- Takes care of IOCTL's received.
SIXCD_driver.c		- Main parts of the driver.
SIXCD_driver.h	 	- Definitions of functions and some structs.
SIXCD_usb.c 		- USB interface work.
SIXCD_hid.h 		- Definitions of constants and macros related to the HID interface.
SIXCD_report.h	 	- Definition of report descriptor.
SIXCD_report.hid	- Definition of report descriptor (binary) for use with HIDTool.
SIXCD.rc 		- Contains properties for driver binary (Version, Company, etc.).
resource.h 		- Needed for driver binary properties.
RemoveLock.c	 	- Functions which replace missing removelock functions in Win98.
sources 		- Lists Files to include for compiling and linking.
SIXCD.inf		- Installation file.


copy "hidclass.lib" and "usbd.lib" from the DDK you are going to build with to the
SIXCD source directory.

If building with Windows 98 DDK:
	Set WIN98 = 1 in SIXCD_driver.h
	Set DBG in SIXCD_driver.h to 1 for debug build or 0 for release build


---------------------------
Redcl0ud
http://phaseone.sytes.net