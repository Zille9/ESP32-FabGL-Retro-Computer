#ifndef ESP32_H
#define ESP32_H

SdFat SD;
#define SS 13. //Bug fix, you have to define this because later it is
used by SD.init. This was missed out on the master branch.
#define SDMHZ 25 // TTGO_T1,LOLIN32_Pro=25 ePaper,ESP32_DevKit=20
#define SDMHZ_TXT "25" // TTGO_T1,LOLIN32_Pro=25 ePaper,ESP32_DevKit=20
#define LED 5 // TTGO_VGA32=32_NC
#define LEDinv 0
#define BOARD "Homemade VGA32"
#define board_esp32
#define board_digital_io
#define SPIINIT 14,25,2,SS // TTGO_VGA32 (sck, miso, mosi, cs)
#define SPIINIT_TXT "14,25,2,13" // TTGO_VGA32 (sck, miso, mosi, cs)
#define SDINIT SS, SD_SCK_MHZ(SDMHZ)
