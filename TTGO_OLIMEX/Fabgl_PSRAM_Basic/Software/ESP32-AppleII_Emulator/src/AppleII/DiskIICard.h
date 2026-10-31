/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : DiskIICard.h
 *  Module : Disk II controller card (interface). Two drives backed
 *           by nibblized images read from SD (.nib, or .dsk/.do/.po
 *           converted on mount), the head stepper and data latch,
 *           and the P5 boot PROM.
 * ============================================================
*/

#ifndef DISKII_CARD_H
#define DISKII_CARD_H

#include <string>
#include "Card.h"
#include "DskImage.h"
#include "../Tools/Log.h"
#include "../Tools/FileSystem.h"

// two disk ][ drive units
struct FloppyDrive
{
	char filename[400];
	bool readOnly;
	// nibblelized disk image
	BYTE *data;
	bool motorOn;
	bool writeMode;
	BYTE track;
	WORD nibble;
	DskImage::ImageType type;   // how 'data' maps back to the file
	uint64_t dirtyTracks;       // bit t: track t written since the last Flush

	FloppyDrive()
	{
		DEBUG_PRINTLN("Construct FloppyDrive");
		data = (BYTE*)ps_malloc(DISKSIZE);
	}

	void Reset()
	{
		memset(data, 0, DISKSIZE);
		memset(filename,0, 400);
		readOnly = false;
		motorOn = false;
		writeMode = false;
		track = 0;
	 	nibble = 0;
		type = DskImage::IMAGE_NONE;
		dirtyTracks = 0;
	}
};

class DiskIICard : public Card
{
public:
	BYTE rom[SL6SIZE];          // P5 boot PROM, filled by Apple2Machine::LoadRoms

	DiskIICard();

	BYTE Io(int reg, BYTE value, bool write) override;
	BYTE* SlotRom() override;
	void Reset() override;
	// The RESET line (Ctrl-Reset too): clears the card's control latch, so
	// both motors stop and the drive leaves write mode. The head stays put.
	void ResetLine();

	bool Mount(const char* path, int drive);
	void Unmount(int drive);
	// Writes the tracks changed since the last flush back to the image file.
	// Called when the motor stops, the drive is deselected, and on eject.
	void Flush(int drive);
	void EjectAll();
	bool HasFloppy(int drive) { return disk[drive].filename[0] != '\0'; }
	std::string GetDiskName(int drive) { return disk[drive].filename; }
	bool MotorOn() { return disk[currentDrive].motorOn; }

	// While the motor runs, Apple2Machine::Run keeps the CPU going in short
	// bursts; false once the motor is off or updatedrive wraps to 0.
	bool UpdateFloppyDisk();

private:
	FloppyDrive disk[2];
	int currentDrive;
	BYTE updatedrive;

	bool phases[2][4];
	// phases states Before
	bool phasesB[2][4];
	// phases states Before Before
	bool phasesBB[2][4];
	// phase index (for both drives)
	int pIdx[2];
	// phase index Before
	int pIdxB[2];
	int halfTrackPos[2];
	BYTE dLatch;

	FileSystem filesystem;

	bool InsertFloppy(const char* filename, int drv);
	void stepMotor(WORD address);
	void setDrv(int drv);
};

#endif
