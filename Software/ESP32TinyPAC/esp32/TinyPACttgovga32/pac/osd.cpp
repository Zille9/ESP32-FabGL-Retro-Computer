#include "gbConfig.h"
#include "gbGlobals.h"
#include "hardware.h"
#include "osd.h"
#include "hardware.h"
#include "PS2Kbd.h"
//#include "gbsdlfont6x8.h"
#include "vga6bit.h"
#ifdef use_lib_sound_digital 
 #ifdef use_lib_sound_dac
  #include <driver/dac.h>
 #endif 
#endif 
//#include "dataFlash/gbpfile.h"
//#include "gbrom82s1237f.h"

#include <SPI.h>
#include <SD.h>
#include <Update.h>

SPIClass spiSD(HSPI);

byte kSD_CS   = 13;
byte kSD_MISO = 16;
byte kSD_MOSI = 17;
byte kSD_CLK  = 14;

//#define BLACK   0
//#define BLUE    4
//#define RED     1
//#define MAGENTA 5
//#define GREEN   2
//#define CYAN    6
//#define YELLOW  3
//#define WHITE   15

//#ifdef COLOR_3B           //       BGR 
// #define BLACK   0x08      // 0000 1000
// #define BLUE    0x0C      // 0000 1100
// #define RED     0x09      // 0000 1001
// #define MAGENTA 0x0D      // 0000 1101
// #define GREEN   0x0A      // 0000 1010
// #define CYAN    0x0E      // 0000 1110
// #define YELLOW  0x0B      // 0000 1011
// #define WHITE   0x0F      // 0000 1111
//#endif



unsigned char gb_show_osd_main_menu=0;




//#define max_gb_osd_screen 1
//const char * gb_osd_screen[max_gb_osd_screen]={
// "Pixels Left"//,
// //"Pixels Top",
// //"Color 8",
// //"Mono Blue 8",
// //"Mono Green 8",
// //"Mono Red 8",
// //"Mono Grey 8"
//};

//#define max_gb_osd_screen_values 5
//const char * gb_osd_screen_values[max_gb_osd_screen_values]={
// "0",
// "2",
// "4", 
// "8", 
// "16"
//};


#define max_gb_main_menu 9
const char * gb_main_menu[max_gb_main_menu]={
 "Reset", 
 "Speed",
 "Video Mode",
 "Video Poll",
 "Keyboard Poll",
 "Show FPS",
 "Sound",
 "Basic32+",
 "Exit"
};

#ifdef use_lib_sound_dac
 #define max_gb_sound_menu 10
 const char * gb_sound_menu[max_gb_sound_menu]={
  "Digital ON",
  "Digital OFF",
  "DAC (125%)",
  "DAC (100%)",
  "DAC (75%)",
  "DAC (50%)",
  "DAC (25%)",
  "DAC (0%)",
  "DAC MedUnsigned",
  "DAC MedSigned"
 };
#else
 #define max_gb_sound_menu 2
 const char * gb_sound_menu[max_gb_sound_menu]={
  "Digital ON",
  "Digital OFF"
 };
#endif 

/*
#define max_gb_delay_cpu_menu 15
const char * gb_delay_cpu_menu[max_gb_delay_cpu_menu]={
 "Auto",
 "0",
 "1",
 "2",
 "3",
 "4",
 "5",
 "6",
 "7",
 "8",
 "9",
 "10",
 "16",
 "20",
 "40"
};
*/

#define max_gb_speed_menu 5
const char * gb_speed_menu[max_gb_speed_menu]={
 "1 (normal)",
 "2",
 "3",
 "4",
 "5 (fast)"
}; 


//#define max_gb_speed_sound_menu 7
//const char * gb_speed_sound_menu[max_gb_speed_sound_menu]={
// "0",
// "1",
// "2",
// "4",
// "8",
// "16",
// "32"
//};

//#define max_gb_value_binary_menu 2
//const char * gb_value_binary_menu[max_gb_value_binary_menu]={
// "0",
// "1"
//};


//#define max_gb_speed_videoaudio_options_menu 2
//const char * gb_speed_videoaudio_options_menu[max_gb_speed_videoaudio_options_menu]={
// "Video poll",
// "Keyboard poll"
//};

#define max_gb_speed_video_poll_menu 7
const char * gb_speed_video_poll_menu[max_gb_speed_video_poll_menu]={
 "0",
 "10 (100 FPS)",
 "16 (60 FPS)",
 "20 (50 FPS)",
 "30 (33 FPS)",
 "40 (25 FPS)",
 "50 (20 FPS)"
};

#define max_gb_speed_keyboard_poll_menu 5
const char * gb_speed_keyboard_poll_menu[max_gb_speed_keyboard_poll_menu]={
 "10",
 "20",
 "30",
 "40",
 "50"
};


#define max_gb_video_mode_menu 6
const char * gb_video_mode_menu[max_gb_video_mode_menu]={ 
 "320x240x60hz V bitluni",
 "320x240x60hz V fabgl",
 "320x350x70hz H bitluni",
 "320x350x70hz V bitluni",
 "400x300x56.2hz H bitluni",
 "400x300x56.2hz V bitluni"
};



#define max_gb_reset_menu 2
const char * gb_reset_menu[max_gb_reset_menu]={
 "Soft",
 "Hard"
};


#define gb_pos_x_menu 60
#define gb_pos_y_menu 20
#define gb_osd_max_rows 10



//***************************************************************************
inline void jj_fast_putpixel(short int x,short int y,unsigned char c)
{
 #ifdef use_lib_tinybitluni_fast
  gb_buffer_vga[y][x^2]= gb_jj_color[c];
 #else
  #ifdef use_lib_cvbs_bitluni   
   gb_buffer_cvbs[y][x]= gb_color_cvbs[(c & 0x01)];
  #endif
 #endif 
}



//**********************************************
void SDLClear()
{
 unsigned int a0= (0|gb_sync_bits);
 unsigned int a32= (a0<<24)|(a0<<16)|(a0<<8)|a0;
 for (unsigned int y=0;y<gb_alto;y++)
 {
  for (unsigned int x=0;x<(gb_ancho>>2);x++)
  {   
   gb_buffer_vga32[y][x]= a32; //gb_color_vga[0];
  }
 }
}



//*************************************************************************************
void SDLprintCharOSD(char car,int x,int y,unsigned char color,unsigned char backcolor)
{ 
// unsigned char aux = gb_sdl_font_6x8[(car-64)];
 int auxId = car << 3; //*8
 unsigned char aux;
 unsigned char auxColor;
 for (unsigned char j=0;j<8;j++)
 {
  //aux = gb_sdl_font_8x8[auxId + j];
  aux = gb_sdl_font[auxId + j];
  for (int i=0;i<8;i++)
  {
   auxColor= ((aux>>i) & 0x01);
   //SDLputpixel(surface,x+(6-i),y+j,(auxColor==1)?color:backcolor);
   jj_fast_putpixel(x+(6-i),y+j,(auxColor==1)?color:backcolor);
  }
 }
}

void SDLprintText(const char *cad,int x, int y, unsigned char color,unsigned char backcolor)
{
//SDL_Surface *surface,
// gb_sdl_font_6x8
 int auxLen= strlen(cad);
 if (auxLen>50)
  auxLen=50;
 for (int i=0;i<auxLen;i++)
 {
  SDLprintCharOSD(cad[i],x,y,color,backcolor);
  x+=7;
 }
}

void OSDMenuRowsDisplayScroll(const char **ptrValue,unsigned char currentId,unsigned char aMax)
{//Dibuja varias lineas
 for (int i=0;i<gb_osd_max_rows;i++)
  SDLprintText("                        ",gb_pos_x_menu,gb_pos_y_menu+8+(i<<3),0,0);
 
 for (int i=0;i<gb_osd_max_rows;i++)
 {
  if (currentId >= aMax)
   break;
  //SDLprintText(gb_osd_sdl_surface,ptrValue[currentId],gb_pos_x_menu,gb_pos_y_menu+8+(i<<3),((i==0)?CYAN:WHITE),((i==0)?BLUE:BLACK),1);
  SDLprintText(ptrValue[currentId],gb_pos_x_menu,gb_pos_y_menu+8+(i<<3),((i==0)?ID_COLOR_BLACK:ID_COLOR_WHITE),((i==0)?ID_COLOR_WHITE:ID_COLOR_BLACK));
  currentId++;
 }     
}

//Maximo 256 elementos
unsigned char ShowTinyMenu(const char *cadTitle,const char **ptrValue,unsigned char aMax,short int aSel)
{
 unsigned char aReturn=0;
 unsigned char salir=0;
 unsigned int curTime_keyboard;
 unsigned int curTime_keyboard_before;

 #ifdef use_lib_keyboard_uart
  unsigned int curTime_keyboard_uart;
  unsigned int curTime_keyboard_before_uart;
  curTime_keyboard_uart = curTime_keyboard_before_uart= millis();
 #endif
 
 curTime_keyboard= curTime_keyboard_before= millis();

 SDLClear();
 SDLprintText("Pacman Arcade by Ackerman",gb_pos_x_menu-(4<<3),gb_pos_y_menu-16,ID_COLOR_WHITE,ID_COLOR_BLACK);
 //for (int i=0;i<20;i++) 
 for (int i=0;i<14;i++) 
  SDLprintCharOSD(' ',gb_pos_x_menu+(i<<3),gb_pos_y_menu,ID_COLOR_BLACK,ID_COLOR_WHITE);
 SDLprintText(cadTitle,gb_pos_x_menu,gb_pos_y_menu,ID_COLOR_BLACK,ID_COLOR_WHITE);

 aReturn = (aSel!=-1)?aSel:0;
 OSDMenuRowsDisplayScroll(ptrValue,aReturn,aMax);
 
 while (salir == 0)
 {
  //case SDLK_UP:
  curTime_keyboard = millis();
  if ((curTime_keyboard - curTime_keyboard_before) >= gb_keyboard_cur_poll_ms)
  {
   curTime_keyboard_before= curTime_keyboard;

   #ifdef use_lib_keyboard_uart
    curTime_keyboard_uart= curTime_keyboard;
    if ((curTime_keyboard_uart - curTime_keyboard_before_uart) >= gb_current_ms_poll_keyboard_uart)
    {
     curTime_keyboard_before_uart = curTime_keyboard_uart;
     keyboard_uart_poll();
    
     if (checkKey_uart(KEY_CURSOR_LEFT)==1)
     {
      if (aReturn>10) aReturn-=10;
      OSDMenuRowsDisplayScroll(ptrValue,aReturn,aMax);
     }
     if (checkKey_uart(KEY_CURSOR_RIGHT)==1)
     {
      if (aReturn<(aMax-10)) aReturn+=10;
      OSDMenuRowsDisplayScroll(ptrValue,aReturn,aMax);       
     }  
     if (checkKey_uart(KEY_CURSOR_UP)==1)
     {
      if (aReturn>0) aReturn--;
      OSDMenuRowsDisplayScroll(ptrValue,aReturn,aMax);
     }
     if (checkKey_uart(KEY_CURSOR_DOWN)==1)
     {
      if (aReturn < (aMax-1)) aReturn++;
      OSDMenuRowsDisplayScroll(ptrValue,aReturn,aMax);
     }
     if (checkKey_uart(KEY_ENTER)==1)
     {
      salir= 1;
     }
     if (checkKey_uart(KEY_ESC))
     {
      salir=1; aReturn= 255;    
     }
    }
   #endif


   if (checkAndCleanKey(KEY_F1))
   {
   }
   if (checkAndCleanKey(KEY_CURSOR_LEFT))
   {
    if (aReturn>10) aReturn-=10;
    OSDMenuRowsDisplayScroll(ptrValue,aReturn,aMax);       
   }
   if (checkAndCleanKey(KEY_CURSOR_RIGHT))
   {
    if (aReturn<(aMax-10)) aReturn+=10;
    OSDMenuRowsDisplayScroll(ptrValue,aReturn,aMax);       
   }
          
   if (checkAndCleanKey(KEY_CURSOR_UP))
   {
    if (aReturn>0) aReturn--;
    OSDMenuRowsDisplayScroll(ptrValue,aReturn,aMax);
   }
   if (checkAndCleanKey(KEY_CURSOR_DOWN))
   {
    if (aReturn < (aMax-1)) aReturn++;
    OSDMenuRowsDisplayScroll(ptrValue,aReturn,aMax);
   }
   if (checkAndCleanKey(KEY_ENTER))
   {
    salir= 1;
   }
   //case SDLK_KP_ENTER: case SDLK_RETURN: salir= 1;break;
   if (checkAndCleanKey(KEY_ESC))
   {
    salir=1; aReturn= 255;    
   }
   //case SDLK_ESCAPE: salir=1; aReturn= 255; break;
   //default: break;             
  }
 } 
 gb_show_osd_main_menu= 0;
 return aReturn;
}


//Menu resetear
void ShowTinyResetMenu()
{
 unsigned char aSelNum;
 aSelNum= ShowTinyMenu("Reset",gb_reset_menu,max_gb_reset_menu,-1);   
 if (aSelNum==255)
 {
  return;
 }
 
 if (aSelNum == 1)
 {
  ESP.restart();
 }
 else
 {
  gb_reset= 1;
 } 
}


//Menu velocidad emulador
void ShowTinySpeedMenu()
{
 //1 (normal)
 //2
 //3
 //4
 //5 (fast)
 unsigned char aSelNum;
 aSelNum = ShowTinyMenu("Speed",gb_speed_menu,max_gb_speed_menu,-1);
 if (aSelNum == 255)
 {
  return;
 } 
 gb_speed= aSelNum+1;
}

void ShowTinyKeyboardPollMenu()
{
 unsigned char aSelNum;
 aSelNum = ShowTinyMenu("Poll ms Keyboard",gb_speed_keyboard_poll_menu,max_gb_speed_keyboard_poll_menu,-1);
 if (aSelNum==255)
 {
   return;
 }
 switch (aSelNum)
 {
  case 0: gb_keyboard_cur_poll_ms= 10; break;
  case 1: gb_keyboard_cur_poll_ms= 20; break;
  case 2: gb_keyboard_cur_poll_ms= 30; break;
  case 3: gb_keyboard_cur_poll_ms= 40; break;
  case 4: gb_keyboard_cur_poll_ms= 50; break;
 }
}


  void IRAM_ATTR onTimerSoundDigital()
  {
   //Para digital
   if (gb_spk_data != gb_spk_data_before)
   {
    //digitalWrite(SPEAKER_PIN, gb_spk_data);
    //GPIO 0..31
    if (gb_spk_data)
    {
     GPIO.out_w1ts= 1<<25; //1<<25;
    }
    else
    {
     GPIO.out_w1tc = 1<<25;
    }
    gb_spk_data_before= gb_spk_data;
   }
  
   if (gb_sillence_all==1)
   {
    gb_spk_data= 0;
    return;
   }

   //digitalWrite(SPEAKER_PIN, !digitalRead(SPEAKER_PIN));
   for (unsigned char j=0;j<3;j++)
   {
    if (gb_ct[j] >= (gb_ct_Pulse[j]-1))
    {
     gb_flip[j]= (~gb_flip[j]) & 0x01;
     gb_ct[j]= 0;
    }
    gb_ct[j]++;
   }

   if ((gb_flip[0]==1)&&(gbVolMixer_now[0]>0))
   {
    gb_spk_data= 1;
   }
   else
   {
    if ((gb_flip[1]==1)&&(gbVolMixer_now[1]>0))
    {
     gb_spk_data= 1;
    }
    else
    {
     gb_spk_data= ((gb_flip[2]==1)&&(gbVolMixer_now[2]>0)) ? 1 : 0;
    }
   }
  } 
/*
// perform the actual update from a given stream
void performUpdate(Stream &updateSource, size_t updateSize) {
  
  if (Update.begin(updateSize)) {
    size_t written = Update.writeStream(updateSource);

    if (Update.end()) {
      SDLprintText("Basic32+ loaded successfully, now Reboot", 30, 160, BLUE, ID_COLOR_BLACK); 
      if (Update.isFinished()) {
        delay(1000);
        ESP.restart();
      }
    }
  }
}
*/
void performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize, U_FLASH, 2, 1, "Basic")) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      SDLprintText("Written :  successfully", 30, 160, BLUE, ID_COLOR_BLACK); 
    }
    else {
      SDLprintText("Written only : . Retry?", 30, 160, BLUE, ID_COLOR_BLACK); 
    }
    if (Update.end()) {
      SDLprintText("OTA done!", 30, 160, BLUE, ID_COLOR_BLACK); 
      if (Update.isFinished()) {
        SDLprintText("Basic32+ loaded successfully, now Reboot", 30, 160, BLUE, ID_COLOR_BLACK); 
        delay(1000);
        ESP.restart();
      }
      else {
        SDLprintText("not finished? Something went wrong!", 30, 160, BLUE, ID_COLOR_BLACK);
      }
    }
    else {
      SDLprintText("Error Occurred. Error #: ", 30, 160, BLUE, ID_COLOR_BLACK);
    }

  }
  else
  {
    SDLprintText("Not enough space to begin OTA", 30, 160, BLUE, ID_COLOR_BLACK);
  }
}
//*********************************
void basic_loader(void) {
  
  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

  if ( !SD.begin( kSD_CS, spiSD )) {                        //SD-Card starten
    SDLprintText("SD-Card Mount-Error!", 30, 160, BLUE, ID_COLOR_BLACK);
    return;
  }
  if ( !SD.exists("/basic.bin") ) {
    SDLprintText("Basic not found!", 30, 160, BLUE, ID_COLOR_BLACK);
    return;
  }

  File updateBin = SD.open("/basic.bin");

  if (updateBin) {
    size_t updateSize = updateBin.size();

    if (updateSize > 0) {
      
      SDLprintText("load Basic32+", (30), (160), BLUE, ID_COLOR_BLACK);
      //vga.print("load Basic32+");
      Serial.println("load Basic32+");
      Serial.println(updateSize);
      performUpdate(updateBin, updateSize);
    }

    updateBin.close();
  }
}

 #ifdef use_lib_sound_dac
  
  static DRAM_ATTR unsigned char gb_sin[360]={
   64,66,68,70,72,75,77,79,81,83,85,87,90,92,94,95,
   97,99,101,103,105,106,108,110,111,113,114,115,117,118,119,120,
   121,122,123,124,124,125,126,126,127,127,127,127,127,128,127,127,
   127,127,127,126,126,125,124,124,123,122,121,120,119,118,117,115,
   114,113,111,110,108,106,105,103,101,99,97,95,94,92,90,87,
   85,83,81,79,77,75,72,70,68,66,64,61,59,57,55,52,
   50,48,46,44,42,40,37,35,33,32,30,28,26,24,22,21,
   19,17,16,14,13,12,10,9,8,7,6,5,4,3,3,2,
   1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,2,
   3,3,4,5,6,7,8,9,10,12,13,14,16,17,19,21,
   22,24,26,28,30,31,33,35,37,40,42,44,46,48,50,52,
   55,57,59,61,63,66,68,70,72,75,77,79,81,83,85,87,
   90,92,94,96,97,99,101,103,105,106,108,110,111,113,114,115,
   117,118,119,120,121,122,123,124,124,125,126,126,127,127,127,127,
   127,128,127,127,127,127,127,126,126,125,124,124,123,122,121,120,
   119,118,117,115,114,113,111,110,108,106,105,103,101,99,97,95,
   94,92,90,87,85,83,81,79,77,75,72,70,68,66,64,61,
   59,57,55,52,50,48,46,44,42,40,37,35,33,32,30,28,
   26,24,22,21,19,17,16,14,13,12,10,9,8,7,6,5,
   4,3,3,2,1,1,0,0,0,0,0,0,0,0,0,0,
   0,1,1,2,3,3,4,5,6,7,8,9,10,12,13,14,
   16,17,19,21,22,24,26,28,30,32,33,35,37,40,42,44,
   46,48,50,52,55,57,59,61
  };




  volatile unsigned int gb_contador_muestra=0;
  volatile unsigned char dentro=0;

  void IRAM_ATTR onTimerSoundDAC()
  {
   //Para DAC   
   unsigned int auxByte;
   unsigned int media=0;
   unsigned int valor;

   if (dentro>0)
    return;

   dentro++;   
  
   if (gb_spk_data != gb_spk_data_before)
   {
    #ifdef SPEAKER_PIN == 25
     dac_output_voltage(DAC_CHANNEL_1, gb_spk_data);
    #else 
     dac_output_voltage(DAC_CHANNEL_2, gb_spk_data);
    #endif 
    gb_spk_data_before= gb_spk_data;
   }
  
   gb_contador_muestra++;    

   if (gb_sillence_all==1)
   {
    gb_spk_data= 0;
    dentro=0;
    return;
   }
  

   for (unsigned char i=0;i<3;i++)
   {
    //Tengo que multiplicar por 6, que es 4+2 aproximado a 2xPI
    if (gbVolMixer_now[i] == 0)
    {
     valor= 0;
    }
    else
    {


     auxByte= (180 * gbFrecMixer_now[i] * gb_contador_muestra)>>13; //DIV 8192
     valor= gb_sin[(auxByte & 360)];

 
          
    }
    //media= (media + valor)>>1; //Media aproximada   
    media= (i==0) ? valor : (media+valor)>>1; //Con esto sale exacto 100 Hz, 250 Hz y 500 Hz con el calculo sin double
   } 

   media= (media & 0xFF);
   switch (gb_dac_vol)
   {
    case 1: media= media>>1; break; //25%
    //case 2: media= media; break; //50%
    case 3: media= (media>127) ? 255 : (media<<1); break;//75%     
    case 4: media= (media>63) ? 255 : (media<<2); break; //100%
    case 5: media= (media>31) ? 255 : (media<<3); break; //125%
   }
   gb_spk_data= media;
   dentro=0;
  }  
 #else
 #endif


void ShowTinySoundMenu()
{  
 //Digital ON
 //Digital OFF
 //DAC (125%)
 //DAC (100%)
 //DAC (75%)
 //DAC (50%)
 //DAC (25%)
 //DAC (0%) 
 //DAC MedUnsigned 
 //DAC MedSigned
 unsigned char aSelNum;
 unsigned char modoDigi=0;
 unsigned char modoDAC=0;
 aSelNum = ShowTinyMenu("Sound Options",gb_sound_menu,max_gb_sound_menu,-1);
 if (aSelNum==255)
 {
   return;
 }

 #ifdef use_lib_sound_dac 
  switch (aSelNum)
  {
   case 0: gb_sillence_all= 0; gb_mute= 0; modoDigi= 1; break;
   case 1: gb_sillence_all= 1; gb_mute= 1;  modoDigi= 1; break;
   case 2: gb_dac_vol= 5; gb_sillence_all= 0; gb_mute= 0; modoDAC= 1; break;
   case 3: gb_dac_vol= 4; gb_sillence_all= 0; gb_mute= 0; modoDAC= 1; break;
   case 4: gb_dac_vol= 3; gb_sillence_all= 0; gb_mute= 0; modoDAC= 1; break;
   case 5: gb_dac_vol= 2; gb_sillence_all= 0; gb_mute= 0; modoDAC= 1; break;
   case 6: gb_dac_vol= 1; gb_sillence_all= 0; gb_mute= 0; modoDAC= 1; break;
   case 7: gb_sillence_all= 1; gb_mute= 1; modoDAC= 1; break;
   case 8: 
    //Le he dado a unsigned y estaba en signed
    if (modoDAC==0){
     modoDAC= 1;
    }
    if (gb_sound_signed==1)
    {
     gb_sound_signed= 0; 
     for (unsigned short int i=0;i<256;i++)
     {
      if (gb_sin[i]<64){
       gb_sin[i]= 64-gb_sin[i];
      }
     }
     //gb_sound_signed= ((~gb_sound_signed) & 0x01);
    }
    break;    
   case 9:
    //Le he dado a signo y estaba en unsigned
    if (modoDAC==0){
     modoDAC= 1;
    }
    if (gb_sound_signed==0)
    {
     gb_sound_signed= 1;
     for (unsigned short int i=0;i<256;i++)
     {
      if (gb_sin[i]<64){
       gb_sin[i]= 64-gb_sin[i];
      }
     }    
     //gb_sound_signed= ((~gb_sound_signed) & 0x01);
    }

    break;
  }

  if (modoDigi==1)
  {
   gb_use_sound_digital= 1;
   timerAlarmDisable(gb_timerSound);   
   delay(100);       
   timerDetachInterrupt(gb_timerSound);
   delay(100);
   #ifdef SPEAKER_PIN == 25
    dac_output_disable(DAC_CHANNEL_1);
   #else 
    dac_output_disable(DAC_CHANNEL_2);
   #endif 

   pinMode(SPEAKER_PIN, OUTPUT);
   timerAttachInterrupt(gb_timerSound, &onTimerSoundDigital, true);
   timerAlarmWrite(gb_timerSound, 125, true); //1000000 1 segundo  125 es 8000 hz
   timerAlarmEnable(gb_timerSound);  
  }
  else
  {
   if (modoDAC==1)
   {
    gb_use_sound_digital= 0;
    timerAlarmDisable(gb_timerSound);   
    delay(100);       
    timerDetachInterrupt(gb_timerSound);
    delay(100);
    
    #ifdef SPEAKER_PIN == 25
     dac_output_enable(DAC_CHANNEL_1);
    #else
     dac_output_enable(DAC_CHANNEL_2);
    #endif 
    timerAttachInterrupt(gb_timerSound, &onTimerSoundDAC, true);
    timerAlarmWrite(gb_timerSound, 125, true); //1000000 1 segundo  125 es 8000 hz
    timerAlarmEnable(gb_timerSound);  
   }
  }
 #else
  //Digital solo tiene
  //Digital ON
  //Digital OFF
  gb_use_sound_digital= 1;
  switch (aSelNum)
  {
   case 0: gb_sillence_all= 0; gb_mute= 0; break;
   case 1: gb_sillence_all= 1; gb_mute= 1; break;
  }
 #endif
}


//***************************
void SetVideoMode(unsigned char id, unsigned char aVertical)
{
 unsigned int a0= (0|gb_sync_bits);
 unsigned int a32= (a0<<24)|(a0<<16)|(a0<8)|a0; 

 //0 320x240x60hz V bitluni
 //1 320x240x60hz V fabgl
 //2 320x350x70hz H bitluni
 //3 320x350x70hz V bitluni
 //4 400x300x56.2hz H bitluni
 //5 400x300x56.2hz V bitluni
 //gb_ini_x= 0;
 //gb_ini_y= 0;

 if (gb_videomode_cur != id)
 {
  if (
      ((gb_videomode_cur==2)&&(id==3))
      ||((gb_videomode_cur==3)&&(id==2))
      ||((gb_videomode_cur==4)&&(id==5))
      ||((gb_videomode_cur==5)&&(id==4))
     ) 
  {//Cambio de horizontal a vertical o alreves

   //gb_ini_x= 0;
   //gb_ini_y= 0;  

   gb_videomode_cur= id;
   //gb_videomode_last= id;
   
   switch (gb_videomode_cur)
   {
    case 2: 
     gb_vertical= 0;
     gb_ini_x=11; //(320-224)=96/2/4=12
     gb_ini_y=30; //(350-288)=62/2=31
     break; //320x350x70hz H bitluni
    case 3: 
     gb_vertical= 1; 
     gb_ini_x=3; //(320-288)=32/2/4=4
     gb_ini_y=62; //(350-224)=126/2=63
     break; //320x350x70hz V bitluni
    case 4: 
     gb_vertical= 0; 
     gb_ini_x=21; //(400-224)=176/2/4=22
     gb_ini_y=5; //(300-288)=12/2=6
     break;//400x300x56.2hz H bitluni
    case 5: 
     gb_vertical= 1; 
     gb_ini_x=13; //(400-288)=112/2/4=14
     gb_ini_y=37; //(300-224)=76/2=38
     break;//400x300x56.2hz V bitluni
   }

   //gb_ini_x= 0;
   //gb_ini_y= 0;

   return;
  }

  gb_videomode_cur= id;
  //gb_videomode_last= id;

  SetVideoInterrupt(0);
  delay(100);
  vga_free();
  delay(100);

  unsigned char usepllcteforce=0;
  unsigned int p0=0;
  unsigned int p1=0;
  unsigned int p2=0;
  unsigned int p3=0;    

  Serial.printf("id:%d\r\n",gb_videomode_cur);
  switch (gb_videomode_cur)
  {
   case 0: //320x240x60hz V bitluni
    gb_ptrVideo_cur= VgaMode_vga_mode_320x240;
    gb_vertical= 1;
    gb_ancho= 320;
    gb_alto= 240;
    //224x288
    gb_ini_x=3; //(320-288)=32/2/4=4
    gb_ini_y=7; //(240-224)=16/2=8
    break;

   case 1: //320x240x60hz V fabgl
    gb_ptrVideo_cur= VgaMode_vga_mode_320x240;    
    gb_vertical= 1;
    usepllcteforce= 1;
    p0= 0x000A;
    p1= 0x0057;
    p2= 0x0007;
    p3= 0x0007;
    gb_ancho= 320;
    gb_alto= 240;
    //224x288
    gb_ini_x=3; //(320-288)=32/2/4=4
    gb_ini_y=7; //(240-224)=16/2=8
    break;

   case 2: //320x350x70hz H bitluni
    gb_ptrVideo_cur= VgaMode_vga_mode_320x350;
    gb_vertical= 0;
    gb_ancho= 320;
    gb_alto= 350;
    //224x288
    gb_ini_x=11; //(320-224)=96/2/4=12
    gb_ini_y=30; //(350-288)=62/2=31
    break;    

   case 3: //320x350x70hz V bitluni
    gb_ptrVideo_cur= VgaMode_vga_mode_320x350;
    gb_vertical= 1;
    gb_ancho= 320;
    gb_alto= 350;
    //224x288
    gb_ini_x=3; //(320-288)=32/2/4=4
    gb_ini_y=62; //(350-224)=126/2=63
    break;

   case 4: //400x300x56.2hz H bitluni
    gb_ptrVideo_cur= VgaMode_vga_mode_400x300;
    gb_vertical= 0;
    gb_ancho= 400;
    gb_alto= 300;    
    gb_ini_x=21; //(400-224)=176/2/4=22
    gb_ini_y=5; //(300-288)=12/2=6
    break;

   case 5: //400x300x56.2hz V bitluni
    gb_ptrVideo_cur= VgaMode_vga_mode_400x300;
    gb_vertical= 1;   
    gb_ancho= 400;
    gb_alto= 300;    
    gb_ini_x=13; //(400-288)=112/2/4=14
    gb_ini_y=37; //(300-224)=76/2=38
    break;
  }

  //gb_ini_x= 0;
  //gb_ini_y= 0;

  Serial.printf("w:%d h:%d v:%d pll:%d\r\n",gb_ancho,gb_alto,gb_vertical,usepllcteforce);
  vga_init(pin_config,gb_ptrVideo_cur,false,usepllcteforce,p0,p1,p2,p3);
  SetVideoInterrupt(1);
  gb_sync_bits= vga_get_sync_bits();
  gb_buffer_vga = vga_get_framebuffer();
  gb_buffer_vga32=(unsigned int **)gb_buffer_vga;

  unsigned int a0= (0|gb_sync_bits);
  unsigned int a32= (a0<<24)|(a0<<16)|(a0<<8)|a0;
  for (unsigned int y=0;y<gb_alto;y++)
  {
   for (unsigned int x=0;x<(gb_ancho>>2);x++)
   {  
    gb_buffer_vga32[y][x]= a32; //gb_color_vga[0];
   }
  } 

  for (unsigned short int i=0;i<32;i++)
  {
   //unsigned char data= p->color_rom[i];
   unsigned char data= gb_ptr_rom_82s1237f[i];
   unsigned char r= (data>>1)&0x03;
   unsigned char g= (data>>4)&0x03;
   unsigned char b= (data>>6)&0x03;
   //gb_jj_color[i]= (r<<4)|(g<<2)|(b&0x03);
   //gb_jj_color[i]= (b<<4)|(g<<2)|(r&0x03); //invierto BBGGRR
   gb_jj_color[i]= (b<<4)|(g<<2)|(r&0x03)|gb_sync_bits; //invierto BBGGRR revisar
  }  

  #ifdef use_lib_log_serial  
   //Serial.printf("Set Video %d\r\n",aSelNum);     
   Serial.printf("RAM free %d\r\n", ESP.getFreeHeap()); 
  #endif           

 }
}

void ShowTinyVideoModeMenu()
{
 //0 320x240x60hz V bitluni
 //1 320x240x60hz V fabgl
 //2 320x350x70hz H bitluni
 //3 320x350x70hz V bitluni
 //4 400x300x56.2hz H bitluni
 //5 400x300x56.2hz V bitluni  
 
 //Para SetVideoMode
 //0 320x240
 //1 320x350
 //2 400x300
 unsigned char aSelNum;
 aSelNum = ShowTinyMenu("Video Mode",gb_video_mode_menu,max_gb_video_mode_menu,-1);
 if (aSelNum==255)
 {
   return;
 }
 switch (aSelNum)
 {
  case 0: SetVideoMode(0,1); break; //320x240x60hz bitluni Vertical       
  case 1: SetVideoMode(1,1); break; //320x240x60hz fabgl Vertical   
  case 2: SetVideoMode(2,0); break; //320x350x70hz bitluni Horizontal
  case 3: SetVideoMode(3,1); break; //320x350x70hz bitluni Vertical   
  case 4: SetVideoMode(4,0); break; //400x300x56.2hz fabgl Horizontal   
  case 5: SetVideoMode(5,1); break; //400x300x56.2hz fabgl Vertical   
 }
}

void ShowTinyVGAPollMenu()
{
 //0
 //10 (100 FPS)
 //16 (60 FPS)
 //20 (50 FPS)
 //30 (33 FPS)
 //40 (25 FPS)
 //50 (20 FPS) 
 unsigned char aSelNum;
 aSelNum = ShowTinyMenu("Poll ms VGA",gb_speed_video_poll_menu,max_gb_speed_video_poll_menu,-1);
 if (aSelNum==255)
 {
   return;
 }

 switch(aSelNum)
 {
  case 0: gb_vga_cur_poll_ms= 1; break;
  case 1: gb_vga_cur_poll_ms= 10; break;
  case 2: gb_vga_cur_poll_ms= 16; break;
  case 3: gb_vga_cur_poll_ms= 20; break;
  case 4: gb_vga_cur_poll_ms= 30; break;
  case 5: gb_vga_cur_poll_ms= 40; break;
  case 6: gb_vga_cur_poll_ms= 50; break;
 }
 
}





//*******************************************
void SDLActivarOSDMainMenu()
{     
 gb_show_osd_main_menu= 1;   
}



//Very small tiny osd
void do_tinyOSD() 
{
 unsigned char aSelNum; 
 unsigned char vol[3];

 //Serial.printf("do_tinyOSD\r\n");
 if (checkAndCleanKey(KEY_F1))
 {
  gb_show_osd_main_menu= 1;
 }

 if (gb_show_osd_main_menu == 1)
 {
  for (unsigned char i=0;i<3;i++)
  {
   vol[i]= gbVolMixer_now[i];
   gbVolMixer_now[i]= 0;
  }

  
  gb_sillence_all=1; //silencio= 1; //silencio  

  aSelNum = ShowTinyMenu("MAIN MENU",gb_main_menu,max_gb_main_menu,-1);
  switch (aSelNum)
  {
   case 0: ShowTinyResetMenu(); break;
   case 1: ShowTinySpeedMenu(); break;
   case 2: ShowTinyVideoModeMenu(); break;
   case 3: ShowTinyVGAPollMenu(); break;
   case 4: ShowTinyKeyboardPollMenu(); break;
   case 5: gb_show_fps= ((~gb_show_fps) & 0x01); break;
   case 6: ShowTinySoundMenu(); break;
   case 7: basic_loader(); break;
  }   
  gb_show_osd_main_menu=0; 
 

  SDLClear();
  //SDLSetBorder(); //TRuco rapido borde color 
  for (unsigned char i=0;i<3;i++)
  {
   gbVolMixer_now[i]= vol[i];  
  }
  
  gb_sillence_all= (gb_mute == 1) ? 1 : 0;  
 
 }

}
