# Name: Makefile
# Project: HIDKeys
# Author: Christian Starkjohann
# Creation Date: 2006-02-02
# Tabsize: 4
# Copyright: (c) 2006 by OBJECTIVE DEVELOPMENT Software GmbH
# License: Proprietary, free under certain conditions. See Documentation.

############################################################################## 
# Fuse values for particular devices 
############################################################################## 
# If your device is not listed here, go to 
# http://palmavr.sourceforge.net/cgi-bin/fc.cgi 
# and choose options for external crystal clock and no clock divider 
# 
################################## ATMega8 ################################## 
# ATMega8 FUSE_L (Fuse low byte): 
# 0x9f = 1 0 0 1 1 1 1 1 
# ^ ^ \ / \--+--/ 
# | | | +------- CKSEL 3..0 (external >8M crystal) 
# | | +--------------- SUT 1..0 (crystal osc, BOD enabled) 
# | +------------------ BODEN (BrownOut Detector enabled) 
# +-------------------- BODLEVEL (2.7V) 
# ATMega8 FUSE_H (Fuse high byte): 
# 0xc9 = 1 1 0 0 1 0 0 1 <-- BOOTRST (boot reset vector at 0x0000) 
# ^ ^ ^ ^ ^ ^ ^------ BOOTSZ0 
# | | | | | +-------- BOOTSZ1 
# | | | | + --------- EESAVE (don't preserve EEPROM over chip erase) 
# | | | +-------------- CKOPT (full output swing) 
# | | +---------------- SPIEN (allow serial programming) 
# | +------------------ WDTON (WDT not always on) 
# +-------------------- RSTDISBL (reset pin is enabled) 
# 
############################## ATMega48/88/168 ############################## 
# ATMega*8 FUSE_L (Fuse low byte): 
# 0xdf = 1 1 0 1 1 1 1 1 
# ^ ^ \ / \--+--/ 
# | | | +------- CKSEL 3..0 (external >8M crystal) 
# | | +--------------- SUT 1..0 (crystal osc, BOD enabled) 
# | +------------------ CKOUT (if 0: Clock output enabled) 
# +-------------------- CKDIV8 (if 0: divide by 8) 
# ATMega*8 FUSE_H (Fuse high byte): 
# 0xde = 1 1 0 1 1 1 1 0 
# ^ ^ ^ ^ ^ \-+-/ 
# | | | | | +------ BODLEVEL 0..2 (110 = 1.8 V) 
# | | | | + --------- EESAVE (preserve EEPROM over chip erase) 
# | | | +-------------- WDTON (if 0: watchdog always on) 
# | | +---------------- SPIEN (allow serial programming) 
# | +------------------ DWEN (debug wire enable) 
# +-------------------- RSTDISBL (reset pin is enabled) 
# 
############################## ATTiny25/45/85 ############################### 
# ATMega*5 FUSE_L (Fuse low byte): 
# 0xef = 1 1 1 0 1 1 1 1 
# ^ ^ \+/ \--+--/ 
# | | | +------- CKSEL 3..0 (clock selection -> crystal @ 12 MHz) 
# | | +--------------- SUT 1..0 (BOD enabled, fast rising power) 
# | +------------------ CKOUT (clock output on CKOUT pin -> disabled) 
# +-------------------- CKDIV8 (divide clock by 8 -> don't divide) 
# ATMega*5 FUSE_H (Fuse high byte): 
# 0xdd = 1 1 0 1 1 1 0 1 
# ^ ^ ^ ^ ^ \-+-/ 
# | | | | | +------ BODLEVEL 2..0 (brownout trigger level -> 2.7V) 
# | | | | +---------- EESAVE (preserve EEPROM on Chip Erase -> not preserved) 
# | | | +-------------- WDTON (watchdog timer always on -> disable) 
# | | +---------------- SPIEN (enable serial programming -> enabled) 
# | +------------------ DWEN (debug wire enable) 
# +-------------------- RSTDISBL (disable external reset -> enabled) 
# 
################################ ATTiny2313 ################################# 
# ATTiny2313 FUSE_L (Fuse low byte): 
# 0xef = 1 1 1 0 1 1 1 1 
# ^ ^ \+/ \--+--/ 
# | | | +------- CKSEL 3..0 (clock selection -> crystal @ 12 MHz) 
# | | +--------------- SUT 1..0 (BOD enabled, fast rising power) 
# | +------------------ CKOUT (clock output on CKOUT pin -> disabled) 
# +-------------------- CKDIV8 (divide clock by 8 -> don't divide) 
# ATTiny2313 FUSE_H (Fuse high byte): 
# 0xdb = 1 1 0 1 1 0 1 1 
# ^ ^ ^ ^ \-+-/ ^ 
# | | | | | +---- RSTDISBL (disable external reset -> enabled) 
# | | | | +-------- BODLEVEL 2..0 (brownout trigger level -> 2.7V) 
# | | | +-------------- WDTON (watchdog timer always on -> disable) 
# | | +---------------- SPIEN (enable serial programming -> enabled) 
# | +------------------ EESAVE (preserve EEPROM on Chip Erase -> not preserved) 
# +-------------------- DWEN (debug wire enable) 

UISP = uisp -dprog=stk500 -dpart=atmega8 -dserial=/dev/ttyS1
COMPILE = avr-gcc -Wall -Os -Iusbdrv -I. -mmcu=atmega8 -DF_CPU=12000000L #-DDEBUG_LEVEL=1
COMMON_OBJS = usbdrv/usbdrv.o usbdrv/usbdrvasm.o usbdrv/oddebug.o main.o
HEXFILE=main.hex

OBJS = $(COMMON_OBJS) snes.o snesmouse.o nes.o db9.o devdesc.o tg16.o segamtap.o


# symbolic targets:
all:	$(HEXFILE)

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

.c.s:
	$(COMPILE) -S $< -o $@


clean:
	rm -f $(HEXFILE) main.lst main.obj main.cof main.list main.map main.eep.hex main.bin *.o usbdrv/*.o main.s usbdrv/oddebug.s usbdrv/usbdrv.s

# file targets:
main.bin:	$(OBJS)	snes.o nes.o snesmouse.o db9.o tg16.o devdesc.o 
	$(COMPILE) -o main.bin $(OBJS) -Wl,-Map=main.map

$(HEXFILE):	main.bin
	rm -f $(HEXFILE) main.eep.hex
	avr-objcopy -j .text -j .data -O ihex main.bin $(HEXFILE)
	./checksize main.bin

flash:	all
	$(UISP) --erase --upload --verify if=$(HEXFILE)

flash_usb:
	sudo avrdude -p m8 -P usb -c avrispmkII -Uflash:w:$(HEXFILE) -B 1.0

# Fuse high byte:
# 0xc9 = 1 1 0 0   1 0 0 1 <-- BOOTRST (boot reset vector at 0x0000)
#        ^ ^ ^ ^   ^ ^ ^------ BOOTSZ0
#        | | | |   | +-------- BOOTSZ1
#        | | | |   + --------- EESAVE (don't preserve EEPROM over chip erase)
#        | | | +-------------- CKOPT (full output swing)
#        | | +---------------- SPIEN (allow serial programming)
#        | +------------------ WDTON (WDT not always on)
#        +-------------------- RSTDISBL (reset pin is enabled)
# Fuse low byte:
# 0x9f = 1 0 0 1   1 1 1 1
#        ^ ^ \ /   \--+--/
#        | |  |       +------- CKSEL 3..0 (external >8M crystal)
#        | |  +--------------- SUT 1..0 (crystal osc, BOD enabled)
#        | +------------------ BODEN (BrownOut Detector enabled)
#        +-------------------- BODLEVEL (2.7V)
fuse:
	$(UISP) --wr_fuse_h=0xc9 --wr_fuse_l=0x9f

fuse_usb:
	sudo avrdude -p m8 -P usb -c avrispmkII -Uhfuse:w:0xc9:m -Ulfuse:w:0x9f:m -B 10.0

