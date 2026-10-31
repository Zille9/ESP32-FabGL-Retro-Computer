/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : Supervisor.h
 *  Module : F1 supervisor menu (interface). The menu's screens,
 *           and the SD directory listing the disk manager shows.
 * ============================================================
*/

#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include "../AppleII/Predef.h"
#include "../AppleII/MachineProfile.h"

class Apple2Machine;

#define SUP_MAX_ENTRIES 256
#define SUP_NAME_LEN    64
#define SUP_PATH_LEN    256

// one SD directory entry shown in the disk manager
struct SupEntry
{
	char name[SUP_NAME_LEN];
	bool isDir;
};

// Tiled menu over the paused Apple picture: disks, setup, reset, about
class Supervisor
{
public:
	Supervisor(Apple2Machine* machine);

	void Open() { requested = true; }    // the menu starts at the next frame
	bool Requested() { return requested; }
	void Run();                          // returns when the menu is closed

private:
	Apple2Machine* machine;
	bool requested;
	bool bootNoteShown;                  // the boot fallback note is said once

	// what the menu changes at once and saves to NVS on the way out
	uint8_t keyboard0, speed0;
	bool debug0;
	void SaveSettings();

	// SD directory shown in the disk manager. The entries live in PSRAM,
	// allocated the first time the disk manager opens.
	SupEntry* entries;
	int entryCount;
	const char* listError;               // shown in place of the list, NULL = none
	char curPath[SUP_PATH_LEN];
	// disk manager cursor: the focused drive button, the list row and scroll
	int diskBtn, diskSel, diskTop;
	bool diskInDrives;                   // focus is on the buttons, not the list

	int  MainMenu(int selected);
	void DrawMain(int selected);
	void AboutScreen();
	void SetupMenu();
	void MachineMenu();
	void KeyboardMenu();
	void SerialMenu();

	void DiskMenu();
	bool AtRoot() { return curPath[1] == '\0'; }
	void ScanDir();
	int  ListCount();
	void ListLabel(int index, char* out, int outlen);
	void DrawDisk();                     // the whole disk manager screen
	void DrawDriveButton(int btn, bool focused);
	void DrawListRow(int index, int top, bool hl);
	const char* DriveImage(int btn, char* buf, int buflen);
	bool Activate(int index);            // Enter on a list row; true = directory changed
	void MountDialog(const char* path);
	void UnmountDialog(int btn);
};

#endif
