/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : DiskIICard.cpp
 *  Module : Disk II controller card. Two drives backed by nibblized
 *           images read from SD - .nib as is, .dsk/.do/.po sector
 *           images nibblized when mounted: head stepping from the four
 *           phase switches, drive select, motor, and the shift/load
 *           data latch at registers $C-$F; tracks written are saved
 *           back to the image file when the motor stops or the disk
 *           is ejected. Shows its boot PROM only while a disk is
 *           inserted.
 * ============================================================
*/

#include "DiskIICard.h"

static_assert(DskImage::NIB_BYTES == DISKSIZE, "nibblized sector images fill the drive buffer");

DiskIICard::DiskIICard()
{
	memset(rom, 0, sizeof(rom));
	EjectAll();
	Reset();
}

void DiskIICard::Reset()
{
	ResetLine();
	updatedrive = 0;
	currentDrive = 0;
	// I/O register
	dLatch = 0;

	memset(phases, 0, sizeof(phases));
	memset(phasesB, 0, sizeof(phasesB));
	memset(phasesBB, 0, sizeof(phasesBB));
	memset(pIdx, 0, sizeof(pIdx));
	memset(pIdxB, 0, sizeof(pIdxB));
	memset(halfTrackPos, 0, sizeof(halfTrackPos));
}

void DiskIICard::EjectAll()
{
	disk[0].Reset();
	disk[1].Reset();
}

// No disk, no PROM: the autostart ROM's slot scan then finds nothing to boot
// and drops into BASIC instead of hanging on an empty drive.
BYTE* DiskIICard::SlotRom()
{
	return (HasFloppy(0) || HasFloppy(1)) ? rom : NULL;
}

BYTE DiskIICard::Io(int reg, BYTE value, bool write)
{
	switch (reg)
	{
		case 0x0: case 0x1: case 0x2: case 0x3:
		case 0x4: case 0x5: case 0x6: case 0x7:
			stepMotor(reg);
			break; // MOVE DRIVE HEAD

		// MOTOR OFF
		case 0x8:
			disk[currentDrive].motorOn = false;
			Flush(currentDrive);
			break;

		// MOTOR ON
		case 0x9:
			disk[currentDrive].motorOn = true;
			break;

		// DRIVE 0
		case 0xA:
			setDrv(0);
			break;
		// DRIVE 1
		case 0xB:
			setDrv(1);
			break;

		// Shift Data Latch
		case 0xC:
		{
			int idx = disk[currentDrive].track * 0x1A00 + disk[currentDrive].nibble;
			if (idx < 0 || idx >= DISKSIZE)
			{
				// head parked beyond the 35 tracks an image actually holds:
				// leave the latch alone rather than running off the buffer
			}
			else if (disk[currentDrive].writeMode)
			{
				disk[currentDrive].data[idx] = dLatch;                                  // writing
				disk[currentDrive].dirtyTracks |= 1ULL << disk[currentDrive].track;
			}
			else
				dLatch = disk[currentDrive].data[idx];                                  // reading

			// turn floppy of 1 nibble
			disk[currentDrive].nibble = (disk[currentDrive].nibble + 1) % 0x1A00;
		}
		return(dLatch);

		// Load Data Latch
		case 0xD:
			dLatch = value;
			break;

		// latch for READ
		case 0xE:
			disk[currentDrive].writeMode = false;
			return(disk[currentDrive].readOnly ? 0x80 : 0);                                 // check protection

		// latch for WRITE
		case 0xF:
			disk[currentDrive].writeMode = true;
			break;
	}
	return 0;
}

// Apple Disk II
bool DiskIICard::InsertFloppy(const char* filename, int drv)
{
	DskImage::ImageType type = DskImage::TypeFromPath(filename);
	if (type == DskImage::IMAGE_NIB)
	{
		int readlen = filesystem.ReadFile(filename, disk[drv].data, DISKSIZE);
		if (readlen != DISKSIZE)
		{
			LOGF("Read Floppy Fail : %s\n",filename);
			return false;
		}
	}
	else if (type == DskImage::IMAGE_DOS || type == DskImage::IMAGE_PRODOS)
	{
		// 140K sector image: read to the tail of the drive buffer, then
		// nibblize it in place into the tracks Io() streams
		BYTE* tail = disk[drv].data + (DISKSIZE - DskImage::IMAGE_BYTES);
		size_t size = 0;
		int readlen = filesystem.ReadFile(filename, tail, DskImage::IMAGE_BYTES, &size);
		if (readlen != DskImage::IMAGE_BYTES || size != (size_t)DskImage::IMAGE_BYTES)
		{
			// 800K .po, .2mg headers and short files all end up here
			LOGF("Read Floppy Fail : %s (%u bytes, want %d)\n", filename, (unsigned)size, DskImage::IMAGE_BYTES);
			return false;
		}
		DskImage::NibblizeInPlace(disk[drv].data,
			type == DskImage::IMAGE_PRODOS ? DskImage::ORDER_PRODOS : DskImage::ORDER_DOS);
	}
	else
	{
		LOGF("Read Floppy Fail : %s (not a .nib, .dsk, .do or .po)\n", filename);
		return false;
	}

	LOGF("Read Floppy OK : %s\n",filename);
	sprintf(disk[drv].filename, "%s", filename);

	// writable: changed tracks go back to the file (Flush)
	disk[drv].readOnly = false;
	disk[drv].type = type;
	disk[drv].dirtyTracks = 0;
	return true;
}

bool DiskIICard::Mount(const char* path, int drive)
{
	if (drive < 0 || drive > 1)
		return false;
	// a disk being replaced saves what was written to it first
	Flush(drive);
	if (!InsertFloppy(path, drive))
	{
		// short/failed read clobbered the buffer - never leave it half-mounted
		disk[drive].Reset();
		return false;
	}
	return true;
}

void DiskIICard::Unmount(int drive)
{
	if (drive < 0 || drive > 1)
		return;
	Flush(drive);
	disk[drive].Reset();
}

void DiskIICard::stepMotor(WORD address)
{
	address &= 7;
	int phase = address >> 1;

	phasesBB[currentDrive][pIdxB[currentDrive]] = phasesB[currentDrive][pIdxB[currentDrive]];
	phasesB[currentDrive][pIdx[currentDrive]] = phases[currentDrive][pIdx[currentDrive]];
	pIdxB[currentDrive] = pIdx[currentDrive];
	pIdx[currentDrive] = phase;

	if (!(address & 1))
	{                                                         // head not moving (PHASE x OFF)
		phases[currentDrive][phase] = false;
		return;
	}

	if ((phasesBB[currentDrive][(phase + 1) & 3]) && (--halfTrackPos[currentDrive] < 0))      // head is moving in
		halfTrackPos[currentDrive] = 0;

	if ((phasesBB[currentDrive][(phase - 1) & 3]) && (++halfTrackPos[currentDrive] > 140))    // head is moving out
		halfTrackPos[currentDrive] = 140;

	phases[currentDrive][phase] = true;                                                 // update track#
	disk[currentDrive].track = (halfTrackPos[currentDrive] + 1) / 2;
}

void DiskIICard::setDrv(int drv)
{
	Flush(!drv);                                                                  // leaving that drive: save what it wrote
	disk[drv].motorOn = disk[!drv].motorOn || disk[drv].motorOn;                  // if any of the motors were ON
	disk[!drv].motorOn = false;                                                   // motor of the other drive is set to OFF
	currentDrive = drv;                                                                 // set the current drive
}

// A sector image takes a track back only when all 16 of its sectors still
// decode; one that does not (copy protection, a half-formatted track) stays
// in memory, and the log says so. If the card will not take the write at
// all, the drive turns write-protected, so DOS reports it instead of later
// writes being lost without a word.
void DiskIICard::Flush(int drv)
{
	FloppyDrive& d = disk[drv];
	if (!d.dirtyTracks || !d.filename[0])
		return;

	static uint8_t trackImage[DskImage::TRACK_BYTES];
	for (int t = 0; t < DskImage::TRACKS; t++)
	{
		if (!(d.dirtyTracks & (1ULL << t)))
			continue;
		const BYTE* nib = d.data + t * DskImage::NIB_TRACK;
		bool ok;
		if (d.type == DskImage::IMAGE_NIB)
			ok = FileSystem::WriteAt(d.filename, (size_t)t * DskImage::NIB_TRACK, nib, DskImage::NIB_TRACK);
		else
		{
			DskImage::SectorOrder order = (d.type == DskImage::IMAGE_PRODOS) ? DskImage::ORDER_PRODOS : DskImage::ORDER_DOS;
			if (!DskImage::DenibblizeTrackToImage(nib, t, order, trackImage))
			{
				LOGF("[disk] %s: track %d does not decode, not saved\n", d.filename, t);
				continue;
			}
			ok = FileSystem::WriteAt(d.filename, (size_t)t * DskImage::TRACK_BYTES, trackImage, DskImage::TRACK_BYTES);
		}
		if (!ok)
		{
			LOGF("[disk] cannot write %s: drive %d is now write-protected\n", d.filename, drv + 1);
			d.readOnly = true;
			break;
		}
	}
	d.dirtyTracks = 0;
}

void DiskIICard::ResetLine()
{
	for (int drv = 0; drv < 2; drv++)
	{
		disk[drv].motorOn = false;
		disk[drv].writeMode = false;
		Flush(drv);
	}
}

// Floppy disk update
bool DiskIICard::UpdateFloppyDisk()
{
	// Done once the floppy motor is off or updatedrive reaches 0
	if (disk[currentDrive].motorOn && ++updatedrive)
		return true;
	else
		return false;
}
