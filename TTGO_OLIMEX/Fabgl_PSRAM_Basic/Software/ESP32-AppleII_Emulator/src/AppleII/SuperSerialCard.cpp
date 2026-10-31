/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : SuperSerialCard.cpp
 *  Module : Apple Super Serial Card. The 6551 ACIA registers the
 *           card's firmware polls - data, status, command, control -
 *           and the two DIP switch banks it reads its configuration
 *           from. Transmitted bytes go out of the ESP32's USB UART
 *           and, when capture is on, into a text file on the SD
 *           card; received bytes come back the same way.
 *
 *           Register map, slot n at $C080 + n*16 (as on the card):
 *             1  SW1: baud rate and firmware mode
 *             2  SW2: data format, and CTS on bit 0
 *             8  data: read receives, write transmits
 *             9  status: TDRE, RDRF, DCD, DSR, IRQ
 *             A  command   B  control
 * ============================================================
*/

#include "SuperSerialCard.h"
#include "RomLoader.h"
#include "../Tools/Log.h"

// 6551 status register, as the firmware reads it
#define ST_IRQ       0x80
#define ST_DSR       0x40       // set when DSR is *not* asserted
#define ST_DCD       0x20       // set when DCD is *not* asserted
#define ST_TX_EMPTY  0x10
#define ST_RX_FULL   0x08

// SW1 low nibble: the firmware personality. 0 is the Communications
// Interface Card mode, which is what PR#n / IN#n drive on both a printer
// and a terminal, so both of our modes use it.
#define FWMODE_CIC   0x00
// SW1 high nibble: the baud rate the firmware reports. The USB UART runs at
// a fixed 115200 whatever the Apple asks for, so this is only what the card
// claims to be set to: 19200, the fastest a real card's switches can say.
#define BAUD_19200   0x0F

// Bytes wait this long in the capture buffer before an unfinished listing is
// written out anyway.
#define SSC_FLUSH_MS 1000

SuperSerialCard::SuperSerialCard()
{
	DEBUG_PRINTLN("Construct SuperSerialCard");
	rom = NULL;
	slot = 2;
	printerMode = false;
	captureToSd = false;
	captureLen = 0;
	lastWriteMs = 0;
	capturePath[0] = '\0';
	Reset();
}

SuperSerialCard::~SuperSerialCard()
{
	FlushCapture();
	free(rom);
}

// The firmware stays in PSRAM: it is only fetched while the Apple is inside
// the card's driver, and internal RAM is scarce on the IIe.
bool SuperSerialCard::Install()
{
	if (rom)
		return true;

	BYTE* buf = (BYTE*)ps_malloc(SSC_ROM_SIZE);
	if (!buf)
	{
		LOGLN("[ssc] no memory for the firmware ROM");
		return false;
	}
	if (RomLoader::Load(SSC_ROM, buf, SSC_ROM_SIZE) != ROM_OK)
	{
		free(buf);
		return false;
	}
	rom = buf;
	return true;
}

void SuperSerialCard::Configure(int newSlot, bool printer, bool capture)
{
	if (captureToSd && !capture)
		FlushCapture();
	slot = newSlot;
	printerMode = printer;
	captureToSd = capture;
}

// $Cn00: the last page of the firmware, which is where the card's entry
// points and Pascal 1.1 signature bytes live.
BYTE* SuperSerialCard::SlotRom()
{
	return rom ? rom + SSC_ROM_SIZE - 0x100 : NULL;
}

// $C800-$CFFF: the whole 2K image. The $Cn00 page above is its last page,
// mapped twice, exactly as the card's single ROM chip is addressed.
BYTE* SuperSerialCard::ExpansionRom()
{
	return rom;
}

void SuperSerialCard::Reset()
{
	// 6551 power-up: the command register clears (DTR low, interrupts off)
	// and the control register keeps whatever the firmware last programmed.
	commandReg = 0;
	controlReg = 0;
	rxByte = 0;
	rxFull = false;
	FlushCapture();
}

BYTE SuperSerialCard::Io(int reg, BYTE value, bool write)
{
	switch (reg)
	{
		case 0x1:
			return DipSw1();

		case 0x2:
			return DipSw2();

		// data register
		case 0x8:
			if (write)
			{
				Transmit(value);
				return 0;
			}
			rxFull = false;
			return rxByte;

		// status: a write is the 6551's programmed reset
		case 0x9:
			if (write)
			{
				commandReg &= 0xE0;
				rxFull = false;
				return 0;
			}
			return Status();

		case 0xA:
			if (write)
				commandReg = value;
			return commandReg;

		case 0xB:
			if (write)
				controlReg = value;
			return controlReg;
	}
	return 0;
}

BYTE SuperSerialCard::DipSw1() const
{
	return (BAUD_19200 << 4) | FWMODE_CIC;
}

// 7 data bits, no parity, one stop bit: the format that puts plain ASCII on
// the wire, since the firmware masks the Apple's high bit to fit it. In
// printer mode the card also adds a linefeed to each carriage return.
BYTE SuperSerialCard::DipSw2() const
{
	BYTE stopBits  = 0;                  // 1 = two stop bits
	BYTE sevenBits = 1;
	BYTE parityOdd = 0, parityOn = 0;
	BYTE noLinefeed = printerMode ? 0 : 1;
	BYTE cts = 0;                        // 0 = clear to send: the USB UART always is
	return (stopBits << 7) | (sevenBits << 5) | (parityOdd << 3) |
	       (parityOn << 2) | (noLinefeed << 1) | cts;
}

// Transmission is instant - there is no shift register to wait for - so TDRE
// always reads set. DSR and DCD read as asserted, which is 0 in this register.
BYTE SuperSerialCard::Status()
{
	return ST_TX_EMPTY | (rxFull ? ST_RX_FULL : 0);
}

void SuperSerialCard::Transmit(BYTE value)
{
	Serial.write(value);
	if (captureToSd)
		CaptureByte(value);
}

// The capture file is meant to be readable text, so the Apple's high bit
// comes off here even though the byte goes out of the UART as it was sent.
void SuperSerialCard::CaptureByte(BYTE value)
{
	captureBuf[captureLen++] = value & 0x7F;
	lastWriteMs = millis();
	if (captureLen == CAPTURE_BUF)
		FlushCapture();
}

void SuperSerialCard::Poll()
{
	if (!rxFull)
	{
		int c = Serial.read();
		if (c >= 0)
		{
			rxByte = (BYTE)c;
			rxFull = true;
		}
	}

	// a listing that has stopped coming should not sit in RAM until the next
	// one fills the buffer
	if (captureLen && millis() - lastWriteMs > SSC_FLUSH_MS)
		FlushCapture();
}

// One file per power-up, numbered so that nothing already on the card is
// overwritten. Created on the first captured byte, not before.
bool SuperSerialCard::OpenCaptureFile()
{
	if (capturePath[0])
		return true;

	if (!SD.exists(SSC_PRINT_DIR))
		SD.mkdir(SSC_PRINT_DIR);

	for (int i = 0; i < 1000; i++)
	{
		char path[32];
		snprintf(path, sizeof(path), "%s/print-%03d.txt", SSC_PRINT_DIR, i);
		if (!SD.exists(path))
		{
			strcpy(capturePath, path);
			LOGF("[ssc] capturing to %s\n", capturePath);
			return true;
		}
	}
	LOGLN("[ssc] no free capture file name");
	return false;
}

void SuperSerialCard::FlushCapture()
{
	if (!captureLen)
		return;
	int len = captureLen;
	captureLen = 0;

	if (!captureToSd || !OpenCaptureFile())
		return;

	File f = SD.open(capturePath, FILE_APPEND);
	if (!f)
	{
		LOGF("[ssc] cannot write %s\n", capturePath);
		return;
	}
	f.write(captureBuf, len);
	f.close();
}
