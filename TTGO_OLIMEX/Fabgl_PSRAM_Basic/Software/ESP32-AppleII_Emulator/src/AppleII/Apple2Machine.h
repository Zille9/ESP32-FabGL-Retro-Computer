/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : Apple2Machine.h
 *  Module : Top-level machine orchestrator (interface). Owns the
 *           CPU, Memory and Apple2Device instances for one
 *           MachineProfile and exposes InitMachine/Run/Reset/
 *           Mount/Unmount.
 * ============================================================
*/

#ifndef APPLE2_MACHINE_H
#define APPLE2_MACHINE_H

#include <stdio.h>
#include "AppleCpu.h"
#include "AppleMem.h"
#include "Apple2Device.h"
#include "MachineProfile.h"

class VGA;

// The hard disk card's view of memory: whatever the CPU sees right now.
class MemoryBus : public CardBus
{
public:
	explicit MemoryBus(Memory& m) : mem(m) {}
	BYTE Read(WORD addr) override { return mem.ReadByte(addr); }
	void Write(WORD addr, BYTE value) override { mem.WriteByte(addr, value); }
private:
	Memory& mem;
};

class Apple2Machine
{
public:
	const MachineProfile& profile;
	CPU cpu;
	Memory mem;
	Apple2Device device;

	// Why the model saved in NVS could not be booted, "" when it was.
	// Set by setup(), shown by the supervisor.
	const char* bootNote;
	// the hard disk card's window on memory
	MemoryBus bus;

private:
	void LoadRoms();
	void InstallSerialCard();
	bool Booting();

public:
	Apple2Machine(const MachineProfile& profile);
	~Apple2Machine();

	void InitMachine();
	void Reset();
	void WarmReset();
	bool Mount(const char* path, int drive);
	bool SetSerialSlot(int slot, bool capture);
	void Unmount(int drive);
	// the hard disk in slot 7
	bool MountHardDisk(const char* path);
	void UnmountHardDisk();
	void Run(long long cycle);
	void Render(VGA *vga, int frame);

	void LoadMachine(std::string path);
	void DumpMachine(std::string path);
};



#endif
