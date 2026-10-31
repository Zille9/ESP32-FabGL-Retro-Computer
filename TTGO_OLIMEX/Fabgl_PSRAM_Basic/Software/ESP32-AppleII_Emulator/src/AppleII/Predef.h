/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : Predef.h
 *  Module : Shared compile-time definitions: memory map constants
 *           (RAMSIZE, ROMSTART, SL6START, Language Card regions),
 *           screen and font geometry, disk image sizes and common
 *           includes used across the emulation core.
 * ============================================================
*/

#ifndef PREDEF_H
#define PREDEF_H


/*
	MOS 6502 CPU Emulator
*/

#include <stdio.h>
#include <memory.h>
#include <Arduino.h>
#include <SD.h>

#ifndef ps_malloc
#define ps_malloc(size) malloc(size)
#endif

#define SBYTE		signed char
#define BYTE		unsigned char
#define WORD		unsigned short
#define MEMORY_SIZE		1024 * 64	// 64k

#define RAMSIZE			0xC000
#define ROMSIZE			0x3000
#define ROMSTART		0xD000
#define SL6START		0xC600
#define SL6SIZE			0x0100

#define LGCSTART 0xD000
#define LGCSIZE  0x3000
#define BK2START 0xD000
#define BK2SIZE  0x1000

#define FONT_X				7
#define FONT_Y				8
#define FONT_NUM			128
#define FONT_NORMAL			0
#define FONT_INVERSE		1
#define FONT_FLASH			2

#define SCREENTEXT_X	40
#define SCREENTEXT_Y	24

#define SCREENSIZE_X	280
#define SCREENSIZE_Y	192

#define TARGET_FRAME	60

#define TEXT_MODE		0
#define LORES_MODE		1
#define LORES_MIX_MODE	2
#define HIRES_MODE		3
#define HIRES_MIX_MODE	4

#define AUDIOBUFFERSIZE		4096

#define DISKSIZE	232960

#define GAMEPAD_LEFT	0
#define GAMEPAD_RIGHT	1
#define GAMEPAD_UP		2
#define GAMEPAD_DOWN	3

struct AppleColor
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	//unsigned char a;

	AppleColor(unsigned char _r, unsigned char _g, unsigned char _b)
	{
		r = _r;
		g = _g;
		b = _b;
		//a = 0xFF;
	}

	int GetColor()
	{
		int color = (r << 24 & 0xF000) | (b << 16 & 0xF00) |(g << 8 & 0xF0) | 0x0F;
		return color;
	}
};


#endif