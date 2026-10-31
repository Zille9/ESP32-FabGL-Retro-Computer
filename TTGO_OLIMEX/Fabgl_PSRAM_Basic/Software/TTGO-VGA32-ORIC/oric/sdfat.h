#include "sdfat_d.h"

#undef THIS 
#define THIS System.SdFat

// SPI dedicado ao SD, construido UMA unica vez como global.
// NUNCA fazer "THIS.spi = SPIClass(VSPI)" - copiar um SPIClass por atribuicao
// deixa o handle de mutex/bus interno em estado invalido e o SD.begin() trava.
// HSPI (nao VSPI): mesmo barramento do bootloader que funciona nessa placa.
// No HSPI o SCK=14 cai no pino IOMUX nativo; no VSPI tudo passa pela matriz
// de GPIO e o cartao nao responde ao CMD0.
static SPIClass sdSPI(HSPI);

//------------------------------------- OTA-Update-Lib --------------------------------------------------------------------------------------------
#include <Update.h>
//-------------------------------------------------------------------------------------------------------------------------------------------------



void sdFatInit() {
	Serial.printf("[SD] sdFatInit: CS=%d CLK=%d MISO=%d MOSI=%d\n",
		SD_PIN_NUM_CS, SD_PIN_NUM_CLK, SD_PIN_NUM_MISO, SD_PIN_NUM_MOSI);
	sdSPI.begin(SD_PIN_NUM_CLK, SD_PIN_NUM_MISO, SD_PIN_NUM_MOSI, SD_PIN_NUM_CS);
}

bool sdFatMount() {
	Serial.printf("[SD] sdFatMount: CS=%d CLK=%d MISO=%d MOSI=%d\n",
		SD_PIN_NUM_CS, SD_PIN_NUM_CLK, SD_PIN_NUM_MISO, SD_PIN_NUM_MOSI);

	// 10MHz para inicializacao confiavel. 40MHz pode travar/falhar em alguns cartoes.
	if (!SD.begin(SD_PIN_NUM_CS, sdSPI, 10000000, "/sd")) {
		Serial.println("[SD] Card Mount failed (SD.begin returned false)");
		THIS.mount = false;
	} else {
		uint8_t cardType = SD.cardType();
		uint64_t cardSize = SD.cardSize() / (1024 * 1024);
		Serial.printf("[SD] Card Mount done. type=%d size=%lluMB\n", cardType, cardSize);
		THIS.mount = true;
	}
	return THIS.mount;
}

void sdFatUnMount() {
	if (THIS.mount) {
		SD.end();
		THIS.mount = false;
	}
}

void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}

void removeDir(fs::FS &fs, const char * path){
    Serial.printf("Removing Dir: %s\n", path);
    if(fs.rmdir(path)){
        Serial.println("Dir removed");
    } else {
        Serial.println("rmdir failed");
    }
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
	Serial.printf("Listing directory: %s\n", dirname);

	File root = fs.open(dirname);
	if (!root) {
		Serial.println("Failed to open directory");
		return;
	}
	if (!root.isDirectory()) {
		Serial.println("Not a directory");
		return;
	}

	File file = root.openNextFile();
	while (file) {
		if (file.isDirectory()) {
			Serial.print("  DIR : ");
			Serial.println(file.name());
			if (levels) {
				listDir(fs, file.name(), levels - 1);
			}
		} else {
			Serial.print("  FILE: ");
			Serial.print(file.name());
			Serial.print("  SIZE: ");
			Serial.println(file.size());
		}
		file = root.openNextFile();
	}
}

//------------------------------------- Loader für Bin-Dateien -----------------------------------------------------------------------------
void performUpdate(Stream &updateSource, size_t updateSize) {

  if (Update.begin(updateSize, U_FLASH)) {
    size_t written = Update.writeStream(updateSource);
    if (Update.end()) {
      if (Update.isFinished()) {
        delay(1000);
        ESP.restart();
      }
    }
  }
}
//*********************************
void load_binary() {

  if ( !SD.exists("/basic.bin") ) Serial.println("Basic.bin not found!");
  File updateBin = SD.open("/basic.bin");
  Serial.println("load Basic...");
  
  if (updateBin) {
    size_t updateSize = updateBin.size();
    Serial.println(updateSize, DEC);
    if (updateSize > 0) {
      performUpdate(updateBin, updateSize);
    }
    updateBin.close();
  }
}
