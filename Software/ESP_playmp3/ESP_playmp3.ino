/*
  // ******************** ESP-MP3-Player Version 1.0 03/2024 *************************
  // als Betandteil des Basic32+ Projektes als ladbares Programm mit der Grafik-
  // Bibliothek FabGl - Dank an Fabrizio Di Vittorio
  // Bedienung: <- und -> (Pfeil-)Tasten schaltet durch die Directrory
  // Taste ESC kehrt zum Basic32+ zurück
  // Programm kommt ohne externe I2S Hardware aus, Audio-Ausgabe über Pin 25
  // sonstige Hardwarekonfiguration siehe Basic32+
  // created by R.Zielinski - Berlin
  // Nutzung auf eigene Gefahr!!! :-)
  // *******************************************************************************
*/
#define Version "Vers1.1 - 03/2024"

#include <Arduino.h>

#include "AudioFileSourceSD.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"


#include "cfg.h"   //********************************************* Konfigurations-Datei *************************************************************
#include "PlayOrder.h"


#include "fabgl.h" //********************************************* Bibliotheken zur VGA-Signalerzeugung *********************************************
fabgl::Terminal         Terminal;
//---------------------------------------- die verschiedenen Grafiktreiber --------------------------------------------------------------------------
#ifdef AVOUT
fabgl::CVBS16Controller VGAController;    //AV-Variante
#define VIDEOOUT_GPIO GPIO_NUM_26         //Ausgabe auf GPIO25 oder 26 möglich ACHTUNG!:Soundausgabe erfolgt auf GPIO25
static const char * MODES_STD[]   = { "I-PAL-B", "P-PAL-B", "I-NTSC-M", "P-NTSC-M", "I-PAL-B-WIDE", "P-PAL-B-WIDE", "I-NTSC-M-WIDE", "P-NTSC-M-WIDE", "P-NTSC-M-EXT",};

#elif defined VGA64
fabgl::VGA16Controller    VGAController;      //VGA-Variante -> max.16 Farben, sonst reicht der Speicher nicht zum streamen

#else
fabgl::ILI9341Controller VGAController;     //TFT-Display ILI9341
#endif


//------------------------------------------ Tastatur,GFX-Treiber- und Terminaltreiber -------------------------------------------------------------
fabgl::PS2Controller    PS2Controller;
fabgl::Keyboard Keyboard;
fabgl::Canvas           GFX(&VGAController);
TerminalController      tc(&Terminal);

//------------------------------ I2C Library ------------------------------------------------------------------------------------------------------
#include <Wire.h>           // for I2C 
#include "RTClib.h"         //to show time
TwoWire myI2C = TwoWire(0); //eigenen I2C-Bus erstellen
RTC_DS3231 rtc;
// dies ist die Standard-Konfiguration
byte SDA_RTC = 3;
byte SCL_RTC = 1;

unsigned int Datum[4];                   //Datums-Array
unsigned int Zeit[4];                    //Zeit-Array
unsigned int tmp_zeit;                   //vergleichswert für Zeitaktualisierung im Display
// ---------------------------------- SD-Karten-Zugriff--------------------------------------------------------------------------------------------
#include <SD.h>
#include <SPI.h>
const char*  Radio_Pics[] PROGMEM = {"Radio1.bmp", "Radio2.bmp", "Radio3.bmp", "Radio4.bmp", "Radio5.bmp"};         //Pics
char tempstring[40];                //String Zwischenspeicher
int filenums = 5;
//-------------------------------------- Verwendung der SD-Karte ----------------------------------------------------------------------------------
//SPI CLASS FOR REDEFINED SPI PINS !
SPIClass spiSD(HSPI);
#define kSD_Fail  0      //Fehler-Marker
#define kSD_OK    1      //OK-Marker
File files;
//------------------------------------- OTA-Update-Lib --------------------------------------------------------------------------------------------
#include <Update.h>

uint32_t bmp_width, bmp_height;
const char* filetype[] = {".bin", ".bmp"};
static char sd_pfad[2];            //SD-Card Root-Datei-Pfad

int Key_l = 0;
int Key_r = 0;

int title_length = 0;
String currentFile;
String Dateiname;
bool Key_esc = false;




AudioFileSourceID3 *id3;
AudioFileSourceSD *source = NULL;
AudioGeneratorMP3 *mp3;
AudioOutputI2S *out;

PlayOrder po;

void fcolor(int fc) {
  GFX.setPenColor((bitRead(fc, 5) * 2 + bitRead(fc, 4)) * 64, (bitRead(fc, 3) * 2 + bitRead(fc, 2)) * 64, (bitRead(fc, 1) * 2 + bitRead(fc, 0)) * 64);
}

void bcolor(int bc) {
  GFX.setBrushColor((bitRead(bc, 5) * 2 + bitRead(bc, 4)) * 64, (bitRead(bc, 3) * 2 + bitRead(bc, 2)) * 64, (bitRead(bc, 1) * 2 + bitRead(bc, 0)) * 64);
}


void playFile(File file, bool addToList = false) {
  String fileName = String(file.name());
  Dateiname = fileName;
  fileName.toLowerCase();
  if (fileName.endsWith(".mp3")) {
    source->close();
    if (source->open(file.name())) {
      currentFile = file.name();
      if (addToList) {
        po.addToList(file.name());
      }


      // source = new AudioFileSourceSD(file.name());
      source->open(file.name());
      id3 = new AudioFileSourceID3(source);
      id3->RegisterMetadataCB(MDCallback, (void*)"ID3");

      mp3->begin(id3, out);

    } else {
      status_text("Error opening file");

    }
  }
}

void playPrevTrack() {
  GFX.fillRectangle(59, 60, 307, 84);                    //Display-Ausschnitt
  Dateiname = po.prev().c_str();
  File file = SD.open(po.prev().c_str());
  if (file) {
    playFile(file);
  } else {
    status_text("Playback form SD card done\n");
    delay(1000);
  }
}

void playNextTrack() {
  const char hi[] = "._";
  bool hidden = false;
  String nextName;
  Serial.println("next");
  File file;
  GFX.fillRectangle(59, 60, 307, 84);                    //Display-Ausschnitt
  
  nextName = po.next();
  nextName.toCharArray(tempstring, nextName.length() + 1);
  if (strstr(tempstring, hi)) hidden = true;



  Dateiname = nextName;
  if (nextName.length() == 0) {
    String fileName = String(file.name());
    fileName.toLowerCase();
    while (!fileName.endsWith(".mp3") || hidden) {
      fileName = String(file.name());
      fileName.toLowerCase();
      file.close();
      file = files.openNextFile();
      if (!file) {
        files.rewindDirectory();
        po.num = 1;
        file = files.openNextFile();
        break;
        //file = files.openNextFile();
      }
      fileName = String(file.name());
      fileName.toCharArray(tempstring, fileName.length() + 1);
      if (strstr(tempstring, hi)) hidden = true;
      else hidden = false;
    }

  }
  else {

    file.close();
    file = SD.open(nextName.c_str());

  }

  if (file) {
    playFile(file, true);
  } else {
    delay(1000);
  }
}


void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  int i;
  bcolor(0);
  fcolor(11);


  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void)cbData;
  bool isTitle = String(type).equals("Title");

  //bool isArtist = String(type).equals("Artist");
  if (isUnicode) {
    string += 2;
  }
  String s = "";

  while (*string) {
    char a = *(string++);
    if (isUnicode) {
      string++;
    }
    i++;
    s += a;
    if (i > 31) break;
  }

  if (isTitle) {
    GFX.fillRectangle(59, 74, 307, 84);                    //Display-Ausschnitt
    drawing_text(2, 60, 75, s);
  }
  else
  {
    GFX.fillRectangle(59, 60, 307, 73);                    //Display-Ausschnitt
    drawing_text(2, 60, 62, Dateiname);
  }
  drawing_text(3, 285, 72, String(po.num));
}


void display_Time(void) {
  String cbuf;
  getdatetime();
  if (tmp_zeit != Zeit[1])
  {
    GFX.fillRectangle(153, 234, 245, 253);
    if (Zeit[0] < 10) cbuf = "0";
    cbuf += String(Zeit[0]) + ":";
    if (Zeit[1] < 10) cbuf += "0";
    cbuf += String(Zeit[1]);
    drawing_text(3, 180, 237, cbuf);
    tmp_zeit = Zeit[1];
  }
}

void getdatetime()
{
  DateTime now = rtc.now();           //RTC Funktion
  Zeit[0] = now.hour();
  Zeit[1] = now.minute();
  Zeit[2] = now.second();

}


void setup()
{
  //Serial.begin(115200);
  Serial.print("Hallo MP3-Player");
  // put your setup code here, to run once:
  Keyboard.begin(GPIO_NUM_33, GPIO_NUM_32);
  PS2Controller.keyboard() -> setLayout(&fabgl::GermanLayout);                //deutsche Tastatur
  delay(200);

  PS2Controller.keyboard()-> onVirtualKey = [&](VirtualKey * vk, bool keyDown) {
    if (*vk == VirtualKey::VK_RIGHT) {
      if (keyDown) {
        Key_r = 1;
        if (po.num < po.maxNum) po.num++;
      }
      *vk = VirtualKey::VK_NONE;
    }
    else if (*vk == VirtualKey::VK_LEFT) {                                               //
      if (keyDown) {
        Key_l = 1;
        if (po.num > 0) po.num--;
      }
      *vk = VirtualKey::VK_NONE;
    }
    /*
        if (*vk == VirtualKey::VK_UP) {
          if (keyDown) {
            Key_y--;
            swap = true;
            if (Key_y < 0) Key_y = 3;
          }
           vk = VirtualKey::VK_NONE;
        }
        else if (*vk == VirtualKey::VK_DOWN) {                                               //
          if (keyDown) {
            Key_y++;
            swap = true;
            if (Key_y > 3) Key_y = 0;
          }
           vk = VirtualKey::VK_NONE;
        }
    */
    else if (*vk == VirtualKey::VK_ESCAPE) {                                               //
      if (keyDown) {
        Key_esc = true;
      }
      *vk = VirtualKey::VK_NONE;
    }

  };


  //************************************************************ welcher Bildschirmtreiber? *********************************************************
  // 64 colors
#ifdef AVOUT                                                                          //AV-Variante
VGAController.begin(VIDEOOUT_GPIO);
VGAController.setHorizontalRate(2);                                                   //320x240
VGAController.setResolution(MODES_STD[7]);                                            //5 scheint optimal ist aber mit 384x240 nicht 100% kompatibel (3) (7-360x200)

#elif defined VGA64
VGAController.begin();                                                                //VGA-Variante //64 Farben
//VGAController.setResolution(QVGA_320x240_60Hz);
VGAController.setResolution(VGA_400x300_60Hz);
#else                                                                                 //ILI9341
VGAController.begin(TFT_SCK, TFT_MOSI, TFT_DC, TFT_RESET, TFT_CS, TFT_SPIBUS);
VGAController.setResolution(TFT_240x320);
VGAController.setOrientation(fabgl::TFTOrientation::Rotate270);  //Kontakte links
//VGAController.setOrientation(fabgl::TFTOrientation::Rotate90);   //Kontakte rechts
#endif


  //***************************************************************************************************************************************************

  Terminal.begin(&VGAController);
  Terminal.connectLocally();                                                           // für Terminal Komandos


  Terminal.loadFont(&fabgl::FONT_8x8);
  Terminal.enableCursor(false);
  fcolor(0);
  bcolor(63);
  tc.setCursorPos(1, 1);
  GFX.clear();
  sd_pfad[0] = '/';                                         //setze Root-Verzeichnis
  sd_pfad[1] = 0;

  import_pic(0, 51, "/radio2.bmp", 1);

  bcolor(1);
  GFX.fillRectangle(0, 0, 399, 50);                        //Platz über dem Radio
  fcolor(60);
  drawing_text(4, 45, 10, "*** MP3-PLAYER by Zille-Soft ***");
  drawing_text(0, 132, 30, Version);

  bcolor(0);
  GFX.fillRectangle(59, 60, 307, 84);                    //Display-Ausschnitt

  fcolor(11);
  drawing_text(3, 285, 72, String(po.num));          //Titelnummer

  // ein I2C-Interface definieren
  myI2C.begin(SDA_RTC, SCL_RTC, 400000); //400kHz
  rtc.begin(&myI2C);
  getdatetime();                                              //ESP32-interne Uhr stellen für Datei-Zeitstempel
  source = new AudioFileSourceSD();
  id3 = new AudioFileSourceID3(source);
  id3->RegisterMetadataCB(MDCallback, (void*)"ID3");
  out = new AudioOutputI2S(0, 1);  // use the internal DAC channel 1 (pin25) on ESP32


  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);             //SCK,MISO,MOSI,SS 13 //HSPI1
  if ( !SD.begin( kSD_CS, spiSD )) {                        //SD-Card starten
    // mount-fehler
    spiSD.end();                                            //unmount
    tc.setCursorPos(1, 1);
    status_text("Mountfehler");
  }

  files = SD.open("/mp3");
  startMP3();

}


void startMP3() {
  //
  File file;
  file = files.openNextFile();
  Dateiname = file.name();
  source->open(file.name());
  id3 = new AudioFileSourceID3(source);
  id3->RegisterMetadataCB(MDCallback, (void*)"ID3");
  //create and start a new decoder
  mp3 = new AudioGeneratorMP3();
  mp3->begin(id3, out);
}


void loop()
{ display_Time();                                                                 //Uhrzeit anzeigen
  if (mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
      delay(1000);
      playNextTrack();
    }
  } 
  if (Key_l || Key_r) {
    mp3->stop();

    if (Key_l) playPrevTrack();
    if (Key_r) playNextTrack();
    Key_l = Key_r = 0;
  }
  if (Key_esc) {
    mp3->stop();
    load_binary();
  }
}


//------------------------------------- Loader für Bin-Dateien -----------------------------------------------------------------------------
int load_binary(void) {

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

  if ( !SD.exists("/basic.bin"))
  {
    status_text("File not found");
    spiSD.end();
    return 1;
  }

  File updateBin = SD.open("/basic.bin");
  if (updateBin) {
    size_t updateSize = updateBin.size();

    if (updateSize > 0) {
      status_text("loading");
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

void status_text(String txt) {

  int x = (50 / 2) - (txt.length() / 2);
  bcolor(0);
  GFX.fillRectangle(0, 290, 399, 299);                    //Rectangle rect x,y,xx,yy,fill=1
  tc.setCursorPos(x, 49);
  bcolor(0);
  fcolor(63);
  Terminal.print(txt);

}

void drawing_text(int fnt, int x_text, int y_text, String txt)
{
  txt.toCharArray(tempstring, txt.length() + 1);
  switch (fnt) {
    case 0:
      GFX.drawText(&fabgl::FONT_8x8, x_text, y_text, tempstring);
      break;
    case 1:
      GFX.drawText(&fabgl::FONT_5x8, x_text, y_text, tempstring);
      break;
    case 2:
      GFX.drawText(&fabgl::FONT_6x8, x_text, y_text, tempstring);
      break;
    case 3:
      GFX.drawText(&fabgl::FONT_LCD_8x14, x_text, y_text, tempstring);
      break;
    case 4:
      GFX.drawText(&fabgl::FONT_10x20, x_text, y_text, tempstring);
      break;
    case 5:
      GFX.drawText(&fabgl::FONT_BLOCK_8x14, x_text, y_text, tempstring);
      break;
    case 6:
      GFX.drawText(&fabgl::FONT_BROADWAY_8x14, x_text, y_text, tempstring);
      break;
    case 7:
      GFX.drawText(&fabgl::FONT_OLDENGL_8x16, x_text, y_text, tempstring);
      break;
    case 8:
      GFX.drawText(&fabgl::FONT_BIGSERIF_8x16, x_text, y_text, tempstring);
      break;
    case 9:
      GFX.drawText(&fabgl::FONT_SANSERIF_8x14, x_text, y_text, tempstring);
      break;
    case 10:
      GFX.drawText(&fabgl::FONT_COURIER_8x14, x_text, y_text, tempstring);
      break;
    case 11:
      GFX.drawText(&fabgl::FONT_SLANT_8x14, x_text, y_text, tempstring);
      break;
    case 12:
      GFX.drawText(&fabgl::FONT_WIGGLY_8x16, x_text, y_text, tempstring);
      break;
    case 13:
      GFX.drawText(&fabgl::FONT_6x10, x_text, y_text, tempstring);
      break;
    case 14:
      GFX.drawText(&fabgl::FONT_BIGSERIF_8x14, x_text, y_text, tempstring);
      break;
    case 15:
      GFX.drawText(&fabgl::FONT_4x6, x_text, y_text, tempstring);
      break;
    case 16:
      GFX.drawText(&fabgl::FONT_6x12, x_text, y_text, tempstring);
      break;
    case 17:
      GFX.drawText(&fabgl::FONT_7x13, x_text, y_text, tempstring);
      break;
    case 18:
      GFX.drawText(&fabgl::FONT_7x14, x_text, y_text, tempstring);
      break;
    case 19:
      GFX.drawText(&fabgl::FONT_8x9, x_text, y_text, tempstring);
      break;
    case 20:
      GFX.drawText(&fabgl::FONT_COMPUTER_8x14, x_text, y_text, tempstring);
      break;
    case 21:
      GFX.drawText(&fabgl::FONT_SANSERIF_8x14, x_text, y_text, tempstring);
      break;
    case 22:
      GFX.drawText(&fabgl::FONT_6x10, x_text, y_text, tempstring);
      break;
    case 23:
      GFX.drawText(&fabgl::FONT_9x15, x_text, y_text, tempstring);
      break;
    case 24:
      GFX.drawText(&fabgl::FONT_8x16, x_text, y_text, tempstring);
      break;
    default:
      GFX.drawText(&fabgl::FONT_6x8, x_text, y_text, tempstring);
      break;
  }


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
    fcolor(0);
    status_text("Mountfehler");
    return 0;
  }

  files = SD.open(String(file), FILE_READ);
  files.read(bmp_header, 54);                                      //BMP-Header einlesen
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
      files.read(buf, 3);                                     //Pixelfarben lesen (blau,grün,rot)
      sx = (dx / xtmp) + x;
      sy = (dy / ytmp) + y;

      if (sx < vh && sy < vv) {                            //nur im Bildschirmbereich pixeln
        GFX.setPenColor(buf[2], buf[1], buf[0]);
        GFX.setPixel(sx, sy);
      }
      skipx +=  3;                                  //ist das Bild > Bildschirmbreite, Pixel*Skalierung überspringen
      files.seek(skipx);
    }
    if (restx) {                                           //bei ungeraden Formaten Restpixel überspringen
      rx = xx - dx;
      if (rx > 0) skipx += abs((xx - dx) * 3);
      else skipx -= abs(xx - dx) * 3;
    }
    skipx += (stepy - 1) * xx * 3;                         //nächste Bildzeile
    files.seek(skipx);
  }

  files.close();
  spiSD.end();
  //sd_ende();                                               //SD-Card unmount
  return 0;
}
