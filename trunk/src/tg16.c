/* TG16 to USB
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
#include "tg16.h"

#define REPORT_SIZE		3
#define GAMEPAD_BYTES	3

/* for chaning IO easily */


#define SET_SELECT()	PORTC |= 0x02
#define CLR_SELECT()	PORTC &= ~0x02

/* Active low */
#define SET_OE()		PORTC &= ~0x01
#define CLR_OE()		PORTC |= 0x01


/*********** prototypes *************/
static char tg16_Init(void);
static void tg16_Update(void);

// the most recent bytes we fetched from the controller
static unsigned char last_read_controller_bytes[REPORT_SIZE];
// the most recently reported bytes
static unsigned char last_reported_controller_bytes[REPORT_SIZE];

static unsigned char readController()
{
	unsigned char a,b;

	/* 
	 * --- tg16 specific pinout ---
	 *  inputs...
	 * PC5: /Up or /I
	 * PC4: /Right or /II
	 * PC3: /South or /Select
	 * PC2: /Left or /Run
	 *
	 *  outputs...
	 * PC1: Data select
	 * PC0: /OE
	 */

	SET_OE();
	_delay_ms(1);

	SET_SELECT();
	_delay_ms(1);
	a = (PINC & 0x3c) >> 2;

	CLR_SELECT();
	_delay_ms(1);		
	b = (PINC & 0x3c) >> 2;

	CLR_OE();

	return a | (b<<4);
}

static char tg16_Init(void)
{
	unsigned char sreg;
	sreg = SREG;
	cli();

	DDRB |= 0x02; // Bit 1 out
	PORTB &= ~0x02; // 0

	/* 
	 * --- tg16 specific pinout ---
	 *  inputs...
	 * PC5: /Up or /I
	 * PC4: /Right or /II
	 * PC3: /South or /Select
	 * PC2: /Left or /Run
	 *
	 *  outputs...
	 * PC1: Data select
	 * PC0: /OE
	 */

	DDRC &= ~0x3c; 
	PORTC |= 0x3c;

	/* disable OE and init DataSelect to 1 */
	PORTC |= 0x03;
	DDRC |= 0x03;
	
	tg16_Update();

	SREG = sreg;

	return 0;
}



static void tg16_Update(void)
{
	unsigned char data;
	int x=128,y=128;
	
	/* 7: I
	 * 6: II
	 * 5: Select
	 * 4: Run
	 *
	 * 3: Up
	 * 2: Right
	 * 1: Down
	 * 0: Left
	 */
	data = readController();

	/* Buttons are active low. Invert values. */
	data ^= 0xff;

	if (data & 0x08) { y = 0; } // up
	if (data & 0x02) { y = 255; } //down
	if (data & 0x01) { x = 0; }  // left
	if (data & 0x04) { x = 255; } // right

	last_read_controller_bytes[0]=x;
	last_read_controller_bytes[1]=y;
 
 	last_read_controller_bytes[2]=0;

	if (data & 0x80) 
		last_read_controller_bytes[2] |= 0x01;
	if (data & 0x40)
		last_read_controller_bytes[2] |= 0x02;
	if (data & 0x20)
		last_read_controller_bytes[2] |= 0x04;
	if (data & 0x10)
		last_read_controller_bytes[2] |= 0x08;

}	

static char tg16_Changed(char id)
{
	static int first = 1;
	if (first) { first = 0;  return 1; }
	
	return memcmp(last_read_controller_bytes, 
					last_reported_controller_bytes, GAMEPAD_BYTES);
}

static char tg16_BuildReport(unsigned char *reportBuffer, char id)
{
	if (reportBuffer != NULL)
	{
		memcpy(reportBuffer, last_read_controller_bytes, GAMEPAD_BYTES);
	}
	memcpy(last_reported_controller_bytes, 
			last_read_controller_bytes, 
			GAMEPAD_BYTES);	

	return REPORT_SIZE;
}

#include "snes_descriptor.c"

Gamepad tg16_Gamepad = {
	.num_reports			=	1,
	.reportDescriptorSize	=	sizeof(snes_usbHidReportDescriptor),
	.init					=	tg16_Init,
	.update					=	tg16_Update,
	.changed				=	tg16_Changed,
	.buildReport			=	tg16_BuildReport
};

Gamepad *tg16_GetGamepad(void)
{
	tg16_Gamepad.reportDescriptor = (void*)snes_usbHidReportDescriptor;

	return &tg16_Gamepad;
}

