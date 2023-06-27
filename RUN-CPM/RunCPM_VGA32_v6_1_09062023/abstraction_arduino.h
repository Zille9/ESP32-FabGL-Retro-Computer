#ifndef ABSTRACT_H
#define ABSTRACT_H

#ifdef PROFILE
#define printf(a, b) Serial.println(b)
#endif

#if defined ARDUINO_SAM_DUE || defined ADAFRUIT_GRAND_CENTRAL_M4
#define HostOS 0x01
#endif
#if defined CORE_TEENSY
#define HostOS 0x04
#endif
#if defined ESP32
#define HostOS 0x05
#endif
#if defined _STM32_DEF_
#define HostOS 0x06
#endif

/* Memory abstraction functions */
/*===============================================================================*/
bool _RamLoad(char* filename, uint16 address) {
	File32 f;
	bool result = false;

	if (f = SD.open(filename, FILE_READ)) {
		while (f.available())
			_RamWrite(address++, f.read());
		f.close();
		result = true;
	}
	return(result);
}

/* Filesystem (disk) abstraction fuctions */
/*===============================================================================*/
File32 rootdir, userdir;
#define FOLDERCHAR '/'

typedef struct {
	uint8 dr;
	uint8 fn[8];
	uint8 tp[3];
	uint8 ex, s1, s2, rc;
	uint8 al[16];
	uint8 cr, r0, r1, r2;
} CPM_FCB;

typedef struct {
	uint8 dr;
	uint8 fn[8];
	uint8 tp[3];
	uint8 ex, s1, s2, rc;
	uint8 al[16];
} CPM_DIRENTRY;

static DirFat_t fileDirEntry;



bool _sys_exists(uint8* filename) {
	return(SD.exists((const char *)filename));
}


File32 _sys_fopen_w(uint8* filename) {
	return(SD.open((char*)filename, O_CREAT | O_WRITE));
}

int _sys_fputc(uint8 ch, File32& f) {
	return(f.write(ch));
}

void _sys_fflush(File32& f) {
	f.flush();
}

void _sys_fclose(File32& f) {
	f.close();
}

int _sys_select(uint8* disk) {
	uint8 result = FALSE;
	File32 f;

	digitalWrite(LED, HIGH ^ LEDinv);
	if (f = SD.open((char*)disk, O_READ)) {
		if (f.isDirectory())
			result = TRUE;
		f.close();
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

long _sys_filesize(uint8* filename) {
	long l = -1;
	File32 f;

	digitalWrite(LED, HIGH ^ LEDinv);
	if (f = SD.open((char*)filename, O_RDONLY)) {
		l = f.size();
		f.close();
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(l);
}

int _sys_openfile(uint8* filename) {
	File32 f;
	int result = 0;

	digitalWrite(LED, HIGH ^ LEDinv);
	f = SD.open((char*)filename, O_READ);
	if (f) {
		f.dirEntry(&fileDirEntry);
		f.close();
		result = 1;
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

int _sys_makefile(uint8* filename) {
	File32 f;
	int result = 0;

	digitalWrite(LED, HIGH ^ LEDinv);
	f = SD.open((char*)filename, O_CREAT | O_WRITE);
	if (f) {
		f.close();
		result = 1;
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

int _sys_deletefile(uint8* filename) {
	digitalWrite(LED, HIGH ^ LEDinv);
	return(SD.remove((char*)filename));
	digitalWrite(LED, LOW ^ LEDinv);
}

int _sys_renamefile(uint8* filename, uint8* newname) {
	File32 f;
	int result = 0;

	digitalWrite(LED, HIGH ^ LEDinv);
	f = SD.open((char*)filename, O_WRITE | O_APPEND);
	if (f) {
    if (f.rename((char*)newname)) {
			f.close();
			result = 1;
		}
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

#ifdef DEBUGLOG
void _sys_logbuffer(uint8* buffer) {
#ifdef CONSOLELOG
	puts((char*)buffer);
#else
	File32 f;
	uint8 s = 0;
	while (*(buffer + s))	// Computes buffer size
		++s;
	if (f = SD.open(LogName, O_CREAT | O_APPEND | O_WRITE)) {
		f.write(buffer, s);
		f.flush();
		f.close();
	}
#endif
}
#endif

bool _sys_extendfile(char* fn, unsigned long fpos)
{
	uint8 result = true;
	File32 f;
	unsigned long i;

	digitalWrite(LED, HIGH ^ LEDinv);
	if (f = SD.open(fn, O_WRITE | O_APPEND)) {
		if (fpos > f.size()) {
			for (i = 0; i < f.size() - fpos; ++i) {
				if (f.write((uint8)0) != 1) {
					result = false;
					break;
				}
			}
		}
		f.close();
	} else {
		result = false;
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

uint8 _sys_readseq(uint8* filename, long fpos) {
	uint8 result = 0xff;
	File32 f;
	uint8 bytesread;
	uint8 dmabuf[BlkSZ];
	uint8 i;

	digitalWrite(LED, HIGH ^ LEDinv);
	f = SD.open((char*)filename, O_READ);
	if (f) {
		if (f.seek(fpos)) {
			for (i = 0; i < BlkSZ; ++i)
				dmabuf[i] = 0x1a;
			bytesread = f.read(&dmabuf[0], BlkSZ);
			if (bytesread) {
				for (i = 0; i < BlkSZ; ++i)
					_RamWrite(dmaAddr + i, dmabuf[i]);
			}
			result = bytesread ? 0x00 : 0x01;
		} else {
			result = 0x01;
		}
		f.close();
	} else {
		result = 0x10;
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

uint8 _sys_writeseq(uint8* filename, long fpos) {
	uint8 result = 0xff;
	File32 f;

	digitalWrite(LED, HIGH ^ LEDinv);
	if (_sys_extendfile((char*)filename, fpos))
		f = SD.open((char*)filename, O_RDWR);
	if (f) {
		if (f.seek(fpos)) {
			if (f.write(_RamSysAddr(dmaAddr), BlkSZ))
				result = 0x00;
		} else {
			result = 0x01;
		}
		f.close();
	} else {
		result = 0x10;
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

uint8 _sys_readrand(uint8* filename, long fpos) {
	uint8 result = 0xff;
	File32 f;
	uint8 bytesread;
	uint8 dmabuf[BlkSZ];
	uint8 i;
	long extSize;

	digitalWrite(LED, HIGH ^ LEDinv);
	f = SD.open((char*)filename, O_READ);
	if (f) {
		if (f.seek(fpos)) {
			for (i = 0; i < BlkSZ; ++i)
				dmabuf[i] = 0x1a;
			bytesread = f.read(&dmabuf[0], BlkSZ);
			if (bytesread) {
				for (i = 0; i < BlkSZ; ++i)
					_RamWrite(dmaAddr + i, dmabuf[i]);
			}
			result = bytesread ? 0x00 : 0x01;
		} else {
			if (fpos >= 65536L * BlkSZ) {
				result = 0x06;	// seek past 8MB (largest file size in CP/M)
			} else {
				extSize = f.size();
				// round file size up to next full logical extent
				extSize = ExtSZ * ((extSize / ExtSZ) + ((extSize % ExtSZ) ? 1 : 0));
				if (fpos < extSize)
					result = 0x01;	// reading unwritten data
				else
					result = 0x04; // seek to unwritten extent
			}
		}
		f.close();
	} else {
		result = 0x10;
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

uint8 _sys_writerand(uint8* filename, long fpos) {
	uint8 result = 0xff;
	File32 f;

	digitalWrite(LED, HIGH ^ LEDinv);
	if (_sys_extendfile((char*)filename, fpos)) {
		f = SD.open((char*)filename, O_RDWR);
	}
	if (f) {
		if (f.seek(fpos)) {
			if (f.write(_RamSysAddr(dmaAddr), BlkSZ))
				result = 0x00;
		} else {
			result = 0x06;
		}
		f.close();
	} else {
		result = 0x10;
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

static uint8 findNextDirName[13];
static uint16 fileRecords = 0;
static uint16 fileExtents = 0;
static uint16 fileExtentsUsed = 0;
static uint16 firstFreeAllocBlock;

uint8 _findnext(uint8 isdir) {
	File32 f;
	uint8 result = 0xff;
	bool isfile;
	uint32 bytes;

	digitalWrite(LED, HIGH ^ LEDinv);
	if (allExtents && fileRecords) {
		_mockupDirEntry();
		result = 0;
	} else {
		while (f = userdir.openNextFile()) {
			f.getName((char*)&findNextDirName[0], 13);
			isfile = !f.isDirectory();
			bytes = f.size();
			f.dirEntry(&fileDirEntry);
			f.close();
			if (!isfile)
				continue;
			_HostnameToFCBname(findNextDirName, fcbname);
			if (match(fcbname, pattern)) {
				if (isdir) {
					// account for host files that aren't multiples of the block size
					// by rounding their bytes up to the next multiple of blocks
					if (bytes & (BlkSZ - 1)) {
						bytes = (bytes & ~(BlkSZ - 1)) + BlkSZ;
					}
					fileRecords = bytes / BlkSZ;
					fileExtents = fileRecords / BlkEX + ((fileRecords & (BlkEX - 1)) ? 1 : 0);
					fileExtentsUsed = 0;
					firstFreeAllocBlock = firstBlockAfterDir;
					_mockupDirEntry();
				} else {
					fileRecords = 0;
					fileExtents = 0;
					fileExtentsUsed = 0;
					firstFreeAllocBlock = firstBlockAfterDir;
				}
				_RamWrite(tmpFCB, filename[0] - '@');
				_HostnameToFCB(tmpFCB, findNextDirName);
				result = 0x00;
				break;
			}
		}
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

uint8 _findfirst(uint8 isdir) {
	uint8 path[4] = { '?', FOLDERCHAR, '?', 0 };
	path[0] = filename[0];
	path[2] = filename[2];
	if (userdir)
		userdir.close();
	userdir = SD.open((char*)path); // Set directory search to start from the first position
	_HostnameToFCBname(filename, pattern);
	fileRecords = 0;
	fileExtents = 0;
	fileExtentsUsed = 0;
	return(_findnext(isdir));
}

uint8 _findnextallusers(uint8 isdir) {
	uint8 result = 0xFF;
	char dirname[13];
	bool done = false;

	while (!done) {
		while (!userdir) {
			userdir = rootdir.openNextFile();
			if (!userdir) {
				done = true;
				break;
			}
			userdir.getName(dirname, sizeof dirname);
			if (userdir.isDirectory() && strlen(dirname) == 1 && isxdigit(dirname[0])) {
				currFindUser = dirname[0] <= '9' ? dirname[0] - '0' : toupper(dirname[0]) - 'A' + 10;
				break;
			}
			userdir.close();
		}
		if (userdir) {
			result = _findnext(isdir);
			if (result) {
				userdir.close();
			} else {
				done = true;
			}
		} else {
			result = 0xFF;
			done = true;
		}
	}
	return result;
}

uint8 _findfirstallusers(uint8 isdir) {
	uint8 path[2] = { '?', 0 };

	path[0] = filename[0];
	if (rootdir)
		rootdir.close();
	if (userdir)
		userdir.close();
	rootdir = SD.open((char*)path); // Set directory search to start from the first position
	strcpy((char*)pattern, "???????????");
	if (!rootdir)
		return 0xFF;
	fileRecords = 0;
	fileExtents = 0;
	fileExtentsUsed = 0;
	return(_findnextallusers(isdir));
}

uint8 _Truncate(char* filename, uint8 rc) {
	File32 f;
	int result = 0;

	digitalWrite(LED, HIGH ^ LEDinv);
	f = SD.open((char*)filename, O_WRITE | O_APPEND);
	if (f) {
		if (f.truncate(rc * BlkSZ)) {
			f.close();
			result = 1;
		}
	}
	digitalWrite(LED, LOW ^ LEDinv);
	return(result);
}

void _MakeUserDir() {
	uint8 dFolder = cDrive + 'A';
	uint8 uFolder = toupper(tohex(userCode));

	uint8 path[4] = { dFolder, FOLDERCHAR, uFolder, 0 };

	digitalWrite(LED, HIGH ^ LEDinv);
	SD.mkdir((char*)path);
	digitalWrite(LED, LOW ^ LEDinv);
}

uint8 _sys_makedisk(uint8 drive) {
	uint8 result = 0;
	if (drive < 1 || drive>16) {
		result = 0xff;
	} else {
		uint8 dFolder = drive + '@';
		uint8 disk[2] = { dFolder, 0 };
		digitalWrite(LED, HIGH ^ LEDinv);
		if (!SD.mkdir((char*)disk)) {
			result = 0xfe;
		} else {
			uint8 path[4] = { dFolder, FOLDERCHAR, '0', 0 };
			SD.mkdir((char*)path);
		}
		digitalWrite(LED, LOW ^ LEDinv);
	}

	return(result);
}


/* Hardware abstraction functions */
/*===============================================================================*/
void _HardwareOut(const uint32 Port, const uint32 Value) {

}

uint32 _HardwareIn(const uint32 Port) {
	return 0;
}


/* Console abstraction functions */
/*===============================================================================*/

// =========================================================================================
// _kbhit
// =========================================================================================

int _kbhit(void) {

if (SERCTL == true)
{
return(Serial.available());
}
else
{
return(Terminal.available()); 
}

}
// =========================================================================================
// _getch
// =========================================================================================

uint8 _getch(void) {

if (SERCTL == true)
{
while (!Serial.available());
return(Serial.read());  
}
else
{
while (!Terminal.available());
return(Terminal.read()); 
}

}
// =========================================================================================
// _getche
// =========================================================================================

uint8 _getche(void) {
  uint8 ch = _getch();

                    #ifdef FABGL                   
                    Terminal.write(ch);
                    #endif

if (SERMIR == true) {
                    Serial.write(ch);
                    }   
return(ch);
}

// =========================================================================================
// _putch
// =========================================================================================

void _putch(uint8 ch) {

// -----------------------------------------------------------------------------------------
// get ASCII-Val of ch in sfilt
// ----------------------------------------------------------------------------------------- 
byte SERFLT_BYTE = ch;

// -----------------------------------------------------------------------------------------
// if CHR$(254) is printed and SERFLT is off/false - switch it to on/true
// ----------------------------------------------------------------------------------------- 
if (SERFLT_BYTE == 254 && SERFLT == false) 
                                      {
                                      // Serial.println("== DEBUG 254 detected - SFLIT switched to on  ==");
                                      SERFLT = true;
                                      }

// -----------------------------------------------------------------------------------------
// if CHR$(255) is printed and SERFLT is on/true - switch it to off/false
// ----------------------------------------------------------------------------------------- 
if (SERFLT_BYTE == 255 && SERFLT == true) {
                                      // Serial.println("== DEBUG 255 detected - SFLIT switched to off ==");  
                                      SERFLT = false;
                                      }

// -----------------------------------------------------------------------------------------
// print chars < 254 to USBSerial if SerialMirror is active
// ----------------------------------------------------------------------------------------- 
if (SERFLT_BYTE < 254 && SERMIR == true)                      {
                                                        Serial.write(ch);
                                                        } 

// -----------------------------------------------------------------------------------------
// print chars < 254 to USBSerial if SerialMirror is deactived, but SERFLT is true
// -----------------------------------------------------------------------------------------
if (SERFLT_BYTE < 254 && SERMIR == false && SERFLT == true) {
                                                        Serial.write(ch);
                                                        } 

// -----------------------------------------------------------------------------------------
// print chars < 254 to VGA-Terminal if SerialMirror is active
// ----------------------------------------------------------------------------------------- 
if (SERFLT_BYTE < 254) {
                                   #ifdef FABGL
                                   Terminal.write(ch);
                                   #endif
                                   }

}

// =========================================================================================
// _clrscr
// =========================================================================================

void _clrscr(void) {

// -----------------------------------------------------------------------------------------
// if FABGL is defined send the clear-screem-sequence to the VGA-Terminal
// -----------------------------------------------------------------------------------------
                                      #ifdef FABGL
                                      Terminal.clear();
                                      #endif

// -----------------------------------------------------------------------------------------
// if SerialMirror is active send the clear-screem-sequence to the USBSerial
// -----------------------------------------------------------------------------------------
                   if (SERMIR == true) {
                                       Serial.println("\e[H\e[J");
                                       }

                   }

// =========================================================================================
// _esp_reboot
// =========================================================================================

void _esp_reboot(void) {
     _clrscr();
      ESP.restart();
}

// =========================================================================================
// _pref_clr
// =========================================================================================

void _pref_clr(void) {
      preferences.clear();
}


// =========================================================================================

#endif
