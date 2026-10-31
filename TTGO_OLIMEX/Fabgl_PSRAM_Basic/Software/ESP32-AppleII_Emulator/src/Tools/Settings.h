/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : Settings.h
 *  Module : Persistent user settings in the ESP32 NVS partition,
 *           via the Arduino Preferences library. Holds the selected
 *           machine model, the keyboard layout and the two mounted
 *           disk paths, so that a
 *           machine switch - which restarts the ESP32 - comes back
 *           up as the chosen model with the same disks mounted.
 * ============================================================
*/

#pragma once

#include <Arduino.h>
#include <Preferences.h>

class Settings
{
public:
	// Machine model id (MachineProfile.h). Returns def when nothing has been
	// saved yet; the caller also treats an id it does not know as def.
	static uint8_t LoadMachine(uint8_t def)
	{
		Preferences p;
		if (!p.begin(NS, true))            // read-only open fails on first boot
			return def;
		uint8_t id = p.getUChar("machine", def);
		p.end();
		return id;
	}

	// false when NVS could not be written
	static bool SaveMachine(uint8_t id)
	{
		Preferences p;
		if (!p.begin(NS, false))
			return false;
		bool ok = p.putUChar("machine", id) == 1;
		p.end();
		return ok;
	}

	// PS/2 keyboard layout id (KeyboardLayouts.h). Returns def when nothing
	// has been saved yet.
	static uint8_t LoadKeyboard(uint8_t def)
	{
		Preferences p;
		if (!p.begin(NS, true))
			return def;
		uint8_t id = p.getUChar("kbd", def);
		p.end();
		return id;
	}

	// false when NVS could not be written
	static bool SaveKeyboard(uint8_t id)
	{
		Preferences p;
		if (!p.begin(NS, false))
			return false;
		bool ok = p.putUChar("kbd", id) == 1;
		p.end();
		return ok;
	}

	// Slot the Super Serial Card sits in: 0 = not installed, 1-7 a slot.
	static uint8_t LoadSerial(uint8_t def)
	{
		Preferences p;
		if (!p.begin(NS, true))
			return def;
		uint8_t slot = p.getUChar("ssc", def);
		p.end();
		return slot > 7 ? def : slot;
	}

	// false when NVS could not be written
	static bool SaveSerial(uint8_t slot)
	{
		Preferences p;
		if (!p.begin(NS, false))
			return false;
		bool ok = p.putUChar("ssc", slot) == 1;
		p.end();
		return ok;
	}

	// Whether the serial card also writes what it sends to a file on the SD
	// card (/printer/print-NNN.txt).
	static bool LoadPrintCapture(bool def)
	{
		Preferences p;
		if (!p.begin(NS, true))
			return def;
		bool on = p.getBool("sscsd", def);
		p.end();
		return on;
	}

	// false when NVS could not be written
	static bool SavePrintCapture(bool on)
	{
		Preferences p;
		if (!p.begin(NS, false))
			return false;
		bool ok = p.putBool("sscsd", on) == 1;
		p.end();
		return ok;
	}

	// Mounted disk image path for drive 0 or 1, "" when the drive is empty.
	static String LoadDisk(int drive)
	{
		Preferences p;
		if (!p.begin(NS, true))
			return String();
		String path = p.getString(DiskKey(drive), "");
		p.end();
		return path;
	}

	static bool SaveDisk(int drive, const char* path)
	{
		Preferences p;
		if (!p.begin(NS, false))
			return false;
		p.putString(DiskKey(drive), path ? path : "");
		p.end();
		return true;
	}

	// Emulation speed, a FramePacer::Speed: 0 = 1X, 1 = MAX.
	static uint8_t LoadSpeed(uint8_t def)
	{
		Preferences p;
		if (!p.begin(NS, true))
			return def;
		uint8_t s = p.getUChar("speed", def);
		p.end();
		return s > 1 ? def : s;
	}

	// false when NVS could not be written
	static bool SaveSpeed(uint8_t s)
	{
		Preferences p;
		if (!p.begin(NS, false))
			return false;
		bool ok = p.putUChar("speed", s) == 1;
		p.end();
		return ok;
	}

	// Menu trace on the serial port, and terminal keys for the menu from it.
	static bool LoadDebug(bool def)
	{
		Preferences p;
		if (!p.begin(NS, true))
			return def;
		bool on = p.getBool("debug", def);
		p.end();
		return on;
	}

	// false when NVS could not be written
	static bool SaveDebug(bool on)
	{
		Preferences p;
		if (!p.begin(NS, false))
			return false;
		bool ok = p.putBool("debug", on) == 1;
		p.end();
		return ok;
	}

	// Hard disk image in slot 7, "" when none: saved across a machine switch
	static String LoadHardDisk()
	{
		Preferences p;
		if (!p.begin(NS, true))
			return String();
		String path = p.getString("hd", "");
		p.end();
		return path;
	}

	static bool SaveHardDisk(const char* path)
	{
		Preferences p;
		if (!p.begin(NS, false))
			return false;
		p.putString("hd", path ? path : "");
		p.end();
		return true;
	}

private:
	static constexpr const char* NS = "apple2";
	static const char* DiskKey(int drive) { return drive == 0 ? "d1" : "d2"; }
};
