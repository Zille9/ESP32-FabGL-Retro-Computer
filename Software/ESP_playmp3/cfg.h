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
#define RAMEND 60928 
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
#define SD_LED   2
#endif

//**************** Keyboard-Pins ******************************************
int keyb_clk  = 33;
int keyb_data = 32;
#define KLayout    3  // 1=US, 2=UK, 3=GE, 4=IT, 5=ES, 6=FR, 7=BE, 8=NO, 9=JP
#ifdef CardKB         //CardKB required Japanese Layout
#define KLayout 9
#endif
//**************** Akku-Alarm *********************************************
//#define Akkualarm_enabled
