#ifndef ESP32_H
#define ESP32_H

// SPI_DRIVER_SELECT must be set to 0 in SdFat/SdFatConfig.h (default is 0)

SdFat SD;
#define SS 13. //Bug fix, you have to define this because later it is used by SD.init. This was missed out on the master branch.
#define SPIINIT 14,16,17,SS // TTGO_VGA32 (sck, miso, mosi, cs)
#define SPIINIT_TXT "14,16,17,13"
#define SDINIT SS, SD_SCK_MHZ(SDMHZ)

// #define ENABLE_DEDICATED_SPI 1

#define SDMHZ 19 // TTGO_T1,LOLIN32_Pro=25 ePaper,ESP32_DevKit=20
#define SDMHZ_TXT "19" // TTGO_T1,LOLIN32_Pro=25 ePaper,ESP32_DevKit=20
#define LED 2 // TTGO_VGA32=32_NC 
#define LEDinv 1
#define BOARD "TTGO VGA32"
#define board_esp32
#define board_digital_io

uint8 esp32bdos(uint16 dmaaddr) {
	return(0x00);
}

#endif
