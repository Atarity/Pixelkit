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
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "gamepad.h"
#include "leds.h"
#include "snes.h"

#define REPORT_SIZE		4
#define GAMEPAD_BYTES	2

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
static char snesInit(void);
static void snesUpdate(void);


// the most recent bytes we fetched from the controller
static unsigned char last_read_controller_bytes[GAMEPAD_BYTES];
// the most recently reported bytes
static unsigned char last_reported_controller_bytes[GAMEPAD_BYTES];

static char nes_mode = 0;

static char snesInit(void)
{
	unsigned char sreg;
	sreg = SREG;
	cli();
	
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

	snesUpdate();

	SREG = sreg;

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
        9               A
        10              X
        11              L
        12              R
        13              none (always high)
        14              none (always high)
        15              none (always high)
        16              none (always high)
 *
 */

static void snesUpdate(void)
{
	int i;
	unsigned char tmp=0;

	SNES_LATCH_HIGH();
	_delay_us(12);
	SNES_LATCH_LOW();

	for (i=0; i<8; i++)
	{
		_delay_us(6);
		SNES_CLOCK_LOW();
		
		tmp <<= 1;	
		if (!SNES_GET_DATA()) { tmp |= 1; }

		_delay_us(6);
		
		SNES_CLOCK_HIGH();
	}
	last_read_controller_bytes[0] = tmp;
	for (i=0; i<8; i++)
	{
		_delay_us(6);

		SNES_CLOCK_LOW();

		// notice that this is different from above. We
		// want the bits to be in reverse-order
		tmp >>= 1;	
		if (!SNES_GET_DATA()) { tmp |= 0x80; }
		
		_delay_us(6);
		SNES_CLOCK_HIGH();
	}
	last_read_controller_bytes[1] = tmp;

}

static char snesChanged(char id)
{
	static int first = 1;
	if (first) { first = 0;  return 1; }
	
	return memcmp(last_read_controller_bytes, 
					last_reported_controller_bytes, GAMEPAD_BYTES);
}

static char snesBuildReport(unsigned char *reportBuffer, char id)
{
	int x, y;
	unsigned char lrcb1, lrcb2;
	
	if (reportBuffer != NULL)
	{
		lrcb1 = last_read_controller_bytes[0];
		lrcb2 = last_read_controller_bytes[1];

		// When reading an extra byte from a NES controller
		// it returns all 0s. 
		//
		// The last 4 bits from the 2nd snes bytes are thought
		// to be ID bits. They must all be high for a controller. If
		// they are all 0 (What we get from a NES controller, dont
		// forget bits are inverted in snesUpdate()),
		// then we probably have a NES controller.
		if ((lrcb2 & 0xf0) == 0xf0) {
			nes_mode = 1;
		} else {
			nes_mode = 0;
		}

		y = x = 128;
		if (lrcb1&0x1) { x = 255; }
		if (lrcb1&0x2) { x = 0; }
		
		if (lrcb1&0x4) { y = 255; }
		if (lrcb1&0x8) { y = 0; }

		reportBuffer[0]=x;
		reportBuffer[1]=y;

		reportBuffer[2]=0;
		reportBuffer[3]=0;

		if (!nes_mode)
		{			
/*
	Y
	B
	A
	X
	L
	R
	-
	-
	St
	Sel
			reportBuffer[2] =	(lrcb1&0x80)>>7;
			reportBuffer[2] |=	(lrcb1&0x40)>>5;
			reportBuffer[2] |=	(lrcb1&0x20)>>3;
			reportBuffer[2] |=	(lrcb1&0x10)>>1;

			reportBuffer[2] |=	(lrcb2&0x08)<<4;
			reportBuffer[2] |=	(lrcb2&0x04)<<4;
			reportBuffer[2] |=	(lrcb2&0x02)<<4;
			reportBuffer[2] |=	(lrcb2&0x01)<<4;
*/
			reportBuffer[2] |=	(lrcb1&0x40)>>6;
			reportBuffer[2] |=	(lrcb1&0x80)>>6;
			reportBuffer[2] |=	(lrcb2&0x01)<<2;
			reportBuffer[2] |=	(lrcb2&0x02)<<2;

			reportBuffer[2] |=	(lrcb2&0x08)<<2;
			reportBuffer[2] |=	(lrcb2&0x04)<<2;


			reportBuffer[3] |=	(lrcb1&0x10)>>3;
			reportBuffer[3] |=	(lrcb1&0x20)>>5;


		} else 
		{

			reportBuffer[2] |=	(lrcb1&0x80)>>7;
			reportBuffer[2] |=	(lrcb1&0x40)>>5;
			reportBuffer[2] |=	(lrcb1&0x20)>>3;
			reportBuffer[2] |=	(lrcb1&0x10)>>1;
			
		}
	}
	memcpy(last_reported_controller_bytes, 
			last_read_controller_bytes, 
			GAMEPAD_BYTES);	

	return REPORT_SIZE;
}

#include "snes_descriptor.c"

Gamepad SnesGamepad = {
	.num_reports			=	1,
	.reportDescriptorSize	=	sizeof(snes_usbHidReportDescriptor),
	.init					=	snesInit,
	.update					=	snesUpdate,
	.changed				=	snesChanged,
	.buildReport			=	snesBuildReport
};

Gamepad *snesGetGamepad(void)
{
	SnesGamepad.reportDescriptor = (void*)snes_usbHidReportDescriptor;

	return &SnesGamepad;
}

