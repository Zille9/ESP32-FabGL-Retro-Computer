/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : disk_test.cpp
 *  Module : Host-side Disk II card test: tracks written through
 *           the data latch reach the image file on the (shimmed) SD
 *           card when the motor stops or the disk is ejected, for
 *           .dsk, .po and .nib; undecodable tracks and failing
 *           writes are handled; reset stops the motor.
 *           Driven by tests/host/run-disk-tests.sh.
 * ============================================================
*/

#include <cstdio>
#include <string>
#include <vector>

#include "DiskIICard.h"

using namespace DskImage;

// registers, as Apple2Device::SoftSwitch passes them for slot 6
#define R_MOTOR_OFF 0x8
#define R_MOTOR_ON  0x9
#define R_SHIFT     0xC
#define R_LOAD      0xD
#define R_READ      0xE
#define R_WRITE     0xF

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

static std::string SectorImage(int seed)
{
	std::string s(IMAGE_BYTES, '\0');
	for (int i = 0; i < IMAGE_BYTES; i++)
		s[i] = (char)((i * 13 + seed) ^ (i >> 8));
	return s;
}

// Writes one whole track through the data latch the way RWTS does - load
// the latch, then shift it out - once round from wherever the head is.
static void WriteTrack(DiskIICard& card, const uint8_t* nib)
{
	card.Io(R_WRITE, 0, true);
	for (int i = 0; i < NIB_TRACK; i++)
	{
		card.Io(R_LOAD, nib[i], true);
		card.Io(R_SHIFT, 0, false);
	}
	card.Io(R_READ, 0, false);
}

// Track 0 of 'from', nibblized in the given order
static std::vector<uint8_t> Track0(const std::string& from, SectorOrder order)
{
	std::vector<uint8_t> nib(NIB_TRACK);
	NibblizeTrack((const uint8_t*)from.data(), 0, order, nib.data());
	return nib;
}

static void TestSectorWriteBack(const char* path, SectorOrder order, const char* label)
{
	HostFiles().clear();
	std::string a = SectorImage(1), b = SectorImage(2);
	HostFiles()[path] = a;
	DiskIICard card;
	expect(card.Mount(path, 0), std::string(label) + ": mounts");

	card.Io(R_MOTOR_ON, 0, false);
	WriteTrack(card, Track0(b, order).data());
	expect(HostFiles()[path] == a, std::string(label) + ": nothing written while the motor runs");
	card.Io(R_MOTOR_OFF, 0, false);

	const std::string& now = HostFiles()[path];
	expect(now.size() == (size_t)IMAGE_BYTES, std::string(label) + ": file keeps its size");
	expect(now.compare(0, TRACK_BYTES, b, 0, TRACK_BYTES) == 0, std::string(label) + ": track 0 saved");
	expect(now.compare(TRACK_BYTES, std::string::npos, a, TRACK_BYTES, std::string::npos) == 0,
	       std::string(label) + ": other tracks untouched");
}

static void TestNibWriteBack()
{
	HostFiles().clear();
	HostFiles()["/t.nib"] = std::string(NIB_BYTES, (char)0xFF);
	DiskIICard card;
	expect(card.Mount("/t.nib", 0), ".nib: mounts");
	// a raw track, not a DOS one: a .nib keeps it anyway
	std::vector<uint8_t> raw(NIB_TRACK);
	for (int i = 0; i < NIB_TRACK; i++)
		raw[i] = 0x96 + (i % 0x69);
	card.Io(R_MOTOR_ON, 0, false);
	WriteTrack(card, raw.data());
	card.Io(R_MOTOR_OFF, 0, false);
	const std::string& now = HostFiles()["/t.nib"];
	expect(now.compare(0, NIB_TRACK, std::string(raw.begin(), raw.end())) == 0, ".nib: raw track saved");
	expect(now.size() == (size_t)NIB_BYTES, ".nib: file keeps its size");
}

static void TestReadsDoNotWrite()
{
	HostFiles().clear();
	std::string a = SectorImage(3);
	HostFiles()["/r.dsk"] = a;
	DiskIICard card;
	card.Mount("/r.dsk", 0);
	card.Io(R_MOTOR_ON, 0, false);
	for (int i = 0; i < 3 * NIB_TRACK; i++)
		card.Io(R_SHIFT, 0, false);
	card.Io(R_MOTOR_OFF, 0, false);
	expect(HostFiles()["/r.dsk"] == a, "reading alone leaves the file as it was");
}

static void TestUndecodableTrackNotSaved()
{
	HostFiles().clear();
	std::string a = SectorImage(4);
	HostFiles()["/u.dsk"] = a;
	DiskIICard card;
	card.Mount("/u.dsk", 0);
	std::vector<uint8_t> sync(NIB_TRACK, 0xFF);
	card.Io(R_MOTOR_ON, 0, false);
	WriteTrack(card, sync.data());
	card.Io(R_MOTOR_OFF, 0, false);
	expect(HostFiles()["/u.dsk"] == a, "a track with no sectors is not written to a .dsk");
	expect(card.Io(R_READ, 0, false) == 0, "and the drive stays writable");
}

static void TestWriteFailureProtects()
{
	HostFiles().clear();
	HostFiles()["/w.dsk"] = SectorImage(5);
	DiskIICard card;
	card.Mount("/w.dsk", 0);
	HostFiles().erase("/w.dsk");              // the card was pulled
	card.Io(R_MOTOR_ON, 0, false);
	WriteTrack(card, Track0(SectorImage(6), ORDER_DOS).data());
	card.Io(R_MOTOR_OFF, 0, false);
	expect(card.Io(R_READ, 0, false) == 0x80, "a failed save turns the drive write-protected");
}

static void TestUnmountFlushes()
{
	HostFiles().clear();
	std::string a = SectorImage(7), b = SectorImage(8);
	HostFiles()["/e.dsk"] = a;
	DiskIICard card;
	card.Mount("/e.dsk", 0);
	card.Io(R_MOTOR_ON, 0, false);
	WriteTrack(card, Track0(b, ORDER_DOS).data());
	card.Unmount(0);                          // ejected with the motor still on
	expect(HostFiles()["/e.dsk"].compare(0, TRACK_BYTES, b, 0, TRACK_BYTES) == 0,
	       "ejecting saves the changed track");
}

static void TestResetLineStopsMotor()
{
	HostFiles().clear();
	std::string a = SectorImage(9), b = SectorImage(10);
	HostFiles()["/m.dsk"] = a;
	DiskIICard card;
	card.Mount("/m.dsk", 0);
	card.Io(R_MOTOR_ON, 0, false);
	expect(card.MotorOn(), "motor on");
	WriteTrack(card, Track0(b, ORDER_DOS).data());
	card.ResetLine();
	expect(!card.MotorOn(), "Ctrl-Reset stops the motor");
	expect(HostFiles()["/m.dsk"].compare(0, TRACK_BYTES, b, 0, TRACK_BYTES) == 0,
	       "Ctrl-Reset saves what was written");

	card.Io(R_MOTOR_ON, 0, false);
	card.Reset();
	expect(!card.MotorOn(), "power-up reset stops the motor");
	expect(card.HasFloppy(0), "and keeps the disk inserted");
}

int main()
{
	TestSectorWriteBack("/t.dsk", ORDER_DOS, ".dsk");
	TestSectorWriteBack("/t.po", ORDER_PRODOS, ".po");
	TestNibWriteBack();
	TestReadsDoNotWrite();
	TestUndecodableTrackNotSaved();
	TestWriteFailureProtects();
	TestUnmountFlushes();
	TestResetLineStopsMotor();

	if (failures)
	{
		printf("FAIL %d of %d checks\n", failures, checks);
		return 1;
	}
	printf("PASS %d checks\n", checks);
	return 0;
}
