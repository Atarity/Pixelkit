/* Sega style multitap to USB  
 * Copyright (C) 2008 Raphaël Assénat
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
#include "segamtap.h"

#define REPORT_SIZE					3

/*********** prototypes *************/
static char segamtapInit(void);
static void segamtapUpdate(void);



static inline unsigned char SAMPLE()
{
	unsigned char res;
	unsigned char c;

	c = PINC;

	res = (c & 0x20) >> 5; // Up/Up/Z
	res |= (c & 0x10) >> 3; // Down/Down/Y
	res |= (c & 0x08) >> 1; // Left/0/X
	res |= (c & 0x04) << 1; // Right/0

	/*  Returned value organized as follows:
	 *
	 *  Bit     Description
	 *   0      Up
	 *   1      Down
	 *   2      Left
	 *   3      Right
	 */

	return res;
}

#define GET_TL()	(PINC & 0x02) 

#define TH_HIGH()	PORTC |= 0x01
#define TH_LOW()	PORTC &= ~0x01

#define TR_HIGH()	PORTB |= 0x20
#define TR_LOW()	PORTB &= ~0x20

static char segamtapInit(void)
{
	unsigned char sreg;

	sreg = SREG;
	cli();

	DDRB |= 0x02; // Bit 1 out
	PORTB &= ~0x02; // 0

	DDRB |= 0x20; // PB5, TR output (handshake out)
	PORTB |= 0x20;

	/* PORTC
	 *
	 * Bit 5 : D0
	 * Bit 4 : D1
	 * Bit 3 : D2
	 * Bit 2 : D3
	 * Bit 1 : TL
	 * Bit 0 : TH
	 *
	 **/

	DDRC |= 1;	// TH output
	PORTC |= 1;
	DDRC &= ~0x3E; // D0-D3 and TL input (handshake in)
	PORTC |= 0x3E;

	SREG = sreg;

	return 0;
}

/* Each group of 3 nibble (on per index) has the following meaning:
 *
 * Nibble 0:
 *  Bit 3: Left
 *  Bit 2: Right
 *  Bit 1: Down
 *  Bit 0: Up
 *
 * Nibble 1:
 *  Bit 3: Start
 *  Bit 2: A
 *  Bit 1: C
 *  Bit 0: B
 *
 * Nibble 2:
 *  Bit 3: Mode
 *  Bit 2: X
 *  Bit 1: Y
 *  Bit 0: Z
 */
static unsigned char last_read_data[12];


static void segamtapUpdate(void)
{
	unsigned char raw_data[18];
	int i, pos, j;

	TH_LOW();
	_delay_us(100);

	for (i=0; i<18; i++) {

		if (i%2) {
			TR_HIGH();
			while (!GET_TL()) {
				//usbPoll();
				/* do nothing */
			}
		}
		else {
			TR_LOW();
			while (GET_TL()) {
				//usbPoll();
				/* do nothing */
			}
		}

		raw_data[i] = SAMPLE();
		raw_data[i] ^= 0xf; // Convert buttons to active-high
	}

	TR_HIGH();
	TH_HIGH();

	/* If the constant bytes are not what we expect, abort. */
	if (raw_data[0] != 0xf)
		return;
	if (raw_data[1] != 0xf)
		return;
	
	/* Raw data structure:
	 * 
	 * 0: Fixed value 0xf
	 * 1: Fixed value 0xf
	 * 2: Port A controller Id
	 * 3: Port B controller Id
	 * 4: Port C controller Id
	 * 5: Port D controller Id
	 * 6-18: Controller data
	 *
	 * 6 Button controller data: 3 nibbles
	 * 3 Button controller data: 2 nibbles
	 * Empty port data: 0 nibbles
	 **/

	/* Using the controller ID nibbles, expand the data
	 * to our 12 byte buffer (equivalent to 6btn controllers only). This
	 * simplifies report building */
	for (i=0, pos=6; i<4; i++) {
		int sz = 0;
		switch(raw_data[i+2])
		{
			case 0xe:	sz = 3; break; // 6 btn
			case 0xf:	sz = 2; break; // 3 btn
		}
		for (j=0; j<sz; j++) {
			last_read_data[i*3+j] = raw_data[pos]; 
			pos++;
		}
	}
}

static unsigned char last_reported_data[12];

static char segamtapChanged(char id)
{
	static int first = 1;
	if (first) { first = 0;  return 1; }

	id--; // first report is 1

	return memcmp(&last_read_data[id*3], &last_reported_data[id*3], 3);
}

static char segamtapBuildReport(unsigned char *reportBuffer, char id)
{
	unsigned char *first_nibble;
	int x=128, y=128;
	unsigned char btns=0;
	
	if (!id)
		return 0;

	first_nibble = &last_read_data[(id-1) * 3];

	memcpy(&last_reported_data[(id-1)*3], first_nibble, 3);

	if (first_nibble[0] & 1) { y = 0; } // up
	if (first_nibble[0] & 2) { y = 255; } // down
	if (first_nibble[0] & 4) { x = 0; } // left
	if (first_nibble[0] & 8) { x = 255; } // right

	
	if (first_nibble[1] & 1) { btns |= 0x02; } // B
	if (first_nibble[1] & 2) { btns |= 0x04; } // C
	if (first_nibble[1] & 4) { btns |= 0x01; } // A
	if (first_nibble[1] & 8) { btns |= 0x08; } // Start

	if (first_nibble[2] & 1) { btns |= 0x40; } // Z
	if (first_nibble[2] & 2) { btns |= 0x20; } // Y
	if (first_nibble[2] & 4) { btns |= 0x10; } // X
	if (first_nibble[2] & 8) { btns |= 0x80; } // Mode

	reportBuffer[0] = id;
	reportBuffer[1] = x;
	reportBuffer[2] = y;
	reportBuffer[3] = btns;

	return 4;
}

#define USBDESCR_DEVICE         1

const char segamtap_usbHidReportDescriptor[] PROGMEM = {

	/* Controller and report_id 1 */
    0x05, 0x01,			// USAGE_PAGE (Generic Desktop)
    0x09, 0x04,			// USAGE (Joystick)
    0xa1, 0x01,			//	COLLECTION (Application)
    0x09, 0x01,			//		USAGE (Pointer)
    0xa1, 0x00,			//		COLLECTION (Physical)
	0x85, 0x01,			//			REPORT_ID (1)
	0x09, 0x30,			//			USAGE (X)
    0x09, 0x31,			//			USAGE (Y)
    0x15, 0x00,			//			LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,	//			LOGICAL_MAXIMUM (255)
    0x75, 0x08,			//			REPORT_SIZE (8)
    0x95, 0x02,			//			REPORT_COUNT (2)
    0x81, 0x02,			//			INPUT (Data,Var,Abs)

    0x05, 0x09,			//			USAGE_PAGE (Button)
    0x19, 1,			//   		USAGE_MINIMUM (Button 1)
    0x29, 8,			//   		USAGE_MAXIMUM (Button 8)
    0x15, 0x00,			//   		LOGICAL_MINIMUM (0)
    0x25, 0x01,			//   		LOGICAL_MAXIMUM (1)
    0x75, 1,			// 			REPORT_SIZE (1)
    0x95, 8,			//			REPORT_COUNT (8)
    0x81, 0x02,			//			INPUT (Data,Var,Abs)
	0xc0,				//		END_COLLECTION
    0xc0,				// END_COLLECTION

	/* Controller and report_id 2 */
    0x05, 0x01,			// USAGE_PAGE (Generic Desktop)
    0x09, 0x04,			// USAGE (Joystick)
    0xa1, 0x01,			//	COLLECTION (Application)
    0x09, 0x01,			//		USAGE (Pointer)
    0xa1, 0x00,			//		COLLECTION (Physical)
	0x85, 0x02,			//			REPORT_ID (2)
	0x09, 0x30,			//			USAGE (X)
    0x09, 0x31,			//			USAGE (Y)
    0x15, 0x00,			//			LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,	//			LOGICAL_MAXIMUM (255)
    0x75, 0x08,			//			REPORT_SIZE (8)
    0x95, 0x02,			//			REPORT_COUNT (2)
    0x81, 0x02,			//			INPUT (Data,Var,Abs)
    0x05, 0x09,			//			USAGE_PAGE (Button)
    0x19, 1,			//   		USAGE_MINIMUM (Button 1)
    0x29, 8,			//   		USAGE_MAXIMUM (Button 8)
    0x15, 0x00,			//   		LOGICAL_MINIMUM (0)
    0x25, 0x01,			//   		LOGICAL_MAXIMUM (1)
    0x75, 1,			// 			REPORT_SIZE (1)
    0x95, 8,			//			REPORT_COUNT (8)
    0x81, 0x02,			//			INPUT (Data,Var,Abs)
	0xc0,				//		END_COLLECTION
    0xc0,				// END_COLLECTION

	/* Controller and report_id 3 */
    0x05, 0x01,			// USAGE_PAGE (Generic Desktop)
    0x09, 0x04,			// USAGE (Joystick)
    0xa1, 0x01,			//	COLLECTION (Application)
    0x09, 0x01,			//		USAGE (Pointer)
    0xa1, 0x00,			//		COLLECTION (Physical)
	0x85, 0x03,			//			REPORT_ID (3)
	0x09, 0x30,			//			USAGE (X)
    0x09, 0x31,			//			USAGE (Y)
    0x15, 0x00,			//			LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,	//			LOGICAL_MAXIMUM (255)
    0x75, 0x08,			//			REPORT_SIZE (8)
    0x95, 0x02,			//			REPORT_COUNT (2)
    0x81, 0x02,			//			INPUT (Data,Var,Abs)
    0x05, 0x09,			//			USAGE_PAGE (Button)
    0x19, 1,			//   		USAGE_MINIMUM (Button 1)
    0x29, 8,			//   		USAGE_MAXIMUM (Button 8)
    0x15, 0x00,			//   		LOGICAL_MINIMUM (0)
    0x25, 0x01,			//   		LOGICAL_MAXIMUM (1)
    0x75, 1,			// 			REPORT_SIZE (1)
    0x95, 8,			//			REPORT_COUNT (8)
    0x81, 0x02,			//			INPUT (Data,Var,Abs)
	0xc0,				//		END_COLLECTION
    0xc0,				// END_COLLECTION

	/* Controller and report_id 4 */
    0x05, 0x01,			// USAGE_PAGE (Generic Desktop)
    0x09, 0x04,			// USAGE (Joystick)
    0xa1, 0x01,			//	COLLECTION (Application)
    0x09, 0x01,			//		USAGE (Pointer)
    0xa1, 0x00,			//		COLLECTION (Physical)
	0x85, 0x04,			//			REPORT_ID (4)
	0x09, 0x30,			//			USAGE (X)
    0x09, 0x31,			//			USAGE (Y)
    0x15, 0x00,			//			LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,	//			LOGICAL_MAXIMUM (255)
    0x75, 0x08,			//			REPORT_SIZE (8)
    0x95, 0x02,			//			REPORT_COUNT (2)
    0x81, 0x02,			//			INPUT (Data,Var,Abs)
    0x05, 0x09,			//			USAGE_PAGE (Button)
    0x19, 1,			//   		USAGE_MINIMUM (Button 1)
    0x29, 8,			//   		USAGE_MAXIMUM (Button 8)
    0x15, 0x00,			//   		LOGICAL_MINIMUM (0)
    0x25, 0x01,			//   		LOGICAL_MAXIMUM (1)
    0x75, 1,			// 			REPORT_SIZE (1)
    0x95, 8,			//			REPORT_COUNT (8)
    0x81, 0x02,			//			INPUT (Data,Var,Abs)
	0xc0,				//		END_COLLECTION
    0xc0,				// END_COLLECTION


};

const char segamtapusbDescrDevice[] PROGMEM = {    /* USB device descriptor */
    18,         /* sizeof(usbDescrDevice): length of descriptor in bytes */
    USBDESCR_DEVICE,    /* descriptor type */
    0x01, 0x01, /* USB version supported */
    USB_CFG_DEVICE_CLASS,
    USB_CFG_DEVICE_SUBCLASS,
    0,          /* protocol */
    8,          /* max packet size */
    USB_CFG_VENDOR_ID,  /* 2 bytes */
    0x9d,0x0a,  /* 2 bytes */ // THIS IS THE SAME PID AS 4NES4SNES. Report descriptor is shared.
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

static Gamepad SegaMtapGamepad = {
	.reportDescriptorSize	=	sizeof(segamtap_usbHidReportDescriptor),
	.deviceDescriptorSize	=	sizeof(segamtapusbDescrDevice),
	.num_reports			=	4,	
	.init					=	segamtapInit,
	.update					=	segamtapUpdate,
	.changed				=	segamtapChanged,
	.buildReport			=	segamtapBuildReport
};

Gamepad *segamtapGetGamepad(void)
{
	SegaMtapGamepad.reportDescriptor = (void*)segamtap_usbHidReportDescriptor;
	SegaMtapGamepad.deviceDescriptor = (void*)segamtapusbDescrDevice;

	return &SegaMtapGamepad;
}

