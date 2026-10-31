/*
 * ============================================================
 *        APPLE II Emulator for ESP32-TTGO-VGA
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/ESP32-TTGO-VGA_AppleII_Emulator
 *   Based on codesafe/ESP32-VGA_AppleII_Emulator , co-developed with Claude Code
 *   MIT License
 * ============================================================
 *  File   : SD.h (host shim)
 *  Module : Stand-in for the ESP32 SD library: a card that lives in a
 *           std::map, in HostFiles(). Reads of a path nothing has
 *           written fail, as they did when this shim was only there to
 *           satisfy Tools/FileSystem.h at compile time; a test that
 *           wants to read a written file looks in HostFiles() itself.
 *           "r+" opens an existing file for update in place: seek()
 *           and write() overwrite at the position, as on the ESP32.
 * ============================================================
*/

#pragma once

#include "Arduino.h"

#include <map>
#include <string>

#define FILE_READ   "r"
#define FILE_WRITE  "w"
#define FILE_APPEND "a"

// The whole card: path -> contents
inline std::map<std::string, std::string>& HostFiles()
{
	static std::map<std::string, std::string> files;
	return files;
}

class File
{
public:
	File() : path(), opened(false), writing(false), pos(0) {}
	// truncate: "w" empties the file; atEnd: "a" writes after what is there.
	// "r+" does neither: it reads and overwrites in place.
	File(const char* p, bool write, bool truncate, bool atEnd)
		: path(p), opened(true), writing(write), pos(0)
	{
		if (truncate)
			HostFiles()[path].clear();
		if (atEnd)
			pos = HostFiles()[path].size();
	}

	explicit operator bool() const { return opened; }

	int read(unsigned char* buf, int len)
	{
		if (!opened)
			return -1;
		const std::string& data = HostFiles()[path];
		int n = 0;
		while (n < len && pos < data.size())
			buf[n++] = (unsigned char)data[pos++];
		return n;
	}

	size_t write(const unsigned char* buf, size_t len)
	{
		if (!opened || !writing)
			return 0;
		std::string& data = HostFiles()[path];
		if (data.size() < pos + len)
			data.resize(pos + len);
		data.replace(pos, len, (const char*)buf, len);
		pos += len;
		return len;
	}

	bool seek(size_t p)
	{
		if (!opened)
			return false;
		pos = p;
		return true;
	}

	void flush() {}

	size_t size() { return opened ? HostFiles()[path].size() : 0; }
	void close() { opened = false; }
	bool isDirectory() { return false; }
	File openNextFile() { return File(); }
	const char* name() { return path.c_str(); }

private:
	std::string path;
	bool opened;
	bool writing;
	size_t pos;
};

struct HostSD
{
	File open(const char* path, const char* mode = FILE_READ)
	{
		bool update = (mode[0] == 'r' && mode[1] == '+');
		bool write = update || mode[0] == 'w' || mode[0] == 'a';
		// reading, and updating in place, need the file to exist already
		if ((!write || update) && !HostFiles().count(path))
			return File();
		return File(path, write, mode[0] == 'w', mode[0] == 'a');
	}
	bool exists(const char* path) { return HostFiles().count(path) != 0; }
	bool mkdir(const char*) { return true; }
	void end() {}
};

static HostSD SD;
