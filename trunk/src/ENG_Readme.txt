This is nes_snes_db9_usb Readme.txt file. 

Table of contents:

1) What is nes_snes_db9_usb?
2) USB Implementation
3) Compilation and installation
4) License
5) About the vendor id/product id pair:
6) Where do I get more information and updates?


1) What is nes_snes_db9_usb?
   -------------------------
	nes_snes_db9_usb if a firmware for Atmel ATmega8 which 
	allows one to connect supported controllers to a PC via an USB
	port as a standard HID device.
	
	Supported controllers:
		- Nintendo Entertainment System (NES)
		- Super Nintendo Entertainment System (SNES)
		- Super Nintendo Mouse,
		- Genesis 3/6 buttons controllers
		- 2 Button atari-style controllers (such as Sega master system)
		- 1 Button atari-style controllers
		- Sega multi-tap MK-1654

2) USB Implementation
   ------------------
	nes_snes_db9_usb uses the software-only usb driver from Objective Development.
	See http://www.obdev.at/products/avrusb/index.html

	A good portion of the main.c code was based on Objective Development's
	HIDKeys example.


3) Compilation and installation
   ----------------------------
	First, you must compile it. To compile, you need a working avr-gcc and
	avr-libc. Under linux or cygwin, simply type make in the project directory.
	(assuming avr-gcc is in your path). 

	Next, you must upload the generated file (main.hex) to the Atmega8 using
	whatever tools you like. Personally, I use uisp. The 'flash' and 'fuse'
	targets in the makefile is a good example about how to use it. 

	The Atmega fuse bits must also be set properly. The idea behind this is to
	enable the external 12mhz crystal instead of the internal clock. Check the
	makefile for good fuse bytes values.


4) License
   -------
	nes_snes_db9_usb is released under Objective Development's extended GPL
	license. See License.txt


5) About the vendor id/product id pair:
   ------------------------------------
	Please dont re-use them for other projects. Instead, get your own. 
	I got mine from mecanique:
	http://www.mecanique.co.uk/products/usb/pid.html


6) Where do I get more information and updates?
   --------------------------------------------
	Visit my nes_snes_db9_usb page:
	http://www.raphnet.net/electronique/snes_nes_usb/index_en.php
	you may also contact me by email:
	Raphael Assenat <raph@raphnet.net>
	

