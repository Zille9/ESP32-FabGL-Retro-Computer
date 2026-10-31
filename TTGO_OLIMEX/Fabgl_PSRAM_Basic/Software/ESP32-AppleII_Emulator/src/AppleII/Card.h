/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : Card.h
 *  Module : Interface for a peripheral card in slots 1-7. The
 *           device routes each slot's 16 I/O addresses to Io(), and
 *           Memory maps the card's 256-byte ROM at $Cn00. The IIc's
 *           built-in ports are meant to become cards in fixed slots.
 * ============================================================
*/

#ifndef CARD_H
#define CARD_H

#include "Predef.h"

class Card
{
public:
	virtual ~Card() {}

	// $C080 + slot * 16 + reg, reg 0-15. value only means something on a write.
	virtual BYTE Io(int reg, BYTE value, bool write) = 0;

	// 256 bytes mapped at $Cn00, or NULL for an empty socket, which reads as
	// $00 and so is passed over by the autostart ROM's boot scan. Call
	// Memory::Remap() whenever what this returns changes.
	virtual BYTE* SlotRom() { return NULL; }

	// 2K mapped at $C800-$CFFF while this slot holds the expansion ROM, or
	// NULL for a card that has none. A card with one is selected by any
	// access to its own $Cn00 page and dropped by an access to $CFFF.
	virtual BYTE* ExpansionRom() { return NULL; }

	// Controller state back to power-up; inserted media stay.
	virtual void Reset() {}
};

#endif
