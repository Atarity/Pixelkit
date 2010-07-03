/* Nes/Snes/Genesis/SMS/Atari to USB
 * Copyright (C) 2006-2007 Raphaël Assénat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The author may be contacted at raph@raphnet.net
 */
#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "usbconfig.h"
#include "gamepad.h"
#include "leds.h"
#include "nes.h"

#define REPORT_SIZE		3
#define GAMEPAD_BYTES	1

/******** IO port definitions **************/
#define NES_LATCH_DDR	DDRC
#define NES_LATCH_PORT	PORTC
#define NES_LATCH_BIT	(1<<4)

#define NES_CLOCK_DDR	DDRC
#define NES_CLOCK_PORT	PORTC
#define NES_CLOCK_BIT	(1<<5)

#define NES_DATA_PORT	PORTC
#define NES_DATA_DDR	DDRC
#define NES_DATA_PIN	PINC
#define NES_DATA_BIT	(1<<3)

/********* IO port manipulation macros **********/
#define NES_LATCH_LOW()	do { NES_LATCH_PORT &= ~(NES_LATCH_BIT); } while(0)
#define NES_LATCH_HIGH()	do { NES_LATCH_PORT |= NES_LATCH_BIT; } while(0)
#define NES_CLOCK_LOW()	do { NES_CLOCK_PORT &= ~(NES_CLOCK_BIT); } while(0)
#define NES_CLOCK_HIGH()	do { NES_CLOCK_PORT |= NES_CLOCK_BIT; } while(0)

#define NES_GET_DATA()	(NES_DATA_PIN & NES_DATA_BIT)


/*********** prototypes *************/
static char nesInit(void);
static void nesUpdate(void);

// the most recent bytes we fetched from the controller
static unsigned char last_read_controller_bytes[GAMEPAD_BYTES];
// the most recently reported bytes
static unsigned char last_reported_controller_bytes[GAMEPAD_BYTES];

static char nesInit(void)
{
	// clock and latch as output
	NES_LATCH_DDR |= NES_LATCH_BIT;
	NES_CLOCK_DDR |= NES_CLOCK_BIT;
	
	// data as input
	NES_DATA_DDR &= ~(NES_DATA_BIT);
	// enable pullup. Prevents rapid random toggling of pins
	// when no controller is connected.
	NES_DATA_PORT |= NES_DATA_BIT;

	// clock is normally high
	NES_CLOCK_PORT |= NES_CLOCK_BIT;

	// LATCH is Active HIGH
	NES_LATCH_PORT &= ~(NES_LATCH_BIT);

	return 0;
}

/*
 *        Clock Cycle     Button Reported
        ===========     ===============
        1               B
        2               Y
        3               Select
        4               Start
        5               Up on joypad
        6               Down on joypad
        7               Left on joypad
        8               Right on joypad
 * 
 */

static void nesUpdate(void)
{
	int i;
	unsigned char tmp=0;

	NES_LATCH_HIGH();
	_delay_us(12);
	NES_LATCH_LOW();

	for (i=0; i<8; i++)
	{
		_delay_us(6);
		NES_CLOCK_LOW();
		
		tmp <<= 1;	
		if (!NES_GET_DATA()) { tmp |= 1; }

		_delay_us(6);
		
		NES_CLOCK_HIGH();
	}
	last_read_controller_bytes[0] = tmp;
}

static char nesChanged(char id)
{
	return memcmp(last_read_controller_bytes, 
					last_reported_controller_bytes, GAMEPAD_BYTES);
}

static char nesBuildReport(unsigned char *reportBuffer, char id)
{
	int x, y;
	unsigned char lrcb;

	if (reportBuffer != NULL)
	{
		y = x = 0x80;
		lrcb = last_read_controller_bytes[0];

		if (lrcb&0x1) { x = 0xff; }
		if (lrcb&0x2) { x = 0; }
		if (lrcb&0x4) { y = 0xff; }
		if (lrcb&0x8) { y = 0; }
		reportBuffer[0]=x;
		reportBuffer[1]=y;

		// swap buttons so they get reported in a more
		// logical order (A-B-Select-Start)
		reportBuffer[2]=(lrcb & 0x80) >> 7;
		reportBuffer[2]|=(lrcb & 0x40) >> 5;
		reportBuffer[2]|=(lrcb & 0x20) >> 3;
		reportBuffer[2]|=(lrcb & 0x10) >> 1;
	}
	memcpy(last_reported_controller_bytes, 
			last_read_controller_bytes, GAMEPAD_BYTES);	

	return REPORT_SIZE;
}

const char nes_usbHidReportDescriptor[] PROGMEM = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x05,                    // USAGE (Game Pad)
    0xa1, 0x01,                    //   COLLECTION (Application)
    0x09, 0x01,                    //     USAGE (Pointer)
    0xa1, 0x00,                    //     COLLECTION (Physical)
    0x09, 0x30,                    //       USAGE (X)
    0x09, 0x31,                    //       USAGE (Y)
    0x15, 0x00,                    //       LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,				//      LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //       REPORT_SIZE (8)
    0x95, 0x02,                    //       REPORT_COUNT (2)
    0x81, 0x02,                    //       INPUT (Data,Var,Abs)
    0xc0,                          //     END_COLLECTION
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x04,                    //     USAGE_MAXIMUM (Button 4)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x95, 0x04,                    //     REPORT_COUNT (4)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)

	/* Padding.*/
	0x75, 0x01,                    //     REPORT_SIZE (1)
	0x95, 0x04,                    //     REPORT_COUNT (4)
	0x81, 0x03,                    //     INPUT (Constant,Var,Abs)
    0xc0,                          //   END_COLLECTION
};

#define USBDESCR_DEVICE         1

// This is the same descriptor as in devdesc.c, but the product id is 0x0a99 

const char nes_usbDescrDevice[] PROGMEM = {    /* USB device descriptor */
    18,         /* sizeof(usbDescrDevice): length of descriptor in bytes */
    USBDESCR_DEVICE,    /* descriptor type */
    0x01, 0x01, /* USB version supported */
    USB_CFG_DEVICE_CLASS,
    USB_CFG_DEVICE_SUBCLASS,
    0,          /* protocol */
    8,          /* max packet size */
    USB_CFG_VENDOR_ID,  /* 2 bytes */
    0x99, 0x0a,  /* 2 bytes */
    USB_CFG_DEVICE_VERSION, /* 2 bytes */
#if USB_CFG_VENDOR_NAME_LEN
    1,          /* manufacturer string index */
#else
    0,          /* manufacturer string index */
#endif
#if USB_CFG_DEVICE_NAME_LEN
    2,          /* product string index */
#else
    0,          /* product string index */
#endif
#if USB_CFG_SERIAL_NUMBER_LENGTH
    3,          /* serial number string index */
#else
    0,          /* serial number string index */
#endif
    1,          /* number of configurations */
};



Gamepad NesGamepad = {
	.num_reports			=	1,
	.reportDescriptorSize	=	sizeof(nes_usbHidReportDescriptor),
	.deviceDescriptorSize	=	sizeof(nes_usbDescrDevice),
	.init					=	nesInit,
	.update					=	nesUpdate,
	.changed				=	nesChanged,
	.buildReport			=	nesBuildReport
};

Gamepad *nesGetGamepad(void)
{
	NesGamepad.reportDescriptor = (void*)nes_usbHidReportDescriptor;
	NesGamepad.deviceDescriptor = (void*)nes_usbDescrDevice;
	
	return &NesGamepad;
}

