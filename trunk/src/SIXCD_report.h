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

// SIXCD_report.h


char ReportDescriptor[213] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x04,                    // USAGE (Joystick)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x05, 0x09,                    //   USAGE_PAGE (Button)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x35, 0x00,                    //   PHYSICAL_MINIMUM (0)
    0x45, 0x01,                    //   PHYSICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x01,                    //   USAGE (Button 1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x02,                    //   USAGE (Button 2)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x03,                    //   USAGE (Button 3)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x04,                    //   USAGE (Button 4)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x05,                    //   USAGE (Button 5)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x06,                    //   USAGE (Button 6)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x07,                    //   USAGE (Button 7)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x08,                    //   USAGE (Button 8)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x09,                    //   USAGE (Button 9)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x0a,                    //   USAGE (Button 10)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x05, 0x09,                    //   USAGE_PAGE (Button)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x0b,                    //   USAGE (Button 11)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x0c,                    //   USAGE (Button 12)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x75, 0x04,                    //   REPORT_SIZE (4)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x81, 0x01,                    //   INPUT (Cnst,Ary,Abs)
    0x75, 0x10,                    //   REPORT_SIZE (16)
    0x16, 0x01, 0x80,              //   LOGICAL_MINIMUM (-32767)
    0x26, 0xff, 0x7f,              //   LOGICAL_MAXIMUM (32767)
    0x35, 0x00,                    //   PHYSICAL_MINIMUM (0)
    0x46, 0xff, 0x7f,              //   PHYSICAL_MAXIMUM (32767)
    0x05, 0x01,                    //   USAGE_PAGE (Generic Desktop)
    0x95, 0x02,                    //   REPORT_COUNT (2)
    0x09, 0x30,                    //   USAGE (X)
    0x09, 0x31,                    //   USAGE (Y)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x05, 0x02,                    //   USAGE_PAGE (Simulation Controls)
    0x95, 0x02,                    //   REPORT_COUNT (2)
    0x09, 0xba,                    //   USAGE (Rudder)
    0x09, 0xbb,                    //   USAGE (Throttle)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x05, 0x01,                    //   USAGE_PAGE (Generic Desktop)
    0x09, 0x39,                    //   USAGE (Hat switch)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x07,                    //   LOGICAL_MAXIMUM (7)
    0x35, 0x00,                    //   PHYSICAL_MINIMUM (0)
    0x46, 0x3b, 0x01,              //   PHYSICAL_MAXIMUM (315)
    0x65, 0x14,                    //   UNIT (Eng Rot:Angular Pos)
    0x75, 0x04,                    //   REPORT_SIZE (4)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x05, 0x09,                    //   USAGE_PAGE (Button)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x35, 0x00,                    //   PHYSICAL_MINIMUM (0)
    0x45, 0x01,                    //   PHYSICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x0d,                    //   USAGE (Button 13)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x0e,                    //   USAGE (Button 14)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x0f,                    //   USAGE (Button 15)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x10,                    //   USAGE (Button 16)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x35, 0x00,                    //   PHYSICAL_MINIMUM (0)
    0x46, 0xff, 0x00,              //   PHYSICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x03,                    //   REPORT_COUNT (3)
    0x05, 0x00,                    //   USAGE_PAGE (Not Defined)
    0x09, 0x00,                    //   USAGE (Undefined)
    0x09, 0x01,                    //   USAGE (Undefined)
    0x09, 0x02,                    //   USAGE (Undefined)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)
    0xc0                           // END_COLLECTION
};

