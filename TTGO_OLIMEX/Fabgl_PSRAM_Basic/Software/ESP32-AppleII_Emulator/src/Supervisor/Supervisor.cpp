/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : Supervisor.cpp
 *  Module : F1 supervisor menu. Run() holds the emulation task
 *           until the menu is closed, which is what pauses the
 *           Apple. A grid of tiles leads to the disk manager, the
 *           setup list, reset and an About page; each screen is a
 *           function with its own key loop, drawn with the
 *           primitives in SupervisorUI. Settings changed in the
 *           menu are saved to NVS on the way out, and the
 *           emulator's render caches are invalidated so it paints
 *           over the menu.
 * ============================================================
*/

#include <Arduino.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include "fabgl.h"
#include "Supervisor.h"
#include "SupervisorUI.h"
#include "../AppleII/Apple2Machine.h"
#include "../Version.h"
#include "../BuildConfig.h"
#include "../AppleII/RomLoader.h"
#include "../Tools/Settings.h"
#include "../Tools/KeyboardLayouts.h"
#include "../Tools/Bootloader.h"
#include "../AppleII/SuperSerialCard.h"

using namespace SupUI;

// the machine, for the row callbacks of lists and popups
static Apple2Machine* popupMachine;

extern fabgl::Keyboard *keyboard_ptr;
extern fabgl::Canvas Canvas;

Supervisor::Supervisor(Apple2Machine* m)
{
	machine = m;
	requested = false;
	bootNoteShown = false;
	entries = NULL;
	entryCount = 0;
	listError = NULL;
	strcpy(curPath, "/");
	diskBtn = diskSel = diskTop = 0;
	diskInDrives = false;
	keyboard0 = speed0 = 0;
	debug0 = false;
}

//////////////////////////////////////////////////////////////////////////
// Main menu: icon tiles

#define TILE_W       160
#define TILE_H       62
#define TILE_X0      (BOX_X + 16)
#define TILE_Y0      HEAD_Y
#define TILE_PITCH_X 176
#define TILE_PITCH_Y 68
#define TILE_COLS    3
#define TILE_COUNT   5

enum { TILE_DISKS, TILE_SETUP, TILE_RESET, TILE_ABOUT, TILE_RESUME };
static const char* const tileLabels[TILE_COUNT] = { "Disks", "Setup", "Reset", "About", "Resume" };
static const uint8_t tileIcons[TILE_COUNT] = { ICON_DISK, ICON_SETUP, ICON_RESET, ICON_ABOUT, ICON_RESUME };

static void DrawTile(fabgl::Canvas& cv, int idx, bool selected)
{
	int x = TILE_X0 + (idx % TILE_COLS) * TILE_PITCH_X;
	int y = TILE_Y0 + (idx / TILE_COLS) * TILE_PITCH_Y;
	fabgl::RGB888 bg = selected ? T_ACCENT : T_TEXT;
	fabgl::RGB888 fg = selected ? T_TEXT : T_INK;

	FillBox(cv, bg, x, y, x + TILE_W - 1, y + TILE_H - 1);
	// thin accent border; the selected tile gets a double white one. A side
	// is two pixels for every row of the top and bottom.
	cv.setPenColor(selected ? T_TEXT : T_ACCENT);
	for (int i = 0; i < (selected ? 4 : 2); i++)
		cv.drawRectangle(x + i, y + i / 2, x + TILE_W - 1 - i, y + TILE_H - 1 - i / 2);

	DrawBigIcon(cv, tileIcons[idx], x + (TILE_W - 64) / 2, y + 8, fg, bg);

	cv.setBrushColor(bg);
	cv.setPenColor(fg);
	TextCentered(cv, &fabgl::FONT_8x14, x, TILE_W, y + 42, tileLabels[idx]);
}

// frame and every tile: the main menu's first paint, and what a popup that
// opens before it sits on
void Supervisor::DrawMain(int selected)
{
	char title[40];
	snprintf(title, sizeof(title), "%s SUPERVISOR", machine->profile.name);
	DrawFrame(Canvas, title, "Arrows   ENTER Select   ESC/F1 Exit");
	for (int i = 0; i < TILE_COUNT; i++)
		DrawTile(Canvas, i, i == selected);
}

// returns the chosen tile, or -1 on Esc/F1
int Supervisor::MainMenu(int selected)
{
	fabgl::Canvas& cv = Canvas;
	if (selected < 0 || selected >= TILE_COUNT)
		selected = 0;
	DrawMain(selected);
	Canvas.waitCompletion();
	TracePixels();

	int drawn = selected;
	for (;;)
	{
		// only the tile the cursor left and the one it reached
		if (drawn != selected)
		{
			DrawTile(cv, drawn, false);
			DrawTile(cv, selected, true);
			drawn = selected;
		}
		cv.waitCompletion();
		Trace("menu: SUPERVISOR > %s\n", tileLabels[selected]);

		switch (WaitKey())
		{
			case K_LEFT:  selected = GridMove(selected, -1, 0, TILE_COLS, TILE_COUNT); break;
			case K_RIGHT: selected = GridMove(selected, 1, 0, TILE_COLS, TILE_COUNT); break;
			case K_UP:    selected = GridMove(selected, 0, -1, TILE_COLS, TILE_COUNT); break;
			case K_DOWN:  selected = GridMove(selected, 0, 1, TILE_COLS, TILE_COUNT); break;
			case K_ENTER: return selected;
			case K_ESC:   return -1;
			case K_F1:    closeAll = true; return -1;
			default: break;
		}
	}
}

void Supervisor::Run()
{
	requested = false;
	serialKeys = (machine->device.SerialSlot() == 0);
	Begin(keyboard_ptr);
	popupMachine = machine;
	Trace("menu: opened\n");

	keyboard0 = CurrentKeyboardLayoutId();
	speed0 = machine->device.speedMode;
	debug0 = debugOn;
	bool reset = false;

	// why the saved model could not boot, if it could not: said once
	if (!bootNoteShown && machine->bootNote[0])
	{
		bootNoteShown = true;
		DrawMain(0);
		Notice("ROMs missing", "Booted the Apple ][+ instead.", "ROM files go in /roms");
	}

	int sel = 0;
	while (!closeAll)
	{
		sel = MainMenu(sel);
		if (sel < 0 || sel == TILE_RESUME)
			break;
		switch (sel)
		{
			case TILE_DISKS: DiskMenu(); break;
			case TILE_SETUP: SetupMenu(); break;
			case TILE_RESET:
			{
				char msg[40];
				snprintf(msg, sizeof(msg), "Reset the %s?", machine->profile.name);
				if (Confirm("Reset", msg, nullptr))
				{
					reset = true;            // after closing, so the boot is visible
					closeAll = true;
				}
				break;
			}
			case TILE_ABOUT: AboutScreen(); break;
		}
	}

	SaveSettings();
	if (reset)
		machine->Reset();
	Trace("menu: closed%s, stack free %u\n", reset ? ", reset" : "",
	      (unsigned)uxTaskGetStackHighWaterMark(NULL));
	// the menu drew over the Apple's picture: have it repaint everything
	machine->device.InvalidateRenderCache();
}

// Keyboard layout, speed and the debug setting take effect as they are
// changed; NVS only has to remember them, and only when they moved.
void Supervisor::SaveSettings()
{
	bool ok = true;
	if (CurrentKeyboardLayoutId() != keyboard0)
	{
		if (Settings::SaveKeyboard(CurrentKeyboardLayoutId()))
			keyboard0 = CurrentKeyboardLayoutId();
		else
			ok = false;
	}
	if (machine->device.speedMode != speed0)
	{
		if (Settings::SaveSpeed(machine->device.speedMode))
			speed0 = machine->device.speedMode;
		else
			ok = false;
	}
	if (debugOn != debug0)
	{
		if (Settings::SaveDebug(debugOn))
			debug0 = debugOn;
		else
			ok = false;
	}
	// they stay in effect until power-off, but will not come back
	if (!ok)
		Notice("Not saved", "Cannot write settings (NVS).", "They last until power-off.");
}

//////////////////////////////////////////////////////////////////////////
// About

// CoCo Byte Club logo, 91x12, one bit per pixel in XBM order (LSB first,
// 12 bytes a row).
#define LOGO_W         91
#define LOGO_H         12
#define LOGO_ROW_BYTES 12
static const unsigned char logoCocoByte[LOGO_H * LOGO_ROW_BYTES] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xf0, 0x01, 0x1b, 0xf0, 0x01, 0x1b, 0xfe, 0xf9, 0x7c, 0xff, 0xfd, 0x03,
	0xf8, 0x83, 0x3b, 0xf8, 0x83, 0x3b, 0xfe, 0xfb, 0x7c, 0xff, 0xfd, 0x03,
	0xfc, 0xc7, 0x7b, 0xfc, 0xc7, 0x7b, 0xfe, 0xfb, 0x7c, 0xff, 0xfd, 0x03,
	0xfe, 0xef, 0xfb, 0xfe, 0xef, 0xfb, 0xf0, 0xfb, 0x7f, 0xff, 0x7d, 0x00,
	0xfe, 0xe1, 0xfb, 0xfe, 0xe1, 0xfb, 0xfe, 0xfb, 0x7f, 0x7c, 0xfc, 0x03,
	0x7e, 0xe0, 0xfb, 0x7e, 0xe0, 0xfb, 0xfe, 0xf1, 0x3f, 0x7c, 0xfc, 0x03,
	0xfe, 0xe1, 0xfb, 0xfe, 0xe1, 0xfb, 0xfe, 0xe3, 0x1f, 0x7c, 0xfc, 0x03,
	0xfe, 0xef, 0xfb, 0xfe, 0xef, 0xfb, 0xe0, 0xc3, 0x0f, 0x7c, 0x7c, 0x00,
	0xfc, 0xc7, 0x7b, 0xfc, 0xc7, 0x7b, 0xfe, 0xc3, 0x0f, 0x7c, 0xfc, 0x07,
	0xf8, 0x83, 0x3b, 0xf8, 0x83, 0x3b, 0xfe, 0xc3, 0x0f, 0x7c, 0xfc, 0x07,
	0xf0, 0x01, 0x1b, 0xf0, 0x01, 0x1b, 0xfe, 0xc1, 0x0f, 0x7c, 0xfc, 0x07
};

// The logo at four screen pixels a dot, each run of set dots as one box.
static void DrawLogo(fabgl::Canvas& cv, int y)
{
	const int sx = 4;
	int x0 = BOX_X + (BOX_W - LOGO_W * sx) / 2;
	for (int row = 0; row < LOGO_H; row++)
	{
		int start = -1;
		for (int col = 0; col <= LOGO_W; col++)
		{
			bool on = col < LOGO_W && (logoCocoByte[row * LOGO_ROW_BYTES + (col >> 3)] & (1 << (col & 7)));
			if (on && start < 0)
				start = col;
			else if (!on && start >= 0)
			{
				FillBox(cv, T_GREEN, x0 + start * sx, y + row, x0 + col * sx - 1, y + row);
				start = -1;
			}
		}
	}
}

void Supervisor::AboutScreen()
{
	fabgl::Canvas& cv = Canvas;
	DrawFrame(cv, "About", "ESC Back   F1 Exit");
	DrawLogo(cv, HEAD_Y);

	static const char* const lines[] =
	{
		"Apple II emulator for ESP32",
		"Reinaldo Torres & Claude Code",
		"CoCoByte Club",
		"codesafe ESP32-VGA_AppleII_Emulator",
		"MIT License",
	};
	cv.setBrushColor(T_BG);
	int y = HEAD_Y + 16;
	for (unsigned i = 0; i < sizeof(lines) / sizeof(lines[0]); i++, y += 10)
	{
		cv.setPenColor(i < 3 ? T_TEXT : T_DIM);
		TextCentered(cv, &fabgl::FONT_6x8, BOX_X, BOX_W, y, lines[i]);
	}
	y += 1;
	FillBox(cv, T_ACCENT, BOX_X + 48, y, BOX_X + BOX_W - 49, y);
	y += 4;

	Row info[5];
	for (Row& r : info)
		r.dim = false;
	strcpy(info[0].label, "Machine");
	strlcpy(info[0].value, machine->profile.name, sizeof(info[0].value));
	strcpy(info[1].label, "Version");
#if BUILD_TARGET == BUILD_TARGET_BOOTLOADER
	strcpy(info[1].value, FW_VERSION_STR " (SD bootloader)");
#else
	strcpy(info[1].value, FW_VERSION_STR);
#endif
	strcpy(info[2].label, "Build date");
	strcpy(info[2].value, FW_BUILD_DATE);
	strcpy(info[3].label, "PSRAM free");
	snprintf(info[3].value, sizeof(info[3].value), "%u KB",
	         (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
	strcpy(info[4].label, "PSRAM total");
	snprintf(info[4].value, sizeof(info[4].value), "%u KB",
	         (unsigned)(heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024));
	for (int i = 0; i < 5; i++, y += LIST_ROW_H + 1)
	{
		DrawRowAt(cv, LIST_X, y, LIST_W, 24, true, info[i], false, T_BG);
		Trace("about: %s %s\n", info[i].label, info[i].value);
	}
	cv.waitCompletion();

	for (;;)
	{
		SupKey key = WaitKey();
		if (key == K_F1)
			closeAll = true;
		if (key == K_F1 || key == K_ESC || key == K_ENTER)
			return;
	}
}

//////////////////////////////////////////////////////////////////////////
// Disk manager

// Drive buttons across the top, the SD card's folders and disk images below.

#define DRIVE_COUNT     3                // D1, D2 and the hard disk in slot 7
#define DRIVE_HD        2
#define DRIVE_BTN_W     168
#define DRIVE_BTN_H     36
#define DRIVE_BTN_PITCH 176
#define DRIVE_NAME_COLS 12               // 6x8 font at double width
#define DISK_LIST_Y     (HEAD_Y + DRIVE_BTN_H + 4)
#define DISK_LIST_ROWS  ((HINT_Y - 4 - DISK_LIST_Y) / LIST_ROW_H)

static const char* const driveLabels[DRIVE_COUNT] = { "D1", "D2", "HD" };

// file name without its directory
static const char* BaseName(const char* path)
{
	const char* p = strrchr(path, '/');
	return p ? p + 1 : path;
}

static int compareEntries(const void* a, const void* b)
{
	const SupEntry* ea = (const SupEntry*)a;
	const SupEntry* eb = (const SupEntry*)b;
	if (ea->isDir != eb->isDir)
		return ea->isDir ? -1 : 1;          // directories first
	return strcasecmp(ea->name, eb->name);
}

// Reads curPath into entries. On failure listError says why and the list is
// empty; a folder that has gone (the card was swapped) falls back to the root.
void Supervisor::ScanDir()
{
	entryCount = 0;
	listError = NULL;
	if (!entries)
		entries = (SupEntry*)heap_caps_calloc(SUP_MAX_ENTRIES, sizeof(SupEntry), MALLOC_CAP_SPIRAM);
	if (!entries)
	{
		listError = "(no memory for the file list)";
		return;
	}

	File root = SD.open(curPath);
	if ((!root || !root.isDirectory()) && !AtRoot())
	{
		if (root)
			root.close();
		strcpy(curPath, "/");
		root = SD.open(curPath);
	}
	if (!root || !root.isDirectory())
	{
		if (root)
			root.close();
		listError = "(SD card error)";
		return;
	}

	for (File file = root.openNextFile(); file && entryCount < SUP_MAX_ENTRIES; file = root.openNextFile())
	{
		const char* name = file.name();
		// hidden entries, and names too long for an entry, are left out
		if (name[0] == '.' || strlen(name) >= SUP_NAME_LEN)
			continue;
		bool isDir = file.isDirectory();
		if (!isDir && DskImage::TypeFromPath(name) == DskImage::IMAGE_NONE)
			continue;
		strcpy(entries[entryCount].name, name);
		entries[entryCount].isDir = isDir;
		entryCount++;
	}
	root.close();

	qsort(entries, entryCount, sizeof(SupEntry), compareEntries);
	if (entryCount == 0 && AtRoot())
		listError = "(no disk images on the SD card)";
}

// rows of the list: ".." below the root, then the entries
int Supervisor::ListCount()
{
	if (listError)
		return 0;
	return (AtRoot() ? 0 : 1) + entryCount;
}

void Supervisor::ListLabel(int index, char* out, int outlen)
{
	if (!AtRoot())
	{
		if (index == 0)
		{
			snprintf(out, outlen, "..  (up one level)");
			return;
		}
		index--;
	}
	snprintf(out, outlen, entries[index].isDir ? "%s/" : "%s", entries[index].name);
}

// path of the image in a drive, "" when it is empty
const char* Supervisor::DriveImage(int btn, char* buf, int buflen)
{
	if (btn == DRIVE_HD)
		snprintf(buf, buflen, "%s", machine->device.hdd7.ImageName());
	else
		snprintf(buf, buflen, "%s", machine->device.GetDiskName(btn).c_str());
	return buf;
}

// Icon (grey when the drive is empty), drive name, and the image's name on
// two short lines without its extension.
void Supervisor::DrawDriveButton(int btn, bool focused)
{
	fabgl::Canvas& cv = Canvas;
	int x = BOX_X + 12 + btn * DRIVE_BTN_PITCH, y = HEAD_Y;
	char path[SUP_PATH_LEN];
	bool mounted = DriveImage(btn, path, sizeof(path))[0] != '\0';
	fabgl::RGB888 bg = focused ? T_ACCENT : T_TEXT;
	fabgl::RGB888 fg = focused ? T_TEXT : T_INK;

	FillBox(cv, bg, x, y, x + DRIVE_BTN_W - 1, y + DRIVE_BTN_H - 1);
	cv.setPenColor(focused ? T_TEXT : T_ACCENT);
	for (int i = 0; i < (focused ? 4 : 2); i++)
		cv.drawRectangle(x + i, y + i / 2, x + DRIVE_BTN_W - 1 - i, y + DRIVE_BTN_H - 1 - i / 2);

	DrawSmallIcon(cv, btn == DRIVE_HD ? ICON_HDD : ICON_DISK, x + 10, y + 4, fg, bg, !mounted);

	cv.setBrushColor(bg);
	cv.setPenColor(fg);
	Text(cv, &fabgl::FONT_8x8, x + 52, y + 6, driveLabels[btn]);

	char line1[DRIVE_NAME_COLS + 1], line2[DRIVE_NAME_COLS + 1];
	if (mounted)
		SplitName(path, DRIVE_NAME_COLS, line1, line2);
	else
	{
		strcpy(line1, "empty");
		line2[0] = '\0';
	}
	cv.setPenColor(mounted ? fg : T_GREY);
	Text(cv, &fabgl::FONT_6x8, x + 10, y + 17, line1);
	if (line2[0])
		Text(cv, &fabgl::FONT_6x8, x + 10, y + 26, line2);
}

void Supervisor::DrawListRow(int index, int top, bool hl)
{
	Row row;
	row.value[0] = 0;
	row.dim = false;
	ListLabel(index, row.label, sizeof(row.label));
	DrawRowAt(Canvas, LIST_X, DISK_LIST_Y + (index - top) * LIST_ROW_H, LIST_W, 8, true, row, hl, T_BG);
}

// the mount popup's rows: each floppy drive and what it holds, then Cancel

static void MountRow(int idx, Row& row)
{
	if (idx >= 2)
	{
		strcpy(row.label, "Cancel");
		return;
	}
	strcpy(row.label, driveLabels[idx]);
	std::string path = popupMachine->device.GetDiskName(idx);
	if (path.empty())
		strcpy(row.value, "(empty)");
	else
		BaseTitle(path.c_str(), row.value, 20);
}

// Asks which drive takes a floppy image; a hard disk image has only slot 7 to
// go to. Asks again before replacing a disk or mounting an image twice.
void Supervisor::MountDialog(const char* path)
{
	char m1[40], m2[40];
	const char* name = BaseName(path);

	if (HardDiskCard::IsHardDiskImage(path))
	{
		const char* cur = machine->device.hdd7.ImageName();
		if (strcmp(cur, path) == 0)
		{
			Notice("Hard disk", "Already mounted on HD.", nullptr);
			return;
		}
		if (cur[0])
		{
			BaseTitle(cur, m2, 27);
			if (!Confirm("Replace disk?", "HD already has a disk:", m2))
				return;
			DrawDisk();                      // a popup may follow this one
		}
		if (machine->MountHardDisk(path))
			Trace("disks: mounted %s on HD\n", name);
		else
			Notice("Mount failed", "Could not load the image:", name);
		return;
	}

	int first = 0;
	for (int d = 1; d >= 0; d--)
		if (!machine->device.HasFloppy(d))
			first = d;
	int drive = Popup("Mount disk on", name, nullptr, 3, MountRow, first);
	if (drive < 0 || drive >= 2)
		return;

	std::string cur = machine->device.GetDiskName(drive);
	if (cur == path)
		return;                              // already there
	// each popup goes over a clean screen, not over the one before it
	DrawDisk();
	if (!cur.empty())
	{
		snprintf(m1, sizeof(m1), "%s already has a disk:", driveLabels[drive]);
		BaseTitle(cur.c_str(), m2, 27);
		if (!Confirm("Replace disk?", m1, m2))
			return;
		DrawDisk();
	}
	else if (machine->device.GetDiskName(1 - drive) == path)
	{
		snprintf(m1, sizeof(m1), "Already mounted on %s.", driveLabels[1 - drive]);
		snprintf(m2, sizeof(m2), "Mount on %s as well?", driveLabels[drive]);
		if (!Confirm("Mount again?", m1, m2))
			return;
		DrawDisk();
	}
	if (machine->Mount(path, drive))
		Trace("disks: mounted %s on %s\n", name, driveLabels[drive]);
	else
		Notice("Mount failed", "Could not load the image:", name);
}

void Supervisor::UnmountDialog(int btn)
{
	char path[SUP_PATH_LEN], m1[40], m2[40];
	if (!DriveImage(btn, path, sizeof(path))[0])
		return;
	snprintf(m1, sizeof(m1), "Unmount %s?", driveLabels[btn]);
	BaseTitle(path, m2, 27);
	if (!Confirm("Unmount disk", m1, m2))
		return;
	if (btn == DRIVE_HD)
		machine->UnmountHardDisk();
	else
		machine->Unmount(btn);
	Trace("disks: unmounted %s\n", driveLabels[btn]);
}

bool Supervisor::Activate(int index)
{
	if (!AtRoot())
	{
		if (index == 0)
		{
			// up one level
			char* p = strrchr(curPath, '/');
			if (p == curPath)
				curPath[1] = '\0';           // back at the root: keep "/"
			else
				*p = '\0';
			ScanDir();
			return true;
		}
		index--;
	}
	if (index >= entryCount)
		return false;

	char path[SUP_PATH_LEN];
	int len = snprintf(path, sizeof(path), AtRoot() ? "/%s" : "%s/%s",
	                   AtRoot() ? entries[index].name : curPath, entries[index].name);
	if (len >= (int)sizeof(path))
	{
		Notice("Path too long", "This folder is nested too", "deep to open.");
		return false;
	}
	if (entries[index].isDir)
	{
		strcpy(curPath, path);
		ScanDir();
		return true;
	}
	MountDialog(path);
	return false;
}

// Frame, buttons and list: the first paint, and the repaint after a popup.
void Supervisor::DrawDisk()
{
	fabgl::Canvas& cv = Canvas;
	int count = ListCount();
	DrawFrame(cv, "Disk Manager", "Arrows   TAB Drives/Files   ENTER   ESC Back");
	for (int b = 0; b < DRIVE_COUNT; b++)
		DrawDriveButton(b, diskInDrives && b == diskBtn);
	for (int r = 0; r < DISK_LIST_ROWS && diskTop + r < count; r++)
		DrawListRow(diskTop + r, diskTop, !diskInDrives && diskTop + r == diskSel);
	if (listError)
	{
		cv.setBrushColor(T_BG);
		cv.setPenColor(T_DIM);
		Text(cv, &fabgl::FONT_8x8, LIST_X + 8, DISK_LIST_Y, listError);
	}
	DrawScrollbar(cv, DISK_LIST_Y, DISK_LIST_ROWS * LIST_ROW_H, diskTop, DISK_LIST_ROWS, count);
}

void Supervisor::DiskMenu()
{
	fabgl::Canvas& cv = Canvas;
	ScanDir();
	diskBtn = diskSel = diskTop = 0;
	diskInDrives = (ListCount() == 0);
	bool full = true;                        // repaint the whole screen
	int pBtn = 0, pSel = 0, pTop = 0;        // what the last paint showed
	bool pIn = false;

	while (!closeAll)
	{
		int count = ListCount();
		if (full)
			DrawDisk();
		else
		{
			// the buttons whose focus changed
			for (int b = 0; b < DRIVE_COUNT; b++)
			{
				bool now = diskInDrives && b == diskBtn;
				bool was = pIn && b == pBtn;
				if (now != was)
					DrawDriveButton(b, now);
			}
			// the list: all of it when it scrolled, else the rows the cursor touched
			if (diskTop != pTop)
			{
				FillBox(cv, T_BG, LIST_X, DISK_LIST_Y, LIST_X + LIST_W - 1,
				        DISK_LIST_Y + DISK_LIST_ROWS * LIST_ROW_H - 1);
				for (int r = 0; r < DISK_LIST_ROWS && diskTop + r < count; r++)
					DrawListRow(diskTop + r, diskTop, !diskInDrives && diskTop + r == diskSel);
				DrawScrollbar(cv, DISK_LIST_Y, DISK_LIST_ROWS * LIST_ROW_H, diskTop, DISK_LIST_ROWS, count);
			}
			else if (count > 0 && (diskSel != pSel || diskInDrives != pIn))
			{
				if (pSel != diskSel)
					DrawListRow(pSel, diskTop, false);
				DrawListRow(diskSel, diskTop, !diskInDrives);
			}
		}
		cv.waitCompletion();
		full = false;
		pBtn = diskBtn; pSel = diskSel; pTop = diskTop; pIn = diskInDrives;

		if (diskInDrives)
		{
			char name[28];
			name[0] = '\0';
			if (diskBtn == DRIVE_HD)
				BaseTitle(machine->device.hdd7.ImageName(), name, 27);
			else
				BaseTitle(machine->device.GetDiskName(diskBtn).c_str(), name, 27);
			Trace("disks: [%s] %s\n", driveLabels[diskBtn], name[0] ? name : "(empty)");
		}
		else if (count > 0)
		{
			char label[SUP_NAME_LEN + 4];
			ListLabel(diskSel, label, sizeof(label));
			Trace("disks: file %s\n", label);
		}

		SupKey key = WaitKey();
		if (key == K_ESC)
			return;
		if (key == K_F1)
		{
			closeAll = true;
			return;
		}

		if (diskInDrives)
		{
			switch (key)
			{
				case K_LEFT:  if (diskBtn > 0) diskBtn--; break;
				case K_RIGHT: if (diskBtn < DRIVE_COUNT - 1) diskBtn++; break;
				case K_DOWN:
				case K_TAB:   if (count > 0) diskInDrives = false; break;
				case K_ENTER: UnmountDialog(diskBtn); full = true; break;
				default: break;
			}
		}
		else
		{
			switch (key)
			{
				case K_UP:
					if (diskSel > 0)
						ListMove(key, count, DISK_LIST_ROWS, diskSel, diskTop);
					else
						diskInDrives = true;
					break;
				case K_TAB:
					diskInDrives = true;
					break;
				case K_ENTER:
					if (Activate(diskSel))
						diskSel = diskTop = 0;
					if (ListCount() == 0)
						diskInDrives = true;
					full = true;
					break;
				default:
					ListMove(key, count, DISK_LIST_ROWS, diskSel, diskTop);
					break;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Setup

enum { SET_MACHINE, SET_KEYBOARD, SET_SERIAL, SET_CAPTURE, SET_SPEED, SET_DEBUG, SET_COUNT };

static const char* const serialNames[3] = { "Not installed", "Slot 1 (printer)", "Slot 2 (modem)" };

static void SetupRow(int idx, Row& row)
{
	switch (idx)
	{
		case SET_MACHINE:
			strcpy(row.label, "Machine");
			strlcpy(row.value, popupMachine->profile.name, sizeof(row.value));
			break;
		case SET_KEYBOARD:
			strcpy(row.label, "Keyboard");
			strlcpy(row.value, GetKeyboardLayoutProfile(CurrentKeyboardLayoutId())->name, sizeof(row.value));
			break;
		case SET_SERIAL:
		{
			strcpy(row.label, "Serial card");
			int slot = popupMachine->device.SerialSlot();
			if (slot)
				snprintf(row.value, sizeof(row.value), "Slot %d", slot);
			else
				strcpy(row.value, "Not installed");
			break;
		}
		case SET_CAPTURE:
			strcpy(row.label, "SD capture");
			strcpy(row.value, Settings::LoadPrintCapture(false) ? "ON" : "OFF");
			break;
		case SET_SPEED:
			strcpy(row.label, "Speed");
			strcpy(row.value, popupMachine->device.speedMode == FramePacer::SPEED_MAX ? "MAX" : "1X");
			break;
		case SET_DEBUG:
			strcpy(row.label, "Debug log");
			strcpy(row.value, debugOn ? "ON" : "OFF");
			break;
	}
}

static int SetupIcon(int idx)
{
	static const uint8_t icons[SET_COUNT] =
		{ ICON_MACHINE, ICON_KEYBOARD, ICON_SERIAL, ICON_CAPTURE, ICON_SPEED, ICON_SPIDER };
	return icons[idx];
}

void Supervisor::SetupMenu()
{
	int sel = 0;
	while (!closeAll)
	{
		sel = RunList("Setup", SET_COUNT, SetupRow, SetupIcon, sel);
		if (sel < 0)
			return;
		switch (sel)
		{
			case SET_MACHINE:  MachineMenu(); break;
			case SET_KEYBOARD: KeyboardMenu(); break;
			case SET_SERIAL:   SerialMenu(); break;
			case SET_CAPTURE:
			{
				// the capture file follows the setting at once
				bool capture = !Settings::LoadPrintCapture(false);
				if (Settings::SavePrintCapture(capture))
					machine->SetSerialSlot(machine->device.SerialSlot(), capture);
				else
					Notice("Not saved", "Cannot write settings (NVS).", nullptr);
				break;
			}
			case SET_SPEED:
				machine->device.speedMode =
					machine->device.speedMode == FramePacer::SPEED_MAX ? FramePacer::SPEED_1X : FramePacer::SPEED_MAX;
				break;
			case SET_DEBUG:
				debugOn = !debugOn;          // saved when the menu closes
				if (debugOn)
					FlushSerialKeys();
				break;
		}
	}
}

// Machine model. Switching saves the choice and the mounted disks in NVS and
// restarts the ESP32, so memory is laid out from scratch for the new model;
// setup() mounts the disks again.

static const char* machineMissing[MACHINE_COUNT];   // first missing ROM per model, NULL = bootable

static void MachineRow(int idx, Row& row)
{
	strlcpy(row.label, GetMachineProfile(idx).name, sizeof(row.label));
	if (idx == popupMachine->profile.id)
		strcpy(row.value, "active");
	else if (machineMissing[idx])
	{
		// a model whose ROMs are not in /roms cannot be picked
		strcpy(row.value, "needs ROM");
		row.dim = true;
	}
}

static int MachineIcon(int) { return ICON_MACHINE; }

void Supervisor::MachineMenu()
{
	for (int i = 0; i < MACHINE_COUNT; i++)
		machineMissing[i] = RomLoader::FirstMissing(GetMachineProfile(i));

	int id = RunList("Machine", MACHINE_COUNT, MachineRow, MachineIcon, machine->profile.id);
	if (id < 0 || id == machine->profile.id)
		return;

	char msg[40];
	if (machineMissing[id])
	{
		Notice("ROM missing", "Copy this file to " ROM_DIR ":", machineMissing[id]);
		return;
	}
	snprintf(msg, sizeof(msg), "%s?", GetMachineProfile(id).name);
	if (!Confirm("Switch machine", "Restart the board as", msg))
		return;

	// restarting without the choice saved would only come back as this model
	if (!Settings::SaveMachine(id))
	{
		Notice("Not saved", "Cannot write settings (NVS).", nullptr);
		return;
	}
	Settings::SaveDisk(0, machine->device.GetDiskName(0).c_str());
	Settings::SaveDisk(1, machine->device.GetDiskName(1).c_str());
	Settings::SaveHardDisk(machine->device.hdd7.ImageName());
	SaveSettings();

	fabgl::Canvas& cv = Canvas;
	DrawFrame(cv, "Machine", "");
	snprintf(msg, sizeof(msg), "Restarting as %s...", GetMachineProfile(id).name);
	cv.setBrushColor(T_BG);
	cv.setPenColor(T_YELLOW);
	TextCentered(cv, &fabgl::FONT_8x14, BOX_X, BOX_W, 92, msg);
	cv.waitCompletion();
	Trace("menu: restarting as %s\n", GetMachineProfile(id).name);
	delay(400);                              // long enough to read
	Bootloader::Restart();                   // back into this app, not the boot menu
}

// Keyboard layout. It only changes how FabGL turns scancodes into
// characters, so it takes effect as soon as it is chosen.

static void KeyboardRow(int idx, Row& row)
{
	strlcpy(row.label, GetKeyboardLayoutProfile(idx)->name, sizeof(row.label));
	if (idx == CurrentKeyboardLayoutId())
		strcpy(row.value, "active");
}

static int KeyboardIcon(int) { return ICON_KEYBOARD; }

void Supervisor::KeyboardMenu()
{
	int id = RunList("Keyboard layout", KEYBOARD_LAYOUT_COUNT, KeyboardRow, KeyboardIcon,
	                 CurrentKeyboardLayoutId());
	if (id >= 0)
		ApplyKeyboardLayout((uint8_t)id, keyboard_ptr);
}

// Super Serial Card. Its line is the ESP32's USB serial port, so installing
// it takes the port from the firmware log and from the menu's serial keys.
// Like mounting a disk it takes effect at once; the Apple sees the change at
// its next PR#/IN#.

static void SerialRow(int idx, Row& row)
{
	strcpy(row.label, serialNames[idx]);
	if (idx == popupMachine->device.SerialSlot())
		strcpy(row.value, "active");
}

static int SerialIcon(int) { return ICON_SERIAL; }

void Supervisor::SerialMenu()
{
	int cur = machine->device.SerialSlot();
	int slot = RunList("Serial card", 3, SerialRow, SerialIcon, (cur >= 1 && cur <= 2) ? cur : 0);
	if (slot < 0 || slot == cur)
		return;

	if (slot && RomLoader::Check(SSC_ROM, SSC_ROM_SIZE) != ROM_OK)
	{
		Notice("ROM missing", "Copy this file to " ROM_DIR ":", SSC_ROM);
		return;
	}
	if (!machine->SetSerialSlot(slot, Settings::LoadPrintCapture(false)))
	{
		Notice("Serial card", "That slot is taken.", nullptr);
		return;
	}
	serialKeys = (slot == 0);
	if (serialKeys)
		FlushSerialKeys();
	if (!Settings::SaveSerial((uint8_t)slot))
		Notice("Not saved", "Cannot write settings (NVS).", nullptr);
}
