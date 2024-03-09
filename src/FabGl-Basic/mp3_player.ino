//#######################################################################################################################################
// ------------------------------------- MP3 - Funktionalität ---------------------------------------------------------------------------
//#######################################################################################################################################
#include "AudioFileSourceSD.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
AudioFileSourceID3 *id3;
AudioFileSourceSD *source = NULL;
AudioGeneratorMP3 *mp3;
AudioOutputI2S *out;

//#######################################################################################################################################
//--------------------------------------------- MP3 - Player ----------------------------------------------------------------------------
//#######################################################################################################################################
// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  String cbuf;
  int i=0;
  const char *ptr = reinterpret_cast<const char *>(cbData);
  const char *tmpptr;
  tmpptr = ptr;

  (void) isUnicode; // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2) - 1] = 0;
  Terminal.printf("%s '%s' = '%s'\n\r", ptr, s1, s2);
  Terminal.flush();
  
  //strncpy(tempstring, string, sizeof(tempstring));
  
  
  //Terminal.print(String(tempstring));
  //Terminal.print(String(s2));
}
/*
  // Called when there's a warning or error (like a buffer underflow or decode hiccup)
  void StatusCallback(void *cbData, int code, const char *string)
  {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  Terminal.print(code);
  Terminal.print(s1);

  }
*/
void play_mp3(void) {
  fp = SD.open(String(sd_pfad) + String(tempstring));     //Datei zum Laden öffnen
  source = new AudioFileSourceSD();
  out = new AudioOutputI2S(0, 1);  // use the internal DAC channel 1 (pin25) on ESP32
  source->open(fp.name());
  id3 = new AudioFileSourceID3(source);
  id3->RegisterMetadataCB(MDCallback, (void*)"ID3");
  mp3 = new AudioGeneratorMP3();
  mp3->begin(id3, out);
  while (!break_marker)           //Abbruch mit ESC
  {
    if (mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
        break_marker = true;      //am Song-Ende Ausstieg
      }
    }
  }

  mp3->stop();
  source->close();
  break_marker = false;
  sd_ende();
  line_terminator();
}
