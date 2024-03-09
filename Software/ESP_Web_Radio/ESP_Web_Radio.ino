/*
// ******************** ESP-Webradio Version 1.0 03/2024 *************************
// als Betandteil des Basic32+ Projektes als ladbares Programm mit der Grafik-
// Bibliothek FabGl - Dank an Fabrizio Di Vittorio 
// Bedienung: <- und -> (Pfeil-)Tasten schaltet durch die Sender (20) 
// Taste ESC kehrt zum Basic32+ zurück
// Programm kommt ohne externe I2S Hardware aus, Audio-Ausgabe über Pin 25
// sonstige Hardwarekonfiguration siehe Basic32+
// created by R.Zielinski - Berlin
// Nutzung auf eigene Gefahr!!! :-)
// *******************************************************************************
*/
#include <Arduino.h>

#include <WiFi.h>
#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "cfg.h"   //********************************************* Konfigurations-Datei *************************************************************

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
File fp;
//------------------------------------- OTA-Update-Lib --------------------------------------------------------------------------------------------
#include <Update.h>

uint32_t bmp_width, bmp_height;
const char* filetype[] = {".bin", ".bmp"};
static char sd_pfad[2];            //SD-Card Datei-Pfad

int Key_l = 0;
int Key_r = 0;

int title_length = 0;


bool Key_esc = false;

// Credential of the local WiFi
const char* ssid     = "**************";
const char* password = "**************";

// URL of internet radio station
const char *URL[] PROGMEM = {
  "http://www.golden-apple.com:680/;",
  "http://46.163.75.84:8000/",
  "http://countrymusic24.powerstream.de:9000/",
  "http://stream2.jenny.fm:9000/",
  "http://de1.streamhosting.ch:8050/",
  "http://www.memoryradio.de:4000/",
  "http://radio1.nightline-radio.de:8000/",
  "http://streaming.fueralle.org:8000/corax.mp3",
  "http://streaming.fueralle.org:8000/Radio-F.R.E.I",
  "http://streaming.fueralle.org:8000/frs-hi.mp3",
  "http://streaming.fueralle.org:8000/frn",
  "http://streaming.fueralle.org:8000/bermudafunk_low",
  "http://streaming.fueralle.org:8000/radiot.mp3",
  "http://streaming.fueralle.org:8000/rfk",
  "http://streaming.fueralle.org:8000/slubfurt_128.mp3",
  "https://onair.dancewave.online:8082/dance.mp3?_ic2=1709836317186",
  "http://topradio-de-hz-fal-stream02-cluster01.radiohost.de/brf_mp3-128?ref=internetradio&amsparams=internetradio",
  "http://orf-live.ors-shoutcast.at/oe3-q1a",
  "http://mp3.ffh.de/radioffh/hqlivestream.mp3",
  "http://cafe80s.eu:8100/"
};


const char* title[] PROGMEM = {"Golden Apple", "Hitradio Potsdam", "Countrymusic24", "JennyFM", "Lounge Grooves", "MemoryRadio", 
                               "Nightline Radio", "Radio Corax", "Radio-F.R.E.I","Radio Stuttgart","Radio Neumuenster","Bermudafunk",
                               "Radio T Chemnitz","Rundfunkkombinat","Buerger-Radio","Dance Wave!","Berliner Rundfunk","ORF Hitradio",
                               "HitRadio FFH","Cafe 80s FM"};
int    x_position[] PROGMEM = {140 , 122 , 132, 167 , 132, 147, 127, 147 , 137 , 127 , 117 , 147, 122, 122, 127, 147, 117, 142, 142, 147};
int stations = 20;
int act_station = 0;

AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;

void fcolor(int fc) {
  GFX.setPenColor((bitRead(fc, 5) * 2 + bitRead(fc, 4)) * 64, (bitRead(fc, 3) * 2 + bitRead(fc, 2)) * 64, (bitRead(fc, 1) * 2 + bitRead(fc, 0)) * 64);
}

void bcolor(int bc) {
  GFX.setBrushColor((bitRead(bc, 5) * 2 + bitRead(bc, 4)) * 64, (bitRead(bc, 3) * 2 + bitRead(bc, 2)) * 64, (bitRead(bc, 1) * 2 + bitRead(bc, 0)) * 64);
}


// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void) isUnicode; // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[32]; //s2 ursprünglich 64
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2) - 1] = 0;
  //Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  GFX.fillRectangle(105, 120, 299, 139);
  drawing_text(2, 105, 120, s1);
  drawing_text(2, 105, 132, s2);
  //Serial.flush();
}

/*
// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[25];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  GFX.fillRectangle(140, 140, 280, 157);
  drawing_text(1, 145, 146, String(code));
  //Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  //Serial.flush();
}
*/

void display_Time(void){
  String cbuf;
  getdatetime();
  if(tmp_zeit!=Zeit[1])
  {
    GFX.fillRectangle(180, 140, 280, 157);
    if(Zeit[0]<10) cbuf="0";
    cbuf=String(Zeit[0])+":";
    if(Zeit[1]<10) cbuf+="0";
    cbuf+=String(Zeit[1]);
    drawing_text(3, 180, 143, cbuf);
    tmp_zeit=Zeit[1];
  }
}

void getdatetime()
{
  DateTime now = rtc.now();           //RTC Funktion
  Zeit[0] = now.hour();
  Zeit[1] = now.minute();
  Zeit[2] = now.second();
//  Datum[0] = now.day();
//  Datum[1] = now.month();
//  Datum[2] = now.year();
//  Datum[3] = now.dayOfTheWeek();
  
}


void setup()
{
  String cbuf;
  Serial.begin(115200);
  // put your setup code here, to run once:
  Keyboard.begin(GPIO_NUM_33, GPIO_NUM_32);
  PS2Controller.keyboard() -> setLayout(&fabgl::GermanLayout);                //deutsche Tastatur
  delay(200);

  PS2Controller.keyboard()-> onVirtualKey = [&](VirtualKey * vk, bool keyDown) {
    if (*vk == VirtualKey::VK_RIGHT) {
      if (keyDown) {
        Key_r = 1;
        act_station++;
        if (act_station > stations - 1) act_station = 0;
      }
      *vk = VirtualKey::VK_NONE;
    }
    else if (*vk == VirtualKey::VK_LEFT) {                                               //
      if (keyDown) {
        Key_l = 1;
        act_station--;
        if (act_station < 0) act_station = stations - 1;
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

  cbuf = String(sd_pfad) + String(Radio_Pics[0]);
  cbuf.toUpperCase();                               //String in Grossbuchstaben umwandeln
  cbuf.toCharArray(tempstring, cbuf.length() + 1);
  import_pic(0, 74, tempstring, 1);

  
  bcolor(1);
  GFX.fillRectangle(0, 0, 399, 73);                        //Platz über dem Radio
  fcolor(60);
  drawing_text(4, 45, 10, "*** WEB-RADIO by Zille-Soft ***");
  drawing_text(0, 132, 30, "Vers1.0 - 03/2024");

  bcolor(11);
  GFX.fillRectangle(100, 92, 299, 157);                    //Display-Ausschnitt

  
  fcolor(1);
  bcolor(11);
  drawing_text(3, 105, 143, "WIFI-SEARCH..");
  WiFi.disconnect();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    fcolor(11);
    drawing_text(3, 105, 143, "WIFI-SEARCH..");
    delay(500);
    fcolor(1);
    drawing_text(3, 105, 143, "WIFI-SEARCH..");
    delay(500);
  }

  fcolor(1);
  bcolor(11);
  GFX.fillRectangle(105, 140, 299, 157);                    //Display-Ausschnitt - 194 Pixel breit
  drawing_text(3, 105, 143, "WIFI");
  drawing_text(3, 285, 143, String(act_station + 1));       //Programmplatz
  drawing_text(4, x_position[act_station], 95, String(title[act_station]));     //aktuelle Station
  
  // ein I2C-Interface definieren
  myI2C.begin(SDA_RTC, SCL_RTC, 400000); //400kHz
  rtc.begin(&myI2C);
  getdatetime();                                              //ESP32-interne Uhr stellen für Datei-Zeitstempel

  out = new AudioOutputI2S(0, 1);  // use the internal DAC channel 1 (pin25) on ESP32
  startUrl();

}


//stop playing the input stream release memory, delete instances
void stopPlaying() {
  if (mp3)  {
    mp3->stop();
    delete mp3;
    mp3 = NULL;
  }
  if (buff)  {
    buff->close();
    delete buff;
    buff = NULL;
  }
  if (file)  {
    file->close();
    delete file;
    file = NULL;
  }
}


void startUrl() {
  stopPlaying();  //first close existing streams
  //open input file for selected url
  //Serial.printf("Active station %s\n",stationlist[actStation].url);
  file = new AudioFileSourceICYStream(URL[act_station]);
  //register callback for meta data
  file->RegisterMetadataCB(MDCallback, NULL);
  //create a new buffer which uses the preallocated memory
  buff = new AudioFileSourceBuffer(file, 8 * 1024);
  //buff->RegisterStatusCB(StatusCallback, (void*)"buffer");
  //Serial.printf_P(PSTR("sourcebuffer created - Free mem=%d\n"), ESP.getFreeHeap());
  //create and start a new decoder
  mp3 = new AudioGeneratorMP3();
  //Serial.printf_P(PSTR("created decoder\n"));
  //Serial.printf_P("Decoder start...\n");
  mp3->begin(buff, out);
}


void loop()
{ display_Time();                                                                 //Uhrzeit anzeigen
  if (mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
    }
  } else {
    delay(1000);
    mp3->begin(buff, out);
  }
  if (Key_l || Key_r) {
    Key_l = Key_r = 0;
    GFX.fillRectangle(105, 95, 299, 115); //Stationsnamenfeld
    GFX.fillRectangle(105, 120, 299, 139); //Streamtitelfeld
    GFX.fillRectangle(280, 140, 299, 157); //Stationsnummernfeld
    drawing_text(3, 280, 143, String(act_station + 1));
    drawing_text(4, x_position[act_station], 95, String(title[act_station]));     //aktuelle Station mittig anzeigen00
    startUrl();
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
