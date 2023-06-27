#ifndef ESP32_H
#define ESP32_H

// SPI_DRIVER_SELECT must be set to 0 in SdFat/SdFatConfig.h (default is 0)

SdFat SD;
#define SS 13. //Bug fix, you have to define this because later it is used by SD.init. This was missed out on the master branch.
#define SPIINIT 14,35,12,SS // Olimex_ESP32_SBC_FabGL (sck, miso, mosi, cs)
#define SPIINIT_TXT "14,35,12,13"
#define SDINIT SS, SD_SCK_MHZ(SDMHZ)

// #define ENABLE_DEDICATED_SPI 1

#define SDMHZ 19
#define SDMHZ_TXT "19"
#define LED 32 // TTGO_VGA32=32_NC 
#define LEDinv 1
#define BOARD "Olimex ESP32 SBC"
#define board_esp32
#define board_digital_io

uint8 esp32bdos(uint16 dmaaddr) {
	return(0x00);
}

#endif
