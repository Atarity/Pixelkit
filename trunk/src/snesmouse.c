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
#include <avr/interrupt.h>
#include <string.h>
#include "usbconfig.h"
#include "gamepad.h"
#include "leds.h"
#include "snesmouse.h"

#define REPORT_SIZE					3
#define SNESMOUSE_GAMEPAD_BYTES		4

/******** IO port definitions **************/
#define SNES_LATCH_DDR	DDRC
#define SNES_LATCH_PORT	PORTC
#define SNES_LATCH_BIT	(1<<4)

#define SNES_CLOCK_DDR	DDRC
#define SNES_CLOCK_PORT	PORTC
#define SNES_CLOCK_BIT	(1<<5)

#define SNES_DATA_PORT	PORTC
#define SNES_DATA_DDR	DDRC
#define SNES_DATA_PIN	PINC
#define SNES_DATA_BIT	(1<<3)

/********* IO port manipulation macros **********/
#define SNES_LATCH_LOW()	do { SNES_LATCH_PORT &= ~(SNES_LATCH_BIT); } while(0)
#define SNES_LATCH_HIGH()	do { SNES_LATCH_PORT |= SNES_LATCH_BIT; } while(0)
#define SNES_CLOCK_LOW()	do { SNES_CLOCK_PORT &= ~(SNES_CLOCK_BIT); } while(0)
#define SNES_CLOCK_HIGH()	do { SNES_CLOCK_PORT |= SNES_CLOCK_BIT; } while(0)

#define SNES_GET_DATA()	(SNES_DATA_PIN & SNES_DATA_BIT)

/*********** prototypes *************/
static char snesmouseInit(void);
static void snesmouseUpdate(void);
static void updatebuttons(unsigned char *dst);


// the most recent bytes we fetched from the controller
static unsigned char last_read_button_byte;

// the most recently reported bytes
static unsigned char last_reported_button_byte;

static int mousespeed=0;
static int mousepresent=0;

static int motion_x=0, motion_y=0;
static int last_reported_motion_x, last_reported_motion_y;

void snesmouse_setSpeed(int speed)
{
	unsigned char btn;
	int retries = 6;
	int i;
	
	speed &= 0x3;
	
	do	
	{
		updatebuttons(&btn);
	
		if ((btn & 0x30)>>4 == speed)
		{
			mousespeed = speed;
			// ok
			break;
		}

		_delay_ms(1);
		
		// do the 31 pulse speed cycling
		for (i=0; i<31; i++)
		{
			SNES_LATCH_HIGH();
			_delay_us(1);
			SNES_CLOCK_LOW();
			SNES_CLOCK_HIGH();
			_delay_us(1);
			SNES_LATCH_LOW();
			_delay_us(12);
		}
		_delay_ms(16);

	}
	while (--retries);


	
}


char isSnesMouse()
{
	unsigned char btn;
	
	updatebuttons(&btn);

	// On an snes mouse, the bits 13-16 seen on the wire should be 1-1-1-0. 
	// Note that we invert bits while we read them in snesmouseUpdate, that
	// is why we compare with the value 0x01
	if ((btn & 0x0f)==0x1)
		return 1;
	
	return 0;
}

static char snesmouseInit(void)
{
	// clock and latch as output
	SNES_LATCH_DDR |= SNES_LATCH_BIT;
	SNES_CLOCK_DDR |= SNES_CLOCK_BIT;
	
	// data as input
	SNES_DATA_DDR &= ~(SNES_DATA_BIT);
	// enable pullup. This should prevent random toggling of pins
	// when no controller is connected.
	SNES_DATA_PORT |= SNES_DATA_BIT;

	// clock is normally high
	SNES_CLOCK_PORT |= SNES_CLOCK_BIT;

	// LATCH is Active HIGH
	SNES_LATCH_PORT &= ~(SNES_LATCH_BIT);

	snesmouseUpdate();

	switch ( (last_read_button_byte & 0xc0) >> 6)
	{
		case 3: // two button down
		case 0: // two button up  (DEFAULT speed)
			snesmouse_setSpeed(0);
			break;
		case 1:
			snesmouse_setSpeed(1);
			break;
		case 2:
			snesmouse_setSpeed(2);
	}

	return 0;
}


/*
 *
       Clock Cycle     Button Reported
        ===========     ===============
        1               B
        2               Y
        3               Select
        4               Start
        5               Up on joypad
        6               Down on joypad
        7               Left on joypad
        8               Right on joypad
        9               A					(mouse button)
        10              X					(mouse button)
        11              L
        12              R
        13              none (always high)
        14              none (always high)
        15              none (always high)
        16              none (always high)	(low on mouse)
		
	2.5ms delay. 

	New clock waveform: 
	0.5us low, 8us high

        17              Y direction (0=up, 1=down)
        18              Y motion bit 6
        19              Y motion bit 5
        20              Y motion bit 4
        21              Y motion bit 3
        22              Y motion bit 2
        23              Y motion bit 1
        24              Y motion bit 0
        25              X direction (0=left, 1=right)
        26              X motion bit 6
        27              X motion bit 5
        28              X motion bit 4
        29              X motion bit 3
        30              X motion bit 2
        31              X motion bit 1
        32              X motion bit 0

 *
 */

static void updatebuttons(unsigned char *dst)
{
	char i;
	
	SNES_LATCH_HIGH();
	_delay_us(12);
	SNES_LATCH_LOW();

	// nothing from the 8 bits from the mouse interest us.
	for (i=0; i<8; i++) {
		_delay_us(6);
		SNES_CLOCK_LOW();
		_delay_us(6);
		SNES_CLOCK_HIGH();
	}
	
	for (i=0; i<8; i++)
	{
		_delay_us(6);
		SNES_CLOCK_LOW();
		
		*dst <<= 1;	
		if (!SNES_GET_DATA()) { *dst |= 1; }

		_delay_us(6);
		
		SNES_CLOCK_HIGH();
	}
}

static void snesmouseUpdate(void)
{
	int i;
	unsigned char x=0,y=0;
	char rel_x, rel_y;
	unsigned char btn;

	updatebuttons(&btn);
	
	if ((btn & 0x0f)!=0x1)	{
		// not a mouse? Mouse disconnected?
		mousepresent = 0;
		return;
	}

	if (!mousepresent)
	{
		// The mouse is back!
		switch ( (btn & 0xc0) >> 6)
		{
			case 3: // two button down
				snesmouse_setSpeed(0);
				break;
			case 1:
				snesmouse_setSpeed(1);
				break;
			case 2:
				snesmouse_setSpeed(2);
			case 0: // two button up. Continue using same speed.
				snesmouse_setSpeed(mousespeed);
		}

		// relatch and read buttons
		updatebuttons(&btn);
	
		if ((btn & 0x0f)!=0x1)
		{
			// gone again!?
			mousepresent = 0;
			return;
		}
	}

	mousepresent = 1;
	last_read_button_byte = btn;
	
	/******** NOW THE MOUSE DATA ****/
	_delay_ms(2); // 2.5 ms
	_delay_us(500);

	for (i=0; i<8; i++)
	{
		_delay_us(8); 
		SNES_CLOCK_LOW();
		y <<= 1;	
		if (!SNES_GET_DATA()) { y |= 1; }
		asm volatile("nop; nop; nop; nop; nop;");
		SNES_CLOCK_HIGH();
	}
	for (i=0; i<8; i++)
	{
		_delay_us(8);
		SNES_CLOCK_LOW();
		x <<= 1;	
		if (!SNES_GET_DATA()) { x |= 1; }
		asm volatile("nop; nop; nop; nop; nop;");
		SNES_CLOCK_HIGH();
	}

	/* Get the relative x and y movement from this packet */
	rel_x = (x & 0x7f);
	rel_y = (y & 0x7f);
	if (x&0x80) rel_x = -rel_x;
	if (y&0x80) rel_y = -rel_y;

	/* Add to global counters for next report */
	motion_x += rel_x;
	motion_y += rel_y;
}

static char snesmouseChanged(char id)
{
	static int first = 1;
	if (first) { first = 0;  return 1; }

	if (last_read_button_byte != last_reported_button_byte)
		return 1; // Button change

	if (motion_x != last_reported_motion_x)
		return 1;
	if (motion_y != last_reported_motion_y)
		return 1;

	return 0;
}

static char snesmouseBuildReport(unsigned char *reportBuffer, char id)
{
	char rel_x, rel_y;
	static int first=0;
	
	if (reportBuffer != NULL)
	{
		if (first) {
			reportBuffer[0] = 0;
			reportBuffer[1] = 0;
			reportBuffer[2] = 0;
			first = 0;
		
			return REPORT_SIZE;
		}
		reportBuffer[0]=(last_read_button_byte & 0xc0)>>6;
		
		if (motion_x < -127)
			rel_x = -127; 
		else if (motion_x > 127)
			rel_x = 127;
		else
			rel_x = motion_x;


		if (motion_y < -127)
			rel_y = -127; 
		else if (motion_y > 127)
			rel_y = 127;
		else
			rel_y = motion_y;
		
	
		reportBuffer[1]=rel_x;
		reportBuffer[2]=rel_y;
	
		last_reported_motion_x = rel_x;
		last_reported_motion_y = rel_y;
		last_reported_button_byte = last_read_button_byte;
		motion_x -= rel_x;
		motion_y -= rel_y;
	}

	return REPORT_SIZE;
}

/* Hidtool's mouse report descriptor example, modified for only 2 buttons.
 **/
const char snesmouse_usbHidReportDescriptor[] PROGMEM = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x02,                    //     USAGE_MAXIMUM (Button 2)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x06,                    //     REPORT_SIZE (6)
    0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)
    0xc0,                          //   END_COLLECTION
    0xc0                           // END_COLLECTION
};

#define USBDESCR_DEVICE         1

// This is the same descriptor as in devdesc.c, but the product id is +1 

const char snesmouseusbDescrDevice[] PROGMEM = {    /* USB device descriptor */
    18,         /* sizeof(usbDescrDevice): length of descriptor in bytes */
    USBDESCR_DEVICE,    /* descriptor type */
    0x01, 0x01, /* USB version supported */
    USB_CFG_DEVICE_CLASS,
    USB_CFG_DEVICE_SUBCLASS,
    0,          /* protocol */
    8,          /* max packet size */
    USB_CFG_VENDOR_ID,  /* 2 bytes */
    1+USB_CFG_DEVICE_ID,  /* 2 bytes */
    USB_CFG_DEVICE_VERSION, /* 2 bytes */
#if USB_CFG_VENDOR_NAME_LEN
    1,          /* manufacturer string index */
#else
    0,          /* manufacturer string index */
#endif

	2,          /* product string index */
    3,          /* serial number string index */


    1,          /* number of configurations */
};

Gamepad SnesMouseGamepad = {
	.num_reports			=	1,
	.reportDescriptorSize	=	sizeof(snesmouse_usbHidReportDescriptor),
	.deviceDescriptorSize	=	sizeof(snesmouseusbDescrDevice),
	.init					=	snesmouseInit,
	.update					=	snesmouseUpdate,
	.changed				=	snesmouseChanged,
	.buildReport			=	snesmouseBuildReport
};

Gamepad *snesmouseGetGamepad(void)
{
	SnesMouseGamepad.reportDescriptor = (void*)snesmouse_usbHidReportDescriptor;
	SnesMouseGamepad.deviceDescriptor = (void*)snesmouseusbDescrDevice;

	return &SnesMouseGamepad;
}

