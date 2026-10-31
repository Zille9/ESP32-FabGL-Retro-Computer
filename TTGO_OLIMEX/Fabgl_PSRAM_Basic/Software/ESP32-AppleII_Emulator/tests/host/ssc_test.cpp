/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : ssc_test.cpp
 *  Module : Host-side Super Serial Card test. Drives the real
 *           src/AppleII/SuperSerialCard.cpp through the registers
 *           the card's firmware polls - DIP switches, data, status,
 *           command and control - against the UART and SD card
 *           stand-ins in tests/host/shim.
 *           Driven by tests/host/run-ssc-tests.sh.
 * ============================================================
*/

#include <cstdio>
#include <string>

#include "SuperSerialCard.h"

// register numbers, as Apple2Device::SoftSwitch passes them in
#define R_SW1     0x1
#define R_SW2     0x2
#define R_DATA    0x8
#define R_STATUS  0x9
#define R_COMMAND 0xA
#define R_CONTROL 0xB

#define ST_TX_EMPTY 0x10
#define ST_RX_FULL  0x08
#define ST_DSR      0x40
#define ST_DCD      0x20

static int failures = 0;
static int checks = 0;

static void expect(bool ok, const std::string& what)
{
	checks++;
	if (!ok)
	{
		printf("  FAIL %s\n", what.c_str());
		failures++;
	}
}

static BYTE Read(SuperSerialCard& card, int reg)
{
	return card.Io(reg, 0, false);
}

static void Write(SuperSerialCard& card, int reg, BYTE value)
{
	card.Io(reg, value, true);
}

static void Reset()
{
	HostSerialTx().clear();
	HostSerialRx().clear();
	HostFiles().clear();
	HostMillis() = 0;
}

//////////////////////////////////////////////////////////////////////////////// tests

// The firmware reads its configuration from the two switch banks: baud rate
// and personality in SW1, data format and CTS in SW2.
static void TestDipSwitches()
{
	SuperSerialCard card;

	card.Configure(2, false, false);
	BYTE sw1 = Read(card, R_SW1);
	expect((sw1 >> 4) == 0x0F, "SW1 reports the top baud rate a real card can be set to");
	expect((sw1 & 0x0F) == 0x00, "SW1 selects the communications firmware");

	BYTE sw2 = Read(card, R_SW2);
	expect((sw2 & 0x80) == 0, "SW2: one stop bit");
	expect((sw2 & 0x20) != 0, "SW2: seven data bits");
	expect((sw2 & 0x0C) == 0, "SW2: no parity");
	expect((sw2 & 0x01) == 0, "SW2: clear to send");
	expect((sw2 & 0x02) != 0, "SW2 in a modem slot: no added linefeed");

	// slot 1 is the printer slot, where a carriage return also feeds a line
	card.Configure(1, true, false);
	expect((Read(card, R_SW2) & 0x02) == 0, "SW2 in the printer slot: linefeed after return");
}

// Transmission is instant, so the firmware never waits on TDRE; a byte only
// shows in RDRF once Poll() has taken it out of the UART.
static void TestStatusAndData()
{
	Reset();
	SuperSerialCard card;
	card.Configure(2, false, false);

	BYTE status = Read(card, R_STATUS);
	expect((status & ST_TX_EMPTY) != 0, "transmit register always reads empty");
	expect((status & ST_RX_FULL) == 0, "nothing received yet");
	expect((status & (ST_DSR | ST_DCD)) == 0, "DSR and DCD read as connected");

	// nothing at the other end yet
	card.Poll();
	expect((Read(card, R_STATUS) & ST_RX_FULL) == 0, "an empty UART leaves RDRF clear");

	HostSerialRx().push_back('A');
	HostSerialRx().push_back('B');
	card.Poll();
	expect((Read(card, R_STATUS) & ST_RX_FULL) != 0, "RDRF set after a byte arrives");
	expect(Read(card, R_DATA) == 'A', "the data register hands over the byte");
	expect((Read(card, R_STATUS) & ST_RX_FULL) == 0, "reading the data register clears RDRF");

	// the second byte waits in the UART until the card takes it
	expect(HostSerialRx().size() == 1, "only one byte is latched per Poll");
	card.Poll();
	expect(Read(card, R_DATA) == 'B', "the next Poll brings the second byte");

	Write(card, R_DATA, 'H');
	Write(card, R_DATA, 'I');
	expect(HostSerialTx() == "HI", "written bytes go out of the UART");
}

// Command and control are plain read/write registers; a write to the status
// register is the 6551's programmed reset.
static void TestCommandControl()
{
	Reset();
	SuperSerialCard card;
	card.Configure(2, false, false);

	Write(card, R_COMMAND, 0x1F);
	expect(Read(card, R_COMMAND) == 0x1F, "the command register reads back");
	Write(card, R_CONTROL, 0x1E);
	expect(Read(card, R_CONTROL) == 0x1E, "the control register reads back");

	HostSerialRx().push_back('X');
	card.Poll();
	Write(card, R_STATUS, 0);
	expect(Read(card, R_COMMAND) == 0x00, "a status write clears the low command bits");
	expect(Read(card, R_CONTROL) == 0x1E, "a status write leaves the control register alone");
	expect((Read(card, R_STATUS) & ST_RX_FULL) == 0, "a status write drops the received byte");
}

// With capture on, what the card sends is also written to the SD card: when
// the buffer fills, when nothing more has come for a second, and never
// before the first byte, so an idle card leaves no files behind.
static void TestPrinterCapture()
{
	Reset();
	SuperSerialCard card;
	card.Configure(1, true, true);

	const char* line = "10 HOME\r";
	for (const char* p = line; *p; p++)
		Write(card, R_DATA, (BYTE)*p);

	expect(HostFiles().empty(), "nothing is written to the card byte by byte");

	// still inside the idle time: the listing has not stopped yet
	HostMillis() = 500;
	card.Poll();
	expect(HostFiles().empty(), "a listing still coming is not flushed");

	HostMillis() = 2000;
	card.Poll();
	expect(HostFiles().size() == 1, "the idle listing is written out");
	if (!HostFiles().empty())
	{
		const std::string& path = HostFiles().begin()->first;
		const std::string& text = HostFiles().begin()->second;
		expect(path == "/printer/print-000.txt", "the first capture file is print-000.txt");
		expect(text == line, "the captured text is what was sent");
	}

	// a full buffer does not wait for the card to go idle
	for (int i = 0; i < 600; i++)
		Write(card, R_DATA, 'X');
	expect(HostFiles().begin()->second.size() > strlen(line), "a full buffer is written at once");

	// the high bit the Apple sets on its characters is not left in the file
	Reset();
	SuperSerialCard hiBit;
	hiBit.Configure(1, true, true);
	Write(hiBit, R_DATA, 0xC1);              // 'A' as the Apple holds it
	hiBit.FlushCapture();
	expect(HostFiles().begin()->second == "A", "the capture file holds plain ASCII");
	expect(HostSerialTx() == std::string(1, (char)0xC1), "the UART gets the byte as it was sent");

	// with capture off nothing reaches the card at all
	Reset();
	SuperSerialCard quiet;
	quiet.Configure(2, false, false);
	Write(quiet, R_DATA, 'Z');
	quiet.FlushCapture();
	expect(HostFiles().empty(), "capture off writes no file");
	expect(HostSerialTx() == "Z", "capture off still sends over the UART");
}

// Without its firmware the card has nothing to map at $Cn00 or $C800: the
// machine must not install it, and the slot stays as empty as before.
static void TestNoFirmware()
{
	Reset();
	SuperSerialCard card;
	expect(!card.Installed(), "a fresh card has no firmware");
	expect(card.SlotRom() == NULL, "no $Cn00 page without firmware");
	expect(card.ExpansionRom() == NULL, "no $C800 image without firmware");
	expect(!card.Install(), "installing without /roms/ssc.rom fails");
}

int main()
{
	printf("Super Serial Card\n");
	TestDipSwitches();
	TestStatusAndData();
	TestCommandControl();
	TestPrinterCapture();
	TestNoFirmware();

	printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS", checks, failures);
	return failures ? 1 : 0;
}
