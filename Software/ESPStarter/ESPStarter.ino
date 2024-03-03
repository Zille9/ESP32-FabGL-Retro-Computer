// **************************************************************************************************************************************************
// *     Programmloader für ESP32 mit Darstellung der Programm-Icons und Auswahl über Pfeiltasten                                                   *
// *     Programmversion 1.0 Author:Reinhard Zielinski  03/2024                                                                                     *
// *     Das Programm ist Bestandteil des ESP32-Retro-Computerprojekts                                                                              *
// *     siehe: https://github.com/Zille9/ESP32-FabGL-Retro-Computer                                                                                *
// **************************************************************************************************************************************************

#include "cfg.h"   //********************************************* Konfigurations-Datei *************************************************************

#include "fabgl.h" //********************************************* Bibliotheken zur VGA-Signalerzeugung *********************************************
fabgl::Terminal         Terminal;
fabgl::LineEditor       LineEditor(&Terminal);


//---------------------------------------- die verschiedenen Grafiktreiber --------------------------------------------------------------------------
#ifdef AVOUT
fabgl::CVBS16Controller VGAController;    //AV-Variante
#define VIDEOOUT_GPIO GPIO_NUM_26         //Ausgabe auf GPIO25 oder 26 möglich ACHTUNG!:Soundausgabe erfolgt auf GPIO25
static const char * MODES_STD[]   = { "I-PAL-B", "P-PAL-B", "I-NTSC-M", "P-NTSC-M", "I-PAL-B-WIDE", "P-PAL-B-WIDE", "I-NTSC-M-WIDE", "P-NTSC-M-WIDE", "P-NTSC-M-EXT",};

#elif defined VGA64
fabgl::VGAController    VGAController;      //VGA-Variante

#else
fabgl::ILI9341Controller VGAController;     //TFT-Display ILI9341
#endif


//------------------------------------------ Tastatur,GFX-Treiber- und Terminaltreiber -------------------------------------------------------------
fabgl::PS2Controller    PS2Controller;
fabgl::Keyboard Keyboard;
fabgl::Canvas           GFX(&VGAController);
TerminalController      tc(&Terminal);

// ---------------------------------- SD-Karten-Zugriff--------------------------------------------------------------------------------------------
#include <SD.h>
#include <SPI.h>

//-------------------------------------- Verwendung der SD-Karte ----------------------------------------------------------------------------------
//SPI CLASS FOR REDEFINED SPI PINS !
SPIClass spiSD(HSPI);
byte Editor_marker = 101;  // EEPROM-Platz 101 beinhaltet den Editor-marker (123)
#define kSD_Fail  0      //Fehler-Marker
#define kSD_OK    1      //OK-Marker
File fp;
//------------------------------------- OTA-Update-Lib --------------------------------------------------------------------------------------------
#include <Update.h>
//------------------------------- BMP-Info-Parameter ----------------------------------------------------------------------------------------------
uint32_t bmp_width, bmp_height;

const char* filetype[] = {".bin", ".bmp"};
static char sd_pfad[2];            //SD-Card Datei-Pfad

char tempstring[40];                //String Zwischenspeicher

const char*  Programs[] PROGMEM = {"Basic32+", "RunCP/M", "   CPC", "  ZX81", "Spectrum", "  KIM1", "Gameboy", "VIC-20", "  KILO", "  Games", "Vectrex", "Altair8800", " Pacman", " Telnet", " PMD-85", "Invaders"};         //Programme
const char* filenames[] PROGMEM = {"basic", "cpm", "cpc", "esp81", "zxesp", "kim1", "gameboy", "Vic20", "kilo", "games", "vectrex", "altair", "pacman", "Telnet", "pmd85", "Invaders"}; //Datei-Namen
byte x_char[]      PROGMEM = {8, 5, 6, 8,  10, 8,  8,  8,  8,  8,  8,  8,  8,  6,  8,  4, 6,  7,  7,  8, 8, 8, 6, 9, 8, 6}; //x-werte der Fontsätze zur Berechnung der Terminalbreite
byte y_char[]      PROGMEM = {8, 8, 8, 14, 20, 14, 14, 16, 16, 14, 14, 14, 16, 10, 14, 6, 12, 13, 14, 9, 14, 14, 10, 15, 16, 8}; //y-werte der Fontsätze zur Berechnung der Terminalhöhe

int filenums = 16;

int Key_x = 0;
int Key_y = 0;
bool Key_enter = false;
int xx_pos[4]  = {3, 12, 21, 30};
int yy_pos[4]  = {10, 16, 22, 28};
bool swap = false;

void setup() {
  // put your setup code here, to run once:
  Keyboard.begin(GPIO_NUM_33, GPIO_NUM_32);
  PS2Controller.keyboard() -> setLayout(&fabgl::GermanLayout);                //deutsche Tastatur
  delay(200);


  //************************************************************ welcher Bildschirmtreiber? *********************************************************
  // 64 colors
#ifdef AVOUT                                                                          //AV-Variante
VGAController.begin(VIDEOOUT_GPIO);
VGAController.setHorizontalRate(2);                                                   //320x240
VGAController.setResolution(MODES_STD[7]);                                            //5 scheint optimal ist aber mit 384x240 nicht 100% kompatibel (3) (7-360x200)

#elif defined VGA64
VGAController.begin();                                                                //VGA-Variante //64 Farben
VGAController.setResolution(QVGA_320x240_60Hz);
//VGAController.setResolution(VGA_400x300_60Hz);
#else                                                                                 //ILI9341
VGAController.begin(TFT_SCK, TFT_MOSI, TFT_DC, TFT_RESET, TFT_CS, TFT_SPIBUS);
VGAController.setResolution(TFT_240x320);
VGAController.setOrientation(fabgl::TFTOrientation::Rotate270);  //Kontakte links
//VGAController.setOrientation(fabgl::TFTOrientation::Rotate90);   //Kontakte rechts
#endif


  //***************************************************************************************************************************************************

  Terminal.begin(&VGAController);
  Terminal.connectLocally();                                                           // für Terminal Komandos

  //Serial.begin(115200);

  Terminal.loadFont(&fabgl::FONT_8x8);
  Terminal.enableCursor(false);
  fcolor(60);
  bcolor(1);
  tc.setCursorPos(1, 1);
  GFX.clear();
  bcolor(11);
  GFX.fillRectangle(5, 5, 315, 235);                    //Rectangle rect x,y,xx,yy,fill=1
  bcolor(63);
  GFX.fillRectangle(10, 10, 310, 230);                    //Rectangle rect x,y,xx,yy,fill=1


  PS2Controller.keyboard()-> onVirtualKey = [&](VirtualKey * vk, bool keyDown) {
    if (*vk == VirtualKey::VK_RIGHT) {
      if (keyDown) {
        Key_x++;
        swap = true;
        if (Key_x > 3) Key_x = 0;
      }
      *vk = VirtualKey::VK_NONE;
    }
    else if (*vk == VirtualKey::VK_LEFT) {                                               //
      if (keyDown) {
        Key_x--;
        swap = true;
        if (Key_x < 0) Key_x = 3;
      }
      *vk = VirtualKey::VK_NONE;
    }
    if (*vk == VirtualKey::VK_UP) {
      if (keyDown) {
        Key_y--;
        swap = true;
        if (Key_y < 0) Key_y = 3;
      }
      *vk = VirtualKey::VK_NONE;
    }
    else if (*vk == VirtualKey::VK_DOWN) {                                               //
      if (keyDown) {
        Key_y++;
        swap = true;
        if (Key_y > 3) Key_y = 0;
      }
      *vk = VirtualKey::VK_NONE;
    }
    else if (*vk == VirtualKey::VK_RETURN) {                                               //
      if (keyDown) {
        Key_enter = true;
      }
      *vk = VirtualKey::VK_NONE;
    }
  };

  //Serial.begin(kConsoleBaud);                                                           // open serial port
  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);             //SCK,MISO,MOSI,SS 13 //HSPI1
  if ( !SD.begin( kSD_CS, spiSD )) {                        //SD-Card starten
    // mount-fehler
    spiSD.end();                                            //unmount
    tc.setCursorPos(1, 1);
    Terminal.write("        Mountfehler");
  }
  spiSD.end();
  sd_pfad[0] = '/';                                         //setze Root-Verzeichnis
  sd_pfad[1] = 0;

  show_screen();
}

void show_screen(void) {
  int a, b;
  String cbuf;

  fcolor(0);

  int y_pos = VGAController.getScreenHeight() / y_char[0];
  int x_pos = VGAController.getScreenWidth() / x_char[0];

  cbuf = "* ESP32-App-Starter *";
  cbuf.toCharArray(tempstring, cbuf.length() + 1);
  drawing_text(1, 50 , 10);
  //Terminal.write("* ESP32-App-Starter *");
  b = -1;
  for (int i = 0; i < filenums; i++) {
    if (i % 4 == 0) {
      a = 0;
      b++;
    }
    else {
      a++;
    }
    cbuf = String(sd_pfad) + String(filenames[i]) + String(filetype[1]);
    cbuf.toUpperCase();                               //String in Grossbuchstaben umwandeln
    cbuf.toCharArray(tempstring, cbuf.length() + 1);
    import_pic((xx_pos[a] * 8) - 8, (yy_pos[b] * 8) - 47, tempstring, 1);

    //tc.setCursorPos(xx_pos[a], yy_pos[b]);
    cbuf = String(Programs[i]);
    cbuf.toCharArray(tempstring, cbuf.length() + 1);
    drawing_text(0, (xx_pos[a] * 8) - 8, (yy_pos[b] * 8) - 7);

  }
  // Basic-Icon invertieren und in der Statuszeile anzeigen
  GFX.swapRectangle((xx_pos[0] * 8) - 8, (yy_pos[0] * 8) - 47, (xx_pos[0] * 8) - 8 + 39, (yy_pos[0] * 8) - 47 + 39); //swap Backcolor
  status_text(Programs[0]);
}

void drawing_text(int fnt, int x_text, int y_text)
{
  switch (fnt) {
    case 0:
      GFX.drawText(&fabgl::FONT_5x8, x_text, y_text, tempstring);
      break;
    case 1:
      GFX.drawText(&fabgl::FONT_10x20, x_text, y_text, tempstring);
      break;

    default:
      GFX.drawText(&fabgl::FONT_8x8, x_text, y_text, tempstring);
      break;
  }
}

int key_press(void) {
  int old_key_x, old_key_y;
  String cbuf;
  while (!Key_enter)
  {
        
    if (swap) {
      status_text(Programs[(Key_y * 4) + Key_x]);
      bcolor(63);
      GFX.swapRectangle((old_key_x * 8) - 8, (old_key_y * 8) - 47, (old_key_x * 8) - 8 + 39, (old_key_y * 8) - 47 + 39); //swap Backcolor
      GFX.swapRectangle((xx_pos[Key_x] * 8) - 8, (yy_pos[Key_y] * 8) - 47, (xx_pos[Key_x] * 8) - 8 + 39, (yy_pos[Key_y] * 8) - 47 + 39); //swap Backcolor
      swap = false;
    }
    old_key_x = xx_pos[Key_x];
    old_key_y = yy_pos[Key_y];
    delay(40);
  }
  cbuf = String(sd_pfad) + String(filenames[(Key_y * 4) + Key_x]) + String(filetype[0]);
  cbuf.toUpperCase();                               //String in Grossbuchstaben umwandeln
  cbuf.toCharArray(tempstring, cbuf.length() + 1);
  
}
//****************************************************** PIC_I(X,Y,Filename.bmp) ******************************************
int import_pic(int x, int y, char *file, float sc) {
  byte r, g, b, cl, buf[3];
  float xtmp, ytmp, dy, dx, rx;
  uint32_t i, sf, vv, vh, xx, yy, skipx, pic;
  byte bmp_header[54];
  uint32_t stepx, stepy, restx, sx, sy;
  char k;
  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);             //SCK,MISO,MOSI,SS 13 //HSPI1
  if ( !SD.begin( kSD_CS, spiSD )) {                        //SD-Card starten
    // mount-fehler
    spiSD.end();                                            //unmount
    tc.setCursorPos(1, 1);
    Terminal.write("        Mountfehler");
    return 0;
  }

  fp = SD.open(String(file), FILE_READ);
  fp.read(bmp_header, 54);                                      //BMP-Header einlesen
  skipx = 54;                                                   //nach dem Header geht's los mit Daten

  if (bmp_header[0] != 0x42 || bmp_header[1] != 0x4D)           //keine BMP-Datei, dann Abbruch!
  {
    Terminal.write("        keine BMP");
    //sd_ende();                                                  //SD-Card unmount
    return 0;
  }
  vv = GFX.getHeight();
  vh = GFX.getWidth();
  //Weite
  xx = bmp_header[21] << 24;
  xx = xx + bmp_header[20] << 16;
  xx = xx + bmp_header[19] << 8;
  xx = xx + bmp_header[18];
  bmp_width = xx;
  //Hoehe
  yy = bmp_header[25] << 24;
  yy = yy + bmp_header[24] << 16;
  yy = yy + bmp_header[23] << 8;
  yy = yy + bmp_header[22];
  bmp_height = yy;
  //color_tiefe;
  cl = bmp_header[28];

  xtmp = ytmp = 1;
  restx = 0;

  stepx = xtmp;
  stepy = ytmp;

  for (dy = yy - 1 ; dy > -1; dy --) {
    for (dx = 0; dx < xx; dx ++) {
      fp.read(buf, 3);                                     //Pixelfarben lesen (blau,grün,rot)
      sx = (dx / xtmp) + x;
      sy = (dy / ytmp) + y;

      if (sx < vh && sy < vv) {                            //nur im Bildschirmbereich pixeln
        GFX.setPenColor(buf[2], buf[1], buf[0]);
        GFX.setPixel(sx, sy);
      }
      skipx +=  3;                                  //ist das Bild > Bildschirmbreite, Pixel*Skalierung überspringen
      fp.seek(skipx);
    }
    if (restx) {                                           //bei ungeraden Formaten Restpixel überspringen
      rx = xx - dx;
      if (rx > 0) skipx += abs((xx - dx) * 3);
      else skipx -= abs(xx - dx) * 3;
    }
    skipx += (stepy - 1) * xx * 3;                         //nächste Bildzeile
    fp.seek(skipx);
  }

  fp.close();
  spiSD.end();
  //sd_ende();                                               //SD-Card unmount
  return 0;
}


void fcolor(int fc) {
  GFX.setPenColor((bitRead(fc, 5) * 2 + bitRead(fc, 4)) * 64, (bitRead(fc, 3) * 2 + bitRead(fc, 2)) * 64, (bitRead(fc, 1) * 2 + bitRead(fc, 0)) * 64);
}

void bcolor(int bc) {
  GFX.setBrushColor((bitRead(bc, 5) * 2 + bitRead(bc, 4)) * 64, (bitRead(bc, 3) * 2 + bitRead(bc, 2)) * 64, (bitRead(bc, 1) * 2 + bitRead(bc, 0)) * 64);
}



//------------------------------------- Loader für Bin-Dateien -----------------------------------------------------------------------------
int load_binary(void) {

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

  if ( !SD.exists(String(sd_pfad) + String(tempstring)))
  {
    status_text("File not found");
    spiSD.end();
    return 1;
  }

  File updateBin = SD.open(String(sd_pfad) + String(tempstring));
  if (updateBin) {
    size_t updateSize = updateBin.size();

    if (updateSize > 0) {
      status_text("load " + String(tempstring));
      performUpdate(updateBin, updateSize);
    }
    else {
      status_text("Error, file is empty");
    }
    updateBin.close();
  }
  else {
    status_text("Could not load");
  }
}

// perform the actual update from a given streams
void performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize, U_FLASH, SD_LED, 1, " ")) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      status_text(String(written) + " successfully");
    }
    else {
      status_text("Written no completed");
    }
    if (Update.end()) {
      status_text("OTA done!");
      if (Update.isFinished()) {
        status_text("success. Now Rebooting.");
        delay(1000);
        ESP.restart();
      }
      else {
        status_text("not finished?");
      }
    }
    else {
      status_text("Error Occurred. Error #: " + String(Update.getError()));
    }

  }
  else
  {
    status_text("Not enough space for OTA");
  }
}

void status_text(String txt){
  int x=(40/2)-(txt.length()/2);
  
  bcolor(11);
  GFX.fillRectangle(5, 230, 315, 239);                    //Rectangle rect x,y,xx,yy,fill=1
  tc.setCursorPos(x, 30);
  Terminal.print(txt);

}

void loop() {
  // put your main code here, to run repeatedly:
  key_press();
  load_binary();
}
