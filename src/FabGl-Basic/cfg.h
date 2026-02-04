//******** select Board-Type - for ESP-Dev-Module deactivate TTGO_T7 ******
//#define TTGO_T7 //TTGO VGA32-Board

//******** select Displaytype - only one allowed **************************
//#define AVOUT
#define VGA64

//#define ILI9341
#ifdef ILI9341
#define TFT_SCK    18
#define TFT_MOSI   23
#define TFT_CS     5
#define TFT_DC     22
#define TFT_RESET  21
#define TFT_SPIBUS VSPI_HOST
#define RAMEND 30000  //bei Verwendung ILI9341 muss der Ram verkleinert werden
#else
#define RAMEND 50000  //um MP3-Funtionalität in allen Auflösungen zu garantieren, sollte der Wert nicht grösser als 52000 sein
#endif
#define kRamSize  RAMEND

//******** select for CardKB with PS2/Software ****************************
//#define CardKB

//******** define SD-Card-Pins ********************************************
#ifdef TTGO_T7
#define kSD_MISO 2
#define kSD_MOSI 12
#define SD_LED -1
#else
#define kSD_CS   13
#define kSD_MISO 16
#define kSD_MOSI 17
#define kSD_CLK  14
#endif

//**************** Keyboard-Pins ******************************************
#define keyb_clk   33
#define keyb_data  32
#define KLayout    3  // 1=US, 2=UK, 3=GE, 4=IT, 5=ES, 6=FR, 7=BE, 8=NO, 9=JP
//#ifdef CardKB         //CardKB required Japanese Layout
//#define KLayout 9
//#endif
//**************** Akku-Alarm *********************************************
//#define Akkualarm_enabled

//**************** Grösse des SPI-RAM'S festlegen *************************
//#define SPI_RAM_SIZE 4 //SPI_RAM_SIZE * 128k -> 23LC1024 = 1 , FRAM 512k = 4, PSRAM 8MB = 64
