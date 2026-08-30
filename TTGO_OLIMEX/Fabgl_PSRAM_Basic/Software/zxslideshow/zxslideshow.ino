
#include "fabgl.h"
#include "zxbitmap.h"
#include <Update.h>

fabgl::VGAController VGAController;
fabgl::Canvas        canvas(&VGAController);
fabgl::PS2Controller    PS2Controller;
fabgl::Keyboard Keyboard;

//#include "FS.h"
#include "SD.h"
#include "SPI.h"
bool break_marker = false;
SPIClass myspi(HSPI);

void setup()
{
  //Serial.begin(115200);
  Keyboard.begin(GPIO_NUM_33, GPIO_NUM_32);

  VGAController.begin();
  VGAController.setResolution(QVGA_320x240_60Hz);//, 320, 200); //geändert

  canvas.setBrushColor(RGB888(0, 0, 0)); // Use as BORDER color
  canvas.selectFont(&fabgl::FONT_4x6);
  canvas.setPenColor( RGB888(0x80, 0x80, 0x80) );
  canvas.clear();
  canvas.drawText(32, 200 - 6, "ZX Spectrum Slideshow - Insert SD - by Carles Oriol 2020");
  canvas.waitCompletion();

  PS2Controller.keyboard()-> onVirtualKey = [&](VirtualKey * vk, bool keyDown) {
    if (*vk == VirtualKey::VK_ESCAPE) {
      if (keyDown) {
        break_marker = true;
      }
      *vk = VirtualKey::VK_NONE;
    }
  };


}

void draw_pic(void) {
  myspi.begin(14, 35, 12, 13); //CLK,MISO,MOIS,SS
  // if sd not ready retry every 2 seconds
  while (!SD.begin( 13, myspi)) {
    delay(3000);
  }
  while (SD.cardType() == CARD_NONE) {
    delay(3000);
  }

  static uint8_t buf[6912];
  const char hi[] = "._";
  String cbuf;


  File root = SD.open("/scr");
  File file = root.openNextFile();
  
  while (file && !break_marker) {
    cbuf = String(file.name());
    
    // Unsichtbare Mac-Dateien ausblenden
    if (cbuf.startsWith(".") || cbuf.indexOf("._") >= 0) {
      file.close(); 
      file = root.openNextFile(); // 💡 WICHTIG: Nächste Datei holen vor dem Springen!
      continue;                   // Jetzt sicher zurück zum Schleifenanfang
    }
    
    if (!file.isDirectory()) {
      file.read(buf, 6912); // Lädt den 6.912 Byte großen ZX Spectrum Screen Buffer
      ZXBitmap zxscreen(buf);

      canvas.clear();
      canvas.drawBitmap(32, 0, &zxscreen);
      canvas.drawText(32, 200 - 6, file.name());

      canvas.waitCompletion();
      delay(3000);
    }
    
    // Datei schließen und die nächste für den regulären Ablauf holen
    file.close(); // 💡 Auch hier sauber schließen, um RAM-Lecks zu vermeiden
    file = root.openNextFile();
  }

  root.close(); // Immer auch das Verzeichnis am Ende schließen!
  myspi.end();
  load_binary();
}
int load_binary(void) {

  myspi.begin(14, 35, 12, 13);       //SCK,MISO,MOSI,SS 13 //HSPI1
  while (!SD.begin( 13, myspi)) {
    delay(3000);
  }
  canvas.selectFont(&fabgl::FONT_8x8);
  if (!SD.exists("/basic.bin"))
  {
    canvas.clear();
    canvas.drawText(32, 190, "File not found");
    myspi.end();
    return 1;
  }

  File updateBin = SD.open("/basic.bin");
  if (updateBin) {
    size_t updateSize = updateBin.size();

    if (updateSize > 0) {
      canvas.fillRectangle(32, 180, 200, 200);
      canvas.drawText(32, 190, "loading");
      performUpdate(updateBin, updateSize);
    }
    else {
      canvas.fillRectangle(32, 180, 200, 200);
      canvas.drawText(32, 190, "Error, file is empty");
    }
    updateBin.close();
  }
  else {
    canvas.fillRectangle(32, 180, 200, 200);
    canvas.drawText(32, 190, "Could not load");
  }
}

// perform the actual update from a given streams
void performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize)) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      canvas.fillRectangle(32, 180, 200, 200);
      canvas.drawText(32, 190, " successfully");
    }
    else {
      canvas.fillRectangle(32, 180, 200, 200);
      canvas.drawText(32, 190, "Written no completed");
    }
    if (Update.end()) {
      canvas.fillRectangle(32, 180, 200, 200);
      canvas.drawText(32, 190, "OTA done!");
      if (Update.isFinished()) {
        canvas.fillRectangle(32, 180, 200, 200);
        canvas.drawText(32, 190, "success. Now Rebooting.");
        delay(1000);
        ESP.restart();
      }
      else {
        canvas.fillRectangle(32, 180, 200, 200);
        canvas.drawText(32, 190, "not finished?");
      }
    }
    else {
      canvas.fillRectangle(32, 180, 200, 200);
      canvas.drawText(32, 190, "Error Occurred");
    }

  }
  else
  {
    canvas.fillRectangle(32, 180, 200, 200);
    canvas.drawText(32, 190, "Not enough space for OTA");
  }
}


void loop()
{
  draw_pic();

}
