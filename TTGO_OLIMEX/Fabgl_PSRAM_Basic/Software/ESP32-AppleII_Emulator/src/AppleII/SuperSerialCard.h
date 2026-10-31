/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : SuperSerialCard.h
 *  Module : Apple Super Serial Card (interface). A 6551 ACIA and
 *           the two DIP switch banks, with the card's serial line
 *           wired to the ESP32's USB UART and, optionally, to a
 *           capture file on the SD card. The 2K firmware ROM is
 *           read from /roms and shows at $Cn00 and $C800-$CFFF.
 * ============================================================
*/

#ifndef SUPER_SERIAL_CARD_H
#define SUPER_SERIAL_CARD_H

#include "Card.h"

// Firmware ROM 341-0065 (the "//c compatible" SSC ROM). Not in the repo:
// like the IIe ROMs it has to be supplied by the user in /roms.
#define SSC_ROM      "ssc.rom"
#define SSC_ROM_SIZE 0x0800

// Directory and file name stem of the printer capture files on the SD card.
#define SSC_PRINT_DIR "/printer"

class SuperSerialCard : public Card
{
public:
	SuperSerialCard();
	~SuperSerialCard();

	// Reads the firmware from /roms into a PSRAM buffer, once. False when
	// the file is missing or the wrong size, and then the card must not be
	// installed in a slot: its I/O would answer with no firmware to drive it.
	bool Install();
	// slot 1-7, and whether output is also written to the SD card. Printer
	// mode (slot 1 by default) only changes the auto-linefeed DIP switch.
	void Configure(int slot, bool printerMode, bool capture);
	bool Installed() const { return rom != NULL; }

	BYTE Io(int reg, BYTE value, bool write) override;
	BYTE* SlotRom() override;
	BYTE* ExpansionRom() override;
	void Reset() override;

	// Once per frame from Apple2Machine::Run: moves a byte from the UART
	// into the receive latch and flushes an idle capture buffer.
	void Poll();
	// Writes what the capture buffer holds and closes the file.
	void FlushCapture();

private:
	BYTE* rom;                  // 2K firmware, PSRAM, NULL until Install()
	int   slot;
	bool  printerMode;
	bool  captureToSd;

	// 6551 registers
	BYTE  commandReg;
	BYTE  controlReg;
	BYTE  rxByte;
	bool  rxFull;

	// Printer capture: bytes wait here so that a listing is not one SD write
	// per character. Flushed when full, or after SSC_FLUSH_MS without output.
	static const int CAPTURE_BUF = 512;
	BYTE  captureBuf[CAPTURE_BUF];
	int   captureLen;
	unsigned long lastWriteMs;
	char  capturePath[32];      // "" until the first byte is captured

	void Transmit(BYTE value);
	void CaptureByte(BYTE value);
	bool OpenCaptureFile();
	BYTE DipSw1() const;
	BYTE DipSw2() const;
	BYTE Status();
};

#endif
