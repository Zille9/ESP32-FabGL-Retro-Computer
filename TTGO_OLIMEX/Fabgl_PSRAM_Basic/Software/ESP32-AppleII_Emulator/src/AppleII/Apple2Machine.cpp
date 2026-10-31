/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : Apple2Machine.cpp
 *  Module : Top-level machine orchestrator. Creates and wires CPU
 *           + Memory + Apple2Device and the slot cards for the
 *           selected MachineProfile, loads the system and Disk II
 *           ROMs from /roms on the SD card (falling back to the
 *           built-in images where the model has them), and drives
 *           the per-frame cycle budget.
 * ============================================================
*/

#include "Predef.h"
#include "rombios.h"
#include "Apple2Machine.h"
#include "RomLoader.h"
#include "../Tools/Log.h"
#include "../Tools/Settings.h"
#include "../VGA/VGA.h"
#include "../BuildConfig.h"

Apple2Machine::Apple2Machine(const MachineProfile& p)
	: profile(p), bootNote(""), bus(mem)
{
	DEBUG_PRINTLN("Construct Apple2Machine");
	cpu.cmos = (profile.cpu == CPU_65C02);
	device.iie = profile.iieMmu;
	device.slots[6] = profile.diskIISlot6 ? &device.disk6 : NULL;
	// slot 7: the autostart ROM scans down from here, so a mounted hard disk
	// boots before the floppies, as on a real machine with one fitted
	device.hdd7.Configure(7, &bus);
	device.slots[7] = &device.hdd7;
}

Apple2Machine::~Apple2Machine()
{

}

void Apple2Machine::InitMachine()
{
	mem.Create(profile.systemRomSize, profile.iieMmu);
	// before LoadRoms: Create() builds the video's default ][+ font, and the
	// IIe character ROM has to replace it, not be wiped by it
	device.Create(&cpu);
	LoadRoms();
	device.InsetFloppy();
	// unset the Power-UP byte
	mem.WriteByte(0x3F4, 0);
	cpu.Reset(mem);
	mem.WriteByte(0x4D, 0xAA);   // Just crashes if this memory location equals zero
	mem.WriteByte(0xD0, 0xAA);   // won't work if this memory location equals zero

	mem.device = &device;
	// after mem.device: installing the card remaps the page tables, which
	// can only see the cards through it
	InstallSerialCard();

	Booting();
}

// Once per power-up: the images stay in memory across resets. A file in
// /roms wins over the built-in copy, so a user can supply their own dump.
void Apple2Machine::LoadRoms()
{
	DEBUG_PRINTLN("Load ROMs");
	if (RomLoader::Load(profile.systemRom, mem.rom, profile.systemRomSize) != ROM_OK)
	{
		// setup() only boots a model without a built-in image when its ROM
		// is on the card, so in practice this is the ][+
		if (profile.embeddedRom)
		{
			Serial.printf("[rom] using the built-in %s ROM\n", profile.name);
			memcpy(mem.rom, appleIIrom, ROMSIZE);
		}
		else
			Serial.printf("[rom] no system ROM for %s\n", profile.name);
	}

	if (RomLoader::Load(DISKII_ROM, device.disk6.rom, SL6SIZE) != ROM_OK)
		memcpy(device.disk6.rom, diskII, SL6SIZE);

	// IIe character generator: the first 2K of the video ROM is the set used
	if (profile.charRom)
	{
		BYTE* buf = (BYTE*)ps_malloc(profile.charRomSize);
		if (buf && RomLoader::Load(profile.charRom, buf, profile.charRomSize) == ROM_OK)
			device.video.LoadCharRom(buf);
		free(buf);
	}
}

// The Super Serial Card as the user last left it. Nothing to do when it is
// switched off; when it is on but has no firmware on the card, the
// supervisor shows the ROM as missing, so only the log says so here.
void Apple2Machine::InstallSerialCard()
{
#if PERF_TRACE
	// a measurement build needs the log, which the card would silence
	return;
#endif
	int slot = Settings::LoadSerial(0);
	if (!slot)
		return;
	if (!SetSerialSlot(slot, Settings::LoadPrintCapture(false)))
		LOGF("[ssc] slot %d: no %s/%s, card not installed\n", slot, ROM_DIR, SSC_ROM);
}

bool Apple2Machine::Booting()
{
	DEBUG_PRINTLN("====> BOOTING ...");
	// Maps the slot ROMs as the cards now present them. With no disk in
	// either drive the Disk II hides its PROM, so the machine boots to BASIC.
	mem.Remap();
	cpu.Reset(mem);
	return true;
}

void Apple2Machine::Reset()
{
	mem.Reset();
	cpu.Reboot(mem);
	device.Reset();

	// unset the Power-UP byte
	mem.WriteByte(0x3F4, 0);
	// dirty hack, fix soon... if I understand why
	mem.WriteByte(0x4D, 0xAA);   // Joust crashes if this memory location equals zero
	mem.WriteByte(0xD0, 0xAA);   // Planetoids won't work if this memory location equals zero

	Booting();
}

// Ctrl+F12: the RESET line. RAM survives; the ROM's reset handler decides
// between a warm restart and a cold boot (Open Apple held on the IIe, or a
// power-up byte that does not match).
void Apple2Machine::WarmReset()
{
	device.resetRequested = false;
	mem.ResetSwitches();
	// the RESET line reaches the Disk II too: a program that hung with the
	// drive running stops here, and so does fast-disk mode
	device.disk6.ResetLine();
	device.col80 = false;
	device.altCharset = false;
	device.dhires = false;
	cpu.Reset(mem);
}

bool Apple2Machine::Mount(const char* path, int drive)
{
	if (!device.Mount(path, drive))
		return false;
	// machine may have booted disk-less with the Disk II PROM hidden;
	// PR#6 needs it present
	mem.Remap();
	return true;
}

// Installing or taking out the serial card: the page tables have to be
// rebuilt, and the $C800 selection dropped in case the card that held it is
// the one that just left. False when the card was asked for and could not
// be installed.
//
// The card sends its data out of the USB UART the firmware logs to, so the
// log stops for as long as the card is in - here rather than in the caller,
// so that it holds however the card was installed: at boot, or from the
// supervisor while the machine runs.
bool Apple2Machine::SetSerialSlot(int slot, bool capture)
{
	Log::Muted() = false;
	bool ok = (slot == 0) ? (device.SetSerialSlot(0, capture), true)
	                      : device.SetSerialSlot(slot, capture);
	mem.expSlot = 0;
	mem.Remap();

	if (device.SerialSlot())
	{
		LOGF("[ssc] slot %d active, the log stops here so that it does not mix"
		     " into the card's output\n", device.SerialSlot());
		Log::Mute();
	}
	return ok;
}

bool Apple2Machine::MountHardDisk(const char* path)
{
	bool ok = device.hdd7.Mount(path);
	// the card shows or hides its firmware with the image
	mem.Remap();
	return ok;
}

void Apple2Machine::UnmountHardDisk()
{
	device.hdd7.Unmount();
	mem.Remap();
}

void Apple2Machine::Unmount(int drive)
{
	device.Unmount(drive);
	// no disk left: the card hides its PROM, so a Reset boots to BASIC
	// instead of hanging on an empty drive scan
	mem.Remap();
}

void Apple2Machine::Run(long long cycle)
{
/*
	if (device.resetMachine)
	{
		Reset();
		return;
	}
*/
	device.UpdateInput();
	if (device.resetRequested)
		WarmReset();
	cpu.Run(mem, (Cycles)cycle);
	while (1)
	{
		if( device.UpdateFloppyDisk() == false )
			break;
		cpu.Run(mem, 1000);
	}
}

void Apple2Machine::Render(VGA *vga, int frame)
{
	// Device draws into the VGA framebuffer itself - no backbuffer, no blit,
	// so an unchanged screen costs nothing beyond the dirty-cell checks.
	device.Render(mem, frame, vga);
}


// Load a DUMP file and resume
void Apple2Machine::LoadMachine(std::string path)
{
}

// Save the entire current state
void Apple2Machine::DumpMachine(std::string path)
{
}

