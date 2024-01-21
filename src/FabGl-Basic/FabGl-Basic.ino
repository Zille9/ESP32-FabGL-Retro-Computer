/////////////////////////////////////////////////////;//////////////////////////////////////////////////////////////////////////////////////////////
//             Basic32+ with FabGL VGA library + PS2 PS2Controller                                                                                //
//               for VGA monitor output - May 2019                                                                                                //
//                                                                                                                                                //
//      Ursprungsversion von: Rob Cai <rocaj74@gmail.com>                                                                                         //
//      erweitert/modifiziert von:Reinhard Zielinski <zille09@gmail.com>                                                                          //
//                                                                                                                                                //
//      Connections:                                                                                                                              //
//      PS2Controller Data to ESP32 pin 32;                                                                                                       //
//      PS2Controller IRQ (clock) to ESP32 pin 33;                                                                                                //
//      VGA RGB to ESP32 pin 21,22, 18,19 and 4,5                                                                                                 //
//      VGA Hsync and Vsync to ESP32 pins 23 and 15                                                                                               //
//      SD-Card 14, 16, 17, 13 (SCK, MISO, MOSI, CS)             ESP32-Eigenboard                                                                 //
//      SD-Card 14, 2, 12, 13 (SCK, MISO, MOSI, CS)              TTGO 1.4                                                                         //
//      FRAM-Board 14, 16, 17, 0 (SCK, MISO, MOSI, CS)           512kB FRAM am SD-SPI-BUS                                                         //
//      TFT-ILI9341 18, 23, 22, 21, 5 (SCK, MOSI, DC, RESET, CS) TFT-Display ILI9341                                                              //
//                                                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hinweis: Um die SD-Karte zu verwenden, diese Pinconfiguration verwenden: 14, 16, 17, 13 (SCK, MISO, MOSI, CS)
//          -> Kompatibilität zu fabgl-Programmen (VIC20 Emu, CPM-Emu, ZX-Emu, Tiny-CPC usw.)
//
// Authors: Mike Field <hamster@snap.net.nz>
//	        Scott Lawrence <yorgle@gmail.com>
//          Brian O'Dell <megamemnon@megamemnon.com>
//
// Die Version meiner Vor-Authoren bildet die Grundlage für einen erweiterten Basic-Interpreter
// die Grundversion wurde erweitert durch:  -fliesskomma Arithmetik
//                                          -Grafikfunktionen
//                                          -mathematische Funktionen
//                                          -Stringfunktionen
//                                          -lange Variablennamen
//                                          -viele zusätzliche Befehle (DATA,READ,STRG$,LEFT$,RIGHT$,MID$,POS,TAB,SPC,SIN,COS,TAN,ATN,LOG,SQR,PI usw.)
//                                          -erweiterte logische Operatoren (AND, OR)
//                                          -BIT-Manipulation &,|,%,SL,SR usw.
//                                          -Klammerrechnung
//                                          -IF THEN ELSE Abfragen
//                                          -Speichermonitor
//                                          -Exponential-Ein/Ausgabe
//                                          -Verschiedene Sensoren und Komponenten HC-S04, Dallas DS18S20, DHT, LCD, Neopixel-LED, BMP180
//                                          -Zeileneditor
//                                          -integrierte Kurzhilfe
//
// Author:Reinhard Zielinski <zille09@gmail.com>
// April 2021
//
//
#define BasicVersion "1.94b"
#define BuiltTime "19.01.2024"
//#pragma GCC optimize ("O2")
// siehe Logbuch.txt zum Entwicklungsverlauf
// v1.94b:19.01.2024          -Grafikbefehl ROUND erstellt -> erschafft Rechtecke mit abgerundeten Ecken
//                            -momentan aber keine Fill-Funktion
//
// v1.93b:13.01.2024          -Taste F10 mit der Ausgabe der mit Option gesicherten Parameter belegt ->Version, BuiltTime, Tastaturlayout, EEProm-Adresse, Arbeitsverzeichnis
//                            -die restlichen Werte werden beim Start schon angezeigt
//
// v1.92b:07.01.2024          -Dump-Ausgabe für ILI9341 angepasst, da hier im Querformat x und y vertauscht sind
//                            -SD-Karte bisher nicht gleichzeitig mit ILI9341 funktionsfähig !? mal sehen, woran das nun wieder liegt :-(
//                            -ab und zu wird die CardKB-Tastatur nach dem Reset nicht erkannt -> ein reset-Befehl muss initiiert werden
//
// v1.91b:30.12.2023          -CardKB als Tastatur mit PS/2 Software eingebunden (über define auswählbar)
//                            -kleine Änderungen in der ESC-Behandlung, da es beim CardKB kein Ctrl-C gibt -> ESC hat die gleiche Wirkung
//                            -mit dem CardKB und dem ILI9341 ist nun eine Pocket-Variante von Basic32+ möglich :-)
//                            -CardKB funktioniert nicht immer nach dem Reset :-( woran liegt das nun wieder?
//                            -ESC-Abfrage verbessert, jetzt wird auch bei List-Ausgaben korrekt abgebrochen
//                            -EEPROM Adresse falsch (0x57), auf dem MCP23017-Board ist der EEPROM auf Adresse 0x50 ->geändert
//                            -SDA und SCL - Pins jetzt nicht mehr mit OPT IIC änderbar, das führte zu Problemen, da nicht immer korrekte Werte aus dem internen ESP32-Eeprom gelesen wurden
//                            -15459 Zeilen/sek.
//
// v1.90b:24.12.2023          -Fehler in der Datumsbehandlung in ESP32_Time entdeckt -> der Monat wird von 0-11 zurückgegeben, daher wurde in der DIR-Datei-Anzeige der Monat falsch angezeigt
//
// v1.89b:12.12.2023          -TFT-Treiber für ILI9341 240x320 Pixel-Display integriert - Treiber ist über #define auswählbar
//                            -Breadboard-Test soweit erfolgreich
//                            -ab und zu startet das Display nicht korrekt, Ursache muss noch erforscht werden -> Print_info etwas geändert, scheint jetzt zu funktionieren
//                            -neue Platinenvariante muss noch erstellt werden, welche alle Grafik-Varianten vereint (AV,VGA,TFT)
//
// v1.88b:03.12.2023          -MIDI-Funktionalität wieder entfernt
//                            -Sound-Befehl wieder die ursprüngliche Routine aktiviert
//                            -Augenmerk soll mehr auf Funktionalität als Basic-Messcomputer gesetzt werden
//                            -Akku-Messroutine über define auswählbar
//                            -15531 Zeilen/sek.
//
// v1.87b:08.08.2023          -erste MIDI-Funktionalität integriert
//                            -der Sound-Befehl wird für die interne Ausgabe von Tönen benutzt, die ursprüngliche Routine wurde deaktiviert
//                            -Syntax: SND_N 0,45,127,100 -> Chan,Note,Velocity,Duration
//                            -die Angabe von Duration (Dauer) muss noch geändert werden, damit mehrere Noten gleichzeitig erklingen können
//                            -16329 Zeilen/sek.
//
// v1.86b:07.08.2023          -Beginn der Einbindung des Adafruit VS1053 Boards für MIDI-Funktionalität, um die Soundfunktionen aufzupeppen
//                            -2GM-Soundbänke + 2Drumkit-Bänke
//                            -Pan-Funktion, Reverb-Effekt, Pitch-Funktion, Polyfonie bis zu 64 Stimmen
//                            -erste Trockentest's sind vielversprechend
//                            -ein neues Board wird eine zusätzliche MIDI-IN-Buchse besitzen, um das Modul auch von außen mit einem
//                            -MIDI-Keyboard spielen zu können
//                            -ebenfalls geplant ist ein MIDI-Player, der MIDI-Dateien von SD-Karte abspielt
//                            -wenn alles funktioniert, wird der ursprüngliche Sound-Befehl überflüssig
//
// v1.85b:31.07.2023          -Umstellung Arduino IDE auf 2.x, scheint jetzt zu funktionieren.....
//                            -Fehlerbehandlung funktioniert nicht mehr, Fehler führen zu einer "Syntax Error!" Dauerschleife ???
//                            -erst mal wieder auf 1.06 zurück-geswitcht :-( ärgerlich.
//                            -Neue Idee: als Soundmodul ein GM-Modul (VS1053 hat MIDI inkl. Midi-Instrumente on board)
//                            -Ansteuerung über MIDI-Befehle, möglich auch über Optokoppler von außen :-)
//                            -16413 Zeilen/sek.
//
// v1.84b:18.07.2023          -Blinkfrequenz des Cursors in fabglconf.h auf 200ms gesetzt
//                            -Befehl FILL zum füllen unregelmässiger Formen begonnen
//                            -ESP32Time.h hinzugefügt, mit dessen Hilfe jetzt der Datei-Zeitstempel beim Speichern hinzugefügt wird
//                            -cmd_dir - Routine mit der Ausgabe des Datei-Zeitstempels erweitert
//                            -16275 Zeilen/sek.
//
// v1.83b:15.07.2023          -cmd_new mit setzen des Hauptfesters ergänzt, nach einem NEW wurden die Fensterparameter zwar gelöscht aber nicht das
//                            -Hauptfenster gesetzt, dadurch hing der Cursor in der linken oberen Ecke fest
//                            -Move_up-Subroutine etwas erweitert, jetzt wird nach dem Kopieren des Fensterinhaltes die letzte Zeile gelöscht,
//                            -das war nötig, weil sonst eventuell existierender Text in der letzten Zeile immer wieder mitkopiert wurde
//                            -Startbildschirm mit der Anzeige von BuiltTime ergänzt
//                            -list_out Anzahl der Ausgabezeilen nun abhängig vom gewählten Font
//                            -list_out um den Parameter bis zu welcher Zeile ausgegeben werden soll, erweitert (LIST 10,50 - Ausgabe Zeile 10 bis 50)
//                            -16305 Zeilen/sek.
//
// v1.82b:11.07.2023          -DATA-Verarbeitung auf Arrays erweitert
//                            -FILE_RD auf Arrays erweitert
//                            -Fehler in Array-Dimensionierung behoben, Array-Felder wurden zu gross berechnet
//                            -Fehler in der ELSE Verarbeitung entdeckt, bei dem Versuch, die Else-Anweisung in der gleichen Zeile zu bearbeiten
//                            -wurden auch die nach nicht erfolgreicher IF-Verarbeitung stehende Befehle ausgeführt,
//                            -was manche Programme nicht ausführbar machte, ELSE erst mal wieder in den Urzustand versetzt (nächste Zeile)
//                            -Routine line_terminator geändert, jetzt erfolgt erst ein Carrige-Return und dann ein Next-Line,
//                            -war vorher umgekehrt, das führte dazu, das in gespeicherten Programmen zwischen den Zeilen immer eine Leerzeile
//                            -eingefügt war, jetzt wird Zeile für Zeile korrekt geschrieben
//                            -Anzahl der Ausgabezeilen bei Memory_Dump und DIR ist jetzt vom verwendeten Font abhängig
//                            -17205 Zeilen/sek.
//
// v1.81b:09.07.2023          -WINDOW-Befehl um die Möglichkeit einen Fenstertitel zu setzen ergänzt
//                            -CLS entsprechend angepasst
//                            -CLS-Scrolleffekt wieder deaktiviert, da die unterste Bildschirmzeile beim Scrolling nicht berücksichtigt wird
//                            -Bildschirmausgabe weiter auf die Windowfunktion angepasst (Datei-Befehle, Memory_Dump)
//                            -Variable Pencolor entfernt, führte zu unlogischen Farbreaktionen
//                            -PEN-Befehl ändert nur temporär die Zeichenfarbe COL-Befehl ändert die Farben permanent
//                            -Fehler in der list_out-Routine behoben, wenn eine Zeile genau eine Bildschirmbreite lang war, wurde kein Zeilenumbruch
//                            -ausgelöst und die nächste Zeile überschrieb die vorherige auf dem Bildchirm
//                            -Funktionstasten dürfen im Fenster (Window) nicht benutzt werden, der ESP hängt sich auf???!
//                            -subroutine printnum etwas optimiert
//                            -16452 Zeilen/sek.
//
// v1.80b:08.07.2023          -WINDOW-komplett neu erstellt - ohne SetscrollRegion, sieht schon viel besser aus
//                            -CLS-Befehl auf Scrolleffekt geändert, das sieht mehr Retro aus :-)
//                            -fehlt noch die Speicherung der Cursorposition, momentan werden immer Initialwerte gesetzt, wenn das Fenster
//                            -gewechselt wird
//                            -Speicherung der Cursorposition erledigt, beim Fensterwechsel wird die alte Cursorposition in der nächsten Zeile gesetzt
//                            -nicht ganz korrekt aber akzeptabel
//                            -Hinweis: die Cursortasten, Del und Ins sind im Fenster wirkungslos bzw. erzeugen Pseudo-char's,
//                            -nur so konnte ein ESP-Absturz verhindert werden
//                            -Taste F1 (Grafiksymbole on/off) schaltet zur optischen Signalisation die Scroll-LED ein und aus
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Konfiguration Grafiktreiber und Akku-Überwachung
//---------------------------------------------------------------- Akku-Überwachung für Akkubetrieb ------------------------------------------------
//#define Akkualarm_enabled
//
//
//---------------------------------------------------------------- Auswahl Bildschirmtreiber -------------------------------------------------------
//#define AVOUT                   //activate for AV
#define VGA64                   //activate VGA 64 Color 320x240 Pixel Driver
//#define ILI9341                 //TFT-Display ILI9341 (mit Touch-Controller XPT2046)
//#define CardKB                  //Verwendung von CardKB als Tastatur (nur mit PS2-Software nutzbar)


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
#define TFT_SCK    18
#define TFT_MOSI   23
#define TFT_CS     5
#define TFT_DC     22
#define TFT_RESET  21
#define TFT_SPIBUS VSPI_HOST
#endif


//------------------------------------------ Tastatur,GFX-Treiber- und Terminaltreiber -------------------------------------------------------------
fabgl::PS2Controller    PS2Controller;
fabgl::Canvas           GFX(&VGAController);
TerminalController      tc(&Terminal);

#define erststart_marker 131                //dieser Marker steht im EEprom an Position 100 - wird der ESP32 zum ersten mal mit dem Basic gestartet werden standard-Werte gesetzt
//damit eine benutzbare Version gestartet wird
//---------------------------------------------------- verfügbare Themes ---------------------------------------------------------------------------
const char * Themes[]    PROGMEM = {"C64", "C128", "CPC", "ATARI 800", "ZX-Spectrum", "KC87", "KC85", "VIC-20", "TRS-80", "ESP32+", "LCD", "User"}; //Theme-Namen
const char * Keylayout[] PROGMEM = {" ", "US", "UK", "GE", "IT", "ES", "FR", "BE", "NO", "JP"};
byte x_char[]      PROGMEM = {8, 5, 6, 8,  10, 8,  8,  8,  8,  8,  8,  8,  8,  6,  8,  4, 6,  7,  7,  8, 8, 8, 6, 9, 8, 6}; //x-werte der Fontsätze zur Berechnung der Terminalbreite
byte y_char[]      PROGMEM = {8, 8, 8, 14, 20, 14, 14, 16, 16, 14, 14, 14, 16, 10, 14, 6, 12, 13, 14, 9, 14, 14, 10, 15, 16, 8}; //y-werte der Fontsätze zur Berechnung der Terminalhöhe

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------- Soundgenerator ----------------------------------------------------------------------------
unsigned int noteTable []  PROGMEM = {16350, 17320, 18350, 19450, 20600, 21830, 23120, 24500, 25960, 27500, 29140, 30870}; //Notentabelle für Soundausgabe
//------------------------------------------------------------- Soundgenerator ----------------------------------------------------------------------------
//------------------------------------------------------- MIDI-Funktionen -------------------------------------------------------------------------
/*
  #define MIDI_Baud 31250
  #define VS1053_MIDI Serial2
  //#include <MD_MIDIFile.h>

  #define MIDI_NOTE_ON  0x90
  #define MIDI_NOTE_OFF 0x80
  #define MIDI_CHAN_MSG 0xB0
  #define MIDI_CHAN_BANK 0x00
  #define MIDI_CHAN_VOLUME 0x07
  #define MIDI_CHAN_PAN 0x0A
  #define MIDI_CHAN_PITCH 0xE0
  #define MIDI_EFFECT_CNTRL 0x0C
  #define MIDI_EFFECT_LEVEL 0x5B
  #define MIDI_CHAN_PROGRAM 0xC0

  #define MIDI_BANK_DEFAULT 0x00
  #define MIDI_BANK_DRUMS1 0x78
  #define MIDI_BANK_DRUMS2 0x7F
  #define MIDI_BANK_MELODY 0x79
  //SDFAT  MIDI_SD;
  //MD_MIDIFile SMF;

  // Define constants for MIDI channel voice message IDs
  const uint8_t NOTE_OFF = 0x80;    // note on
  const uint8_t NOTE_ON = 0x90;     // note off. NOTE_ON with velocity 0 is same as NOTE_OFF
  const uint8_t POLY_KEY = 0xa0;    // polyphonic key press
  const uint8_t CTL_CHANGE = 0xb0;  // control change
  const uint8_t PROG_CHANGE = 0xc0; // program change
  const uint8_t CHAN_PRESS = 0xd0;  // channel pressure
  const uint8_t PITCH_BEND = 0xe0;  // pitch bend

  // Define constants for MIDI channel control special channel numbers
  const uint8_t CH_RESET_ALL = 0x79;    // reset all controllers
  const uint8_t CH_LOCAL_CTL = 0x7a;    // local control
  const uint8_t CH_ALL_NOTE_OFF = 0x7b; // all notes off
  const uint8_t CH_OMNI_OFF = 0x7c;     // omni mode off
  const uint8_t CH_OMNI_ON = 0x7d;      // omni mode on
  const uint8_t CH_MONO_ON = 0x7e;      // mono mode on (Poly off)
  const uint8_t CH_POLY_ON = 0x7f;      // poly mode on (Omni off)

  //------------------------------------------------------- MIDI-Funktionen -------------------------------------------------------------------------
*/

#define RAMEND 60928//----------------------------------- RAM increment for ESP32 ----------------------------------------------------------------- 

// ---------------------------------- SD-Karten-Zugriff--------------------------------------------------------------------------------------------
#include "FS.h"
#include <SD.h>
#include <SPI.h>

//-------------------------------------- Verwendung der SD-Karte ----------------------------------------------------------------------------------
//SPI CLASS FOR REDEFINED SPI PINS !
SPIClass spiSD(HSPI);
//Konfiguration der SD-Karte unter FabGl - kann mit OPT geändert werden
byte kSD_CS   = 13;
byte kSD_MISO = 16;
byte kSD_MOSI = 17;
byte kSD_CLK  = 14;
byte SD_SET   = 44;      // -steht 44 im EEprom-Platz 10, dann sind die Werte im EEprom gültig

#define kSD_Fail  0      //Fehler-Marker
#define kSD_OK    1      //OK-Marker
File fp;

//------------------------------------- OTA-Update-Lib --------------------------------------------------------------------------------------------
#include <Update.h>

//------------------------------------- ESP32-Time-Lib fuer Datei-zeitstempel ---------------------------------------------------------------------
#include <ESP32Time.h>
ESP32Time e_rtc(0);  // offset in seconds GMT+1

//------------------------------------- Mathematische Funktionen fuer printnum --------------------------------------------------------------------
#include "MathHelpers.h"

//------------------------------------- SPI-FRAM-Lib ----------------------------------------------------------------------------------------------
#include "Adafruit_FRAM_SPI.h"

// ------------------------------ Dallas Temp-Sensor ----------------------------------------------------------------------------------------------
#include <OneWire.h>
#include <DallasTemperature.h>
bool twire = false;

//--------------------------------- DHT-Sensor ----------------------------------------------------------------------------------------------------
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
uint32_t delayMS;

//-------------------------------- BMP180-Sensor---------------------------------------------------------------------------------------------------
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
// Store an instance of the BMP180 sensor.
Adafruit_BMP085 bmp;
// Store the current sea level pressure at your location in Pascals.
float seaLevelPressure = SENSORS_PRESSURE_SEALEVELHPA;

// -------------------------- EEPROM Routinen für Parameter-Speicherung ---------------------------------------------------------------------------
#include <EEPROM.h>
#define EEPROM_SIZE 512  //2048 byte lesen/speichern

//---------------------------- EEPROM o.FRAM-Chip I2C-Adressen ------------------------------------------------------------------------------------

short int EEprom_ADDR = 0x50; //-> Adresse 0x50 ist der EEPROM auf dem MCP23017 Board


// ---------------------------- W2812-seriell LED-Treiber -----------------------------------------------------------------------------------------
#include <Adafruit_NeoPixel.h>
unsigned int LED_COUNT      = 255;
unsigned int LED_PIN        = 2;
unsigned int LED_BRIGHTNESS = 50;
unsigned int LED_TYP        = 2;
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);                //WS2812

//------------------------------ MCP23017 IO-Expander ---------------------------------------------------------------------------------------------
#include <Adafruit_MCP23X17.h>   //Adafruit MCP23xx Treiber
Adafruit_MCP23X17 mcp;
short int MCP23017_ADDR = 0x20 ; //Adresse 32 (0x20) für eingebauten MCP23017
bool mcp_start_marker = false;

//------------------------------ I2C Library ------------------------------------------------------------------------------------------------------
#include <Wire.h>           // for I2C 
#include "RTClib.h"         //to show time
TwoWire myI2C = TwoWire(0); //eigenen I2C-Bus erstellen
RTC_DS3231 rtc;

//-------------------------------------- LCD-Treiber ----------------------------------------------------------------------------------------------
#include "HD44780_LCD_PCF8574.h"
#define DISPLAY_DELAY_INIT 50 // mS
int LCD_SPALTEN, LCD_ZEILEN, LCD_ADRESSE, LCD_NACHKOMMA;
bool LCD_start_marker = false;
bool LCD_Backlight = true;

//---------------------------------------- Konfiguration FRAM -------------------------------------------------------------------------------------
byte FRAM_CS  = 0;//13;             //SPI_FRAM 512kB CS-Pin
Adafruit_FRAM_SPI spi_fram = Adafruit_FRAM_SPI(kSD_CLK, kSD_MISO, kSD_MOSI, FRAM_CS);

//---------------------------------------- spezielle FRam-Adressen --------------------------------------------------------------------------------
word FRAM_OFFSET      = 0x8000;     //Offset für Poke-Anweisungen, um zu verhindern, das in den Array-Bereich gepoked wird
word FRAM_PIC_OFFSET  = 0x12C04;    //Platz pro Bildschirm im Speicher 320x240=76800 + 4Byte für die Dimension = 76804
long load_adress      = 0x70000;    //hier kann ein Basicprogramm abgelegt werden (Eingabe: LOAD oder SAVE ohne Parameter)
//---------------------------------------- Array-Parameter ----------------------------------------------------------------------------------------
//Der Arraybereich befindet sich 0x0..0x7fff
word Var_Neu_Platz =  0;            //Adresse nächstes Array-Feld Start bei 0x77e00
static word VAR_TBL = 0x7e00;       //Variablen-Array-Tabelle im FRAM
static word STR_TBL = 0x7f00;       //String-Array-Tabelle im FRAM
//-------------------------------------------------------------------------------------------------------------------------------------------------



//------------------------------- Konfiguration serielle Schnittstelle ----------------------------------------------------------------------------
uint8_t prx, ptx;             //RX- und TX-Pin
uint32_t pbd;                 //Baudrate
bool ser_marker = false;      //seriell-Marker, wenn gesetzt erfolgt jede Printausgabe auch auf die serielle Schnittstelle
bool list_send = false;
bool serout_marker = false;
#define SERIAL_SIZE_RX 1024

//diese Konstellation funktioniert mit ESP-Eigenboard und RTC-Modul oder 27 und 26 für die Freigabe des Seriellports
byte IIC_SET = 55;      // -steht 55 im EEprom-Platz 13, dann sind die Werte im EEprom gültig
// dies ist die Standard-Konfiguration
byte SDA_RTC = 3;
byte SCL_RTC = 1;


byte Keyboard_lang = 3; //Tastatur-Layout (Deutsch)
byte KEY_SET = 66;      //-steht 66 im EEprom Platz 15, dann Nummer des Keyboard-Layouts aus dem EEProm laden
byte THEME_SET = 77;    //-steht 77 im EEPROM Platz 17, dann setze das gespeicherte Theme
byte PATH_SET = 88;     //-steht 88 im EEPROM Platz 19, dann setze Arbeits-Pfad


//------------------------------------- Akku-Überwachung ------------------------------------------------------------------------------------------
#ifdef Akkualarm_enabled
hw_timer_t *Akku_timer = NULL;      //Interrupt-Routine Akku-Überwachung
#endif
#define Batt_Pin 39                 //Pin wird in jedem Fall definiert

//------------------------------- BMP-Info-Parameter für PIC-Befehl -------------------------------------------------------------------------------
uint32_t bmp_width, bmp_height;

// RAM Puffer-Größe für Programm und Benutzereingaben
#define kRamSize  RAMEND

#define bool int
#define true 1
#define false 0

//für Texte/Zeichenketten im Flash
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte( A ) *(A)
#endif

#define STR_LEN 40
#define STR_SIZE 26*STR_LEN             //Stringspeicher = Stringlänge 26*40 Zeichen (A..Z * 40 Zeichen)

//------------------------------ hier wird der Funktionsstring gespeichert ------------------------------------------------------------------------
#define FN_SIZE STR_LEN                 //Funktionsspeicher für benutzerdefinierte Funktionen mit bis zu vier Operatoren-> FN A(A,B,C,D)

bool inhibitOutput = false;
static bool autorun = false;            //Programm nach dem Laden automatisch starten
static bool triggerRun = false;

short int Vordergrund = 43;             //Standard-Vordergrundfarbe (wenn noch nichts im EEprom steht)
short int Hintergrund = 18;             //Standard-Hintergrundfarbe (wenn noch nichts im EEprom steht)
short int fontsatz = 0;                 //Nummer des ausgewählten Fontsatzes
short int user_vcolor = Vordergrund;    //User-Vordergrundfarbe
short int user_bcolor = Hintergrund;    //User-Hintergrundfarbe
short int user_font = fontsatz;         //User-Fontsatz

short int Prezision = 6;                //Standardfestlegung der Nachkommastellen
static bool chr = false;                //marker für CHR$ für Print
static bool string_marker = false;      //marker für Strings (Print)
static bool func_string_marker = false; //marker für String$-Funktion (Print)
static bool tab_marker = false;         //marker für TAB (Print)
static bool semicolon = false;          //marker für semikolon (Print)
static byte fstring = 0;                //Übergabewert für STRING$(n,"string")
short int Theme_state = 0;              //aktuelle Theme Nummer (im EEPROM gespeichert)
static bool Theme_marker = false;       //Theme-Marker, falls Farben geändert
short int Mode_state = 0;               //aktuelle Auflösung (im EEProm gespeichert)

static bool break_marker = false;      //********** Test für CardKB *****************

//------------------------------ Grid-Parameter ---------------------------------------------------------------------------------------------------
int Grid[15];  //0=x, 1=y, 2=xx, 3=yy, 4=zell_x, 5=zell_y, 6=pix_x, 7=pix_y, 8=frame-col, 9=grid_col
int Grid_point_x, Grid_point_y;

//------------------------------ Window-Parameter -------------------------------------------------------------------------------------------------

int Frame_nr;                 //5 Fenster können erstellt werden
int Frame_x[6];
int Frame_y[6];
int Frame_xx[6];
int Frame_yy[6];
int Frame_curx[6];            //X-Cursor Initialwert
int Frame_curtmpx[6];         //X-Cursor temporärer Wert
int Frame_curtmpy[6];         //Y-Cursor temporärer Wert
int Frame_cury[6];            //Y-Cursor Initialwert
int Frame_col[6];             //Rahmenfarbe
int Cursor_x, Cursor_y;       //temporäre Cursorpositionen
int Frame_vcol[6];            //Vordergrundfarbe
int Frame_hcol[6];            //Hintergrundfarbe
bool Frame_title[6];          //Titeltext
char Frame_ttext[6][STR_LEN]; //Fenster-Titel-String

//-------------------------------------------------------------------------------------------------------------------------------------------------

short int onoff = 1;          //Cursor status

//-------------------------------------------------------------------------------------------------------------------------------------------------

// these will select, at runtime, where IO happens through for load/save
enum {
  kStreamTerminal = 0,
  kStreamFile,
  kStreamSerial,
  kStreamFram
};
static char inStream = kStreamTerminal;
static char outStream = kStreamTerminal;

static char program[kRamSize];            //Basic-Programmspeicher
static char Stringtable[STR_SIZE];        //Stringvariablen mit 1 Buchstaben -> 26*40 = 1040 Bytes
//-------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------- DEFN - FN -------------------------------------------------------------------------------------------------
static char Fntable[26][FN_SIZE];         // bytes 40 String = 40*26 ->1040 bytes ->Funktionsstring-Array
int Fnvar = 0;                            //Operatorenzähler
int Fnoperator[27 * 5];                   //DEFN A(a,b,c,d,e,f,g,h)-> Name 0-26,0-26=Operator1,0-26=operator2,0-26=Operator3,0-26=Operator4 + 1 Anzahl
bool fn_marker = false;

//------------------------------------ Editor -----------------------------------------------------------------------------------------------------
char const * Edit_line = nullptr;        //Editor-Zeile
long editpos;                            //Position innerhalb des Programs
//------------------------------------ Interpreter ------------------------------------------------------------------------------------------------
char tempstring[STR_LEN];                //String Zwischenspeicher

//------------------------------------ Dateifunktionen FREAD,FWRITE -------------------------------------------------------------------------------
char filestring[STR_LEN];                //Namensstring für Dateioperationen Fread,Fwrite
static bool Datei_open = false;          //FREAD, FWRITE Open-marker
long File_pos = 0;                       //Dateipositions-merker der geöffneten Datei
long File_size = 0;                      //Dateigrösse der geöffneten Datei

static char *txtpos, *list_line, *tmptxtpos, *dataline;
static char expression_error;
static char *tempsp;
static char sd_pfad[STR_LEN];            //SD-Card Datei-Pfad

char path1[STR_LEN], path2[STR_LEN];     //Variablen für Dateioperationen
unsigned int Datum[4];                   //Datums-Array
unsigned int Zeit[4];                    //Zeit-Array

//----------------------------------------- DATA Variablen ----------------------------------------------------------------------------------------
unsigned int datapointer = 0;       //data-Zeiger innerhalb des Datanfeldes
unsigned int restorepointer = 0;    //begin des Datanfeldes
unsigned int num_of_datalines = 0;  //Anzahl DATA-Zeilen
unsigned int current_dataline = 0;  //aktuelle DATA-Zeile
unsigned int data_numbers[300];     //Array zur speicherung von 300 DATA Zeilennummern

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ASCII Characters
#define CR	'\r'
#define NL	'\n'
#define LF  0x0a
#define TAB	'\t'
#define BELL	'\b'
#define SPACE   ' '
#define SQUOTE  '\''
#define DQUOTE  '\"'
#define CTRLC	0x03
#define CTRLH	0x08
#define CTRLS	0x13

bool Graph_char = false;                               //Grafiksymbole "AUS"
bool tron_marker = false;                              //TRON "AUS"

typedef short unsigned LINENUM;

/***************************Basic-Befehle ********************************/
// Keyword table and constants - the last character has 0x80 added to it
const static char keywords[] PROGMEM = {
  'L', 'I', 'S', 'T' + 0x80,
  'L', 'O', 'A', 'D' + 0x80,
  'N', 'E', 'W' + 0x80,
  'R', 'U', 'N' + 0x80,
  'S', 'A', 'V', 'E' + 0x80,
  'N', 'E', 'X', 'T' + 0x80,
  'R', 'E', 'N' + 0x80,
  'I', 'F' + 0x80,
  'G', 'O', 'T', 'O' + 0x80,
  'G', 'O', 'S', 'U', 'B' + 0x80,
  'R', 'E', 'T', 'U', 'R', 'N' + 0x80,
  'R', 'E', 'M' + 0x80,
  'F', 'O', 'R' + 0x80,
  'I', 'N', 'P', 'U', 'T' + 0x80,
  'P', 'R', 'I', 'N', 'T' + 0x80,
  'P', 'O', 'K', 'E' + 0x80,
  'D', 'I', 'R' + 0x80,
  'C', 'L', 'S' + 0x80,
  'P', 'O', 'S' + 0x80,
  'C', 'O', 'L' + 0x80,
  'P', 'S', 'E', 'T' + 0x80,
  'C', 'I', 'R', 'C' + 0x80,
  'L', 'I', 'N', 'E' + 0x80,
  'R', 'E', 'C', 'T' + 0x80,
  'F', 'O', 'N', 'T' + 0x80,
  'P', 'A', 'U', 'S', 'E' + 0x80,
  'E', 'N', 'D' + 0x80,
  'C', 'L', 'E', 'A', 'R' + 0x80,
  'T', 'H', 'E', 'N' + 0x80,
  'E', 'L', 'S', 'E' + 0x80,
  'C', 'U', 'R' + 0x80,
  'P', 'R', 'Z' + 0x80,
  'D', 'M', 'P' + 0x80,
  'S', 'T', 'Y', 'L', 'E' + 0x80,
  'S', 'C', 'R', 'O', 'L', 'L' + 0x80,
  'S', 'T', 'A', 'R', 'T' + 0x80,
  'T', 'H', 'E', 'M', 'E' + 0x80,
  'D', 'A', 'T', 'A' + 0x80,
  'R', 'E', 'A', 'D' + 0x80,
  'R', 'E', 'S', 'T', 'O', 'R', 'E' + 0x80,
  'D', 'E', 'L' + 0x80,
  'A', 'N', 'D' + 0x80,
  'O', 'R' + 0x80,
  'R', 'T', 'C' + 0x80,
  'I', 'I', 'C' + 0x80,
  'D', 'O', 'U', 'T' + 0x80,
  'P', 'W', 'M' + 0x80,
  'D', 'A', 'C' + 0x80,
  'D', 'R', 'A', 'W' + 0x80,
  'S', 'P', 'R', 'T' + 0x80,
  'P', 'U', 'L', 'S', 'E' + 0x80,
  'S', 'N', 'D' + 0x80,
  'P', 'E', 'N' + 0x80,
  'O', 'N' + 0x80,
  'L', 'C', 'D' + 0x80,
  'P', 'O', 'R', 'T' + 0x80,
  'P', 'I', 'N' + 0x80,
  'C', 'H', 'D' + 0x80,
  'M', 'K', 'D' + 0x80,
  'R', 'M', 'D' + 0x80,
  'D', 'E', 'F', 'N' + 0x80,
  'L', 'E', 'D' + 0x80,
  'E', 'D', 'I', 'T' + 0x80,
  'D', 'O', 'K', 'E' + 0x80,
  'B', 'E', 'E', 'P' + 0x80,
  'D', 'I', 'M' + 0x80,
  'O', 'P', 'T' + 0x80,
  'F', 'P', 'O', 'K', 'E' + 0x80,
  'M', 'N', 'T' + 0x80,
  'C', 'O', 'M' + 0x80,
  'P', 'I', 'C' + 0x80,
  'O', 'P', 'E', 'N' + 0x80,
  'C', 'L', 'O', 'S', 'E' + 0x80,
  'F', 'I', 'L', 'E' + 0x80,
  'T', 'Y', 'P', 'E' + 0x80,
  'B', 'L', 'O', 'A', 'D' + 0x80,
  'G', 'R', 'I', 'D' + 0x80,
  'T', 'E', 'X', 'T' + 0x80,
  '?' + 0x80,
  'W', 'I', 'N', 'D', 'O', 'W' + 0x80,
  'H', 'E', 'L', 'P' + 0x80,
  'R', 'O', 'U', 'N', 'D' + 0x80,
  0
};

// by moving the command list to an enum, we can easily remove sections
// above and below simultaneously to selectively obliterate functionality.
enum {
  KW_LIST = 0,
  KW_LOAD,
  KW_NEW,
  KW_RUN,
  KW_SAVE,
  KW_NEXT,
  KW_REN,
  KW_IF,
  KW_GOTO,
  KW_GOSUB,
  KW_RETURN,    //10
  KW_REM,
  KW_FOR,
  KW_INPUT,
  KW_PRINT,
  KW_POKE,
  KW_DIR,
  KW_CLS,
  KW_POS,
  KW_COLOR,
  KW_PSET,      //20
  KW_CIRCLE,
  KW_LINE,
  KW_RECT,
  KW_FONT,
  KW_DELAY,
  KW_END,
  KW_CLEAR,
  KW_THEN,
  KW_ELSE,
  KW_CURSOR,    //30
  KW_PREZISION,
  KW_DUMP,
  KW_STYLE,
  KW_SCROLL,
  KW_START,
  KW_THEME,
  KW_DATA,
  KW_READ,
  KW_RESTORE,
  KW_DEL,      //40
  KW_AND,
  KW_OR,
  KW_SRTC,
  KW_IIC,
  KW_DWRITE,
  KW_PWM,
  KW_DAC,
  KW_DRAW,
  KW_SPRITE,
  KW_PULSE,    //50
  KW_SOUND,
  KW_PEN,
  KW_ON,
  KW_LCD,
  KW_MCPPORT,
  KW_MCPPIN,
  KW_CHDIR,
  KW_MKDIR,
  KW_RMDIR,
  KW_DEFFUNC, //60
  KW_LED,
  KW_EDIT,
  KW_DOKE,
  KW_BEEP,
  KW_DIM,
  KW_OPTION,
  KW_FPOKE,
  KW_MOUNT,
  KW_COM,
  KW_PIC,     //70
  KW_OPEN,
  KW_CLOSE,
  KW_FILE,
  KW_TYPE,
  KW_CPM,
  KW_GRID,
  KW_TEXT,
  KW_PRINTING,
  KW_WINDOW,
  KW_HELP,    //80
  KW_ROUND,
  KW_DEFAULT  //82/* hier ist das Ende */
};

int KW_WORDS = KW_DEFAULT;

// Variablen zur Zwischenspeicherung von logischen Operationen (AND OR)

int logic_counter;
int logic_ergebnis[10];

//-> bis zu 5 AND oder OR Vergleiche können in einer Zeile vorkommen

struct stack_for_frame {
  char frame_type;    //1byte
  int for_var;        //2byte
  float to_var;       //4byte
  float step;         //4byte
  char *current_line; //1byte
  char *txtpos;       //1byte
};

struct stack_gosub_frame {
  char frame_type;
  char *current_line;
  char *txtpos;
};

#define User_Ram (kRamSize-STACK_SIZE-(26 * 27 * VAR_SIZE))

//**************************** Basic-Funktionen *********************************
const static char func_tab[] PROGMEM = {
  'P', 'E', 'E', 'K' + 0x80,
  'A', 'B', 'S' + 0x80,
  'R', 'N', 'D' + 0x80,
  'S', 'I', 'N' + 0x80,
  'C', 'O', 'S' + 0x80,
  'T', 'A', 'N' + 0x80,
  'L', 'O', 'G' + 0x80,
  'S', 'G', 'N' + 0x80,
  'S', 'Q', 'R' + 0x80,
  'I', 'N', 'T' + 0x80,
  'M', 'I', 'N' + 0x80,
  'M', 'A', 'X' + 0x80,
  'E', 'X', 'P' + 0x80,
  'G', 'E', 'T' + 0x80,
  'C', 'H', 'R', '$' + 0x80,
  'I', 'N', 'K', 'E', 'Y' + 0x80,
  'F', 'O', 'N', 'T' + 0x80,
  'A', 'S', 'C' + 0x80,
  'T', 'I', 'M', 'E', 'R' + 0x80,
  'A', 'T', 'N' + 0x80,
  'L', 'E', 'N' + 0x80,
  'I', 'N', 'S', 'T', 'R' + 0x80,
  'C', 'O', 'M', 'P', '$' + 0x80,
  'B', 'I', 'N' + 0x80,
  'H', 'E', 'X' + 0x80,
  'L', 'E', 'F', 'T', '$' + 0x80,
  'R', 'I', 'G', 'H', 'T', '$' + 0x80,
  'M', 'I', 'D', '$' + 0x80,
  'T', 'A', 'B' + 0x80,
  'S', 'P', 'C' + 0x80,
  'S', 'T', 'R', '$' + 0x80,
  'D', 'A', 'T', 'E' + 0x80,
  'T', 'I', 'M', 'E' + 0x80,
  'I', 'I', 'C' + 0x80,
  'A', 'K', 'K', 'U' + 0x80,
  'A', 'I', 'N' + 0x80,
  'D', 'I', 'N' + 0x80,
  'U', 'C', '$' + 0x80,
  'L', 'C', '$' + 0x80,
  'V', 'A', 'L' + 0x80,
  'M', 'E', 'M' + 0x80,
  '!'  + 0x80,
  'T', 'E', 'M', 'P' + 0x80,
  'D', 'H', 'T' + 0x80,
  'C', 'O', 'L' + 0x80,
  'B', 'M', 'P' + 0x80,
  'F', 'N' + 0x80,
  'P', 'O', 'R', 'T' + 0x80,
  'P', 'I', 'N' + 0x80,
  'P', 'I' + 0x80,
  'L', 'N' + 0x80,
  'D', 'E', 'E', 'K' + 0x80,
  'F', 'P', 'E', 'E', 'K' + 0x80,
  'G', 'P', 'X' + 0x80,
  'G', 'P', 'I', 'C' + 0x80,
  'M', 'A', 'P' + 0x80,
  'C', 'O', 'N', 'S' + 0x80,
  'S', 'T', 'R', 'I', 'N', 'G', '$' + 0x80,
  'F', 'I', 'L', 'E' + 0x80,
  'G', 'R', 'I', 'D' + 0x80,
  0
};

enum {
  FUNC_PEEK = 0,
  FUNC_ABS,
  FUNC_RND,
  FUNC_SIN,
  FUNC_COS,
  FUNC_TAN,
  FUNC_LOG,
  FUNC_SGN,
  FUNC_SQR,
  FUNC_INT,
  FUNC_MIN,     //10
  FUNC_MAX,
  FUNC_EXP,
  FUNC_GET,
  FUNC_CHR,
  FUNC_INKEY,
  FUNC_FONT,
  FUNC_ASC,
  FUNC_TIMER,
  FUNC_ATAN,
  FUNC_LEN,       //20
  FUNC_INSTR,
  FUNC_COMPARE,
  FUNC_BIN,
  FUNC_HEX,
  FUNC_LEFT,
  FUNC_RIGHT,
  FUNC_MID,
  FUNC_TAB,
  FUNC_SPC,
  FUNC_STR,       //30
  FUNC_GDATE,
  FUNC_GTIME,
  FUNC_IIC,
  FUNC_BATT,
  FUNC_AREAD,
  FUNC_DREAD,
  FUNC_UCASE,
  FUNC_LCASE,
  FUNC_VAL,
  FUNC_MEM,      //40
  FUNC_NOT,
  FUNC_TEMP,
  FUNC_DHT,
  FUNC_GETCOL,
  FUNC_BMPREAD,  //ungetestet BMP180 Drucksensor
  FUNC_FN,
  FUNC_PORT,
  FUNC_PIN,
  FUNC_PI,
  FUNC_LN,       //50
  FUNC_DEEK,
  FUNC_FPEEK,
  FUNC_GPIX,
  FUNC_PIC,
  FUNC_MAP,
  FUNC_CONSTRAIN,
  FUNC_STRING,
  FUNC_FILE,
  FUNC_GRID,
  FUNC_UNKNOWN   //60
};

int FUNC_WORDS = FUNC_UNKNOWN;

//------------------------------- OPTION-Tabelle - alle Optionen, die dauerhaft gespeichert werden sollen -----------------------------------------
const static char options[] PROGMEM = {
  'S', 'D', 'C', 'A', 'R', 'D' + 0x80,      //Pin-Festlegung SD-Karte
  'F', 'R', 'A', 'M' + 0x80,                //CS-Pin FRAM
  'E', 'E', 'P', 'R', 'O', 'M' + 0x80,      //Adresse des I2C-EEproms
  'F', 'O', 'N', 'T' + 0x80,                //Font dauerhaft speichern
  'C', 'O', 'L' , 'O', 'R' + 0x80,          //Vordergrund- und Hintergrundfarbe dauerhaft speichern
  'K', 'E', 'Y' + 0x80,                     //Keyboardlayout 1=US,2=UK,3=GE,4=IT,5=ES,6=FR,7=BE,8=NO,9=JP
  'T', 'H', 'E', 'M', 'E' + 0x80,           //THEME
  'P', 'A', 'T', 'H' + 0x80,                //Arbeitsverzeichnis
  0
};

enum {
  OPT_SDCARD = 0,
  OPT_FRAM,
  OPT_EEP,
  OPT_FONT,
  OPT_COLOR,
  OPT_KEYBOARD,
  OPT_THEME,
  OPT_PATH,
  OPT_UNKNOWN  //8
};
//-------------------------------------------------------------------------------------------------------------------------------------------------


const static char to_tab[] PROGMEM = {
  'T', 'O' + 0x80,
  0
};

const static char step_tab[] PROGMEM = {
  'S', 'T', 'E', 'P' + 0x80,
  0
};

const static char relop_tab[] PROGMEM = {
  '>', '=' + 0x80,  //größer gleich
  '<', '>' + 0x80,  //ungleich
  '>', '>' + 0x80,  //SHIFT RIGHT
  '>' + 0x80,       //größer
  '=' + 0x80,       //gleich
  '<', '=' + 0x80,  //kleiner gleich
  '<', '<' + 0x80,  //SHIFT LEFT
  '|', '|' + 0x80,  //XOR
  '<' + 0x80,       //kleiner
  '%' + 0x80,       //MODULO
  '&' + 0x80,       //AND
  '|' + 0x80,       //OR
  '^' + 0x80,       //POW
  0
};

#define RELOP_GE		0
#define RELOP_NE		1
#define RELOP_SHR   2
#define RELOP_GT		3
#define RELOP_EQ		4
#define RELOP_LE		5
#define RELOP_SHL   6
#define RELOP_XOR   7
#define RELOP_LT		8
#define RELOP_MOD   9
#define RELOP_AND   10
#define RELOP_OR    11
#define RELOP_POW   12
#define RELOP_UNKNOWN	13

#define STACK_SIZE (sizeof(struct stack_for_frame)*26)   // 26 verschachtelte For-Next-Schleifen erlaubt (32bytes pro frame) 
#define VAR_SIZE sizeof(float)                           // Variablengrösse für float 4Bytes
static char *stack_limit;
static char *program_start;
static char *program_end;
static char *stack; // Software stack for things that should go on the CPU stack
static char *variables_begin;
static char *current_line;
static char *data_line;
static char *sp;
#define STACK_GOSUB_FLAG 'G'
#define STACK_FOR_FLAG 'F'
static char table_index;
static char keyword_index;
static LINENUM linenum;

//---------------------------------- Fehlermeldungen des Interpreters -----------------------------------------------------------------------------

static const char syntaxmsg[]        PROGMEM = "Syntax Error! ";                //1
static const char mathmsg[]          PROGMEM = "Math Error!";                   //2
static const char gosubmsg[]         PROGMEM = "Gosub Error!";                  //3
static const char fornextmsg[]       PROGMEM = "For-Next Error!";               //4
static const char memorymsg[]        PROGMEM = " bytes free";
static const char missing_then[]     PROGMEM = "Missing THEN Error!";           //5
static const char breakmsg[]         PROGMEM = "Break! in Line:";
static const char breaks[]           PROGMEM = "Break!";
static const char datamsg[]          PROGMEM = "Out of DATA Error!";            //6
static const char invalidmsg[]       PROGMEM = "Invalid comparison!";           //7
static const char sderrormsg[]       PROGMEM = "SD card error.";                //8
static const char sdfilemsg[]        PROGMEM = "SD file error.";                //9
static const char dirextmsg[]        PROGMEM = "(dir)";
static const char slashmsg[]         PROGMEM = "/";
static const char spacemsg[]         PROGMEM = " ";
static const char notexistmsg[]      PROGMEM = "File not exist!";               //10
static const char portmsg[]          PROGMEM = "Wrong Port-Number!";            //11
static const char valmsg[]           PROGMEM = "Invalid Value!";                //12
static const char dirmsg[]           PROGMEM = "Dir not empty!";                //13
static const char illegalmsg[]       PROGMEM = "Illegal quantity Error!";       //14
static const char zeroerror[]        PROGMEM = "Div/0-Error!";                  //15
static const char outofmemory[]      PROGMEM = "Out of Memory Error!";          //16
static const char mountmsg[]         PROGMEM = "SD-Card mounted";               //17
static const char notmount[]         PROGMEM = "SD-Card can't mount";           //18
static const char dimmsg[]           PROGMEM = "Array-Dimension Error!";        //19
static const char commsg[]           PROGMEM = "No COM-Port defined!";          //20
static const char comsetmsg[]        PROGMEM = "Wrong COM-Port Definition!";    //21
static const char bmpfilemsg[]       PROGMEM = "No BMP-File!";                  //22
static const char no_prg_msg[]       PROGMEM = "No Program in Memory !";        //23
static const char no_command_msg[]   PROGMEM = "Keyword not found !";           //24
static const char not_openmsg[]      PROGMEM = "File not open Error !";         //25
static const char dirnotfound[]      PROGMEM = "DIR not found !";               //26

//----------------------------------- Interpreter-Variablen ---------------------------------------------------------------------------------------
char *pstart;
char *newEnd;
char linelen;
bool then_marker = false;
bool else_marker = false;
float val;
int Zahlenformat = 0;
int logica = 0;                          //logikzähler für IF abfragen
unsigned int ongosub = 0;                //ON-Gosub Goto marker
//----------------------------------- Interpreter-Variablen ---------------------------------------------------------------------------------------
//#################################################################################################################################################
//############################################# Ende Deklarationsungsbereich ######################################################################
//#################################################################################################################################################

//--------------------------------------------- Ausgabe Fehlermeldungen ---------------------------------------------------------------------------
static char syntaxerror(const char *msg)
{
  printmsg(msg, 1);
  if (current_line != NULL)
  {
    char tmp = *txtpos;           //Position merken
    if (*txtpos != NL) *txtpos = '^';
    list_line = current_line;
    printline();
    *txtpos = tmp;                //gemerkte Position zurückschreiben
  }
  Beep(0, 0);                     //Error-BEEP
  current_line = 0;
  line_terminator();
}

float NoteToFreq(int mnote)
{
  int octave;
  octave = mnote / 12;
  mnote -= octave * 12;
  return noteTable[mnote] >> (8 - octave);
}

void Beep(int n, int len)
{
  if (n == 0) n = 60;
  if (len == 0) len = 100;
  if (n > 80) n = 80;
  if (n < 20) n = 20;
  n = NoteToFreq(n);
  //SND(waveform,frequency,duration,volume)

  Terminal.print("\e_S0;" + String(n, DEC) + ";" + String(len, DEC) + ";126$");
  delay(len);
}

//--------------------------------------------- Unterprogramm - Zeichen überspringen --------------------------------------------------------------
//--------------------------------------------- nächstes Zeichen zurückgeben ----------------------------------------------------------------------

static char skip_spaces(void) {
  if (*txtpos)
  {
    txtpos++;
    return spaces();
  }
}


//--------------------------------------------- Unterprogramm - Leerzeichen überspringen ----------------------------------------------------------
//--------------------------------------------- erstes gültiges Zeichen zurückgeben ---------------------------------------------------------------

static char spaces(void) {                      //Leerzeichen oder Tab überspringen
  char c;
  while (1) {
    c = *txtpos;
    if (c == SPACE || c == TAB) txtpos++;
    else break;
  }
  return (c);
}

//--------------------------------------------- Unterprogramm - Befehls-und Funktionstabellen lesen -----------------------------------------------

static void scantable(const char *table)
{
  int i = 0;
  table_index = 0;

  while (1)
  {
    // Keine Tabelleneinträge mehr?
    if (pgm_read_byte( table ) == 0)
      return;

    // Passt das Zeichen??
    if (txtpos[i] == pgm_read_byte( table ))
    {
      i++;
      table++;
    }
    else
    {
      //Wort komplett, dann zurück
      if (txtpos[i] + 0x80 == pgm_read_byte( table ))
      {
        txtpos += i + 1;
        spaces();
        return;
      }

      while ((pgm_read_byte( table ) & 0x80) == 0)
        table++;

      // Fortfahren mit dem ersten Zeichen des nächsten Wortes, Positionsindex zurücksetzen
      table++;
      table_index++;
      spaces();
      i = 0;
    }
  }
}


//--------------------------------------------- Unterprogramm - Zahlenausgabe ---------------------------------------------------------------------

void printnum(float num, int modes)   //Ausgabe als float
{ char c[32];
  float tmp;
  int stellen;
  String stz;
  int len;
  unsigned long hexnum;
  String cbuf;

  switch (modes)
  {
    case 0:                                       //normale Zahlenausgabe
      if (num > 9999999 || num < -9999999) {
        printmsg(sci(num, Prezision), 0);         //Ausgabe in Exponential-Schreibweise
      }

      else {
        tmp = num - int(num);                     //Nachkommastelle vorhanden oder 0 ?
        if (tmp == 0) {                           //keine Nachkommastelle
          itoa(num, c, 10);
          printmsg(c, 0);                         //dann Integerausgabe

        }
        else {                                    //Nullen in der Nachkommastelle sollen abgeschnitten werden
          dtostrf(num, 1, Prezision, c);          //Nullen abschneiden
          stz = c;
          len = stz.length();
          stellen = Prezision;
          for (int i = len - 1; i > 0; i--) {
            if ((c[i] > '0') || (c[i] == '.')) break; //keine Null oder komma?
            if (c[i] == '0') stellen -= 1;
          }
          dtostrf(num, 1, stellen, c);                //formatierte Ausgabe ohne Nullen
          printmsg(c, 0);
        }
      }
      break;

    case 1:                                           //Ausgabe als Binärzahl
      hexnum = num;
      outchar('%');
      itoa(num, c, 2);
      printmsg(c, 0);
      Zahlenformat = 0;
      break;

    case 2:                                           //Ausgabe als Hexadezimalzahl
      outchar('#');
    case 3:                                           //Ausgabe als Hexadezimalzahl für Memory_dump ohne #
      hexnum = num;
      itoa(num, c, 16);
      printmsg(c, 0);
      Zahlenformat = 0;
      break;

    default:
      break;
  }//switch(modes)
}

//#######################################################################################################################################
//--------------------------------------------- DUMP - Befehl ---------------------------------------------------------------------------
//#######################################################################################################################################

static int Memory_Dump() {                       //DMP Speichertyp 0..2 <,Adresse>
  int ex = 0, c, was, tpm;
  int ln = (VGAController.getScreenHeight() / y_char[fontsatz]) - 3; //Anzahl Zeilen abhängig vom Fontsatz
  //if (Frame_nr) win_set_cursor(0);               //sind Fenster gesetzt?, dann Hauptfenster setzen
  int x_weite = VGAController.getScreenWidth() / x_char[fontsatz];

#ifdef ILI9341                                  //bei TFT x und y vertauscht
x_weite = (VGAController.getScreenHeight() / x_char[fontsatz]) ;
ln = VGAController.getScreenWidth() / y_char[fontsatz] - 3;    //Anzahl Zeilen abhängig vom Fontsatz
#endif

  byte rdbyte[8];

  word of = FRAM_OFFSET;
  long n;
  long adr = 0;
  was = abs(int(get_value()));                  //nur ganze Zahlen
  if (*txtpos == ',')
  {
    txtpos++;
    adr = abs(get_value());                      //nur ganze Zahlen
  }

  if (*txtpos == 'V') {
    of = 0;                                       //wird V angegeben, kann man den Variablenbereich ansehen
    txtpos++;
  }


  if (was > 2) {
    syntaxerror(syntaxmsg);
    return 1;
  }
  //spi_fram.begin(3);
  while (!ex) {
    for (int i = 1; i < ln; i++)
    {
      if (adr < 0x1000)outchar('0');
      if (adr < 0x100)outchar('0');
      if (adr < 0x10)outchar('0');
      n = adr;
      printnum(adr, 3);                                 //Hexausgabe
      outchar(' ');

      for (int f = 0; f < 8; f++) {                     //8 Speicherplätze lesen und anzeigen
        switch (was) {
          case 1:  //FRAM
            if (spi_fram.read8(of + n) < 16) outchar('0');
            printnum(spi_fram.read8(of + n++), 3);
            if (x_weite > 39) outchar(' ');                       //wenn genug Platz, dann Leerzeichen zwischen den Werten
            break;
          case 2:  //EEPROM
            if (readEEPROM(EEprom_ADDR, n ) < 16) outchar('0');
            printnum(readEEPROM(EEprom_ADDR, n++ ), 3);
            if (x_weite > 39) outchar(' ');
            break;
          default:  //interner RAM
            if (program[int(n)] < 16) outchar('0');
            printnum(program[int(n++)], 3);
            if (x_weite > 39) outchar(' ');
            break;
        }
      }
      if (x_weite < 40) outchar(' ');                             //wenn nicht genug Platz, dann nur ein Leerzeichen nach 8Bytes


      for (int i = 0; i < 8; i++) {
        switch (was) {
          case 1:  //FRAM
            c = spi_fram.read8(of + adr++);//readEEPROM(FRam_ADDR, adr++);
            break;
          case 2:  //EEPROM
            c = readEEPROM(EEprom_ADDR, adr++);
            break;
          default:  //interner RAM
            c = program[int(adr++)];
            break;
        }

        if (c > 31 && c < 127)
          outchar(c);
        else
          outchar('.');
      }
      line_terminator();
    }
    if (wait_key(true) == 3) ex = 1;    //Ctrl-C oder ESC Abbruch

  }//while (ex)
  return 0;
}

//--------------------------------------------- Unterprogramm teste auf gültige Zeilennummer ------------------------------------------------------

static unsigned short testnum(void)               // Überprüfung auf Zeilennummer -> Zeilennummern > 65535 erzeugen einen Überlauf (es fängt wieder bei 1 an)
{
  unsigned int num = 0;
  spaces();

  while (*txtpos >= '0' && *txtpos <= '9' )
  {
    num = num * 10 + *txtpos - '0';
    txtpos++;
  }
  return	num;
}

//--------------------------------------------- Unterprogramm - Fehler/Nachrichtausgabe -----------------------------------------------------------

void printmsg(const char *msg, int nl)
{
  //fbcolor(Vordergrund, Hintergrund);                                //Themen-Farben setzen ist nötig, sonst fällt der Interpreter in die Terminalfarben zurück
  //if (Frame_nr) fbcolor(Frame_vcol[Frame_nr], Frame_hcol[Frame_nr]); //Wenn Fenster, dann Fensterfarben
  while ( pgm_read_byte( msg ) != 0 )
  {
    outchar( pgm_read_byte( msg++ ) );
  }

  if (nl == 1) line_terminator();
}

//--------------------------------------------- Unterprogramm - Tastenabfrage (list-Ausgaben) -----------------------------------------------------

static unsigned short wait_key(bool modes) {
  char c;
  if (modes)
  {
    line_terminator();//Terminal.println();
    printmsg("SPACE<Continue>/CTR+C or ESC<Exit>", 1);
  }
  while (1) {
    if (Terminal.available())
    {
      c = Terminal.read();
      break;
    }
    if (break_marker == true)
    {
      c = 3;
      break_marker == false;
      break;
    }
  }
  return c;
}

//--------------------------------------------- Unterprogramm - Zeile eingeben --------------------------------------------------------------------

static void getln(char prompt)
{
  outchar('O');
  outchar('K');
  outchar(prompt);
  txtpos = program_end + sizeof(LINENUM);

  while (1)
  {

    char c = inchar();
    if (c == 27 && Frame_nr) continue;


    switch (c)
    {
      case NL:

      case CR:
        line_terminator();
        // Terminate all strings with a NL
        txtpos[0] = NL;
        return;

      case 0x7F:
        if (txtpos == program_end)
          break;
        txtpos--;

        if (Frame_nr) {                   //im Fenster kein Backspace, um das Fenster nicht zu beschädigen
          Cursor_x = tc.getCursorCol();
          Cursor_y = tc.getCursorRow();
          tc.setCursorPos(tc.getCursorCol() - 1, Cursor_y);
          tc.setChar(' ');
          tc.setCursorPos(tc.getCursorCol() - 1, Cursor_y);
        }
        else
          Terminal.write("\b\e[K");      //nicht im Fenster, dann Backspace
        break;
      case 0x03:       // ctrl+c
        line_terminator();
        printmsg(breaks, 1);
        current_line = 0;
        sp = program + sizeof(program);
        txtpos[0] = NL;
        return;
        break;

      default:
        // Wir müssen mindestens ein Leerzeichen lassen, damit wir die Zeile in die richtige Reihenfolge bringen können
        if (txtpos == variables_begin - 2)
          outchar(CTRLH);
        else
        {
          txtpos[0] = c;
          txtpos++;
          outchar(c);

        }
    }
  }

}


//--------------------------------------------- Unterprogramm - Programmzeile finden ---------------------------------------------------------------
static char *findline(void)
{
  char *line = program_start;
  while (1)
  {
    if (line == program_end)
      return line;

    if (((LINENUM *)line)[0] >= linenum)
      return line;

    // Add the line length onto the current address, to get to the next line;
    line += line[sizeof(LINENUM)];
  }
}


//--------------------------------------------- Unterprogramm - Data-zeile finden -----------------------------------------------------------------

static char *find_data_line(void)
{
  char *line = program_start;
  while (1)
  {
    if (line == program_end)
      return 0;

    dataline = line + sizeof(LINENUM) + sizeof(char);         //Programmzeile übergeben

    if (dataline[0] == 'D' && dataline[1] == 'A' && dataline[2] == 'T' && dataline[3] == 'A')
    {
      data_numbers[num_of_datalines++] = *((LINENUM *)line);
    }
    // Add the line length onto the current address, to get to the next line;
    line += line[sizeof(LINENUM)];
  }
  num_of_datalines -= 1;
  return 0;
}

//--------------------------------------------- Unterprogramm - Grossbuchstabenumwandlung ---------------------------------------------------------

static void toUppercaseBuffer(void)
{
  char *c = program_end + sizeof(LINENUM);
  char quote = 0;

  while (*c != NL)
  {
    // Are we in a quoted string?
    if (*c == quote)
      quote = 0;
    else if (*c == '"' || *c == '\'')
      quote = *c;
    else if (quote == 0 && *c >= 'a' && *c <= 'z')
      *c = *c + 'A' - 'a';
    c++;
  }
}

//--------------------------------------------- Unterprogramm - Zeile ausgeben (Bildschirm oder Datei) --------------------------------------------

int printline(void)
{ int digits = 0;
  int num;
  LINENUM line_num;

  line_num = *((LINENUM *)(list_line));
  list_line += sizeof(LINENUM) + sizeof(char);

  num = line_num;

  printnum(num, 0);

  outchar(' ');
  while (*list_line != NL)
  {
    outchar(*list_line);
    list_line++;
  }

  list_line++;
  line_terminator();
  return line_num;
}

//--------------------------------------------- Unterprogramm - RTC auslesen ----------------------------------------------------------------------

void getdatetime()
{
  DateTime now = rtc.now();           //RTC Funktion
  Zeit[0] = now.hour();
  Zeit[1] = now.minute();
  Zeit[2] = now.second();
  Datum[0] = now.day();
  Datum[1] = now.month();
  Datum[2] = now.year();
  Datum[3] = now.dayOfTheWeek();
  e_rtc.setTime(Zeit[2], Zeit[1], Zeit[0], Datum[0], Datum[1], Datum[2], 0);
}

//--------------------------------------------- Unterprogramm - Hexadezimalzahl in Dezimalzahl konvertieren ---------------------------------------

static int hexDigit(char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return -1;
}


/*************************************************************************************************************************************************/
//--------------------------------------------- Start - Werteausgabe ------------------------------------------------------------------------------
//--------------------------------------------- Zahlen,Variablen oder Strings auslesen ------------------------------------------------------------
//--------------------------------------------- logische Abhängigkeiten auswerten -----------------------------------------------------------------
//--------------------------------------------- mathematische Funtionen ausführen -----------------------------------------------------------------
/*************************************************************************************************************************************************/


static float expr4(void)
{
  float a = 0;
  float b = 0;
  float c = 0;
  float d = 0;
  float map_var[4] = {0, 0, 0, 0};
  int fnv;
  uint8_t puf[3];
  unsigned long t = 0;
  unsigned long f = 0;
  char g = 0;
  int pointmarker = 0;
  int minusmarker = 0;
  int octal[10];
  int i;
  int asc_quoted = 0;
  bool quota = false;
  String cbuf, dbuf;
  char *st;
  byte buf[4];

  spaces();

  if ( *txtpos == '-' ) {
    txtpos++;
    return -expr4();
  }
  else if (*txtpos == '(') {
    txtpos++;
    a = get_value();
    if (Test_char(')')) goto expr4_error;
    return a;
  }
  //******************************************** Zahleneingabe mit Exponentialschreibweise ************************************

  else if ((*txtpos >= '0' && *txtpos <= '9') || (*txtpos == '.') || ((*txtpos == 'E') && (txtpos[1] == '+' || txtpos[1] == '-')))
  {
    float a = 0;
    char an[] = {"                  "};
    int i = 0;
    int m = 0;

    do 	{
      if (*txtpos == '.' && pointmarker == 0) pointmarker++; //Überprüfung auf mehr als einem Punk
      //###################### Exponentialschreibweise ######################### 1E-6 bis 1E+38
      if ((*txtpos == 'E') && (txtpos[1] == '-' || txtpos[1] == '+')) {
        an[i] = *txtpos;
        txtpos++;
        i++;
        an[i] = *txtpos;
        txtpos++,
               i++;
      }
      an[i] = *txtpos;
      txtpos++;
      i++;
    }
    while ((*txtpos >= '0' && *txtpos <= '9') || (*txtpos == '.') || ((*txtpos == 'E') && (txtpos[1] == '+' || txtpos[1] == '-')));

    cbuf = an;
    a = cbuf.toFloat();

    if (pointmarker > 1) goto expr4_error;
    return a;
  }

  //----------------------------------------- Hexadezimalzahlen -------------------------------------------------------------------------------------
  else if (*txtpos == '#') {
    txtpos++;
    g = *txtpos;
    f = 0;
    f = hexDigit(g);
    if (f < 0)
      goto expr4_error;
    t = f;
    txtpos++;
    g = *txtpos;

    f = 0;
    while (f >= 0)          //alle Stellen lesen
    {
      f = hexDigit(g);
      t = t << 4 | f;
      txtpos++;
      g = *txtpos;
      switch (g)
      { // Überprüfung auf gültige Zeichen
        case 'a' ... 'f':
          break;
        case 'A' ... 'F':
          break;
        case '0' ... '9':
          break;
        default:            //ungültiges Zeichen -> Ausstieg
          return t;
      }
    }
    return t;
  }

  //----------------------------------------- Binärzahlen -------------------------------------------------------------------------------------------
  else if (*txtpos == '%') {
    txtpos++;
    g = *txtpos;
    f = 0;
    t = 0;
    if (g < '0' || g > '1')
      goto expr4_error;
    t = g - '0';
    txtpos++;
    g = *txtpos;
    while ( g == '0' || g == '1')
    {
      t = t << 1 | ( g - '0' );
      txtpos++;
      g = *txtpos;
      if ( g == NL || g == ':') break;
    }
    return t;
  }

  //***************************************************** ein- oder zweibuchstabige variablen ***************************************************
  // Is it a function or variable reference?
  //--------------------------------------------------------------------------------------------------------------------------------------------------------------
  scantable(func_tab);                                                  //Funktionstabelle lesen

  if (table_index == FUNC_UNKNOWN)                                      //am ende angekommen, Funktion nicht gefunden
  {
    //------------------------------------------------------------------------------------------------------------------------------------------------------------

    if (txtpos[0] >= 'A' && txtpos[0] <= 'Z') {

      float a, tmp, tmo, tms;
      int stmp, i, len, v_name;
      char c;
      word v_adr;
      byte var_art = 0;

      tmp = (*txtpos - 'A');
      v_name = tmp;                                                     //Variablenname für Array sichern
      stmp = (int)(tmp * STR_LEN);
      a = ((float *)variables_begin)[*txtpos - 'A'];
      fnv = tmp;                                                        //Zwischenspeicher für FN
      txtpos++;
      if (*txtpos >= 'A' && *txtpos <= 'Z' )                            //zweiter Variablenbuchstabe
      {
        tmo = ((*txtpos - 'A' + 1) * 26);
        a = ((float *)variables_begin)[int(tmp + tmo)];
        txtpos++;
      }

      while (*txtpos >= 'A' && *txtpos <= 'Z') txtpos++;                //lange Variablennamen

      //----------------------------------------- Stringvariablen -------------------------------------------------------------------------------------
      if (*txtpos == '$')                                               //Stringvariable
      { txtpos++;

        if (*txtpos == '(') {                                           //Stringarray?
          txtpos++;
          var_art = 2;
          expression_error = 0;
          v_adr = rw_array(v_name, STR_TBL);

          if (expression_error) goto expr4_error;
        }

        i = 0;
        while (1)
        {
          if (var_art == 2) {                                           //String-Array?
            c = spi_fram.read8(v_adr + i);//readEEPROM(FRam_ADDR, v_adr + i );
          }
          else {                                                        //normaler String
            c = Stringtable[stmp + i];
          }

          if (i < STR_LEN)
          {
            if (c == 0)                                                 //Stringende-Kennung?
            {
              tempstring[i] = '\0';                                     //String abschließen
              break;
            }

            if (var_art == 2) {                                         //String-Array?
              tempstring[i] = spi_fram.read8(v_adr + i);//readEEPROM(FRam_ADDR, v_adr + i );
            }
            else {
              tempstring[i] = Stringtable[stmp + i];                    //normaler String
            }

            i++;
          }
          else
          {
            tempstring[i] = '\0';                                       //Stringlänge erreicht?,dann String abschließen
            break;
          }
        }

        string_marker = true;
        return a;
      }
      else if (*txtpos == '(') {                                        //Numerisches Array?
        txtpos++;
        var_art = 1;                                                    //numerisches Array
        expression_error = 0;
        v_adr = rw_array(v_name, VAR_TBL);
        if (expression_error) goto expr4_error;
        spi_fram.read(v_adr, buf, 4);

        a = *((float *)buf);                                            //byte-array des Wertes nach float konvertieren

        string_marker = false;                                          //kein String dann string_marker immer zurücksetzen
        chr = false;                                                    //Char Marker ebenfalls zurücksetzen

        return a;
      }

      else {
        string_marker = false;                                          //falls kein String dann string_marker immer zurücksetzen
        chr = false;                                                    //Char Marker ebenfalls zurücksetzen
      }
      return a;
    }
  }
  //--------------------------------------------------------------------------------------------------------------------------------------------------------------

  else
  {
    //------------------------------------------------------------------------------------------------------------------------------------------------------------
    char fu = table_index, iic, cc;
    int charb, fname;                                         //Hilfsvariable für FN
    unsigned long result;                                     //Hilfsvariable für Peek

    //-------------------------------------------------------- einfache Funktionen ohne Klammer ------------------------------------------------------------------
    switch (fu)                                               // Rückgabe einfache Werte (ohne Klammer)
    {
      case FUNC_INKEY:
        a = 0;
        a = Terminal.read(5);
        if (a > 122 || a == 63 || a == 64) return 0;
        return a;                                             // INKEY - Taste abfragen
        break;

      case FUNC_FILE:
        if (Test_char('_')) goto expr4_error;                 //FILE_PS ->Pos
        if (*txtpos == 'P') {
          txtpos++;
          if (Test_char('S')) goto expr4_error;
          return File_pos;
        }
        else if (*txtpos == 'S') {                            //FILE_SZ ->Size
          txtpos++;
          if (Test_char('Z')) goto expr4_error;
          return File_size;
        }
        break;

      case FUNC_NOT:                                          //NOT-Funktion
        a = get_value();
        return int(!a);
        break;

      case FUNC_FONT:
        return fontsatz;                                      // aktueller Fontsatz
        break;

      case FUNC_TIMER:
        return millis();
        break;


      case FUNC_MEM:                                          //Abfrage freier Speicher MEM
        return int(variables_begin - program_end);
        break;

      case FUNC_FN:                                           //FN-Funktion
        if (*txtpos < 'A' || *txtpos > 'Z')
          goto expr4_error;
        charb = *txtpos - 'A';                                //Funktionsname
        fname = charb;
        charb = charb * 5;                                    //Position im Funktionsspeicher berechnen -> Buchstabe * 5 Plätze
        txtpos++;
        break;

      default:
        break;
    }
    //------------------------------------------------ komplexe Funktionen mit Klammer ----------------------------------------------------------------
    if (Test_char('(')) goto expr4_error;       //Klammer auf

    if (*txtpos == '"') {
      a = String_quoted_read();                 // erstes Zeichen in a sichern für ASC
      asc_quoted = 1;                           // ASC Zeichenkette in Anführungszeichen

      cbuf = String(tempstring);                //erste Zeichenkette lesen und nach cbuf kopieren
      string_marker = true;
      quota = true;
    }
    else if ( fu == FUNC_PI )                    //PI benötigt nur die Klammern PI()
      a = M_PI;


    else if (fu == FUNC_IIC) {                   //*********** I2C-Befehle ***********
      iic = *txtpos;
      txtpos++;
      switch (iic) {
        case 'R':
          a = int(myI2C.read());                 //I2C-Read
          break;
        case 'E':
          a = int(myI2C.endTransmission());      //I2C-End
          break;
        case 'A':
          a = int(myI2C.available());            //I2C-Available
          break;
      }
    }

    else if ( fu == FUNC_PORT)
    {
      iic = *txtpos;                             //A oder B MCP23017
      if (iic == 'A') {
        a = mcp.readGPIOA();                     //Port(A) liest Port A
        txtpos++;
      }
      else if (iic == 'B') {
        a = mcp.readGPIOB();                     //PORT(B) liest Port B
        txtpos++;
      }
      else
        a = mcp.readGPIOAB();                   //Port() liest Port A+B
    }

    else  {
      a = get_value();                         //1.Zahl (bei Stringvariablen steht in tempstring die 1.Zeichenkette)
    }

    switch (fu)                                 //Rückgabe komplexer Werte (mit Klammer und 1-4 Operatoren)
    {
      case  FUNC_PEEK:                          //Peek Byte
      case  FUNC_DEEK:                          //Deek Word
      case  FUNC_FPEEK:                         //FPEEK float
      case  FUNC_GPIX:                          //GPIX(x,y)
      case  FUNC_MIN:
      case  FUNC_MAX:
        if (Test_char(',')) goto expr4_error;
        b = get_value();                       //2.Zahl
        break;

      case FUNC_SQR:                            //2.Zahl für n'te Wurzel ist optional
        b = 0;
        if (*txtpos == ',')
        {
          *txtpos++;
          b = get_value();
        }
        break;


      case FUNC_FN:
        ((float *)variables_begin)[Fnoperator[charb]] = a;
        Fnvar = 0;
        b = Fnoperator[charb + 4];
        while (*txtpos == ',') {
          txtpos++;
          Fnvar += 1;
          if (Fnvar == b) {                                      //mehr Parameter als mit DEFN dimensioniert?
            syntaxerror(illegalmsg);
            goto expr4_error;
          }
          ((float *)variables_begin)[Fnoperator[charb + Fnvar]] = get_value();
        }
        break;

      case FUNC_LEFT:
        if (Test_char(',')) goto expr4_error;
        b = get_value();                           //2.Parameter eine Zahl
        tempstring[int(b)] = 0;
        string_marker = true;
        break;

      case FUNC_RIGHT:
        if (Test_char(',')) goto expr4_error;
        b = get_value();                           //2.Parameter eine Zahl
        cbuf = String(tempstring);
        dbuf = cbuf.substring(cbuf.length() - b, cbuf.length());
        dbuf.toCharArray(tempstring, dbuf.length() + 1);
        string_marker = true;
        break;

      case FUNC_STR:                                //str$(12.34,n) ->Umwandlung Zahl nach String - n=Nachkommastellen
        if (Test_char(',')) goto expr4_error;
        b = get_value();                           //2.Parameter eine Zahl
        break;

      case FUNC_MID:
        if (Test_char(',')) goto expr4_error;
        b = get_value();                           //2.Parameter eine Zahl
        if (Test_char(',')) goto expr4_error;
        c = get_value();
        cbuf = String(tempstring);
        dbuf = cbuf.substring(b - 1, b - 1 + c);
        dbuf.toCharArray(tempstring, dbuf.length() + 1);
        string_marker = true;
        break;

      case FUNC_STRING:
        if (Test_char(',')) goto expr4_error;
        b = get_value();                           //Zeichenkette in tempstring
        string_marker = true;
        func_string_marker = true;
        fstring = a;
        break;

      case FUNC_TEMP:
        if (Test_char(',')) goto expr4_error;
        b = get_value();                           //2.Parameter Temperaturkanal
        break;

      case FUNC_DHT:                                // DHT-Sensor DHT(Port,Typ,temp/humi)
        if (Test_char(',')) goto expr4_error;
        b = get_value();                           //2.Parameter Typ
        if (Test_char(',')) goto expr4_error;
        c = get_value();                           //3.Parameter Messwert 0=Temp, 1=Humi
        break;


      case FUNC_AREAD:                              //Analog Read IO-Pin 2,26,34,35,36,39 - ein zusätzlicher Parameter bedeutet Ausgabe in Volt
        if (*txtpos == ',') {
          txtpos++;
          b = get_value();                         //2.Parameter ->Ausgabe als Spannungswert in entsprechender physikalischer Grösse - Volt (bei SR04 in cm)
        }
        break;

      case FUNC_COMPARE:                            //COMP$(a$,b$)(0=beide Strings gleich, 1=a$>b$, -1=a$<b$)
      case FUNC_INSTR:                              //INSTR(Suchstring,Zeichenkette)
        if (Test_char(',')) goto expr4_error;
        quota = false;
        if (!quota) cbuf = String(tempstring);     //ersten String sichern
        if (*txtpos == '"')                         //Zeichenkette in Anführungszeichen?
        {
          String_quoted_read();                     //zweite Zeichenkette lesen und nach dbuf kopieren
          dbuf = String(tempstring);
        }
        else
        {
          b = get_value();                         //2.String
          dbuf = String(tempstring);
        }
        break;

      case FUNC_MAP:                                //x=map(value,fromLow, fromHigh, toLow, toHigh)
        if (Test_char(',')) goto expr4_error;
        map_var[0] = get_value();
        if (Test_char(',')) goto expr4_error;
        map_var[1] = get_value();
        if (Test_char(',')) goto expr4_error;
        map_var[2] = get_value();
        if (Test_char(',')) goto expr4_error;
        map_var[3] = get_value();
        break;

      case FUNC_CONSTRAIN:
        if (Test_char(',')) goto expr4_error;
        b = get_value();
        if (Test_char(',')) goto expr4_error;
        c = get_value();
        break;

      default:
        break;

    }
    if (*txtpos != ')')                             //Klammer zu
      goto expr4_error;

    txtpos++;

    switch (fu)
    {
      case FUNC_PI:                                 // Ausgabe PI
        return a;
        break;

      case FUNC_BATT:                               // Akku abfragen
        b = 3.3 / 4095 * analogRead(Batt_Pin);
        b = b / 0.753865;                           //(Umess/(R2/(R1+R2)) R1=3.327kohm R2=10.19kohm
        c = 100 - ((4.2 - b) * 100);                //Akkuwert in Prozent
        if (c > 100) c = 100;
        if (a == 0) return b;                       //Spannungswert zurückgeben
        else return int(c);                         //Ladung in Prozent
        break;

      case FUNC_PEEK:
        if (a == 0)
          result = program[int(b)];                      //RAM
        else if (a == 1)
          result = spi_fram.read8(FRAM_OFFSET + b);      //FRAM
        else
          result = readEEPROM(EEprom_ADDR, b );          //EEPROM
        return result;
        break;

      case FUNC_DEEK:
        if (a == 0) {
          result = (program[int(b)]) << 8;               //RAM
          result = result + program[int(b + 1)];
        }
        else if (a == 1)
        { result = spi_fram.read8(FRAM_OFFSET + b) << 8;        //FRAM
          result = result + spi_fram.read8(FRAM_OFFSET + b + 1);; //FRAM
        }
        else
        {
          result = readEEPROM(EEprom_ADDR, b ) << 8;     //EEPROM
          result = result + readEEPROM(EEprom_ADDR, b + 1); //EEPROM
        }
        return result;
        break;

      case FUNC_FPEEK:
        if (a == 0) {                                     //RAM float
          buf[0] = program[int(b)];
          buf[1] = program[int(b) + 1];
          buf[2] = program[int(b) + 2];
          buf[3] = program[int(b) + 3];
        }
        else if (a == 1) {
          spi_fram.read(FRAM_OFFSET + b, buf, 4);         //FRAM float
        }
        else {//if (a == 2) {                                //EEPROM float
          readBuffer(EEprom_ADDR, b, 4, buf);
        }
        return *(float*)buf;
        break;


      case FUNC_GPIX:
        return Test_pixel(a, b, 1);
        break;

      case FUNC_GET:
        if (a > 0) return int(tc.getCursorRow());        //get(0)=x
        else    return int(tc.getCursorCol());           //get(1)=y
        break;

      case FUNC_VAL:                                     //VAL("numerische Zeichenkette")
        //dbuf = String(tempstring);
        a = cbuf.toFloat();
        string_marker = false;
        return a;
        break;

      case FUNC_ABS:                                    //ABS(x)
        if (a < 0)
          return -a;
        return a;
        break;

      case FUNC_CHR:                                    //CHR$(x)
        chr = true;                                     //merker für chr setzen (für Print-Befehl)
        tempstring[0] = int(a);                         //Char-Zeichen übergeben
        tempstring[1] = 0;
        string_marker = true;                           //String-marker setzen
        return int(a);
        break;

      case FUNC_SIN:                                    //SIN(x)
        return sin(a);
        break;

      case FUNC_COS:                                    //COS(x)
        return cos(a);
        break;

      case FUNC_TAN:                                    //TAN(x)
        return tan(a);
        break;

      case FUNC_ATAN:                                   //ATN(x)
        return atan(a);
        break;

      case FUNC_LOG:                                    //LOG(x) Logarithmus zur Basis 10 (X>0)
        if (a < 0)
        {
          printmsg(mathmsg, 1);
          return (a);
        }
        return log10(a);
        break;

      case FUNC_LN:                                     //LN(x) natürlicher Logarithmus (X>0)
        if (a < 0)
        {
          printmsg(mathmsg, 1);
          return (a);
        }
        return log(a);
        break;

      case FUNC_LEN:                                    //LEN(a$) -> Rückgabe Stringlänge
        cbuf = String(tempstring);
        a = cbuf.length();
        string_marker = false;                          //String-Marker zurücksetzen, für korrekte Printausgabe/Werteübergabe
        return a;
        break;

      case FUNC_UCASE:
        dbuf = String(tempstring);
        dbuf.toUpperCase();                               //String in Grossbuchstaben umwandeln
        dbuf.toCharArray(tempstring, dbuf.length() + 1);  //und nach tempstring zurückschreiben
        return a;
        break;

      case FUNC_LCASE:
        dbuf = String(tempstring);
        dbuf.toLowerCase();                              //String in Kleinbuchstaben umwandeln
        dbuf.toCharArray(tempstring, dbuf.length() + 1); //und nach tempstring zurückschreiben
        return a;
        break;

      case FUNC_SGN:                  //SGN(x)
        if (a < 0) return -1.0;
        else if (a == 0) return 0.0;
        else if (a > 0) return 1.0;
        break;

      case FUNC_SQR:                  //SQR(x)
        if (a < 0) {
          printmsg(mathmsg, 1);
          return (a);
        }
        if (b == 0) return sqrt(a);   //Quadratwurzel aus a
        else return pow(a, 1 / b);    //N'te Wurzel aus a
        break;

      case FUNC_MIN:                  //MIN(x,y)
        return min(a, b);
        break;

      case FUNC_MAX:                  //MAX(x,y)
        return max(a, b);
        break;

      case FUNC_EXP:                  //EXP(x)
        return exp(a);
        break;

      case FUNC_INT:                  //INT()
        return int(a);
        break;

      case FUNC_RND:                  //Random-Funktion RND(x)
        return ( random( a + 1 ));
        break;

      case FUNC_ASC:
        string_marker = false;
        if (asc_quoted == 0) a = int(tempstring[0]); //ASC(a$) oder asc("Zeichenkette")
        asc_quoted = 0;
        return a;                     // ASC(x) (ASCII-Code des ersten Zeichens der Zeichenette x oder der Stringvariablen x$
        break;

      case FUNC_INSTR:                // INSTR(Suchstring,zeichenkette2)
        a = dbuf.indexOf(cbuf);
        string_marker = false;        //String-Marker zurücksetzen, für korrekte Printausgabe/Werteübergabe
        return a + 1;
        break;

      case FUNC_COMPARE:              // COMP(zeichenkette1,zeichenkette2)
        a = cbuf.compareTo(dbuf);
        string_marker = false;        //String-Marker zurücksetzen, für korrekte Printausgabe/Werteübergabe
        //return abs(a);
        if (a < 0) return -1;         //zeichenkette a kleiner b
        if (a > 1) return 1;          //zeichenkette a grösser b
        return a;                     //zeichenkette a gleich  b
        break;

      case FUNC_BIN:                  //Ausgabe als Binärwert
        Zahlenformat = 1;
        return a;
        break;

      case FUNC_HEX:                  //Ausgabe als Hexwert
        Zahlenformat = 2;
        return a;
        break;

      case FUNC_LEFT:                 // LEFT$(String,Anzahl)
        return a;                     //Rückgabe Dummywert
        break;

      case FUNC_RIGHT:                // RIGHT$(String,Anzahl)
        return a;                     //Rückgabe Dummywert
        break;

      case FUNC_MID:                  // MID$(String,Start,Anzahl)
        return a;                     //Rückgabe Dummywert
        break;

      case FUNC_STRING:               //STRINGS$(n,"string")
        return a;                     //Rückgabe Dummywert
        break;

      case FUNC_TAB:                  //TAB-Funktion
        b = tc.getCursorRow();
        tc.setCursorPos(a, b);
        tab_marker = true;
        return a;
        break;

      case FUNC_AREAD:                     //Analog Read IO-Pin 2,12,26,27,34,35,36,39
        if ((a == 2) || (a == 12) || (a == 26)  || (a == 27)  || (a == 34) || (a == 35) || (a == 36) || (a == 39)) //nur diese Pins sind erlaubt
        {
          if ((a == 2) || (a == 12)  || (a == 26) || (a == 26)) pinMode(a, INPUT_PULLUP); //Pin2,12,26,27 als Eingang falls vorher als Ausgang
          switch (int(b)) {
            case 1:
              return 3.3 / 4096 * analogRead(a);              //Ausgabe als Spannungs-Wert
              break;
            case 2:
              return HCSR04(a);                               //Ausgabe Ultraschallsensor HC-SR04 Entfernung in cm
              break;
            default:
              return analogRead(a);                           //Ausgabe als Digitalwert
              break;
          }
        }
        else
        {
          printmsg(portmsg, 1);
        }
        break;

      case FUNC_DREAD:                     //Digital Read IO-Pin 2,12,26,27,34,35,36
        if ((a == 2) || (a == 12) || (a == 26) || (a == 27) || (a == 34) || (a == 35) || (a == 36)) //nur diese Pins sind erlaubt
        {
          if ((a == 2) || (a == 12) || (a == 26) || (a == 27)) pinMode(a, INPUT_PULLUP); //Pin2,26 als Eingang falls vorher als Ausgang
          return digitalRead(a);
        }
        else
        {
          printmsg(portmsg, 1);
        }
        break;


      case FUNC_SPC:
        for (int i = 0; i < a; i++) {
          outchar(' ');
        }
        tab_marker = true;
        return a;
        break;

      case FUNC_STR:
        cbuf = String(a, b);
        cbuf.trim();
        cbuf.toCharArray(tempstring, cbuf.length() + 1);
        string_marker = true;
        return a;
        break;

      case FUNC_GDATE:
        getdatetime();
        return Datum[int(a)];
        break;

      case FUNC_GTIME:
        getdatetime();
        return Zeit[int(a)];
        break;

      case FUNC_IIC:
        switch (iic)
        {
          case 'R':
            return a;
            break;
          case 'A':
            return a;
            break;
          case 'E':
            return a;
            break;
        }
        break;


      case FUNC_TEMP:
        if ((a == 2) || (a == 26))        //nur diese Pins sind erlaubt
          return init_temp(a, b);         //temp(port,kanal)->Ausgabe Temperatur Dallas DS18S20
        else
        {
          printmsg(portmsg, 1);
        }
        break;

      case FUNC_DHT:
        if ((a == 2) || (a == 26))        //nur diese Pins sind erlaubt
        {
          return init_dht(a, b, c);
        }
        else
        {
          printmsg(portmsg, 1);
        }
        break;

      case FUNC_GETCOL:
        if (a == 0) return Vordergrund;
        else return Hintergrund;
        break;

      case FUNC_BMPREAD:

        if (!bmp.begin(BMP085_ULTRAHIGHRES, &myI2C))
        {
          printmsg("no BMP-Sensor connected!", 0);
          //if(ser_marker) Serial1.print("no BMP-Sensor connected!");
          return -1;
        }
        else
        {

          if (a < 0 || a > 2) a = 0;                      //0=Druck in Pa, 1=Höhe in m 2=Temp in C - Werte <0 und grösser 2 ergeben den Druck

          if (a == 0) return bmp.readPressure();

          else if (a == 1) return bmp.readAltitude();

          else return bmp.readTemperature();

        }
        break;

      case FUNC_FN:                                       //hier muss der gespeicherte Funktionsstring zurückgeholt und ausgeführt werden
        tmptxtpos = txtpos;                               //txtpos sichern
        txtpos = Fntable[fname];                          //Formelstring nach txtpos kopieren
        a = get_value();                                 //Formel ausführen
        txtpos = tmptxtpos;                               //txtpos zurückschreiben
        return a;
        break;

      case FUNC_PORT:
        return a;
        break;

      case FUNC_PIN:
        return mcp.digitalRead(a);
        break;

      case FUNC_PIC:
        if (a == 0) return bmp_width;
        else return bmp_height;
        break;

      case FUNC_MAP:
        return map(a, map_var[0], map_var[1], map_var[2], map_var[3]);
        break;

      case FUNC_CONSTRAIN:
        return constrain(a, b, c);
        break;

      case FUNC_GRID:
        if (a > 11) a = 11;
        return Grid[abs(int(a))];
        break;

      default:

        break;
    }
  }

expr4_error:
  expression_error = 1;
  return 0;

}



/***************************************************************************/
static float expr3(void)
{
  float a, b;

  a = expr4();

  spaces();                               // fix for eg:  100 a = a + 1

  while (1)
  {
    if (*txtpos == '*')
    {
      txtpos++;
      b = expr4();

      a *= b;
    }
    else if (*txtpos == '/')
    {
      txtpos++;
      b = expr4();
      if (b != 0)
        a /= b;
      else
      {
        printmsg(zeroerror, 1);             //Division by Zero
        expression_error = 1;
      }
    }

    else
      return a;
  }
}

/***************************************************************************/
static float expr2(void)
{
  float a, b;
  String abuf, bbuf, cbuf;
  int i;

  if (*txtpos == '-' || *txtpos == '+')
    a = 0;
  else
  {
    if (*txtpos == '"') {
      String_quoted_read();
      string_marker = true;
    }
    else {
      a = expr3();
    }

    if (string_marker == true) {
      abuf = String(tempstring);  //erster String
      abuf.trim();
      cbuf = abuf;
    }

  }
  while (1)
  {
    if (*txtpos == '-')
    {
      txtpos++;
      b = expr3();
      a -= b;
    }
    else if (*txtpos == '+')
    {
      txtpos++;
      if (*txtpos == '"') {
        String_quoted_read();
        string_marker = true;

      }
      else b = expr3();


      if (string_marker == true)
      {
        bbuf = String(tempstring);  //zweiter...n'ter String
        bbuf.trim();
        cbuf += bbuf;
        cbuf.trim();
        cbuf.toCharArray(tempstring, cbuf.length() + 1);
      }
      else

        a += b;

    }
    else
      return a;
  }

}
/***************************************************************************/
static float get_value(void)
{
  float a, b;

  expression_error = 0;

  a = expr2();

  // Check if we have an error
  if (expression_error)  return a;

  scantable(relop_tab);
  if (table_index == RELOP_UNKNOWN)
    return a;

  switch (table_index)
  {
    case RELOP_GE:
      b = expr2();
      if (a >= b) return 1;
      break;
    case RELOP_NE:
      //case RELOP_NE_BANG:
      b = expr2();
      if (a != b) return 1;
      break;
    case RELOP_GT:
      b = expr2();
      if (a > b) return 1;
      break;
    case RELOP_EQ:
      b = expr2();
      if (a == b) return 1;
      break;
    case RELOP_LE:
      b = expr2();
      if (a <= b) return 1;
      break;
    case RELOP_LT:
      b = expr2();
      if (a < b) return 1;
      break;
    case RELOP_MOD:               //Modulo  x mod y
      b = expr2();
      return int(a) % int(b);
      break;
    case RELOP_SHL:               //Bit-wise SHL  x shl y
      b = expr2();
      return (int(a) << int(b));
      break;
    case RELOP_SHR:               //Bit-wise SHR  x >> y
      b = expr2();
      return (int(a) >> int(b));
      break;

    case RELOP_XOR:               //Bit-wise XOR   x || y
      b = expr2();
      return (int(a) ^ int(b));
      break;

    case RELOP_AND:               //Bit-wise AND  x & y
      b = expr2();
      return (int(a)&int(b));
      break;

    case RELOP_OR:                //Bit-wise OR   x | y
      b = expr2();
      return (int(a) | int(b));
      break;

    case RELOP_POW:               //x^y
      b = expr2();
      return pow(a, b);
      break;

    default:
      break;
  }
  return 0;
}

//--------------------------------------------- ende - Werteausgabe -------------------------------------------------------------------------------
//--------------------------------------------- ende - Werteausgabe -------------------------------------------------------------------------------
//--------------------------------------------- ende - Werteausgabe -------------------------------------------------------------------------------


//#######################################################################################################################################
//--------------------------------------------- SDT - Befehl "SDT Tag,Monat,Jahr,Stunde,Minute,Sekunde"----------------------------------
//#######################################################################################################################################

static int set_TimeDate(void)
{ int tagzeit[7];
  expression_error = 0;
  tagzeit[0] = abs(int(get_value()));         //nur ganze Zahlen
  if (expression_error)
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  for (int i = 1; i < 6; i++)
  {
    if (Test_char(',')) return 1;
    tagzeit[i] = abs(int(get_value()));         //nur ganze Zahlen
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
  }
  rtc.adjust(DateTime(tagzeit[2], tagzeit[1], tagzeit[0], tagzeit[3], tagzeit[4], tagzeit[5]));
  e_rtc.setTime(tagzeit[5], tagzeit[4], tagzeit[3], tagzeit[1], tagzeit[2], tagzeit[3], 0);

  return 0;
}

//#######################################################################################################################################
//--------------------------------------------- PRZ - Befehl ----------------------------------------------------------------------------
//#######################################################################################################################################

static int set_prezision(void)
{
  if (Test_char('(')) return 1;
  expression_error = 0;
  Prezision = abs(int(get_value()));         //nur ganze Zahlen
  if (expression_error)
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  if (Test_char(')')) return 1;
  return 0;
}

//#######################################################################################################################################
//--------------------------------------------- LIST - Befehl ---------------------------------------------------------------------------
//#######################################################################################################################################

void list_out()
{
  int l = 0;
  int bis, num;
  bool b_bis = false;

  linenum = testnum();                                                    // gibt 0 zurück, wenn keine Zeilennummer angegeben wird

  if (*txtpos == ',') {                                                   // optionaler Wert bis zu welcher Zeile ausgegeben werden soll
    txtpos++;
    bis = get_value();
    b_bis = true;
  }

  if (txtpos[0] != NL)                                                    //List darf nur im Kommandomodus benutzt werden
  {
    syntaxerror(syntaxmsg);
    return;
  }

  list_line = findline();                                                 // Finde Zeile
  while (list_line != program_end) {

    num = printline();                                                    //Zeile ausgeben
    if (num >= bis && b_bis) break;                                       //Zeile bis zu der ausgegeben werden soll erreicht?
    l++;
    if (!list_send) {
      if (l == (VGAController.getScreenHeight() / y_char[fontsatz]) - 8)  //Anzahl Zeilen abhängig vom gewählten Font, auf Taste warten
      {
        l = 0;
        if (wait_key(true) == 3) break;
      }
    }
  }
  line_terminator();
  warmstart();
  return;
}
//--------------------------------------------- Unterprogramm - Neustart nach Fehler --------------------------------------------------------------

void warmstart() {
  // this signifies that it is running in 'direct' mode.
  current_line = 0;
  sp = program + sizeof(program);
  return;
}

//--------------------------------------------- Unterprogramm - Direkteingabe ---------------------------------------------------------------------

static int direct(void) {
  txtpos = program_end + sizeof(LINENUM);
  if (*txtpos == NL) return 0;                //Zeilenende?
  else return 1;  //                          //nein?, nächster Befehl
}

//--------------------------------------------- Unterprogramm - gehe zu nächsten Befehl -----------------------------------------------------------

static int run_next(void) {
  if (spaces() == ':') txtpos++;
  if (spaces() == NL ) return 1;              //nächste Zeile
  return 0;                                   //nächster Befehl
}

//#######################################################################################################################################
//--------------------------------------------- GOSUB - Befehl --------------------------------------------------------------------------
//#######################################################################################################################################

static int gosub(void)
{
  struct stack_gosub_frame *f;
  if (sp + sizeof(struct stack_gosub_frame) < stack_limit)
  {
    printmsg(gosubmsg, 1);
    warmstart();
    return 1;//continue;
  }

  sp -= sizeof(struct stack_gosub_frame);
  f = (struct stack_gosub_frame *)sp;
  f->frame_type = STACK_GOSUB_FLAG;
  f->txtpos = txtpos;
  f->current_line = current_line;
  current_line = findline();
  return 0;

}

//#######################################################################################################################################
//--------------------------------------------- INPUT - Befehl --------------------------------------------------------------------------
//#######################################################################################################################################

static char Input_String(void)
{
  spaces();
  // make sure there are no quotes or spaces, search for valid characters
  int i = 0;
  while (*txtpos >= ' ' || *txtpos <= 'Z' || *txtpos >= 'a' || *txtpos <= 'z')
  {
    if (*txtpos == NL || *txtpos == ',') {      //Ende, Return
      break;
    }
    tempstring[i++] = *txtpos++; //Tempstring füllen
  }
  tempstring[i] = '\0';       //tempstring abschliessen
  return 0;
}

static int input(void)
{
  char var, c, d, e;
  float value, vl;
  int tmp[10], tmo[10], stmp = 0, i = 0, a = 0, nr = 0;
  bool str_m[10];
  //----- Arrays löschen ---------
  memset(tmp, 0, sizeof(tmp));
  memset(tmo, 0, sizeof(tmo));
  memset(str_m, 0, sizeof(str_m));

  c = spaces();
  if (c == '"') {                                 //Input-Text als Zeichenkette
    print_quoted_string();
  }
  else if (c >= 'A' || c <= 'Z')                  // oder als Stringvariable
  {
    a = get_value();                                //1.Zahl (bei Stringvariablen steht in tempstring die 1.Zeichenkette)
    printmsg(tempstring, 0);
    tempstring[0] = 0;
    string_marker = false;                        //String-Marker zurücksetzen, für korrekte Printausgabe/Werteübergabe
  }
  if (Test_char(';')) return 1;

  while (1)
  {
    c = spaces();
    if (c < 'A' || c > 'Z') return 1;

    var = c;
    tmp[nr] = var - 'A';
    txtpos++;
    d = spaces();
    if (d >= 'A' && d <= 'Z') {
      e = d;
      tmo[nr] = (e - 'A' + 1) * 26;
      txtpos++;
    }

    //------------------------------------------------------------------------------------------------------------------------------------------------------------
    while (*txtpos >= 'A' && *txtpos <= 'Z') txtpos++;  //so sind auch lange Variablennamen möglich ->siehe auch expr4()
    //------------------------------------------------------------------------------------------------------------------------------------------------------------

    c = spaces();
    if (c == '$') {                                      //Zeichenkette
      str_m[nr] = true;
      c = skip_spaces();
    }

    if (c == ',')
    {
      //Terminal.println(c);
      nr++;
      txtpos++;
      continue;
    }

    if (c == NL || c == ':')
    {
      break;
    }

  }

inputagain:
  tmptxtpos = txtpos;
  expression_error = 0;
  getln( '?' );
  toUppercaseBuffer();
  txtpos = program_end + sizeof(unsigned short);
  spaces();
  for (int f = 0; f < nr + 1; f++)
  {
    if (str_m[f] == true)
    {
      Input_String();
      stmp = tmp[f] * STR_LEN;                                  //Position im Stringspeicher
      i = 0;                                                    //Stringpositionszähler zurücksetzen
      while (1)
      {
        c = tempstring[i];
        if (c == '\0') {
          Stringtable[stmp + i] = '\0';                                   //Nullterminator setzen
          break;
        }
        else {
          if (i < STR_LEN) {
            Stringtable[stmp + i++] = c;
          }
          else
          {
            Stringtable[stmp + i] = '\0';
            break;
          }

        }
      }
    }
    else
    {

      value = get_value();
      if (expression_error) {
        Beep(0, 0);
        txtpos = tmptxtpos;
        goto inputagain;
      }

      ((float *)variables_begin)[tmp[f] + tmo[f]] = value;
    }
    if (f < nr) {
      if (Test_char(',')) return 1;
    }

  }

  txtpos = tmptxtpos;
  return 0;

}

//#######################################################################################################################################
//--------------------------------------------- DATA - Befehl ---------------------------------------------------------------------------
//#######################################################################################################################################

static float data_get(void)
{
  float value;
  float *var;
  int tmp, stmp, i, var_pos, array_art ;
  char c;
  word arr_adr;
  array_art = 0;

  if (current_dataline <= num_of_datalines)                //DATA-Zeile gültig?
  {
    if (datapointer == 0)
    {
      linenum = data_numbers[current_dataline++];                 //Zeilennummer übergeben
      if (linenum > 0)
      {
        dataline = findline() + sizeof(LINENUM) + sizeof(char);  //entsprechende Zeile nach dataline laden
        dataline += 4;
      }
    }
  }

  if (*txtpos < 'A' || *txtpos > 'Z')                                     //erster Variablenbuchstabe
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  var_pos = *txtpos - 'A';
  var = (float *)variables_begin + *txtpos - 'A';
  stmp = (int) (*txtpos - 'A') * STR_LEN;                                 //Strings nur als einbuchstabige Variablen erlaubt, deshalb Variablenadresse sichern
  txtpos++;
  if (*txtpos >= 'A' && *txtpos <= 'Z') {                                 //zweiter Variablenbuchstabe
    tmp = (int) ((*txtpos - 'A' + 1) * 26);
    var = var + tmp;
    //svar = (int)((*txtpos - 'A' + 1)* 40 * 26);                         //zweiter Variablenbuchstabe für Strings
    //stmp = stmp+svar;
    txtpos++;
  }

  //------------------------------------------------------------------------------------------------------------------------------------------------------------
  while (*txtpos >= 'A' && *txtpos <= 'Z') txtpos++;  //so sind auch lange Variablennamen möglich ->siehe auch expr4()
  //------------------------------------------------------------------------------------------------------------------------------------------------------------
  if (*txtpos == '(') {
    txtpos++;
    expression_error = 0;
    arr_adr = rw_array(var_pos, VAR_TBL);                                  //numerische Array? Adresse im Zahlen-Arrayfeld
    if (expression_error) return 1;                                        //Fehler? dann zurück
    array_art = 1;
  }

  else if (spaces() == '$')
  { //String?
    txtpos++;
    if (*txtpos == '(') {                                                 //kommt eine Klammer vor, muss es sich um ein Array handeln
      txtpos++;
      expression_error = 0;
      arr_adr = rw_array(var_pos, STR_TBL);                               //String_array?, Adresse im String-Arrayfeld
      if (expression_error) return 1;                                     //Fehler? dann zurück
      array_art = 2;
    }
    Data_String_quoted_read();
    i = 0;
    while (1)
    {
      c = tempstring[i];
      if (c == '\0') {
        if (array_art == 2) {
          SPI_RAM_write8(arr_adr + i, '\0');
        }
        else {
          Stringtable[stmp + i] = '\0';                                   //Nullterminator setzen
        }
        break;
      }
      else {
        if (i < STR_LEN) {
          if (array_art == 2) {
            SPI_RAM_write8(arr_adr + i++, c);
          }
          else Stringtable[stmp + i++] = c;
        }
        else {
          if (array_art == 2) {
            SPI_RAM_write8(arr_adr + i, '\0');
            break;
          }
          Stringtable[stmp + i] = '\0';
          break;
        }

      }
    }

    return 0;
  }//String

  spaces();
  value = data_expr();


  if (array_art == 1) {
    //if(Test_char(')')) return 1;
    byte* bytes = (byte*)&value;                            //float nach byte-array umwandeln
    SPI_RAM_write(arr_adr, bytes, 4);
    return 0;
  }

  *var = value;
  return 0;
}


static char data_spaces(void) {
  char c;
  while (1) {
    c = *dataline;
    if (c == SPACE) dataline++;
    else break;
  }
  return (c);
}

//#######################################################################################################################################
//--------------------------------------------- Wert aus DATA - Anweisung lesen ---------------------------------------------------------
//#######################################################################################################################################

static float data_expr(void)
{
  unsigned long t = 0;
  unsigned long f = 0;
  char g = 0;
  int pointmarker = 0;
  int i;
  String cbuf;

  data_spaces();
  datapointer++;                            //DATA-Zeiger erhöhen

  if ( *dataline == '-' ) {
    dataline++;
    return -data_expr();
  }

  //******************************************** Zahleneingabe mit Exponentialschreibweise ************************************

  else if ((*dataline >= '0' && *dataline <= '9') || (*dataline == '.') || ((*dataline == 'E') && (dataline[1] == '+' || dataline[1] == '-')))
  {
    float a = 0;
    char an[] = {"                  "};
    int i = 0;
    int m = 0;

    do   {
      if (*dataline == '.' && pointmarker == 0) pointmarker++; //Überprüfung auf mehr als einem Punkt
      //###################### Exponentialschreibweise ######################### 1E-6 bis 1E+38
      if ((*dataline == 'E') && (dataline[1] == '-' || dataline[1] == '+')) {
        an[i] = *dataline;
        dataline++;
        i++;
        an[i] = *dataline;

        dataline++,
                 i++;
      }

      an[i] = *dataline;
      dataline++;
      i++;
    }
    while ((*dataline >= '0' && *dataline <= '9') || (*dataline == '.') || ((*dataline == 'E') && (dataline[1] == '+' || dataline[1] == '-')));

    cbuf = an;

    a = cbuf.toFloat();
    if (*dataline == ',')
      dataline++;
    if (*dataline == NL || *dataline == NULL)
    {
      datapointer = 0;
    }
    return a;
  }

  //----------------------------------------- Hexadezimalzahlen -------------------------------------------------------------------------------------
  else if (*dataline == '#') {
    dataline++;
    g = *dataline;
    f = 0;
    f = hexDigit(g);
    if (f < 0)
      goto data_expr_error;
    t = f;
    dataline++;
    g = *dataline;

    f = 0;
    while (f >= 0)          //alle Stellen lesen
    {
      f = hexDigit(g);
      t = t << 4 | f;
      dataline++;
      g = *dataline;
      switch (g)
      { // Überprüfung auf gültige Zeichen
        case 'a' ... 'f':
          break;
        case 'A' ... 'F':
          break;
        case '0' ... '9':
          break;
        default:                                              //ungültiges Zeichen -> Ausstieg
          return float(t);
      }
    }
    if (*dataline == ',')
      dataline++;
    if (*dataline == NL || *dataline == NULL)
    {
      datapointer = 0;
    }
    return float(t);
  }


data_expr_error:
  expression_error = 1;
  return 0;

}

//#######################################################################################################################################
//--------------------------------------------- Zeichenkette aus DATA - Anweisung lesen -------------------------------------------------
//#######################################################################################################################################

static char Data_String_quoted_read(void)
{
  char c;
  data_spaces();
  c = *dataline;
  if (c != '"')
  {
    printmsg(syntaxmsg, 1);
    expression_error = 1;
    return 0;
  }
  dataline++;
  expression_error = 0;
  // make sure there are no quotes or spaces, search for valid characters
  int i = 0;

  for (i = 0; i < STR_LEN; i++) tempstring[i] = 0; //Tempstring löschen

  i = 0;
  while (*dataline != '"')
  {
    tempstring[i++] = *dataline++;  //Tempstring füllen
  }
  tempstring[i] = '\0';             //tempstring abschliessen

  dataline++;                       //Anführungszeichen überspringen


  if (*dataline == NL || *dataline == NULL)
  {
    datapointer = 0;
  }
  if (*dataline == ',')
  {
    dataline++;
    datapointer++;              //DATA-Zeiger erhöhen
  }
  return 0;
}


//#######################################################################################################################################
//---------------------------------------- Zeile ans Ende des Speichers verschieben -----------------------------------------------------
//#######################################################################################################################################
void move_line() {

  toUppercaseBuffer();                              //Zeile in Großbuchstaben umwandeln

  txtpos = program_end + sizeof(unsigned short);

  while (*txtpos != NL)                             // Finde das Ende der Eingabezeile
    txtpos++;

  // Move it to the end of program_memory

  char *dest;
  dest = variables_begin - 1;
  while (1)
  {
    *dest = *txtpos;
    if (txtpos == program_end + sizeof(unsigned short))  //am ende angekommen?
      break;
    dest--;
    txtpos--;
  }
  txtpos = dest;


}
//#######################################################################################################################################
//--------------------------------------------------- neue Zeile einfügen ---------------------------------------------------------------
//#######################################################################################################################################
int insert_line() {

  linelen = 0;
  while (txtpos[linelen] != NL)                                 //Zeilenlänge ermitteln
    linelen++;
  linelen++; // Include the NL in the line length
  linelen += sizeof(unsigned short) + sizeof(char);             //Leerzeichen einfügen für Zeilennummer + Zeile

  //Terminal.print(linelen,DEC);

  txtpos -= 3;                                                  //Zeilennummer ermitteln
  *((unsigned short *)txtpos) = linenum;
  txtpos[sizeof(LINENUM)] = linelen;


  // Zeile ins Programm einfügen/anhängen
  pstart = findline();


  if (pstart != program_end && *((LINENUM *)pstart) == linenum) // wenn Zeilennummer existiert, dann überschreiben
  {
    char *dest, *from;
    unsigned tomove;

    from = pstart + pstart[sizeof(LINENUM)];
    dest = pstart;

    tomove = program_end - from;
    while ( tomove > 0)
    {
      *dest = *from;
      from++;
      dest++;
      tomove--;
    }
    program_end = dest;
  }

  if (txtpos[sizeof(LINENUM) + sizeof(char)] == NL)             // Zeile hat keinen Text, dann wird gelöscht
    return 1;//continue;


  while (linelen > 0)                                           // Platz schaffen für neue Zeile (entweder in einem Stück oder nach und nach)
  {
    unsigned int tomove;
    char *from, *dest;
    unsigned int space_to_make;

    space_to_make = txtpos - program_end;

    if (space_to_make > linelen)
      space_to_make = linelen;
    newEnd = program_end + space_to_make;
    tomove = program_end - pstart;

    from = program_end;                                         //Da sich Quelle und Ziel möglicherweise überschneiden, müssen wir uns von unten nach oben bewegen
    dest = newEnd;
    while (tomove > 0)
    {
      from--;
      dest--;
      *dest = *from;
      tomove--;
    }

    for (tomove = 0; tomove < space_to_make; tomove++)          // Zeile an neue Stelle kopieren
    {
      *pstart = *txtpos;
      txtpos++;
      pstart++;
      linelen--;
    }
    program_end = newEnd;
  }
  return 0;
}



//********************************************* Main - Programm ***********************************************************************************

void loop()
{

  //--------------------------------------------- hier geht's los -----------------------------------------------------------------------------------
  Basic_Interpreter();
}


//********************************************* Main - Programm ***********************************************************************************


//#################################################################################################################################################
//############################################# Start Basic_Interpreter ###########################################################################
//#################################################################################################################################################

void Basic_Interpreter()
{
  char pa, pb;
  initSD();                                              //SD-Karte initialisieren
  cmd_new();                                             //alles löschen
  int a, e;


  //################################################# Hauptprogrammschleife ######################################################
  while (1)
  {

    expression_error = 0;                               //alle Errors zurücksetzen

    if ( triggerRun ) {                                 // AUTO-START-FUNKTION
      triggerRun = false;
      clear_var();                                      //Variablen und Array-Tabelle löschen, Data-Zeiger zurücksetzen
      find_data_line();                                 //Data-Zeilen finden
      current_line = program_start;
      sp = program + sizeof(program);
      goto execline;
    }

    else {

      getln('>');
      //-------------------------------- Start Zeile einfügen ---------------------------------------------
      move_line();                                                  //Zeile in Großbuchstaben umwandeln und ans Ende des Speicher verschieben

      linenum = testnum();                                          // Zeilennummer vorhanden?
      spaces();
      if (linenum == 0) {
        if (direct() == 0) continue;                                //keine Zeilennummer
        else goto interpreteAtTxtpos;                               //Zeile ausführen
      }
      if (insert_line()) {                                          //Zeile in die richtige Position im Speicher einfügen oder löschen, wenn kein Text vorhanden
        continue;
      }
    }
    //------------------------------------------ Ende Zeile einfügen -------------------------------------------------
    continue;


run_next_statement:

    if (run_next() == 1) goto execnextline;

interpreteAtTxtpos:

    if (breakcheck() || break_marker)                                //Programmabbruch mit Ctrl-C oder ESC
    {
      break_marker = false;
      line_terminator();

      if (current_line != NULL)
      {
        printmsg(breakmsg, 0);
        linenum = *((LINENUM *)(current_line));
        printnum(linenum, 0);
        //if(ser_marker) Serial1.print(linenum);
      }

      line_terminator();
      warmstart();
      Terminal.enableCursor(true);                                 //Cursor einschalten
      continue;

    }

    scantable(keywords);                                            //Befehlstabelle scannen
    keyword_index = table_index;


    //####################################################################################################
    //############################### Abarbeitung Befehlstabelle #########################################
    //####################################################################################################

    switch (keyword_index)
    {
      case KW_LIST:                                       // LIST
        list_out();
        continue;
        break;


      case KW_LOAD:                                       // LOAD filename
        if (*txtpos == NL) {
          load_ram();
          continue;
        }

        if (load_file()) {
          continue;
        }
        string_marker = false;
        if (*txtpos == ',') {
          txtpos++;
          if (Test_char('1')) continue;
          autorun = true;                                 //Load"Filename",1 ->autostart
        }
        continue;
        break;

      case KW_NEW:                                        // NEW
        cmd_new();
        continue;
        break;

      case KW_RUN:                                        // RUN
        clear_var();
        find_data_line();                                 //Data-Zeilen finden
        current_line = program_start;                     //beginn mit erster Zeile
        sp = program + sizeof(program);
        goto execline;
        break;

      case KW_SAVE:                                       // SAVE filename (/filename.bas)
        if (*txtpos == NL) {
          save_ram();
          continue;
        }
        save_file();
        string_marker = false;
        continue;
        break;

      case KW_NEXT:                                       // NEXT
        goto next;
        break;

      case KW_REN:                                        // RENAME
        expression_error = 0;
        get_value();
        strcpy(filestring, tempstring);       //Tempstring nach filestring kopieren
        if (Test_char(',')) continue;
        get_value();
        renameFile(SD, filestring, tempstring);
        continue;
        break;

      case KW_IF:                                         // IF
        float val;
        logic_counter = 0;
        logica = 0;
        else_marker = false;
        then_marker = true;                               //THEN marker muss noch abgefragt werden (missing_then)
        memset (logic_ergebnis, 0, sizeof (logic_ergebnis));//Logik-Puffer leeren
        expression_error = 0;
        val = get_value();
        logic_ergebnis[logic_counter++] = int(val);       //Ergebnis in Puffer speichern und logicmarker hochzählen
        if (val != 0) {
          val = 0;                                        //für den Fall, das kein AND,OR vorkommt
          break;
        }
        else
        {
          else_marker = true;
          val = 1;                                        //Für den Fall, das es kein AND OR gibt (einfaches IF)
          goto run_next_statement;                        //die ELSE Bedingung muss in der nächsten Zeile stehen
        }
        break;

      case KW_ON:                                         //ON (GOTO/GOSUB)
        ongosub = 0;
        ongosub = get_value();
        if (ongosub == 0) goto execnextline;              //ist der Wert der Variable=0 dann wird mit der nächsten Zeile weitergemacht
        break;

      case KW_GOTO ... KW_GOSUB:                          // GOTO/GOSUB
        expression_error = 0;
        linenum = get_value();
        if (ongosub > 0)
        {
          e = 1;
          while (*txtpos == ',' && e < ongosub)           //so oft wiederholen, wie Zeilennummern existieren
          {
            txtpos++;
            e++;
            linenum = get_value();
          }

          if (a < 0 || a > 65535)
          {
            //syntaxerror(syntaxmsg);
            continue;
          }
        }
        if (keyword_index == KW_GOSUB)
        {
          gosub();
        }

        current_line = findline();
        goto execline;
        break;

      case KW_RETURN:                                     // RETURN
        goto gosub_return;
        break;

      case KW_REM:                                        // REM
        goto execnextline;                                // Ignoriere die komplette Zeile
        break;

      case KW_FOR:
        goto forloop;
        break;

      case KW_INPUT:
        if (input()) continue;
        break;

      case KW_PRINTING:                                   // ?
      case KW_PRINT:                                      // Print
        if (command_Print()) continue;
        break;

      case KW_DOKE:
      case KW_POKE:                                       // POKE adresse,wert
      case KW_FPOKE:
        if (poke(keyword_index)) continue;
        break;

      case KW_DIR:                                        // SD-Card Directory DIR
        cmd_Dir();
        break;

      case KW_CHDIR:                                      // SD-Card Change-Directory CD
        cmd_chdir();
        break;

      case KW_MKDIR:                                      // SD-Card Make-Dir MD
        if (cmd_mkdir(1));
        continue;
        break;

      case KW_RMDIR:                                      // SD-Card Remove-Dir RD
        if (cmd_mkdir(0));
        continue;
        break;

      case KW_CLS:                                        // CLS
        if (Frame_nr) {
          win_cls(Frame_nr);
        }
        else {
          tc.setCursorPos(1, 1);
          //cmd_cls();
          GFX.clear();
        }
        break;

      case KW_POS:                                        // POS x,y
        if (set_pos())
          continue;
        break;

      case KW_COLOR:                                      // COLOR v,b
        if (color())
          continue;
        break;

      case KW_PSET:                                       // PSET x,y
        if (pset())
          continue;
        break;

      case KW_CIRCLE:                                     // CIRCLE x,y,w,h,fill
        if (line_rec_circ(1, 4))                          //1=Circle, 0..4 Parameter
          continue;
        break;

      case KW_LINE:                                       // LINE x,y,xx,yy
        if (line_rec_circ(0, 3))                          // 0=Lines, 0..3 Parameter
          continue;
        break;

      case KW_RECT:                                       // RECT x,y,w,h,fill
        if (line_rec_circ(2, 4))                          // 2=Rect, 0..4 Parameter
          continue;
        break;

      case KW_ROUND:                                      // ROUND x,y,w,h,r
        if (line_rec_circ(3, 5))                          // 3=Round, 0..4 Parameter
          continue;
        break;

      case KW_FONT:                                       // FONT f
        expression_error = 0;
        val = int(get_value());
        set_font(val);
        break;

      case KW_DELAY:                                       //PAUSE
        expression_error = 0;
        val = get_value();
        delay( val );
        break;

      case KW_END:
        current_line = program_end;
        goto execline;
        break;

      case KW_CLEAR:
        clear_var();
        break;

      case KW_THEN:                                   // THEN
        then_marker = false;
        if ( val == 0 ) {                             //Überprüfung, ob alle If Bedingungen war sind
          break;
        }
        else
        {
          else_marker = true;
          goto execnextline;                             //Bedingung nicht erfüllt -> nächste Zeile
        }

      case KW_ELSE:                                       // ELSE
        if (else_marker == true) {
          else_marker = false;
          break;
        }
        goto execnextline;                                //Bedingung nicht erfüllt -> nächste Zeile

      case KW_CURSOR:
        if (cursor_onoff())
          continue;
        break;

      case KW_PREZISION:                                  // PREZ 6 ->Nachkommastellen
        if (set_prezision())
          continue;
        break;

      case KW_DUMP:                                       // DUMP adresse
        if (Memory_Dump())
          continue;
        break;

      case KW_STYLE:
        if (set_style())
          continue;
        break;

      case KW_SCROLL:
        if (scroll_xy())
          continue;
        break;

      case KW_START:                                      // START filename
        autorun = true;
        if (*txtpos == NL) {
          load_ram();
          clear_var();
          find_data_line();                                 //Data-Zeilen finden
          current_line = program_start;                     //beginn mit erster Zeile
          sp = program + sizeof(program);
          goto execline;
          break;
        }
        if (load_file()) {
          continue;
        }
        continue;
        break;

      case KW_THEME:
        int a;
        a = int(get_value());
        set_theme(a);
        print_info();
        break;

      case KW_DATA:                                       // DATA - wird ignoriert
        goto execnextline;
        break;

      case KW_READ:
        while (1) {
          if (data_get())
          {
            syntaxerror(datamsg);
            continue;
          }
          if (*txtpos == ',')
            txtpos++;
          else break;
        }
        break;

      case KW_RESTORE:                                                //setzen des Datazeigers
        if (*txtpos == NL || *txtpos == ':')                          //wird kein Wert gelesen, datapointer zurücksetzen
        {
          string_marker = false;
          datapointer = 0;
          current_dataline = 0;
        }
        else
        {
          expression_error = 0;                                       //es wird eine Zeilennummer angegeben
          linenum = get_value();
          for (int i = 0; i <= (num_of_datalines - 1); i++)           //scannen, ob Zeilennummer existiert, wenn ja, übernehmen
          {
            if (data_numbers[i] == linenum)
            {
              datapointer = 0;                                       //datapointer zurücksetzen
              current_dataline = i;                                  //Zeiger auf die exitierende Zeile setzen
              break;
            }
          }
        }
        break;

      case KW_DEL:                                        // DEL
        if (cmd_delFiles())
        {
          syntaxerror(notexistmsg);
        }
        continue;
        break;

      case KW_AND:
        expression_error = 0;
        val = get_value();
        logic_ergebnis[logic_counter++] = int(val);       //alle Ergebnisse einlesen und auswerten
        for (int i = 0; i < logic_counter + 1; i++)
        {
          logica += logic_ergebnis[i];
        }
        val = logica - logic_counter;                     //sind alle Bedingungen erfüllt lautet das Ergebnis 0
        break;

      case KW_OR:
        expression_error = 0;
        val = get_value();

        logic_ergebnis[logic_counter++] = int(val);
        for (int i = 0; i < logic_counter + 1; i++)       //alle Ergebnisse einlesen und auswerten
        {
          logica += logic_ergebnis[i];
        }
        val = logica;                                   //ist mindestens eine Bedingung erfüllt lautet das Ergebnis 0 ->war

        if (val == 0)                                   //sind alle Bedingungen falsch wird val auf 1 (falsch gesetzt)
          val = 1;
        else val = 0;                                   //mindestens eine Bedingung ist erfüllt, also -> war
        break;

      case KW_SRTC:                                     //RTC stellen
        if (set_TimeDate())
          continue;
        break;

      case KW_IIC:                                      // I2C Start o. Write o. RequestFrom
        float val1;
        if (Test_char('(')) continue;
        char h;
        h = *txtpos;
        if (h == 'S' || h == 'W' || h == 'Q')
        {
          txtpos += 2;
          expression_error = 0;
          val = get_value();
          if ( h == 'Q') {
            txtpos++;
            expression_error = 0;
            val1 = get_value();
          }
          if (Test_char(')')) continue;

          if ( h == 'S') myI2C.beginTransmission(int(val));           //IIC-Start IIC(S,Adresse)
          else if ( h == 'W') myI2C.write(int(val));                  //IIC-Write IIC(W,Befehl)
          else if ( h == 'Q') myI2C.requestFrom(int(val), int(val1)); //IIC-RequestFrom IIC(Q,Adresse,Anzahl Bytes)
        }
        else {
          continue;
        }
        break;

      case KW_DWRITE:                                   //Port schreiben
        if (set_port())
          continue;
        break;

      case KW_PWM:
        if (set_pwm())
          continue;
        break;

      case KW_DAC:                                        //DAC nur Pin26 - Eingabe in gewünschter Spannung 0-3,3
        if (Test_char('(')) continue;
        expression_error = 0;
        val = get_value();
        if ((val > 3.3) || (val < 0))
        {
          syntaxerror(valmsg);                            //falscher Wert
          continue;
        }
        if (Test_char(')')) continue;
        dacWrite(26, int(val / 0.0129));                  //Wert 0-255 (255= 3.3V 128=1.65V) x=wert/0,0129 (3,3/255)
        break;

      case KW_DRAW:
        if (draws()) {
          //syntaxerror(syntaxmsg);
          continue;
        }
        break;

      case KW_SPRITE:
        if (Test_char('(')) continue;
        pa = *txtpos;
        if (pa == 'C' || pa == 'D' || pa == 'S') {
          txtpos++;
          if (sprite(pa)) {
            //syntaxerror(syntaxmsg);
            continue;
          }
        }
        else {
          syntaxerror(syntaxmsg);
          continue;
        }
        break;

      case KW_SOUND:
        if (Sound())
          continue;
        break;

      case KW_PULSE:                  //PULSE(PORT,MS)

        if (set_pulse()) {
          //  syntaxerror(syntaxmsg);
          continue;
        }
        break;

      case KW_PEN:

        if (set_pen()) {
          //  syntaxerror(syntaxmsg);
          continue;
        }
        break;


      case KW_LCD:
        if (LCD_Set()) {
          //syntaxerror(syntaxmsg);
          continue;
        }
        break;

      case KW_DEFFUNC:
        if (def_func()) {
          continue;
        }
        break;

      case KW_MCPPORT:
        if (Test_char('(')) continue;
        pa = *txtpos;
        if (pa == 'S' || pa == 'W')  //PORT(S,A/B,0/1)
        {
          txtpos++;
          if (Test_char(',')) continue;
          pb = *txtpos;
          if (pb == 'A' || pb == 'B') {
            txtpos += 2;
          }
          else pb = 'C';

          expression_error = 0;
          val = get_value();
        }

        if (Test_char(')')) continue;
        if (pa == 'S') {
          mcp_Port_direction(pb, int(val));                        //Port AB
        }
        else if (pa == 'W') {
          mcp_Port_write(pb, int(val));
        }

        else {
          continue;
        }
        break;

      case KW_MCPPIN:                                 //Befehl PIN(S/W,Nr,0/1)
        if (Test_char('(')) continue;
        pa = *txtpos;
        txtpos++;
        if (pa == 'S' || pa == 'W')                   //PIN(S/W,Nr,0/1)
        {
          if (Test_char(',')) continue;
          expression_error = 0;
          val = get_value();
          txtpos++;
          expression_error = 0;
          val1 = get_value();
          if (Test_char(')')) continue;
          if (pa == 'S')  mcp_Pin_direction(int(val), int(val1));
          else if (pa == 'W')  mcp_Pin_write(int(val), int(val1));
        }
        else
        {
          continue;
        }
        break;

      case KW_LED:
        if (LED_Set()) {
          //syntaxerror(syntaxmsg);
          continue;
        }
        break;

      case KW_EDIT:
        val = int(get_value());
        Editor(val);
        continue;
        break;

      case KW_BEEP:
        a = 0;
        val = 0;
        if (*txtpos == '(') {
          txtpos++;
          a = get_value();
          if (*txtpos == ',') {
            txtpos++;
            val = get_value();
          }
          if (expression_error) continue;
          if (Test_char(')')) continue;
        }
        Beep(byte(a), val);
        break;

      case KW_DIM:
        if (Array_Dim())
          continue;
        break;

      case KW_OPTION:
        if (Option())
          continue;
        break;

      case KW_MOUNT:
        initSD();                                              //SD-Karte initialisieren
        //spi_fram.begin(3);                                     //Fram select
        break;

      case KW_COM:
        if (cmd_serial())
          continue;
        break;

      case KW_PIC:
        if (show_Pic())
          continue;
        break;

      case KW_OPEN:
        file_rw_open();
        break;

      case KW_CLOSE:
        Datei_open = false;
        File_pos = 0;
        File_size = 0;
        break;

      case KW_FILE:                 //File_Open,File_Read,File_Write,File_Close
        File_Operations();
        break;

      case KW_TYPE:
        type_file();
        break;

      case KW_CPM:
        load_binary();
        break;

      case KW_GRID:
        if (make_grid())
          continue;
        break;

      case KW_TEXT:
        if (draw_text())
          continue;
        break;

      case KW_WINDOW:
        if (win())
          continue;
        break;

      case KW_HELP:
        if (*txtpos == NL) show_help();
        else show_help_name();
        *txtpos = NL;                     //Zeile muss beendet werden,da bei Eingabe des Befehls nach Help sonst Fehler auftreten (es werden evt. Parameter erwartet)
        break;

      case KW_DEFAULT:
        if (var_get())
          continue;
        break;


      default:

        break;

    } //switch(table_index)


    goto run_next_statement;

    //################################### Ende der Befehlstabelle #########################################

execnextline:
    if (current_line == NULL)   // Processing direct commands?
      continue;
    current_line +=  current_line[sizeof(LINENUM)];

execline:
    if (current_line == program_end) { // Out of lines to run
      warmstart();
      continue;
    }

    //----------------------- TRON-Funktion --------------------------------------
    if (tron_marker) {
      Terminal.print('<' + String(*((LINENUM *)(current_line))) + '>');
    }
    //----------------------------------------------------------------------------

    txtpos = current_line + sizeof(LINENUM) + sizeof(char);
    goto interpreteAtTxtpos;


forloop:
    {

      //char var, c, d;
      float initial, step, to_var;
      int var, c, d, z;
      z = 0;
      c = spaces();
      if (c < 'A' || c > 'Z')
      {
        syntaxerror(syntaxmsg);
        continue;
      }
      var = int(c - 'A');
      *txtpos++;
      d = int(spaces());

      if (d >= 'A' && d <= 'Z')
      { c = (d - 'A' + 1) * 26;
        var = var + c;
        *txtpos++;

      }
      if (Test_char('=')) continue;
      spaces();

      initial = get_value();
      if (expression_error) continue;

      scantable(to_tab);
      if (table_index != 0)
      {
        syntaxerror(syntaxmsg);
        continue;
      }

      to_var = get_value();
      if (expression_error) continue;

      scantable(step_tab);
      if (table_index == 0)
      {
        step = get_value();
        if (expression_error) continue;
      }
      else
        step = 1;
      c = spaces();

      if (c != NL && c != ':')
      {
        syntaxerror(syntaxmsg);
        continue;
      }

      if (!expression_error && (*txtpos == NL || *txtpos == ':'))
      {
        string_marker = false;
        struct stack_for_frame *f;
        if (sp + sizeof(struct stack_for_frame) < stack_limit) {
          printmsg(fornextmsg, 1);
          printnum(linenum, 0);

          //if(ser_marker) Serial1.print(linenum,DEC);
          warmstart();
          continue;
        }

        sp -= sizeof(struct stack_for_frame);
        f = (struct stack_for_frame *)sp;
        ((float *)variables_begin)[var] = initial;// - 'A'];

        f->frame_type = STACK_FOR_FLAG;
        f->for_var  = var;
        f->to_var   = to_var;
        f->step     = step;
        f->txtpos   = txtpos;
        f->current_line = current_line;
        goto run_next_statement;
      }

    }

    //syntaxerror(syntaxmsg);
    //continue;


next:
    // Fnd the variable name
    spaces();

    if (*txtpos < 'A' || *txtpos > 'Z')
    {
      printmsg(fornextmsg, 1);
      continue;
    }

    txtpos++;
    spaces();

gosub_return:
    // Now walk up the stack frames and find the frame we want, if present
    tempsp = sp;
    while (tempsp < program + sizeof(program) - 1)
    {
      switch (tempsp[0])
      {
        case STACK_GOSUB_FLAG:
          if (table_index == KW_RETURN)
          {
            struct stack_gosub_frame *f = (struct stack_gosub_frame *)tempsp;
            current_line  = f->current_line;
            txtpos      = f->txtpos;
            sp += sizeof(struct stack_gosub_frame);
            if (ongosub == 0)
            {
              goto run_next_statement;   //nur Gosub führt nach dem Return den nächsten Befehl in der Zeile aus
            }
            else {
              goto execnextline;       //bei on gosub wird nach dem Return in die nächste Zeile gesprungen
            }
          }
          // This is not the loop you are looking for... so Walk back up the stack
          tempsp += sizeof(struct stack_gosub_frame);
          break;
        case STACK_FOR_FLAG:
          // Flag, Var, Final, Step
          if (table_index == KW_NEXT)
          { int tmp;
            struct stack_for_frame *f = (struct stack_for_frame *)tempsp;
            // Is the the variable we are looking for?

            if (txtpos[-1] >= 'A' && txtpos[-1] <= 'Z') //== f->for_var)          //erster Variablenbuchstabe
            {
              int ef, xf;
              tmp = int(txtpos[-1] - 'A');


              if (*txtpos >= 'A' && *txtpos <= 'Z') //txtpos[0] == f -> for_var)   //zweiter Variablenbuchstabe
              {
                ef = int(*txtpos - 'A' + 1) * 26;
                tmp = tmp + ef;
                txtpos++;
              }

              //Terminal.println(*txtpos,DEC);
              if (tmp == f->for_var)
              {
                float *varaddr = ((float *)variables_begin) + tmp;//txtpos[-1] - 'A';

                *varaddr = *varaddr + f->step;
                // Use a different test depending on the sign of the step increment
                if ((f->step > 0 && *varaddr <= f->to_var) || (f->step < 0 && *varaddr >= f->to_var))
                {
                  // We have to loop so don't pop the stack
                  txtpos = f->txtpos;
                  current_line = f->current_line;
                  goto run_next_statement;
                }
                // We've run to the end of the loop. drop out of the loop, popping the stack
                sp = tempsp + sizeof(struct stack_for_frame);
                goto run_next_statement;
              }
            }
          }
          // This is not the loop you are looking for... so Walk back up the stack
          tempsp += sizeof(struct stack_for_frame);
          break;
        default:
          warmstart();
          continue;
      }
    }
    // Didn't find the variable we've been looking for

    syntaxerror(syntaxmsg);
    //continue;

  }//while(1)
} //Basic_interpreter

//#################################################################################################################################################
//############################################# Ende Basic_Interpreter ############################################################################
//#################################################################################################################################################





//#######################################################################################################################################
//--------------------------------------------- PRINT - Befehl --------------------------------------------------------------------------
//#######################################################################################################################################
static int command_Print(void)
{
  int k = 0;
  int xp, yp;

  while (!k)
  { char c = spaces();

    switch (c)
    {

      case ';':
        semicolon = true;
        if (skip_spaces() == NL)
        {
          k = 1;
        }
        break;

      case '"':
        print_quoted_string();
        break;

      case ':':
        if (!semicolon) line_terminator();
        txtpos++;
        k = 1;
        semicolon = false;
        break;

      case NL:
        line_terminator();
        semicolon = false;
        k = 1;
        break;

      case ',':
        xp = tc.getCursorCol();             //Tabulator ausgeben
        yp = tc.getCursorRow();
        if (xp < 37)
          tc.setCursorPos(xp + 8, yp);
        else {
          line_terminator();
        }
        semicolon = true;
        if (skip_spaces() == NL)
        {
          k = 1;
        }
        break;

      case 'A':                             //Print AT(x,y);
        tmptxtpos = txtpos;
        txtpos++;
        if (*txtpos == 'T')
        {
          txtpos++;
          if (*txtpos == '(')
          {
            txtpos++;
            xp = get_value();
            if (*txtpos == ',')
            {
              txtpos++;
              yp = get_value();
              tc.setCursorPos(xp, yp);
              if (*txtpos != ')') k = 2;
              else {
                txtpos++;
                break;
              }
            }
            else k = 2;
          }
          else  txtpos = tmptxtpos; //wenn nach AT keine Klammer kommt kann es sich um die Funktion ATN(x) handeln

        }
        else {
          txtpos = tmptxtpos;
        }

      default:
        float e;

        e = get_value();                    //Zahl oder Variable lesen

        if (expression_error) k = 2;
        if (func_string_marker == true) {
          for (int i = 0; i < fstring; i++) {
            printmsg(tempstring, 0);
          }
          func_string_marker = false;
          string_marker = false;
          chr = false;
        }
        else if (string_marker == true)                  //String?
        {
          printmsg(tempstring, 0);
          func_string_marker = false;
          string_marker = false;
          chr = false;
        }
        else if (chr == true)
        {
          outchar(int(e));
          chr = false;                       //marker zurücksetzen
          string_marker = false;
        }
        else if (tab_marker == true)         //TAB und SPC-Funktion
        {
          tab_marker = false;                //marker zurücksetzen
        }
        else {
          printnum(e, Zahlenformat);         //Zahl
        }
    }


  }//while(!k)

  if (k == 2)
    return 1;                               //Fehler - wieder von vorn

  return 0;                                 // nächster Befehl
}

//#######################################################################################################################################
//--------------------------------------------- Variablen - Eingabe ---------------------------------------------------------------------
//#######################################################################################################################################

static float var_get(void)
{
  float value;
  float *var;
  //char *st;
  int tmp, stmp, i, var_pos, array_art ;
  char c;
  word arr_adr;

  array_art = 0;

  if (*txtpos < 'A' || *txtpos > 'Z')                                     //erster Variablenbuchstabe
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  var_pos = *txtpos - 'A';
  var = (float *)variables_begin + var_pos;
  stmp = (int) (*txtpos - 'A') * STR_LEN;                                //Strings nur als einbuchstabige Variablen erlaubt, deshalb Variablenadresse sichern
  txtpos++;
  if (*txtpos >= 'A' && *txtpos <= 'Z') {                            //zweiter Variablenbuchstabe
    tmp = (int) ((*txtpos - 'A' + 1) * 26);
    var = var + tmp;
    txtpos++;
  }

  //------------------------------------------------------------------------------------------------------------------------------------------------------------
  while (*txtpos >= 'A' && *txtpos <= 'Z') txtpos++;  //so sind auch lange Variablennamen möglich ->siehe auch expr4()
  //------------------------------------------------------------------------------------------------------------------------------------------------------------
  if (*txtpos == '(') {
    txtpos++;
    expression_error = 0;
    arr_adr = rw_array(var_pos, VAR_TBL);                                  //numerische Array? Adresse im Zahlen-Arrayfeld
    if (expression_error) return 1;                                        //Fehler? dann zurück
    array_art = 1;
  }
  else if (*txtpos == '$')
  { //String?
    txtpos++;

    if (*txtpos == '(') {                                                 //kommt eine Klammer vor, muss es sich um ein Array handeln
      txtpos++;
      expression_error = 0;
      arr_adr = rw_array(var_pos, STR_TBL);                               //String_array?, Adresse im String-Arrayfeld
      if (expression_error) return 1;                                     //Fehler? dann zurück
      array_art = 2;
    }

    if (Test_char('=')) return 1;                                         //Test auf '='

    get_value();                                                         //Stringvariable?, Zeichenkette oder numerische Variable

    i = 0;
    while (1)
    {
      c = tempstring[i];
      if (c == '\0') {
        if (array_art == 2) {
          SPI_RAM_write8(arr_adr + i, '\0');
        }
        else {
          Stringtable[stmp + i] = '\0';                                   //Nullterminator setzen
        }
        break;
      }
      else {
        if (i < STR_LEN) {
          if (array_art == 2) {
            SPI_RAM_write8(arr_adr + i++, c);
          }
          else Stringtable[stmp + i++] = c;
        }
        else {
          if (array_art == 2) {
            SPI_RAM_write8(arr_adr + i, '\0');
            break;
          }
          else {
            Stringtable[stmp + i] = '\0';
            break;
          }
        }
      }
    }
    return 0;
  }// Ende String?

  if (Test_char('='))
    return 1;


  spaces();
  value = get_value();
  if (expression_error) return 1;//continue;

  // Check that we are at the end of the statement
  if (*txtpos != NL && *txtpos != ':')
  {
    syntaxerror(syntaxmsg);
    return 1;

  }

  if (array_art == 1) {
    byte* bytes = (byte*)&value;                            //float nach byte-array umwandeln
    SPI_RAM_write(arr_adr, bytes, 4);
    return 0;
  }

  *var = value;
  return 0;
}

float rw_array(int num, word table) {
  int x, y, z, xx, yy, zz;    //Dimensionswerte aus der Klammer lesen
  word vadresse, ort;
  byte p_data[8], len;        //Dimensionswerte im FRAM
  x = y = z = 0;

  x = get_value();            //erste Dimension
  if (*txtpos == ',') {
    txtpos++;
    y = get_value();          //eventuelle zweite Dimension
  }
  if (*txtpos == ',') {
    txtpos++;
    z = get_value();          //eventuelle dritte Dimension
  }
  if (Test_char(')')) {
    expression_error = 1;
    return 0;
  }

  ort = table + (num * 8);                                         //1=num.Var-Position der Array-Dimension in Array-Tabelle (A=0, B=1, C=2 usw.)
  if (table == VAR_TBL) len = sizeof (float);                        //Eintragslänge bei float 4
  else if (table == STR_TBL) len = STR_LEN;

  //Dimensionswerte aus dem FRAM lesen und mit Eingabe vergleichen
  spi_fram.read(ort, p_data, 6);
  //readBuffer(FRam_ADDR, ort, 6, p_data);

  vadresse = word(p_data[0], p_data[1]);                                //word(h, l) //vadresse = p_data[0] << 8; vadresse += p_data[1]; Variablen-Feldadresse - Adresse bei dem das Array-Feld beginnt
  xx = word (p_data[2], p_data[3]);                                     //1.Dimension
  yy = p_data[4];                                                       //2.Dimension
  zz = p_data[5];                                                       //3.Dimension

  if (x > xx || y > yy || z > zz) {                                     //Dimensionsfehler entdeckt
    expression_error = 1;
    syntaxerror(dimmsg);
    return 0;
  }
  ort = vadresse + ((xx + 1) * y * len) + (x * len) + ((xx + 1) * (yy + 1) * len * z);
  return ort;
}

//--------------------------------------------- Variablen löschen ---------------------------------------------------------------------------------

void clear_var()
{
  float w_ert = 0;
  byte* bytes = (byte*)&w_ert;

  memset(tempstring, '\0', sizeof(tempstring));

  for (int i = 0; i < VAR_SIZE * 26 * 27; i++)                    //Variablen löschen
  {
    variables_begin[i] = 0;
  }
  //for (int i = 0; i < 26; i++)                              //Strings löschen
  //{
  memset(Stringtable, '\0', sizeof(Stringtable));
  //}
  for (int i = 0x7e00; i < 0x7fff; i += 4) SPI_RAM_write(i, bytes, 4);   //Array-Tabelle löschen
  Var_Neu_Platz = 0;                                              //Array-Zeiger zurücksetzen
  num_of_datalines = 0;                                           //Datazeilenzähler zurücksetzen
  del_window();                                                   //Fensterparameter löschen
  return;
}

//#######################################################################################################################################
//--------------------------------------------- NEW-Befehl ------------------------------------------------------------------------------
//#######################################################################################################################################
void cmd_new(void) {
  long w_ert = 0;
  byte* bytes = (byte*)&w_ert;
  program_start = program;
  program_end = program_start;
  sp = program + sizeof(program);                                     // Needed for printnum
  stack_limit = program + sizeof(program) - STACK_SIZE;               // - ARRAY_SIZE;
  variables_begin = stack_limit - (26 * 27 * VAR_SIZE) ;              //26*27 (2)Buchstaben als Variablen
  memset(program, 0, 1000);                                           //die ersten 1000 Bytes des Speichers löschen
  clear_var();                                                        //Variablen und Array-Tabelle löschen
  for (int i = 0x0; i < 0x7fff; i += 4) SPI_RAM_write(i, bytes, 4);   //Array-Bereich löschen
  del_window();                                                       //Fensterparameter löschen
  Frame_nr = 0;                                                       //Hauptfenster setzen
  print_info();                                                       //Start-Bildschirm anzeigen
}

//#######################################################################################################################################
//--------------------------------------------- THEME - Befehl --------------------------------------------------------------------------
//#######################################################################################################################################

static int set_theme(int value)
{
  //int fn;

  switch (value)
  {
    case 0: //C64
      Vordergrund = 43;
      Hintergrund = 18;
      set_font(0);
      break;

    case 1: //C128
      Vordergrund = 12;
      Hintergrund = 21;
      set_font(0);
      break;

    case 2: //CPC
      Vordergrund = 60;
      Hintergrund = 1;
      set_font(19);
      break;

    case 3: //ATARI-800
      Vordergrund = 26;
      Hintergrund = 5;
      set_font(21);
      break;

    case 4: //ZX-SPECTRUM
      Vordergrund = 0;
      Hintergrund = 42;
      set_font(22);
      break;

    case 5: //KC87
      Vordergrund = 63;
      Hintergrund = 0;
      set_font(22);
      break;

    case 6: //KC 85
      Vordergrund = 63;
      Hintergrund = 2;
      set_font(19);
      break;

    case 7: //VIC20
      Vordergrund = 2;
      Hintergrund = 63;
      set_font(18);
      break;

    case 8: //TRS80
      Vordergrund = 24;
      Hintergrund = 0;
      set_font(18);
      break;

    case 9: //ESP32+
      Vordergrund = 57;
      Hintergrund = 16;
      set_font(7);
      break;

    case 10:    //LCD
      Vordergrund = 20;
      Hintergrund = 41;
      set_font(3);
      break;

    default:
      Vordergrund = user_vcolor;
      Hintergrund = user_bcolor;
      fontsatz = user_font;
      set_font(fontsatz);
      value = 11;
      break;
  }
  fbcolor(Vordergrund, Hintergrund);
  tc.setCursorPos(1, 1);
  GFX.clear();
  Theme_state = value;
  return 0;
}

//--------------------------------------------- Unterprogramm - Überprüfung auf Zeichen -----------------------------------------------------------

static int Test_char(char az)
{

  // check for char az
  if (*txtpos != az)
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  txtpos++;
  spaces();
  return 0;
}

//#######################################################################################################################################
//--------------------------------------------- POKE - Befehl ---------------------------------------------------------------------------
//#######################################################################################################################################

static int poke(int fn)             //POKE WAS,ADRESSE,WERT
{
  unsigned long address, wert;
  float w_ert;
  int was, weite;
  byte value, p_data[2];

  was = abs(get_value());                                       //Speicherort 0..2 ->0-RAM, 1-FRAM, 2-EEPROM
  if (was > 2) was = 2;
  if (Test_char(',')) return 1;

  address = abs(get_value());                                  //Speicheradresse
  if (Test_char(',')) return 1;

  if (fn == KW_FPOKE) {
    w_ert = get_value();                                        //floatwert poken
    goto next;
  }

  wert = abs(get_value());                                     //zu speichernder Wert
  if (*txtpos != ',') weite = 1;                               //wenn kein Komma,dann Byte
  else
    weite = abs(get_value());                                  //byte, word ->1,2

next:
  // Testen auf Zeilenende oder ':'
  if (*txtpos != NL && *txtpos != ':')
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  //---------------------------- RAM -----------------------------------------------
  if (was == 0) {
    if (fn == KW_POKE)  program[address] = byte(wert);            //RAM  Byte
    else if (fn == KW_DOKE)
    {
      program[address] = highByte(wert);                          //RAM  Word
      program[address + 1] = lowByte(wert);                       //RAM
    }
    else if (fn == KW_FPOKE) {
      byte* bytes = (byte*)&w_ert;
      program[address] = byte(bytes[0]);
      program[address + 1] = byte(bytes[1]);
      program[address + 2] = byte(bytes[2]);
      program[address + 3] = byte(bytes[3]);
    }
    return 0;
  }
  //---------------------------- FRAM ---------------------------------------------
  else if (was == 1) {
    if (fn == KW_POKE)  SPI_RAM_write8(FRAM_OFFSET + address, byte(wert));   //FRAM Byte
    else if (fn == KW_DOKE)
    {
      p_data[0] = highByte(wert);
      p_data[1] = lowByte(wert);
      SPI_RAM_write(FRAM_OFFSET + address, p_data, 2);                       //FRAM Word
    }
    else if (fn == KW_FPOKE) {
      byte* bytes = (byte*)&w_ert;
      SPI_RAM_write(FRAM_OFFSET + address, bytes, 4);                        //FRAM float
    }
    return 0;
  }
  //----------------------------- EEPROM -------------------------------------------
  else if (fn == KW_POKE)  writeEEPROM(EEprom_ADDR, address, byte(wert));   //EEPROM Byte
  else if (fn == KW_DOKE)
  {
    p_data[0] = highByte(wert);
    p_data[1] = lowByte(wert);
    WriteBuffer(EEprom_ADDR, address, 2, p_data);                           //EEPROM Word
  }
  else if (fn == KW_FPOKE) {                                                //EEPROM float
    byte* bytes = (byte*)&w_ert;
    WriteBuffer(EEprom_ADDR, address, 4, bytes);
  }
  return 0;

}

//#######################################################################################################################################
//--------------------------------------------- STYLE - Befehl --------------------------------------------------------------------------
//#######################################################################################################################################

static int set_style(void)
{
  int st;

again:
  expression_error = 0;
  st = abs(int(get_value()));         //nur ganze Zahlen
  if (expression_error) return 1;

  switch (st) {
    case 0: Terminal.print("\e[0m");                          //normal
      break;
    case 1: Terminal.print("\e[1m");                          //bold
      break;
    case 2: Terminal.print("\e[3m");                          //italic
      break;
    case 3: Terminal.print("\e[4m");                          //underline
      break;
    case 4: Terminal.print("\e[7m");                          //invers
      break;
    case 5: Terminal.print("\e[5m");                          //blink
      break;
    case 6: Terminal.print("\e#6");                           //float width
      break;

    default:
      break;
  }
  if (spaces() == ',')                                        //weitere Style-Parameter? dann wieder von vorn
  {
    txtpos++;
    goto again;
  }
  return 0;
}

//#######################################################################################################################################
//--------------------------------------------- SCROLL - Befehl -------------------------------------------------------------------------
//#######################################################################################################################################

static int scroll_xy(void)
{
  short int par[6];

  expression_error = 0;
  par[0] = get_value();
  if (expression_error) return 1;
  if (Test_char(',')) return 1;

  expression_error = 0;
  par[1] = get_value();
  if (expression_error) return 1;

  // Check that we are at the end of the statement
  if (*txtpos != NL && *txtpos != ':') return 1;

  GFX.scroll(par[0], par[1]);

  return 0;
}


//#######################################################################################################################################
//--------------------------------------------- Lines/CIRCLE/RECT - Befehl --------------------------------------------------------------
//#######################################################################################################################################

static int line_rec_circ(int circ_or_rect, int param)
{
  //#################### Linie, Rechteck oder Kreis zeichnen #################

  short int i, par[6];
  i = 0;

  while (i < param) {                 //3 o.4 Parameter eingeben
    expression_error = 0;
    par[i] = get_value();
    if (expression_error) return 1;

    // check for a comma
    if (Test_char(',')) return 1;
    i++;
  }

  expression_error = 0;
  par[param] = get_value();            //param bei circ und rect=4 -> 5.Parameter , lines=3 -> 4.Parameter
  if (expression_error) return 1;

  // Check that we are at the end of the statement
  if (*txtpos != NL && *txtpos != ':') return 1;

  //----------------- abhängig vom Parameter circ_or_rect wird zwischen Circle,Rect und Lines ausgewählt -------
  switch (circ_or_rect) {
    case 1:
      if (par[4] == 0) GFX.drawEllipse(par[0], par[1], par[2], par[3]);       //Circle circ x,y,xx,yy,fill=0
      else {
        bcolor(Hintergrund);
        GFX.fillEllipse(par[0], par[1], par[2], par[3]);                      //Circle circ x,y,xx,yy,fill=1
      }
      break;
    case 2:
      if (par[4] == 0) GFX.drawRectangle(par[0], par[1], par[2], par[3]);     //Rectangle rect x,y,xx,yy,fill=0
      else {
        bcolor(Hintergrund);
        GFX.fillRectangle(par[0], par[1], par[2], par[3]);                    //Rectangle rect x,y,xx,yy,fill=1
      }
      break;                                                                   //               0 1 2  3  4
    case 3:                                                                   //Roundrect round x,y,xx,yy,r,fill
      
      GFX.drawLine(par[0] + 1.6 * par[4], par[1] - 1, par[0] + par[2] - 1.6 * par[4], par[1] - 1);
      GFX.drawLine(par[0] + 1.6 * par[4], par[1] + par[3], par[0] + par[2] - 1.6 * par[4], par[1] + par[3]);
      GFX.drawLine(par[0] - 1, par[1] + par[4], par[0] - 1, par[1] + par[3] - par[4]);
      GFX.drawLine(par[0] + par[2], par[1] + par[4], par[0] + par[2], par[1] + par[3] - par[4]);
      for (int i = 0; i <= 25; i++) {
        GFX.setPixel(par[0] + par[2] - par[4] * 1.6 * (1 - cos(i / 25.*3.1415 / 2.)), par[1] + par[4] * (1 - sin(i / 25.*3.1415 / 2.)));
        GFX.setPixel(par[0] + par[4] * 1.6 * (1 - cos(i / 25.*3.1415 / 2.)), par[1] + par[4] * (1 - sin(i / 25.*3.1415 / 2.)));
        GFX.setPixel(par[0] + par[2] - par[4] * 1.6 * (1 - cos(i / 25.*3.1415 / 2.)), par[1] + par[3] - par[4] * (1 - sin(i / 25.*3.1415 / 2.)));
        GFX.setPixel(par[0] + par[4] * 1.6 * (1 - cos(i / 25.*3.1415 / 2.)), par[1] + par[3] - par[4] * (1 - sin(i / 25.*3.1415 / 2.)));
      }
      if(par[5]==1){
        bcolor(Vordergrund);
        GFX.fillRectangle(par[0] + (1.6 * par[4])-1, par[1], par[0] + par[2] - (1.6 * par[4])+1, par[1] + par[3]);
        GFX.fillRectangle(par[0] , par[1] + par[4], par[0] + par[2], par[1] + par[3] - par[4]);
        GFX.fillEllipse(par[0]+par[4], par[1] + par[4], par[4]*1.9, par[4]*1.6);
        bcolor(Hintergrund);
        //fill_area(par[0]+(par[2]/2),par[1]); 
      }
      break;
    default:
      GFX.drawLine(par[0], par[1], par[2], par[3]);                           //Line line x,y,xx,yy
      break;
  }
  //bcolor(Hintergrund);
  return 0;
}

//#######################################################################################################################################
//------------------------------------------------------------- DRAW-Befehl -------------------------------------------------------------
//#######################################################################################################################################
static int draws(void)
{
  short int p[3];
  // Work out where to put it
  expression_error = 0;
  for (int i = 0; i < 3; i++) {
    p[i] = get_value();
    if (i < 2) {
      if (Test_char(',')) return 1;
    }
  }

  if (p[2] == 0) GFX.moveTo(p[0], p[1]);
  else GFX.lineTo(p[0], p[1]);

  return 0;
}

//#######################################################################################################################################
//----------------------------------------------------- Sprite-Befehl -------------------------------------------------------------------
//#######################################################################################################################################
static int sprite(char cm) {
  short int cnt;
  String abuf;
  short int nr, par[8];

  if (Test_char(',')) return 1;
  switch (cm) {
    case 'C':
      expression_error = 0;
      cnt = get_value();
      if (expression_error) return 1;
      if (cnt > 16)                                        //16 Sprites sind erlaubt
      {
        syntaxerror(valmsg);                               // falscher wert
        return 1;
      }

      break;
    case 'D':
      expression_error = 0;
      nr = get_value();                                  //Sprite-Nr
      if (expression_error) return 1;
      if (Test_char(',')) return 1;
      for (int i = 1; i < 4; i++)
      {
        expression_error = 0;
        par[i] = get_value();                             //Frame
        if (expression_error) return 1;
        if (Test_char(',')) return 1;
      }
      //par[3]=farbe des Sprites r,g,b
      par[4] = (bitRead(par[3], 5) * 2 + bitRead(par[3], 4)) * 64;
      par[5] = (bitRead(par[3], 3) * 2 + bitRead(par[3], 2)) * 64;
      par[6] = (bitRead(par[3], 1) * 2 + bitRead(par[3], 0)) * 64;

      get_value();                                      //String in tempstring;
      break;

    case 'S':
      expression_error = 0;
      nr = get_value();                                 //Sprite-Nr
      if (expression_error) return 1;
      if (Test_char(',')) return 1;
      String_quoted_read();
      if (Test_char(',')) return 1;
      for (int i = 1; i < 3; i++)
      {
        expression_error = 0;
        par[i] = get_value();
        if (expression_error) return 1;
        if (i < 2) {
          if (Test_char(',')) return 1;
        }
      }
      break;
  }
  if (Test_char(')')) return 1;                        //Test auf Klammer
  // Check that we are at the end of the statement
  if (*txtpos != NL && *txtpos != ':' )  return 1;

  switch (cm) {
    case 'C':
      //myString = "\e_GSPRITECOUNT" + String(cnt, DEC) + "$";
      Terminal.print("\e_GSPRITECOUNT" + String(cnt, DEC) + "$");
      //Terminal.print(String(cnt, DEC) + "$");
      break;
    case 'D':
      abuf = String(tempstring);                          //Stringkopie für D-Befehl nach abuf kopieren
      abuf.trim();
      Terminal.print("\e_GSPRITEDEF" + String(nr, DEC) + ";" + String(par[1], DEC) + ";" + String(par[2], DEC) + ";M;" + String(par[4], DEC) + ";" + String(par[5], DEC) + ";" + String(par[6], DEC) + ";" + abuf + "$"); //sprtd(n,"H;W;R;G;B;Data")
      //Terminal.print(abuf);
      break;
    case 'S':
      Terminal.print("\e_GSPRITESET" + String(nr, DEC) + ";" + tempstring + ";0;" + String(par[1], DEC) + ";" + String(par[2], DEC) + "$"); //sprts(n,"V..H;Frame;x;y");
      //Terminal.print(String(nr, DEC) + ";" + tempstring + ";0;" + String(par[1], DEC) + ";" + String(par[2], DEC) + "$"); //sprts(n,"V..H;Frame;x;y");
      break;
  }

  return 0;
}

//#######################################################################################################################################
//----------------------------------------------------- SND-Befehl ----------------------------------------------------------------------
//#######################################################################################################################################
static int Sound(void) {
  short int  i, par[7];
  char c;
  //if (Test_char('_')) return 1;                           //Klammer-auf vorhanden?
  //c = *txtpos;
  //expression_error = 0;
  //txtpos++;
  /*
    switch (c) {
    case 'N':
      for (int i = 1; i < 5; i++)
      {
        expression_error = 0;
        par[i] = int(get_value());
        if (expression_error) return 1;
        if (i < 4) {
          if (Test_char(',')) return 1;
        }
      }
      midiNoteOn(par[1], par[2], par[3]);
      delay(par[4]);
      midiNoteOff(par[1], par[2], par[3]);
      break;

    case 'C':
      for (int i = 1; i < 4; i++)
      {
        expression_error = 0;
        par[i] = int(get_value());
        if (expression_error) return 1;
        if (i < 3) {
          if (Test_char(',')) return 1;
        }
      }
      midiChannelMessage(par[1], par[2], par[3]);
      break;

    case 'I':
      expression_error = 0;
      par[1] = int(get_value());
      if (Test_char(',')) return 1;
      par[2] = int(get_value());
      midiSetInstrument(par[1], par[2]);
      break;

    case 'R':
      VS1053_MIDI.write(0xff);
      break;

    default:
      break;

    }
  */
  //Sound(Chan,note,vel,duration)
  /*
       Sequence:
      SND(waveform,frequency,duration,volume)

      Parameters:
      waveform:
          "0" = SINE
          "1" = SQUARE
          "2" = TRIANGLE
          "3" = SAWTOOTH
          "4" = NOISE
          "5" = VIC NOISE
      frequency:
          frequency in Hertz
      duration:
          duration in milliseconds
      volume:
          volume (min is 0, max is 127)  snd(waveform,freq,duration,vol)
  */
  if (Test_char('(')) return 1;                           //Klammer-auf vorhanden?

  for (i = 1; i < 5; i++)
  {
    expression_error = 0;
    par[i] = get_value();
    if (expression_error) return 1;
    if (i < 4) {
      if (*txtpos != ',') return 1;
      txtpos++;
    }
  }



  if (par[1] > 5)   par[1] = 5;
  if (par[4] > 127) par[4] = 127;
  par[2] = NoteToFreq(par[2]);
  //Terminal.write("\e_S0;800;1000;100$");
  Terminal.print("\e_S" + String(par[1], DEC) + ";" + String(par[2], DEC) + ";" + String(par[3], DEC) + ";" + String(par[4], DEC) + "$");
  if (Test_char(')')) return 1;                           //Klammer-zu vorhanden?

  // Check that we are at the end of the statement
  if (*txtpos != NL && *txtpos != ':' )  return 1;
  return 0;

}



//#######################################################################################################################################
//--------------------------------------------- PSET - Befehl ---------------------------------------------------------------------------
//#######################################################################################################################################

static int pset(void)
{

  short int xp, yp, pc;
  // Work out where to put it
  expression_error = 0;
  xp = get_value();
  if (expression_error) return 1;

  // check for a comma
  if (Test_char(',')) return 1;

  // Now get the value to assign
  expression_error = 0;
  yp = get_value();
  if (expression_error) return 1;


  if (*txtpos == ',') {                  //optional Angabe der Farbe
    txtpos++;
    expression_error = 0;
    pc = get_value();
    if (expression_error) return 1;

    fcolor(pc);
  }
  // Check that we are at the end of the statement
  if (*txtpos != NL && *txtpos != ':') return 1;

  GFX.setPixel(xp, yp);

  return 0;
}

//#######################################################################################################################################
//--------------------------------------------- COL - Befehl ----------------------------------------------------------------------------
//#######################################################################################################################################

static int color(void)
{

  short int fc, bc;
  // Work out where to put it
  expression_error = 0;
  fc = get_value();
  if (expression_error) return 1;

  // check for a comma
  if (Test_char(',')) return 1;

  // Now get the value to assign
  expression_error = 0;
  bc = get_value();
  if (expression_error) return 1;

  // Check that we are at the end of the statement
  if (*txtpos != NL && *txtpos != ':') return 1;

  Vordergrund = fc;
  Hintergrund = bc;
  fbcolor(fc, bc);
  if (!Frame_nr) {
    Frame_vcol[0] = Vordergrund;
    Frame_hcol[0] = Hintergrund;
  }
  else {
    Frame_vcol[Frame_nr] = Vordergrund;
    Frame_hcol[Frame_nr] = Hintergrund;
  }
  return 0;
}

//--------------------------------------------- Unterrogramm - Farben setzen ----------------------------------------------------------------------

void fbcolor(int fc, int bc)
{
  if (!Frame_nr) {
    Frame_vcol[0] = fc;
    Frame_hcol[0] = bc;
  }

  fcolor(fc);
  bcolor(bc);
}

void fcolor(int fc) {
  GFX.setPenColor((bitRead(fc, 5) * 2 + bitRead(fc, 4)) * 64, (bitRead(fc, 3) * 2 + bitRead(fc, 2)) * 64, (bitRead(fc, 1) * 2 + bitRead(fc, 0)) * 64);
}

void bcolor(int bc) {
  GFX.setBrushColor((bitRead(bc, 5) * 2 + bitRead(bc, 4)) * 64, (bitRead(bc, 3) * 2 + bitRead(bc, 2)) * 64, (bitRead(bc, 1) * 2 + bitRead(bc, 0)) * 64);
}


//#######################################################################################################################################
//--------------------------------------------- PEN - Befehl --------------------------------------------------------------------------------------
//#######################################################################################################################################

static int set_pen(void)
{

  short int pc, pw;

  expression_error = 0;                         //Pen-Farbe
  pc = int(get_value());

  if (expression_error) return 1;

  if (*txtpos == ',')                             //wurde die PEN-Weite angegeben?
  {
    txtpos++;
    expression_error = 0;
    pw = int(get_value());
    if (expression_error) return 1;

    GFX.setPenWidth(pw);                           //Pen-Weite
  }
  // Check that we are at the end of the statement
  else if (*txtpos != NL && *txtpos != ':') return 1;

  fcolor(pc);
  return 0;

}



//#######################################################################################################################################
//---------------------------------------------- DEF_FN Befehl --------------------------------------------------------------------------
//#######################################################################################################################################

static int def_func(void)
{
  int fname, fnpos, fault, i;

  if (*txtpos < 'A' || *txtpos > 'Z') return 1;                           //Funktionsname

  fname = (*txtpos - 'A');                                                //Position im Funktionsspeicher
  fnpos = fname * 5;
  txtpos++;

  if (Test_char('(')) return 1;                                           //Klammer Auf vorhanden?

  fault = 0;
  i = 0;

  while (1) {                                                             //Eingabeschleife für bis zu vier Operatoren
    if (*txtpos < 'A' || *txtpos > 'Z')                                   //Überprüfung auf gültiges Zeichen
    {
      syntaxerror(syntaxmsg);
      fault = 1;
      break;
    }
    Fnoperator[fnpos + i] = *txtpos - 'A';
    i++;
    txtpos++;

    if (*txtpos == ',') txtpos++;                              //Komma vorhanden?

    else if (*txtpos == ')') break;                            //oder Klammer ZU

    if ( i > 3) {                                              //mehr als 3 Kommas?
      fault = 1;
      syntaxerror(illegalmsg);
      break;
    }
  }
  Fnoperator[fnpos + 4] = i;                                  //Anzahl Operatoren speichern
  if (fault) return fault;                                    //wenn es bis hier Fehler gab, dann zurück

  txtpos++;
  if (Test_char('=')) return 1;                                // = vorhanden?
  if (Test_char('[')) return 1;                                // [ vorhanden?

  i = 0;
  while (*txtpos != ']') {                                    //Funktionsstring in Funktionsspeicher einlesen
    Fntable[fname][i++] = *txtpos++;
  }
  Fntable[fname][i] = NL;
  Fntable[fname][i + 1] = 0;
  txtpos++;
  return 0;
}


//--------------------------------------------- Unterprogramm - String in Anführungszeichen ausgeben ----------------------------------------------

static char print_quoted_string(void)
{
  int i = 0;
  char delim = *txtpos;
  if (delim != '"' && delim != '\'')
    return 1;
  txtpos++;

  // Check we have a closing delimiter
  while (txtpos[i] != delim)
  {
    if (txtpos[i] == NL) {
      return 1;
    }
    i++;
  }

  // Print the characters
  while (*txtpos != delim)
  {
    outchar(*txtpos);
    txtpos++;
  }
  txtpos++; // Skip over the last delimiter

  return 0;
}

//#######################################################################################################################################
//--------------------------------------------- PULSE - Befehl --------------------------------------------------------------------------
//#######################################################################################################################################

static int set_pulse(void)
{
  int p, x, y, pl;
  if (Test_char('(')) return 1;

  expression_error = 0;
  p = get_value();             //IO-Port
  if (expression_error) return 1;

  if ((p == 2) || (p == 12) || (p == 26) || (p == 27))
  {
    ledcDetachPin(p);     //PWM freimachen falls benutzt
    pinMode(p, OUTPUT);
  }
  else
  {
    syntaxerror(portmsg);
    return 1;
  }
  // check for a comma
  if (Test_char(',')) return 1;

  // Now get the value to assign
  expression_error = 0;
  pl = get_value();                     //Anzahl-Pulse
  if (expression_error) return 1;

  if (Test_char(',')) return 1;

  expression_error = 0;
  x = get_value();                      //Pause1-Zeit
  if (expression_error) return 1;

  if (Test_char(',')) return 1;

  expression_error = 0;
  y = get_value();                     //Pause2-Zeit
  if (expression_error) return 1;

  if (Test_char(')')) return 1;

  if (*txtpos != NL && *txtpos != ':') return 1;

  for (int i = 0; i < pl; i++) {                    //Anzahl pl-Impulse
    digitalWrite(p, HIGH);                      //setze Port - High
    delay(x);                                   //Pause x
    digitalWrite(p, LOW);                       //Low
    delay(y);
  }

  return 0;
}

//#######################################################################################################################################
//--------------------------------------------- DOUT-Befehl -----------------------------------------------------------------------------
//#######################################################################################################################################
static int set_port(void)
{
  int p, x;

  if (Test_char('(')) return 1;

  expression_error = 0;
  p = get_value();
  if (expression_error) return 1;

  if ((p == 2) || (p == 12) || (p == 26) || (p == 27))
  {
    ledcDetachPin(p);     //PWM freimachen falls benutzt
    pinMode(p, OUTPUT);
  }
  else
  {
    syntaxerror(portmsg);
    return 1;
  }
  // check for a comma
  if (Test_char(',')) return 1;

  // Now get the value to assign
  expression_error = 0;
  x = get_value();
  if (expression_error) return 1;

  if (Test_char(')')) return 1;

  if (*txtpos != NL && *txtpos != ':') return 1;

  if (x > 0) digitalWrite(p, HIGH);                //setze Port - alles ausser 0 ist High
  else digitalWrite(p, LOW);                       //sonst Low

  return 0;
}

//#######################################################################################################################################
//--------------------------------------------- PWM-Befehl ------------------------------------------------------------------------------
//#######################################################################################################################################

static int set_pwm(void)
{
  int p, x, chan;
  if (Test_char('(')) return 1;

  expression_error = 0;
  p = get_value();
  if (expression_error) return 1;

  if ((p == 2) || (p == 12) || (p == 26) || (p == 27)) //nur gültige Pins setzen
  {
    switch (p) {
      case 2:
        chan = 1;
        break;
      case 12:
        chan = 2;
        break;
      case 26:
        chan = 3;
        break;
      case 27:
        chan = 4;
        break;
    }
    ledcSetup(chan, 500, 8);
    ledcAttachPin(p, chan);
  }
  else
  {
    syntaxerror(portmsg);
    return 1;
  }
  // check for a comma
  if (Test_char(',')) return 1;

  // Now get the value to assign
  expression_error = 0;
  x = get_value();
  if (expression_error) return 1;

  if (Test_char(')')) return 1;

  if (*txtpos != NL && *txtpos != ':') return 1;

  ledcWrite(chan, x);                //PWM-Wert setzen (pwm-channel,wert)
  return 0;
}

//#######################################################################################################################################
//--------------------------------------------- CUR - Befehl ----------------------------------------------------------------------------
//#######################################################################################################################################

static int cursor_onoff(void)
{

  // Work out where to put it
  expression_error = 0;
  onoff = get_value();
  if (expression_error) return 1;

  if (onoff == 0) Terminal.enableCursor(false);
  else Terminal.enableCursor(true);
  return 0;

}

//--------------------------------------------- CLS mit Scrolleffekt ------------------------------------------------------------------------------
/*
  void cmd_cls(void) {
  int t = VGAController.getScreenHeight();

  for (int i = 0; i < (t+1); i++) {
    GFX.scroll(0, -1);
  }
  }
*/
//#######################################################################################################################################
//--------------------------------------------- POS - Befehl --------------------------------------------------------------------------------------
//#######################################################################################################################################

static int set_pos(void)
{
  int xp, yp;

  expression_error = 0;
  xp = get_value();
  if (expression_error) return 1;

  // check for a comma
  if (Test_char(',')) return 1;

  // Now get the value to assign
  expression_error = 0;
  yp = get_value();
  if (expression_error) return 1;

  if (*txtpos != NL && *txtpos != ':') return 1;

  tc.setCursorPos(xp, yp);

  return 0;
}


//----------------------------Unterprogramm - einzelnen Char oder Zeichenkette in Anführungszeichen lesen -----------------------------------------

static char String_quoted_read(void)
{
  char c;

  c = spaces();

  if (c != '"')
  {
    printmsg(syntaxmsg, 1);
    expression_error = 1;
    return 0;
  }
  txtpos++;

  tempstring[0] = 0;

  c = *txtpos;
  expression_error = 0;


  int i = 0;
  while (*txtpos >= char(32) && *txtpos <= char(255)) //gültige Zeichen von ASCII-Zeichen 32 - ASCII-Zeichen 126
  {
    if (*txtpos == '"') break; //Zeichenkettenende erreicht, dann raus
    if (*txtpos == NL) {      //Fehler, bei fehlenden Anführungszeichen
      expression_error = 1;
      break;
    }
    tempstring[i++] = *txtpos++; //Tempstring füllen
  }
  tempstring[i] = '\0';       //tempstring abschliessen

  txtpos++;                   //Anführungszeichen überspringen
  return c;
}

//--------------------------------------------- Unterprogramm - Zeilenabschluss -------------------------------------------------------------------
static void line_terminator(void)
{
  outchar(CR);
  outchar(NL);
}


//#######################################################################################################################################
//--------------------------------------------- FONT - Befehl -------------------------------------------------------------------------------------
//#######################################################################################################################################

void set_font(int fnt) {
  switch (fnt) {
    case 0: Terminal.loadFont(&fabgl::FONT_8x8);
      break;
    case 1: Terminal.loadFont(&fabgl::FONT_5x8);
      break;
    case 2: Terminal.loadFont(&fabgl::FONT_6x8);
      break;
    case 3: Terminal.loadFont(&fabgl::FONT_LCD_8x14);
      break;
    case 4: Terminal.loadFont(&fabgl::FONT_10x20); //(siehe Ordner Fonts)
      break;
    case 5: Terminal.loadFont(&fabgl::FONT_BLOCK_8x14); //(siehe Ordner Fonts)
      break;
    case 6: Terminal.loadFont(&fabgl::FONT_BROADWAY_8x14); //(siehe Ordner Fonts)
      break;
    case 7: Terminal.loadFont(&fabgl::FONT_OLDENGL_8x16); //(siehe Ordner Fonts)
      break;
    case 8: Terminal.loadFont(&fabgl::FONT_BIGSERIF_8x16); //(siehe Ordner Fonts)
      break;
    case 9: Terminal.loadFont(&fabgl::FONT_SANSERIF_8x14); //(siehe Ordner Fonts)
      break;
    case 10: Terminal.loadFont(&fabgl::FONT_COURIER_8x14); //(siehe Ordner Fonts)
      break;
    case 11: Terminal.loadFont(&fabgl::FONT_SLANT_8x14); //(siehe Ordner Fonts)
      break;
    case 12: Terminal.loadFont(&fabgl::FONT_WIGGLY_8x16); //(siehe Ordner Fonts)
      break;
    case 13: Terminal.loadFont(&fabgl::FONT_6x10); //(siehe Ordner Fonts)
      break;
    case 14: Terminal.loadFont(&fabgl::FONT_BIGSERIF_8x14); //(siehe Ordner Fonts)
      break;
    case 15: Terminal.loadFont(&fabgl::FONT_4x6); //(siehe Ordner Fonts)
      break;
    case 16: Terminal.loadFont(&fabgl::FONT_6x12); //(siehe Ordner Fonts)
      break;
    case 17: Terminal.loadFont(&fabgl::FONT_7x13); //(siehe Ordner Fonts)
      break;
    case 18: Terminal.loadFont(&fabgl::FONT_7x14); //(siehe Ordner Fonts)
      break;
    case 19: Terminal.loadFont(&fabgl::FONT_8x9); //(siehe Ordner Fonts)
      break;
    case 20: Terminal.loadFont(&fabgl::FONT_COMPUTER_8x14); //(siehe Ordner Fonts)
      break;
    case 21: Terminal.loadFont(&fabgl::FONT_SANSERIF_8x14); //(siehe Ordner Fonts)
      break;
    case 22: Terminal.loadFont(&fabgl::FONT_6x10); //(siehe Ordner Fonts)
      break;
    case 23: Terminal.loadFont(&fabgl::FONT_9x15); //(siehe Ordner Fonts)
      break;
    case 24: Terminal.loadFont(&fabgl::FONT_8x16); //(siehe Ordner Fonts)
      break;
    default: Terminal.loadFont(&fabgl::FONT_6x8);
      fnt = 25;
      break;
  }
  if (fnt != fontsatz)                 // nur speichern, wenn anderer Wert als bisher
  {
    fontsatz = fnt;
  }

}


//#######################################################################################################################################
//--------------------------------------------- Unterprogramm - Startbildchirm  -------------------------------------------------------------------
//#######################################################################################################################################

void print_info()
{ int c, d, e, f;
  String built;

#ifdef ILI9341                                     //beim ILI9341 ist x und y vertauscht, da die Grundausrichtung Hochkant ist (240x320)
int x_pos = VGAController.getScreenHeight() / x_char[fontsatz];
int y_pos = VGAController.getScreenWidth() / y_char[fontsatz];
#else
int y_pos = VGAController.getScreenHeight() / y_char[fontsatz];
int x_pos = VGAController.getScreenWidth() / x_char[fontsatz];
#endif

#ifdef Akkualarm_enabled
float g = 3.3 / 4095 * 4000;//analogRead(Batt_Pin);
g = g / 0.753865;                                 //(Umess/(R2/(R1+R2)) R1=3.327kohm R2=10.19kohm
int   h = 100 - ((4.2 - g) * 100);                //Akkuwert in Prozent
if (h > 100) h = 100;
#endif

  Terminal.enableCursor(false);
  GFX.clear();
  delay(100);
  fbcolor(Vordergrund, Hintergrund);


  fcolor(Vordergrund);
  //strcpy(tempstring, BuiltTime);
  //drawing_text(15, 278, 1);

  tc.setCursorPos((x_pos - 26) / 2, 1);
  Terminal.write("Basic32+ V");
  Terminal.write(BasicVersion);
  Terminal.write(" Zille-Soft");//\r\n");

#ifdef Akkualarm_enabled                                  //Akku in Prozent anzeigen
Terminal.write("  ");
Terminal.print(int(h), DEC);
Terminal.write("%");
#endif

  tc.setCursorPos((x_pos - 16) / 2 , 2);
  // memory free
  Terminal.print(int(variables_begin - program_end), DEC);
  printmsg(memorymsg, 1);
  tc.setCursorPos(1, 4);

  Terminal.write("Terminal-Size:");
  Terminal.print(x_pos);
  Terminal.write("x");
  Terminal.print(y_pos);

  tc.setCursorPos(1, 5);
  Terminal.write("ESP-Memory   :");
  Terminal.print(ESP.getFreeHeap());

  tc.setCursorPos(1, 6);
  Terminal.print("Fontset      :");
  Terminal.print(fontsatz);

  tc.setCursorPos(1, 7);
  Terminal.print("Theme        :");
  Terminal.print(Theme_state);
  Terminal.print("=");
  Terminal.print(Themes[Theme_state]);
  Terminal.enableCursor(true);
  tc.setCursorPos(1, 9);

  //Terminal.println(RAMEND-STACK_SIZE-(26 * 27 * VAR_SIZE));
}


//#######################################################################################################################################
//--------------------------------------------- Unterprogramm - Üerprüfung auf Abbruch-Taste (Ctrl-C) -----------------------------------
//#######################################################################################################################################

static char breakcheck(void)
{
  if (Terminal.available()) {
    return Terminal.read() == CTRLC;
  }
  return 0;
}

void break_program(void)
{
  printmsg(breakmsg, 1);
  if (current_line != NULL)
  {
    linenum = *((LINENUM *)(current_line));
    printnum(linenum, 0);
  }
  line_terminator();
  warmstart();
  return;
}

//#######################################################################################################################################
//--------------------------------------------- Unterprogramm Zeichen von Tastatur oder aus Datei lesen ---------------------------------
//#######################################################################################################################################

static int inchar()
{
  int v;
  char c;

  switch ( inStream ) {
    case ( kStreamFile ):

      v = fp.read();
      if ( v == NL ) v = CR; // file translate
      if ( !fp.available() ) {
        fp.close();
        goto inchar_loadfinish;

      }
      return v;

      break;

    case ( kStreamFram ):

      break;

    case ( kStreamTerminal ):

    default:

      while (1)
      {
        //if(Serial.available()){
        //  c=Serial.read();
        if (Terminal.available() ) {
          c = Terminal.read();          //Standard-Tasteneingabe

          switch (c) {

            case 0x03:       // ctrl+c        -> BREAK
              current_line = 0;
              sp = program + sizeof(program);
              break;

            default:

              break;
          }

          if (Graph_char && c != 13 && c != 32 && c != 0x7F) return c + 121; //alle Tasten außer Enter und Space und Backspace umwandeln in Grafik-chars
          return c;
        }//if(Terminal.available)

        if (break_marker) {                     //ESC-Abfrage -> Break
          break_marker = false;
          current_line = 0;
          sp = program + sizeof(program);
          return 0x03;
        }

      }//while

  }//switch (inStream)



inchar_loadfinish:
  inStream = kStreamTerminal;
  inhibitOutput = false;
  sd_ende();                                                //SD-Card unmount

  if ( autorun ) {
    autorun = false;                                        //Autostartmarker zurücksetzen
    triggerRun = true;                                      //Programm sofort starten
  }
  return NL; // trigger a prompt.

}

//#######################################################################################################################################
//--------------------------------------------- Unterprogramm Zeichen zum Bildschirm oder in Datei schreiben ----------------------------
//#######################################################################################################################################

static void outchar(char c)
{
  int x_pos, y_pos;

  if ( inhibitOutput ) return;

  if ( outStream == kStreamFile ) {
    // output to a file
    fp.write( c );
  }
  else {
    if (ser_marker && list_send) Serial1.write(c);       //User-Seriellschnittstelle

    else if (Frame_nr) {                                 //************************** im Fenster schreiben ******************

      x_pos = tc.getCursorCol();
      y_pos = tc.getCursorRow();

      if ((x_pos > (Frame_xx[Frame_nr] / x_char[fontsatz]) - 1)) {        //Zeilenende
        y_pos += 1;
        x_pos = Frame_curx[Frame_nr];

        if (y_pos > (Frame_yy[Frame_nr] / y_char[fontsatz]) - 1) {        //eine Zeile hochscrollen
          y_pos -= 1;
          x_pos = Frame_curx[Frame_nr];
          move_up(Frame_nr);
        }
        tc.setCursorPos(x_pos, y_pos);
      }
      if (c == CR) {
        y_pos = tc.getCursorRow();
        if (y_pos > (Frame_yy[Frame_nr] / y_char[fontsatz]) - 1) {
          y_pos -= 1;

          move_up(Frame_nr);

        }
        tc.setCursorPos(Frame_curx[Frame_nr], y_pos);
        return;
      }
    }                                                    //************************** im Fenster schreiben ******************
    Terminal.write(c);                                   //auf FabGl VGA-Terminal schreiben----------------------------------
    //Serial.write(c);
  }
}



//############################################# Dateioperationen auf der SD-Karte #######################################################
//--------------------------------------------- Unterprogramm SD-Karte initialisieren ---------------------------------------------------
//#######################################################################################################################################
static int initSD( void )
{
  int c;
  int adr, i;

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1
  SPI.setFrequency(20000000);
  if ( !SD.begin( kSD_CS, spiSD )) {                        //SD-Card starten
    // mount-fehler
    spiSD.end();                                            //unmount
    syntaxerror(sderrormsg);
    return kSD_Fail;
  }

  // file redirection flags
  outStream = kStreamTerminal;                              //Ein-und Ausgabe-Stream auf Terminal setzen
  inStream = kStreamTerminal;
  inhibitOutput = false;
  sd_pfad[0] = '/';                                         //setze Root-Verzeichnis
  sd_pfad[1] = 0;

  adr = 20;                                                 //ab Adresse 20 im EEPROM ist der User-Pfad abgelegt
  i = 0;
  if (EEPROM.read(19) == PATH_SET) {                        //Pfad im EEPROM gespeichert?
    while (1) {
      c = EEPROM.read(adr++);
      sd_pfad[i++] = char(c);
      if (c == 0) break;
    }
  }


  if ( !SD.open(String(sd_pfad)))                          //Überprüfung, ob Pfad gültig
  {
    printmsg(dirnotfound, 1);
    sd_pfad[0] = '/';                                      //Verzeichnis ungültig->Root-Verzeichnis
    sd_pfad[1] = 0;
    sd_ende();                                             //SD-Card unmount
    return 1;
  }

  printmsg("SD-Card OK", 1);
  sd_ende();                                                 //unmount
  return kSD_OK;
}

//#######################################################################################################################################
//--------------------------------------------- SPI-Bus umschalten ----------------------------------------------------------------------
//#######################################################################################################################################

void sd_ende(void) {
  spiSD.end();                                              //SD-Card unmount
  spi_fram.begin(3);                                        //FRAM aktivieren

}


//#######################################################################################################################################
//--------------------------------------------- LOAD - Befehl ---------------------------------------------------------------------------
//#######################################################################################################################################

static int load_file(void)
{

  // Programmspeicher löschen
  program_end = program_start;

  // lade BAS-Datei in den Speicher

  expression_error = 0;
  get_value();                                              //in tempstring steht der Dateiname

  if (expression_error) return expression_error;

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

  if ( !SD.exists(String(sd_pfad) + String(tempstring)))    //Datei vorhanden?
  {
    syntaxerror(sdfilemsg);                                 //Datei nicht vorhanden -> Fehlerausgabe
    sd_ende();
    expression_error = 1;
    return expression_error;
  }
  else {
    fp = SD.open(String(sd_pfad) + String(tempstring));     //Datei zum Laden öffnen
    inStream = kStreamFile;
    inhibitOutput = true;
  }

  warmstart();
  return expression_error;
}

//#######################################################################################################################################
//--------------------------------------------- SAVE - Befehl ---------------------------------------------------------------------------
//#######################################################################################################################################

static int save_file()
{

  char c;

  expression_error = 0;
  get_value();                                                      //in tempstring steht der Dateiname

  if (expression_error) return 1;

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);

  // remove the old file if it exists
  if ( SD.exists( String(sd_pfad) + String(tempstring))) {          //Datei existiert schon, überschreiben?
    printmsg("File exist, overwrite? (y/n)", 0);
    while (1)
    {
      c = wait_key(false);                                               //Ja/Nein?
      if (c == 'y' || c == 'n')
        break;
    }
    if (c == 'y') {
      SD.remove( String(sd_pfad) + String(tempstring));             //ja, Datei löschen
      outchar(c);
    }
    else
    {
      outchar(c);                                                   //nein gedrückt, Abbruch
      line_terminator();
      sd_ende();                                                    //SD-Card unmount
      warmstart();
      return 0;
    }

  }

  // open the file, switch over to file output
  fp = SD.open( String(sd_pfad) + String(tempstring), FILE_WRITE);  //Datei wird zum Schreiben geöffnet
  if (!fp) {                                                        //Fehler?
    printmsg("Open File-Error!", 1);
  }
  outStream = kStreamFile;

  // copied from "List"
  list_line = findline();
  while (list_line != program_end)                                  //Zeile für Zeile des Programms in die Datei schreiben
    printline();

  outStream = kStreamTerminal;                                      // zurück zum standard output, Datei schließen

  fp.close();

  line_terminator();
  sd_ende();                                                        //SD-Card unmount
  warmstart();
  return 0;

}


//#######################################################################################################################################
//---------------------------------------- Save ohne Parameter speichert das Programm ab 0x7000 im FRAM ---------------------------------
//#######################################################################################################################################
static int save_ram(void) {
  word n_bytes;
  long adress = load_adress;

  n_bytes = User_Ram - int(variables_begin - program_end);

  if (n_bytes < 10) {
    syntaxerror(no_prg_msg);
    return 0;
  }
  //------ Kennung f. Programm im FRAM ------
  SPI_RAM_write8(adress++, 'B');
  SPI_RAM_write8(adress++, 'S');

  SPI_RAM_write8(adress++, lowByte(n_bytes));           //Anzahl zu speichernde Programm-Bytes schreiben
  SPI_RAM_write8(adress++, highByte(n_bytes));
  for (int i = 0; i < n_bytes; i++) {                   //Arbeitsspeicher in FRAM ablegen
    SPI_RAM_write8(adress++, program[i]);
  }
  return 0;
}


//#######################################################################################################################################
//----------------------------------------------- Load ohne Parameter lädt das Programm aus dem FRAM ab 0x7000 --------------------------
//#######################################################################################################################################
static int load_ram(void) {
  word n_bytes, i;
  long adress = load_adress;
  byte a, b;
  program_start = program;                                             //programmstart zurücksetzen
  program_end = program_start;
  memset(program, 0, 0x8000);                                          //die ersten 32kb des Arbeitsspeichers löschen

  a = spi_fram.read8(adress++);
  b = spi_fram.read8(adress++);
  //------ Kennung f. Programm im FRAM ------
  if (a == 'B' && b == 'S') {

    n_bytes = spi_fram.read8(adress++);                                //Anzahl der zu lesenden Bytes
    n_bytes = n_bytes + (spi_fram.read8(adress++) << 8);
    for (i = 0; i < n_bytes; i++) {                                   //Programm in Arbeitsspeicher schreiben
      program[i] = spi_fram.read8(adress++);
      program_end++;
    }

    warmstart();
    return 0;

  }
  else {
    syntaxerror(no_prg_msg);                                          //kein Programm im FRAM
    return 0;
  }
}

//#######################################################################################################################################
//--------------------------------------------- DEL - Befehl ----------------------------------------------------------------------------
//#######################################################################################################################################

static int cmd_delFiles(void)
{

  char c;
  int n = 0;

  // eingabe Dateiname
  expression_error = 0;
  get_value();

  if (expression_error) return 1;

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

  // Datei löschen, wenn sie existiert
  if ( SD.exists(String(sd_pfad) + String(tempstring))) {
    printmsg("delete File? (y/n)", 0);
    while (1)
    {
      c = wait_key(false);
      if (c == 'y' || c == 'n')
        break;
    }
    if (c == 'y') {
      SD.remove( String(sd_pfad) + String(tempstring));
      outchar(c);
    }
  }
  else n = 1;
  line_terminator();
  fp.close();
  sd_ende();                                             //SD-Card unmount

  warmstart();
  return n;

}

//#######################################################################################################################################
//--------------------------------------------- CHDIR - Befehl --------------------------------------------------------------------------
//#######################################################################################################################################

void cmd_chdir(void)
{
  int i = 0;
  if (*txtpos == '"') *txtpos++;
  else
  {
    printmsg(syntaxmsg, 1);
    return;
  }
  if ( *txtpos == '\0' ) {              //Leerstring
    for (i = 0; i < STR_LEN; i++)      //Pfad-Char-String löschen
      sd_pfad[i] = 0;
    sd_pfad[0] = '/';                  //springe ins Root-Verzeichnis
  }

  while ( isValidFnChar( *txtpos )) {   //Überprüfung auf gültige Zeichen
    sd_pfad[i++] = *txtpos++;
  }
  sd_pfad[i] = 0;
  if (*txtpos == '"') {                 //abschliessendes Anführungszeichen vorhanden?
    *txtpos++;
  }
  else                                  //kein Anführungszeichen vorhanden
  {
    for (i = 0; i < STR_LEN; i++)      //Pfad-Char-String löschen
      sd_pfad[i] = 0;
    sd_pfad[0] = '/';                  //springe ins Root-Verzeichnis

    printmsg(syntaxmsg, 1);             //Fehlermeldung ausgeben
    return;
  }
  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

  //prüfen, ob der Pfad gültig ist
  if ( !SD.open(String(sd_pfad))) {
    printmsg(dirnotfound, 1);
    sd_pfad[0] = '/';                                     //kein gültiger Pfad, dann Root-Verzeichnis setzen
    sd_pfad[0] = 0;
  }

  sd_ende();                                             //SD-Card unmount

}

//#######################################################################################################################################
//------------------------------------------- Befehl MD und RD (MKDIR und Remove Dir -----------------------------------
//#######################################################################################################################################
static int cmd_mkdir(int mod)
{

  // eingabe Verzeichnisname
  expression_error = 0;
  get_value();

  if (expression_error) return 1;

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

  if (mod == 1) {
    // Verzeichnis erstellen
    if ( !SD.mkdir(String(sd_pfad) + String(tempstring)))
    {
      printmsg(sderrormsg, 1);
      sd_ende();                                             //SD-Card unmount
      return 1;
    }
  }
  else
  {
    if ( !SD.rmdir(String(sd_pfad) + String(tempstring)))
    {
      printmsg(dirmsg, 1);
      sd_ende();                                             //SD-Card unmount
      return 1;
    }

  }
  sd_ende();                                             //SD-Card unmount

}

//#######################################################################################################################################
//--------------------------------------------- DIR - Befehl ----------------------------------------------------------------------------
//#######################################################################################################################################

void cmd_Dir(void)
{ int ln = 1;
  int ex = 0;
  String cbuf;
  int wd = GFX.getWidth() / x_char[fontsatz];
  int Dateien = 0;

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);

  File dir = SD.open(String(sd_pfad));
  dir.seek(0);                                                  //zum Verzeichnis-Anfang

  while ( !ex ) {
    File entry = dir.openNextFile();                            //nächsten Eintrag holen
    if ( !entry ) {                                             //kein Eintrag mehr, dann abbruch
      entry.close();
      break;
    }
    printmsg(spacemsg, 0);
    printmsg(entry.name(), 0);                                  //Datei- oder Verzeichnisname ausgeben

    if ( entry.isDirectory() ) {
      printmsg(slashmsg, 0);

      for ( int i = strlen( entry.name()) ; i < 17 ; i++ ) {    //abhängig von der Stringlänge, Leerzeichen ausgeben
        printmsg(spacemsg, 0);
      }
      printmsg(dirextmsg, 0);                                   //'dir' ausgeben
    }
    else {
      for ( int i = strlen( entry.name()) ; i < 17 ; i++ ) {    //abhängig von der Stringlänge, Leerzeichen ausgeben
        printmsg(spacemsg, 0);
      }
      printnum(int(entry.size()), Zahlenformat);                //Dateigrösse ausgeben
      Dateien++;
      itoa(entry.size(), tempstring, 10);                       //Dateigrösse in String umwandeln
      for ( int i = strlen(tempstring) ; i < 8 ; i++ ) {        //abhängig von der Stringlänge, Leerzeichen ausgeben
        printmsg(spacemsg, 0);
      }
      time_t t = entry.getLastWrite();                          //Datei-Zeitstempel lesen
      struct tm * tmstruct = localtime(&t);
      if (tmstruct->tm_mday < 10) outchar('0');                 //führende Null bei Werten < 10
      printnum(tmstruct->tm_mday, 0);                           //Tag ausgeben
      outchar('.');
      if ((tmstruct->tm_mon + 1) < 10) outchar('0');            //führende Null bei Werten < 10
      printnum(tmstruct->tm_mon + 1, 0);                        //Monat ausgeben -> +1, da der Monat von 0-11 zurückgegeben wird
      outchar('.');
      printnum(((tmstruct->tm_year) + 1900), 0);                //Jahr ausgeben
      if (wd > 40) {                                            //bei Terminalbreite > 40 zusätzlich Zeit anzeigen
        outchar(' ');
        if (tmstruct->tm_hour < 10) outchar('0');               //Stunde ausgeben
        printnum(tmstruct->tm_hour, 0);
        outchar(':');
        if (tmstruct->tm_min < 10) outchar('0');                //Minute ausgeben
        printnum(tmstruct->tm_min, 0);
      }

    }
    line_terminator();
    entry.close();
    ln++;

#ifdef ILI9341                                                      //bei TFT x und y vertauscht
if (ln == VGAController.getScreenWidth() / y_char[fontsatz] - 3)  //Anzahl Zeilen abhängig vom Fontsatz
{
if (wait_key(true) == 3) ex = 1;                              //nach ln Zeilen auf Tastatur warten ->SPACE,ENTER=weiter, CTRL+C oder ESC=EXIT
ln = 1;
}
#else
if (ln == (VGAController.getScreenHeight() / y_char[fontsatz]) - 3) //Ausgabezeilen abhängig vom gewählten Fontsatz
{
if (wait_key(true) == 3) ex = 1;                              //nach ln Zeilen auf Tastatur warten ->SPACE,ENTER=weiter, CTRL+C oder ESC=EXIT
ln = 1;
}
#endif

  }
  line_terminator();
  printmsg(spacemsg, 0);
  printnum(Dateien, Zahlenformat);
  printmsg(" Files on SD-Card", 1);
  printmsg("  Total space: ", 0);
  printnum(SD.totalBytes() / (1024 * 1024), Zahlenformat);
  printmsg("MB", 1);
  printmsg("  Used  space: ", 0);
  printnum(SD.usedBytes() / (1024 * 1024), Zahlenformat);
  printmsg("MB", 1);

  dir.close();
  sd_ende();                                             //SD-Card unmount
}

//#######################################################################################################################################
//--------------------------------------------- RENAME - Befehl REN(Filename_old,Filename_new) ----------------------------------------------------
//#######################################################################################################################################

void renameFile(fs::FS &fs, const char * path1, const char * path2) {

  printmsg("Renaming file ", 0);
  printmsg(path1, 0);
  printmsg("to ", 0);
  printmsg(path2, 0);
  line_terminator();
  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

  if (fs.rename(path1, path2)) {
    printmsg("File renamed", 1);
  } else {
    printmsg("Rename failed", 1);
  }
  sd_ende();                                             //SD-Card unmount
}

//----------------------------------------- Unterprogramm - Überprüfung auf gültige Zeichen -------------------------------------------------------

// returns 1 if the character is valid in a filename
static int isValidFnChar( char c )
{
  if ( c >= '0' && c <= '9' ) return 1; // number
  if ( c >= 'A' && c <= 'Z' ) return 1; // LETTER
  if ( c >= 'a' && c <= 'z' ) return 1; // letter (for completeness)
  if ( c == '/' ) return 1;
  if ( c == '_' ) return 1;
  if ( c == '-' ) return 1;
  if ( c == '+' ) return 1;
  if ( c == '.' ) return 1;
  if ( c == '~' ) return 1; // Window~1.txt
  return 0;
}

//#######################################################################################################################################
//--------------------------------------------- Timer-Interrupt für Akku-Überwachung ----------------------------------------------------
//#######################################################################################################################################

void IRAM_ATTR onTimer()
{
  //----------------------- Akku-Überwachung -----------------------------------
  /*
    Batt>--+
      |
     | |
     | | R1 3.3kohm 3.327
     | |
      |
      +------Pin39
      |
     | |
     | | R2 10kohm  10.19
     | |
      |          13517
    GND+-----GND
  */
  float batterie = 3.3 / 4095 * analogRead(Batt_Pin);
  batterie = batterie / 0.753865; //10190/(10190+3327);               //zurückrechnen auf die ursprünglichen 4.2V (Uin/(R2/(r1+r2))
  if ( batterie < 3.4 )
  {
    tc.setCursorPos(5, 0);
    printmsg("   * AKKU LOW!!! * ", 0);
    printnum(batterie, 0);
    printmsg(" V", 0);
  }
}

//#######################################################################################################################################
//--------------------------------------------- SETUP -----------------------------------------------------------------------------------
//#######################################################################################################################################

void setup()
{

  setCpuFrequencyMhz(240);                                                           //mit dieser Option gibt's Startschwierigkeiten

  EEPROM. begin ( EEPROM_SIZE ) ;
  delay(200);
  if (EEPROM.read(100) == erststart_marker) {                                         //auf jungfräulichkeit prüfen

    //################ Farbschema aus dem EEPROM lesen ############################
    Vordergrund = EEPROM.read(0) ;   //512 Byte Werte im EEPROM speicherbar
    Hintergrund = EEPROM.read(1);
    //Pencolor = Vordergrund;
    user_vcolor = Vordergrund;    //User-Vordergrundfarbe merken
    user_bcolor = Hintergrund;    //User-Hintergrundfarbe merken
    //#############################################################################

    //Mode_state = EEPROM.read(1);
    fontsatz = EEPROM.read(2);
    user_font = fontsatz;         //User-Fontsatz merken

    LCD_ZEILEN = EEPROM.read(3);
    LCD_SPALTEN = EEPROM.read(4);
    LCD_ADRESSE = EEPROM.read(5);

    //--- ist der SD-Marker (44) auf Platz 10 gesetzt, dann sind folgende Werte für die SD-Card-Konfiguration zu verwenden ---------------------------
    if (EEPROM.read(10) == 44) {
      kSD_CLK  = EEPROM.read(6);
      kSD_MISO = EEPROM.read(7);
      kSD_MOSI = EEPROM.read(8);
      kSD_CS   = EEPROM.read(9);
    }

    //--- ist der IIC_Marker (55) auf Platz 13 gesetzt, dann sind die folgenden Werte zu verwenden
    if (EEPROM.read(13) == 55) {
      EEprom_ADDR = EEPROM.read(11);  //Adresse des zu verwendenden EEProms
      //SCL_RTC = EEPROM.read(12);
    }


    //--- ist der KEY_Marker (66) auf Platz 15 gestzt, dann ist das gespeicherte Layout zu wählen
    if (EEPROM.read(15) == 66) {
      Keyboard_lang = EEPROM.read(14);
    }

    // --- ist der Theme_marker (77) auf Platz 17 gesetzt, dann das gespeicherte Theme setzen
    if (EEPROM.read(17) == 77) {
      Theme_state = EEPROM.read(16);
      Theme_marker = true;
    }
    else Theme_state = 0;
  }
  else                                                  //der ESP ist noch jungfräulich, also standard-Werte setzen
  {
    Vordergrund = 60;                                     //CPC Theme
    Hintergrund = 1;
    user_font   = 19;
    //Vordergrund = 43;                                   //Standard-Vordergrundfarbe (wenn noch nichts im EEprom steht)
    //Hintergrund = 18;                                   //Standard-Hintergrundfarbe (wenn noch nichts im EEprom steht)
    //user_font = 19;
    Theme_state = 2;                                      //CPC Theme
  }

  delay(1000);                                              //eine sek warten, damit die CardKB-Tastatur starten kann

  //VGAController.queueSize = 400;
  PS2Controller.begin(PS2Preset::KeyboardPort0);
#ifdef CardKB
Keyboard_lang = 9;                                        //bei Verwendung von CardKB wird auf die japanische Tastaturbelegung umgeschaltet, damit die Symbolik passt
#endif
Set_Layout();                                             //Keyboard-Layout setzen

  delay(200);


  //************************************************************ welcher Bildschirmtreiber? *********************************************************
  // 64 colors
#ifdef AVOUT                                                                          //AV-Variante
VGAController.begin(VIDEOOUT_GPIO);
VGAController.setHorizontalRate(2);                                                   //320x240
VGAController.setResolution(MODES_STD[5]);                                            //5 scheint optimal ist aber mit 384x240 nicht kompatibel

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

  set_font(fontsatz);                                                                  // Fontsatz laden (1 Byte)
  Terminal.enableCursor(true);
  fbcolor(Vordergrund, Hintergrund);
  tc.setCursorPos(1, 1);
  GFX.clear();
  if (Theme_marker) set_theme(Theme_state);                                            //Theme setzen, wenn im EEprom gespeichert

  PS2Controller.keyboard()-> onVirtualKey = [&](VirtualKey * vk, bool keyDown) {
    if (*vk == VirtualKey::VK_ESCAPE) {
      if (keyDown) {
        break_marker = true;                                                          //ESC abfangen und in Ctrl-C wandeln
      }
      *vk = VirtualKey::VK_NONE;
    }
    else if (*vk == VirtualKey::VK_F1) {                                               //Grafiksymbole on/off
      if (keyDown) {
        Graph_char = !Graph_char;
        PS2Controller.keyboard()->setLEDs(false, false, Graph_char);
      }
      *vk = VirtualKey::VK_NONE;
    }
    else if (*vk == VirtualKey::VK_F2) {                                               //TRON/TROFF
      if (keyDown) {
        tron_marker = !tron_marker;
        if (tron_marker) printmsg("TRON", 1);
        else printmsg("TROFF", 1);
        printmsg("OK>", 0);
      }
      *vk = VirtualKey::VK_NONE;
    }
    else if (*vk == VirtualKey::VK_F3) {                                              //Ausgabe Char-Table 32..127
      if (keyDown) {
        char_out(32, 128);
      }
      *vk = VirtualKey::VK_NONE;
    }
    else if (*vk == VirtualKey::VK_F4) {                                              //Ausgabe Char-Table 128..255
      if (keyDown) {
        char_out(128, 256);
      }
      *vk = VirtualKey::VK_NONE;
    }
    else if (*vk == VirtualKey::VK_F5) {                                              //Ausgabe Color-Tabelle
      if (keyDown) {
        color_out();
      }
      *vk = VirtualKey::VK_NONE;
    }
    else if (*vk == VirtualKey::VK_F10) {                                              //Ausgabe Color-Tabelle
      if (keyDown) {
        show_systemparameters();
      }
      *vk = VirtualKey::VK_NONE;
    }

  };

  //Serial.begin(kConsoleBaud);                                                           // open serial port

  // ein I2C-Interface definieren
  myI2C.begin(SDA_RTC, SCL_RTC, 400000); //400kHz
  rtc.begin(&myI2C);
  getdatetime();                                              //ESP32-interne Uhr stellen für Datei-Zeitstempel


  //-------------------------------- Akku-Überwachung per Timer0-Interrupt --------------------------------------------
#ifdef Akkualarm_enabled
Akku_timer = timerBegin(0, 80, true);
timerAttachInterrupt(Akku_timer, &onTimer, true);
timerAlarmWrite(Akku_timer, 60000000, true);         //ca.60sek bis Interrupt ausgelöst wird
timerAlarmEnable(Akku_timer);                        //Interrupt-Routine
#endif
//-------------------------------------------------------------------------------------------------------------------



}




//#######################################################################################################################################
//################################################## HCSR04 Ultraschall-Sensor ##########################################################
//#######################################################################################################################################
long HCSR04(int p) {

  long duration, inches, cm;

  // The PING))) is triggered by a HIGH pulse of 2 or more microseconds.
  // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:
  pinMode(p, OUTPUT);
  digitalWrite(p, LOW);
  delayMicroseconds(2);
  digitalWrite(p, HIGH);
  delayMicroseconds(5);
  digitalWrite(p, LOW);

  // The same pin is used to read the signal from the PING))): a HIGH pulse
  // whose duration is the time (in microseconds) from the sending of the ping
  // to the reception of its echo off of an object.
  pinMode(p, INPUT);
  duration = pulseIn(p, HIGH);
  delay(100);
  // convert the time into a distance
  //if(mo=3) inches = microsecondsToInches(duration);
  return duration / 29 / 2 ; //microsecondsToCentimeters(duration);
}

//#######################################################################################################################################
//############################################### Testbereich Dallas Temp-Sensor ########################################################
//#######################################################################################################################################
float init_temp(int p, int kanal) {

  OneWire oneWire(p);
  DallasTemperature sensors(&oneWire);

  if (!twire) {
    sensors.begin();
    twire = true;
  }
  sensors.requestTemperatures(); // Send the command to get temperatures
  twire = false;
  return sensors.getTempCByIndex(kanal);
}

//#######################################################################################################################################
//############################################### DHT Temp/Humidity-Sensor ##############################################################
//#######################################################################################################################################

float init_dht(int p, int m, int w)
{
  float a;
  switch (m) {
    case 1:
#define DHTTYPE DHT11
break;
case 2:
#define DHTTYPE DHT22
break;
case 3:
#define DHTTYPE DHT21
break;
default:
#define DHTTYPE DHT22
break;
}

      DHT_Unified dht(p, DHTTYPE);
      sensor_t sensor;
      dht.begin();
      delayMS = sensor.min_delay / 1000;
      delay(delayMS);

      sensors_event_t event;
      if (w == 0)
      { dht.temperature().getEvent(&event);
        if (isnan(event.temperature))
        {
          //Terminal.println("Error reading temperature!");
          return -1;
        }

        a = event.temperature;
      }
      else
      { dht.humidity().getEvent(&event);
        if (isnan(event.relative_humidity))
        {
          //Terminal.println("Error reading humidity!");
          return -1;
        }

        a = event.relative_humidity;
      }

      return a;
  }

  //#######################################################################################################################################
  //############################################### MCP23017-Funktionen ###################################################################
  //#######################################################################################################################################
  void mcp_start()
  {
    mcp.begin_I2C(MCP23017_ADDR, &myI2C);
    mcp_start_marker == true;
  }

  void mcp_Port_direction(char Port, int dir)
  { int i;
    if (mcp_start_marker != true) mcp_start();
    if (Port == 'A' || Port == 'C') {
      if (dir == 0) {
        for (i = 0; i < 8; i++) mcp.pinMode(i, OUTPUT); //alle pins von Port A auf Ausgang
      }
      else if (dir == 1) {
        for (i = 0; i < 8; i++) mcp.pinMode(i, INPUT_PULLUP); //alle pins von Port A auf Eingang
      }
    }
    if (Port == 'B' || Port == 'C') {
      if (dir == 0) {
        for (i = 8; i < 16; i++) mcp.pinMode(i, OUTPUT); //alle pins von Port B auf Ausgang
      }
      else if (dir == 1) {
        for (i = 8; i < 16; i++) mcp.pinMode(i, INPUT_PULLUP); //alle pins von Port B auf Eingang
      }
    }
  }
  //---------------------- Port-Write --------------------------
  void mcp_Port_write(char Port, int wert)
  {
    if (Port == 'A') mcp.writeGPIOA(wert);            //Port A
    else if (Port == 'B') mcp.writeGPIOB(wert);       //Port B
    else if (Port == 'C') mcp.writeGPIOAB(wert);      //Port A+B
  }

  //---------------------- Pin-Direction -----------------------
  void mcp_Pin_direction(int pin, int dir)
  {
    if (mcp_start_marker != true) mcp_start();
    if (pin > 0 && pin < 16)
    {
      if (dir == 0) mcp.pinMode(pin, OUTPUT);
      else if (dir == 1) mcp.pinMode(pin, INPUT_PULLUP);
    }
  }

  //----------------------- Pin-Write --------------------------
  void mcp_Pin_write(int pin, int wert)
  {
    if (wert == 0) {
      if (pin > 0 && pin < 16) mcp.digitalWrite(pin, LOW);
    }
    else if (wert != 0) {
      if (pin > 0 && pin < 16) mcp.digitalWrite(pin, HIGH);
    }
  }

  //#######################################################################################################################################
  //############################################### LCD-Befehl ############################################################################
  //#######################################################################################################################################

  static int LCD_Set(void) {
    char c;
    int bl, x, y, p, z;
    float a;

    HD44780LCD myLCD(LCD_ZEILEN, LCD_SPALTEN, LCD_ADRESSE, &myI2C);  // LCD object.rows ,cols ,PCF8574 I2C addr, Interface)
    myLCD.PCF8574_LCDBackLightSet(LCD_Backlight);
    c = spaces();

    if (Test_char('(')) return 1;

    c = *txtpos;
    expression_error = 0;
    txtpos++;
    switch (c) {
      case 'C':                             //CLS
        myLCD.PCF8574_LCDClearScreen();
        myLCD.PCF8574_LCDHome();
        break;

      case 'B':                             //Backlight
        if (Test_char(',')) return 1;
        p = get_value();
        if (p != 1 ) {
          LCD_Backlight = false;
          //myLCD.PCF8574_LCDBackLightSet(false);
        }
        else
          LCD_Backlight = true;
        myLCD.PCF8574_LCDBackLightSet(LCD_Backlight);
        myLCD.PCF8574_LCDHome();
        break;

      case 'G':                             //Goto
        if (Test_char(',')) return 1;
        p = get_value();
        if (Test_char(',')) return 1;
        z = get_value();
        if (z == 1) myLCD.PCF8574_LCDGOTO(LCDLineNumberOne, p);
        else if (z == 2) myLCD.PCF8574_LCDGOTO(LCDLineNumberTwo, p);
        else if (z == 3) myLCD.PCF8574_LCDGOTO(LCDLineNumberThree, p);
        else
          myLCD.PCF8574_LCDGOTO(LCDLineNumberFour, p);
        break;

      case 'I':                             //Init
        if (Test_char(',')) return 1;
        p = get_value();
        if (p == 1) myLCD.PCF8574_LCDInit(LCDCursorTypeOff);
        else if (p == 2)  myLCD.PCF8574_LCDInit(LCDCursorTypeBlink);
        else if (p == 3)  myLCD.PCF8574_LCDInit(LCDCursorTypeOn);
        else
          myLCD.PCF8574_LCDInit(LCDCursorTypeOnBlink);
        myLCD.PCF8574_LCDClearScreen();
        break;

      case 'L':                             //Clear Line Number
        if (Test_char(',')) return 1;
        z = get_value();
        if (z == 1) myLCD.PCF8574_LCDClearLine(LCDLineNumberOne);
        else if (z == 2) myLCD.PCF8574_LCDClearLine(LCDLineNumberTwo);
        else if (z == 3) myLCD.PCF8574_LCDClearLine(LCDLineNumberThree);
        else
          myLCD.PCF8574_LCDClearLine(LCDLineNumberFour);
        break;

      case 'M':                             //Move
        if (Test_char(',')) return 1;
        z = get_value();
        if (Test_char(',')) return 1;
        p = get_value();
        if (z == 0) myLCD.PCF8574_LCDScroll(LCDMoveRight, p);
        else myLCD.PCF8574_LCDScroll(LCDMoveLeft, p);
        break;

      case 'N':                             //Nachkommastellen einstellen
        if (Test_char(',')) return 1;
        z = byte(get_value());
        if (z > 8) {
          z = 8;                            //Nachkommastellen werden auf 8 begrenzt
        }
        else {
          LCD_NACHKOMMA = z;
        }
        break;

      case 'W':
      case 'P':                             //Print
        if (Test_char(',')) return 1;

        if (c == 'P')
        {
          x = get_value();                    //x-Position
          if (Test_char(',')) return 1;
          y = get_value();                    //y-Position
          if (Test_char(',')) return 1;
          if (y == 1) myLCD.PCF8574_LCDGOTO(LCDLineNumberOne, x);
          else if (y == 2) myLCD.PCF8574_LCDGOTO(LCDLineNumberTwo, x);
          else if (y == 3) myLCD.PCF8574_LCDGOTO(LCDLineNumberThree, x);
          else
            myLCD.PCF8574_LCDGOTO(LCDLineNumberFour, x);
        }

nochmal:
        if (*txtpos == '"') {               //Text in Anführungszeichen
          txtpos++;
          while (*txtpos != '"')
          {
            myLCD.PCF8574_LCDSendChar(*txtpos);
            txtpos++;
          }
          txtpos++;
        }
        else {
          a = get_value();
          if (string_marker == true) {
            myLCD.PCF8574_LCDSendString(tempstring);  //Strings
            string_marker = false;
            chr = false;
          }
          else if (chr == true) {                     //Chars
            myLCD.write(int(a));
            chr = false;
            string_marker = false;
          }
          else
            myLCD.print(a, LCD_NACHKOMMA);            //Zahlenwerte
        }
        if (*txtpos == ',') {                       //,?
          myLCD.PCF8574_LCDSendString("   ");
          if (skip_spaces() == ')')
          {
            break;
          }
          goto nochmal;
        }
        else if (*txtpos == ';') {                  //;?
          if (skip_spaces() == ')')
          {
            break;
          }
          goto nochmal;
        }

        break;


      case 'S':                                     //Set
        if (set_lcd())
          return 1;
        break;
      default:
        break;
    }

    if (Test_char(')')) return 1;

    return 0;
  }


  static int set_lcd(void) {              //LCD(S,X,Y,Adresse)

    if (Test_char(',')) return 1;
    LCD_SPALTEN = get_value();                //Spalten
    if (Test_char(',')) return 1;
    LCD_ZEILEN = get_value();                 //Zeilen
    if (Test_char(',')) return 1;
    LCD_ADRESSE = get_value();                //I2C-Adresse

    HD44780LCD myLCD(LCD_ZEILEN, LCD_SPALTEN, LCD_ADRESSE, &myI2C);  // LCD object.rows ,cols ,PCF8574 I2C addr, Interface)
    EEPROM.write ( 3, LCD_ZEILEN ) ;       // Themen nummer im Flash speichern
    EEPROM.write ( 4, LCD_SPALTEN ) ;
    EEPROM.write ( 5, LCD_ADRESSE ) ;
    EEPROM.commit () ;

    delay(DISPLAY_DELAY_INIT);

    myLCD.PCF8574_LCDInit(LCDCursorTypeOn);
    myLCD.PCF8574_LCDClearScreen();
    myLCD.PCF8574_LCDBackLightSet(LCD_Backlight);
    myLCD.PCF8574_LCDGOTO(LCDLineNumberOne, 0);
    /*
      myLCD.PCF8574_LCDSendString("Zille-Soft-GmbH");
      myLCD.PCF8574_LCDGOTO(LCDLineNumberThree, 0);
      myLCD.PCF8574_LCDSendString("Neue Zeile!");
    */
    return 0;
  }

  //#######################################################################################################################################
  //############################################### LED-Befehl ############################################################################
  //#######################################################################################################################################
  void colorWipe(uint32_t c, uint8_t wait) {
    for (uint16_t i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
      delay(wait);
    }
  }

  // Input a value 0 to 255 to get a color value.
  // The colours are a transition r - g - b - back to r.
  uint32_t Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if (WheelPos < 85) {
      return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    if (WheelPos < 170) {
      WheelPos -= 85;
      return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }

  void rainbow(uint8_t wait) {
    uint16_t i, j;

    for (j = 0; j < 256; j++) {
      for (i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, Wheel((i + j) & 255));
      }
      strip.show();
      delay(wait);
    }
  }


  //Theatre-style crawling lights with rainbow effect
  void theaterChaseRainbow(uint8_t wait) {
    for (int j = 0; j < 256; j++) {   // cycle all 256 colors in the wheel
      for (int q = 0; q < 3; q++) {
        for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
          strip.setPixelColor(i + q, Wheel( (i + j) % 255)); //turn every third pixel on
        }
        strip.show();

        delay(wait);

        for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
          strip.setPixelColor(i + q, 0);      //turn every third pixel off
        }
      }
    }
  }

  //Theatre-style crawling lights.
  void theaterChase(uint32_t c, uint8_t wait) {
    for (int j = 0; j < 10; j++) { //do 10 cycles of chasing
      for (int q = 0; q < 3; q++) {
        for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
          strip.setPixelColor(i + q, c);  //turn every third pixel on
        }
        strip.show();

        delay(wait);

        for (uint16_t i = 0; i < strip.numPixels(); i = i + 3) {
          strip.setPixelColor(i + q, 0);      //turn every third pixel off
        }
      }
    }
  }


  static int LED_Set(void) {

    char c;
    int n, first, cnt, md;
    uint8_t r, g, b;
    uint32_t color;
    c = spaces();

    if (Test_char('(')) return 1;

    c = *txtpos;
    expression_error = 0;
    txtpos++;
    switch (c) {

      case 'S':                             //LED(S,Anzahl,Pin,Typ)
        if (Test_char(',')) return 1;       //Komma überspringen
        LED_COUNT = int(get_value());       //Anzahl LED's
        strip.updateLength(LED_COUNT);
        if (Test_char(',')) return 1;       //Komma überspringen
        LED_PIN = int(get_value());         //Pin
        strip.setPin(LED_PIN);
        if (*txtpos == ',') {
          txtpos++;
          LED_TYP = int(get_value());
          switch (LED_TYP) {
            case 0:
              strip.updateType(NEO_RGB);
              break;
            case 1:
              strip.updateType(NEO_RBG);
              break;
            case 2:
              strip.updateType(NEO_GRB);
              break;
            case 3:
              strip.updateType(NEO_GBR);
              break;
            case 4:
              strip.updateType(NEO_BRG);
              break;
            case 5:
              strip.updateType(NEO_BGR);
              break;
            default:
              strip.updateType(NEO_GRB);
              break;
          }
        }
        break;

      case 'B':                             //Brightness LED(B,0..255)
        if (Test_char(',')) return 1;       //Komma überspringen
        LED_BRIGHTNESS = get_value();
        strip.setBrightness(int(LED_BRIGHTNESS));
        break;

      case 'C':                             //Brightness LED(B,0..255)
        strip.clear();                      //Set all pixels in RAM to 0 (off) -> LED(C)
        break;

      case 'M':
        if (Test_char(',')) return 1;
        md = get_value();                  //Modus
        if (Test_char(',')) return 1;
        cnt = get_value();                //2.Parameter
        if (md == 1) rainbow(cnt);              //Rainbow
        else if (md == 2) theaterChaseRainbow(cnt); //Theatre Rainbow

        break;

      case 'F':                             //FILL LED(F,r,g,b,start,Anzahl)
      case 'P':                             //PAINT LED(C,r,g,b,nr)
      case 'W':                             //WIPE  LED(W,r,g,b,delay)
      case 'T':                             //THEATRE LED(T,r,g,b,delay)
        if (Test_char(',')) return 1;
        r = get_value();
        if (Test_char(',')) return 1;
        g = get_value();
        if (Test_char(',')) return 1;
        b = get_value();
        if (Test_char(',')) return 1;
        n = get_value();
        color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        if (c == 'P') {
          strip.setPixelColor(int(n), int(color));
        }
        else if (c == 'W') {
          colorWipe(strip.Color(r, g, b), n);
        }
        else if (c == 'T') {
          theaterChase(strip.Color(r, g, b), n);
        }
        else if (c == 'F') {
          if (Test_char(',')) return 1;
          cnt = get_value();
          strip.fill(strip.Color(int(r), int(g), int(b), LED_BRIGHTNESS), int(n), int(cnt));
        }
        break;

    }

    if (Test_char(')')) return 1;
    strip.show();            // Update Pixels
    return 0;

  }

  //#######################################################################################################################################
  //############################################### Zeileneditor ##########################################################################
  //#######################################################################################################################################
  void Editor(int lnr) {

    linenum = lnr;
    list_line = findline();

    edit_getline();                            //Zeile nach tempstring kopieren
    LineEditor.setText(tempstring);            //Zeile ausgeben
    LineEditor.edit();                         //Editor starten
    Edit_line = LineEditor.get();              //Zeile editieren
    txtpos = program_end + sizeof(linenum);    //Zeiger setzen
    while (*Edit_line)                         //editierte Zeile nach txtpos kopieren
    {
      txtpos[0] = *Edit_line;
      Edit_line++;
      txtpos++;
    }
    txtpos[0] = NL;                             //Zeile abschliessen
    line_terminator();

    move_line();                               //Zeile in Großbuchstaben umwandeln und ans Ende des Speicher verschieben
    insert_line();                             //aktualisierte Zeile in Speicher einfügen

  }


  //-------------------------------------------- zu editierende Zeile in den Puffer schreiben -----------------------------------------------
  void edit_getline()
  {
    int num, i;
    LINENUM line_num;

    line_num = *((LINENUM *)(list_line));
    list_line += sizeof(LINENUM) + sizeof(char);
    num = line_num;

    i = 0;

    printnum(num, 0);
    outchar(' ');

    while (*list_line != NL)
    {
      tempstring[i] = *list_line;
      list_line++;
      i++;

    }
    tempstring[i] = '\0';

  }

  //#######################################################################################################################################
  //********************************************************** DIM-Befehl **************************************************************
  //#######################################################################################################################################
  int Array_Dim(void) {
    while (1) {

      if (*txtpos >= 'A' && *txtpos <= 'Z')
      {
        int tmp, x, y, z;
        int stmp, i;
        word grenze, ort, ad;
        char c;
        byte p_data[8], len;
        bool str = false;

        //len = 4;
        x = y = z = 0;
        tmp = (*txtpos - 'A');                                    //Variablennummer
        txtpos++;
        while (*txtpos >= 'A' && *txtpos <= 'Z') txtpos++;        //lange Variablennamen
        if (*txtpos == '$') {
          len = STR_LEN;
          str = true;
          txtpos++;
        }
        else {
          len = sizeof (float);
        }
        if (Test_char('(')) return 1;
        x = abs(get_value());
        if (*txtpos == ',') {
          txtpos++;
          y = abs(get_value());
          if (*txtpos == ',') {
            txtpos++;
            z = abs(get_value());
          }
        }
        if (Test_char(')')) return 1;                            //Überprüfung der generellen Feldgrenzen

        grenze = (z + 1) * (y + 1) * (x + 1) * len;
        if (grenze > (32256) ) {                                 //Feldgrenze überschritten maximal 32256 Bytes
          syntaxerror(outofmemory);
          return 1;
        }

        if (str) {                                              //Überprüfung unter Berücksichtigung des schon zugewiesenen Array-Speichers
          ad = Var_Neu_Platz + grenze;
          if (ad > (32256))                                     //überprüfung, ob noch Platz vorhanden ist maximal 32256 Bytes
          {
            syntaxerror(outofmemory);
            return 1;
          }
          else
          {
            ort = STR_TBL + (tmp * 8);
          }
        }

        else
        {
          ad = Var_Neu_Platz + grenze;                          //Überprüfung unter Berücksichtigung des schon zugewiesenen Array-Speichers
          if (ad > (32256)) {                                   //überprüfung, ob noch Platz vorhanden ist maximal 32256 Bytes
            syntaxerror(outofmemory);
            return 1;
          }
          else
          {
            ort = VAR_TBL + (tmp * 8);
          }
        }

        p_data[0] = highByte(Var_Neu_Platz);                     //Adresse im FRAM
        p_data[1] = lowByte(Var_Neu_Platz);
        p_data[2] = highByte(x);
        p_data[3] = lowByte(x);
        p_data[4] = y;
        p_data[5] = z;
        SPI_RAM_write(ort, p_data, 6);
        Var_Neu_Platz += grenze;//* len);
      }

      if (*txtpos != ',') return 0;                              //kein weiteres dim
      txtpos++;                                                  //nächstes Dim
    }//while(1)

    syntaxerror(syntaxmsg);
    return 1;
  }

  //#######################################################################################################################################
  //----------------------------------------------------------- OPTION-Befehl -------------------------------------------------------------
  //#######################################################################################################################################
  int Option(void) {
    byte p[6];
    scantable(options);                                                  //Optionstabelle lesen
    char fu = table_index;
    int i, adr;

    if (Test_char('=')) return 1;                                        //nach der Option kommt ein '='

    switch (fu) {

      case OPT_FONT:
        p[0] = get_value();
        EEPROM.write(2, p[0]);          //Font-Nummer im Flash speichern
        EEPROM.write(17, 0);            //THEME-Marker löschen
        EEPROM.commit () ;
        set_font(p[0]);                 //setze Font
        break;

      case OPT_COLOR:
        p[0] = get_value();
        if (Test_char(',')) return 1;
        p[1] = get_value();
        EEPROM.write(0, p[0]);          //Vordergrundfarbe im Flash speichern
        EEPROM.write(1, p[1]);          //Hintergrundfarbe im Flash speichern
        EEPROM.write(17, 0);            //THEME-Marker löschen
        EEPROM.commit () ;
        Vordergrund = p[0];
        Hintergrund = p[1];
        fbcolor(Vordergrund, Hintergrund); //Farben setzen
        if (EEPROM.read(100) != erststart_marker) {                              //marker-setzen, das werte im EEprom stehen
          EEPROM.write ( 100, erststart_marker) ;
          EEPROM.commit () ;
        }
        break;

      case OPT_SDCARD:
        p[0] = get_value();             //SCK,MISO,MOSI,CS
        if (Test_char(',')) return 1;
        p[1] = get_value();
        if (Test_char(',')) return 1;
        p[2] = get_value();
        if (Test_char(',')) return 1;
        p[3] = get_value();
        EEPROM.write(6, p[0]);          //SCK-PIN im Flash speichern
        EEPROM.write(7, p[1]);          //MISO-PIN im Flash speichern
        EEPROM.write(8, p[2]);          //MOSI-PIN im Flash speichern
        EEPROM.write(9, p[3]);          //CS-PIN im Flash speichern ->CS ist fest auf GND
        EEPROM.write(10, SD_SET);       //Marker, das Pinkonfiguration im EEprom abgelegt wurde
        EEPROM.commit () ;
        break;

      case OPT_EEP:                       //Adresse des verwendeten EEPROM's
        p[0] = get_value();
        //if (Test_char(',')) return 1;
        //p[1] = get_value();
        EEPROM.write(11, p[0]);          //EEPROM-Adresse im Flash speichern
        //EEPROM.write(12, p[1]);          //SCL im Flash speichern
        EEPROM.write(13, IIC_SET);       //Marker, das Pinkonfiguration im EEprom abgelegt wurde
        EEPROM.commit () ;
        EEprom_ADDR = p[0];             //EEPROM Adresse sofort setzen
        break;

      case OPT_KEYBOARD:
        p[0] = get_value();
        EEPROM.write(14, p[0]);          //Nummer des Keyboard-Layouts 1=US, 2=UK, 3=GE, 4=IT, 5=ES, 6=FR, 7=BE, 8=NO, 9=JP
        EEPROM.write(15, KEY_SET);       //Key-Marker für Tastatur-Layout
        EEPROM.commit();
        Set_Layout();                    //layout sofort setzen
        break;

      case OPT_THEME:
        p[0] = get_value();
        EEPROM.write(16, p[0]);          //Nummer des Themes
        EEPROM.write(17, THEME_SET);     //THEME-Marker
        EEPROM.commit();
        set_theme(p[0]);
        if (EEPROM.read(100) != erststart_marker) {                              //marker-setzen, das werte im EEprom stehen
          EEPROM.write ( 100, erststart_marker) ;
          EEPROM.commit () ;
        }
        break;

      case OPT_PATH:                    //Arbeits-Pfad im EEPROM-Platz 20-50 (max. 30 Zeichen)
        cmd_chdir();
        adr = 20;
        i = 0;
        EEPROM.write(19, PATH_SET);
        EEPROM.commit();
        while (sd_pfad[i]) {
          EEPROM.write (adr++, sd_pfad[i++]);
          EEPROM.commit();
        }
        EEPROM.write(adr, 0);
        EEPROM.commit();

        break;

      default:
        break;
    }


    if (fu == OPT_UNKNOWN)                                              //am ende angekommen, Option nicht gefunden
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
    return 0;
  }

  void Set_Layout(void) {

    switch (Keyboard_lang) {
      case 1:
        PS2Controller.keyboard() -> setLayout(&fabgl::USLayout);                    //amerikanische Tastatur
        break;
      case 2:
        PS2Controller.keyboard() -> setLayout(&fabgl::UKLayout);                    //britische Tastatur
        break;
      case 3:
        PS2Controller.keyboard() -> setLayout(&fabgl::GermanLayout);                //deutsche Tastatur
        break;
      case 4:
        PS2Controller.keyboard() -> setLayout(&fabgl::ItalianLayout);               //italienische Tastatur
        break;
      case 5:
        PS2Controller.keyboard() -> setLayout(&fabgl::SpanishLayout);               //spanische Tastatur
        break;
      case 6:
        PS2Controller.keyboard() -> setLayout(&fabgl::FrenchLayout);                //französische Tastatur
        break;
      case 7:
        PS2Controller.keyboard() -> setLayout(&fabgl::BelgianLayout);               //belgische Tastatur
        break;
      case 8:
        PS2Controller.keyboard() -> setLayout(&fabgl::NorwegianLayout);             //norwegische Tastatur
        break;
      case 9:
        PS2Controller.keyboard() -> setLayout(&fabgl::JapaneseLayout);              //japanische Tastatur
        break;
      default:
        PS2Controller.keyboard() -> setLayout(&fabgl::GermanLayout);                //deutsche Tastatur
        break;

    }

  }

  //#######################################################################################################################################
  //**************************************************************** Seriell-Funktionen ***************************************************
  //#######################################################################################################################################

  int cmd_serial(void) {
    float value;
    char c;
    if (Test_char('_')) return 1;                       //Unterstrich für folgenden Befehlsbuchstaben
    c = spaces();                                       //Befehlsbuchstabe lesen
    txtpos++;

    switch (c) {

      case 'S':
        if (Test_char('(')) return 1;
        prx = get_value();                              //RX-Pin
        if (prx > 0) {
          if (Test_char(',')) return 1;
          ptx = get_value();                            //TX-Pin
          if (Test_char(',')) return 1;
          pbd = get_value();                            //Baud-Rate
          if (Test_char(')')) return 1;
          if (Portcheck(prx, ptx, pbd)) return 0;       //Überprüfung der Portnummern und der Baudrate
          Serial1.begin(pbd, SERIAL_8N1, prx, ptx);     //Com-Port öffnen
          Serial1.setRxBufferSize(SERIAL_SIZE_RX);      //Puffer auf 1024 bytes
          ser_marker = true;
          delay(200);
          return 0;
        }
        else
        {
          if (Test_char(')')) return 1;                 //COM S(0) schliesst den Com-Port
          Serial1.end();
          ser_marker = false;
          return 0;
        }
        break;

      case 'P':
      case 'W':
        if (ser_marker) {
          if (PW_OUT(c)) {
            syntaxerror(syntaxmsg);
            return 1;
          }
          return 0;
        }
        while (*txtpos != NL && *txtpos != ':') txtpos++;
        syntaxerror(commsg);
        break;

      case 'T':                     //Transfer Programm zum PC
        if (ser_marker) {
          list_send = true;
          list_out();
          list_send = false;
          return 0;
        }
        syntaxerror(commsg);
        break;

      default:
        break;
    }
    return 0;
  }

  int Portcheck(uint8_t r, uint8_t t, uint32_t b) {

    if (b >= 1200 && b <= 115200) {
      if (r == 2 || r == 12 || r == 26 || r == 27 || r == 34 || r == 35 || r == 36) {
        if (t == 2 || t == 12 || t == 26 || t == 27) return 0;          // alles ok
      }
    }
    syntaxerror(comsetmsg);
    return 1;                                                             //Fehler
  }

  int PW_OUT(char c) {
    float a;
    char d;
    int k = 0;

    if (Test_char('(')) return 1;

    while (!k) {
      d = spaces();
      switch (d) {

        case ',':
          Serial1.print("        ");
          txtpos++;
          if (*txtpos == NL) k = 2;
          break;

        case ';':
          txtpos++;
          if (*txtpos == NL) k = 2;
          break;

        case '"':
          if (serial_quoted_string()) k = 2;
          break;

        case '\'':
          k = 2;
          break;

        case ':':
          txtpos++;
          k = 2;
          break;

        case ')':
          txtpos++;
          k = 1;
          break;

        default:
          a = get_value();
          if (expression_error) k = 2;

          if (string_marker == true) {
            Serial1.print(tempstring);                            //Strings
            string_marker = false;
            chr = false;
          }
          else if (chr == true) {                                 //Chars
            Serial1.write(int(a));
            chr = false;
            string_marker = false;
          }
          else {                                                  //Zahlenwerte
            serout_marker = true;
            printnum(a, Zahlenformat);                            //Zahl
            serout_marker = false;
          }

      }//switch(d)

    }//while(!k)


    if (k == 2) return 1;
    if (c == 'P')  Serial1.println();                              //P ->Zeilenumbruch
    return 0;
  }


  static char serial_quoted_string(void)
  {
    int i = 0;
    char quote = *txtpos;
    if (quote != '"' && quote != '\'')
      return 1;
    txtpos++;


    while (txtpos[i] != quote)                                    // Checken, ob abschließendes Anführungszeichen vorhanden ist
    {
      if (txtpos[i] == NL) {
        return 1;
      }
      i++;
    }

    // Zeichenusgabe
    while (*txtpos != quote)
    {
      Serial1.print(*txtpos);
      txtpos++;
    }
    txtpos++;                                                   // überspringe Anführungszeichen

    return 0;
  }

  //#######################################################################################################################################
  //********************************************************** PIC-Befehl ****************************************************
  //#######################################################################################################################################
  int show_Pic(void) {
    long ad, n_bytes;
    int x, y, iv, pn;
    int dx, dy, ddx, ddy, px, py, vv, vh;
    float scal;
    byte w, a, buf[4];
    char c;
    char *filename;

    dx = 0;
    dy = 0;
    ddx = 0;
    ddy = 0;
    iv = 0;
    vv = GFX.getHeight();
    vh = GFX.getWidth();
    px = vv;
    py = vh;
    pn = 490000 / (vv * vh);                            //Anzahl der im Speicher ablegbaren Bilder berechnen
    if (Test_char('_')) return 1;                       //Unterstrich für folgenden Befehlsbuchstaben
    c = spaces();                                       //Befehlsbuchstabe lesen
    txtpos++;
    switch (c) {

      //****************************************************** PIC_D(PIC_Nr<,swap Backcolor><,X,Y>) ******************************************
      case 'D':                                         //Grafik im FRAM auf dem Bildschirm ausgeben
        if (Test_char('(')) return 1;
        ad = get_value();
        if (ad > 4) ad = 4;
        ad = ad * FRAM_PIC_OFFSET;                      //0..4 Bildspeicherplatz (320x240) bzw.0..2 (400x300)
        if (*txtpos == ',') {                           //Modus
          txtpos++;
          iv = get_value();
        }
        if (*txtpos == ',') {                           //Komma?, dann x,y-Position eingeben
          txtpos++;
          dx = get_value();                             //x
          if (Test_char(',')) return 1;                 //Komma überspringen
          dy = get_value();                             //y
        }
        if (Test_char(')')) return 1;
        spi_fram.read(FRAM_OFFSET + ad, buf, 4);         //Dimension lesen
        px = buf[0] + (buf[1] << 8);
        py = buf[2] + (buf[3] << 8);
        ad += 4;
        for (y = dy + py - 1 ; y > dy - 1; y--) {
          for (x = dx; x < (dx + px); x++) {
            w = spi_fram.read8(FRAM_OFFSET + ad++);
            fcolor(w);
            if (x < vh && y < vv) GFX.setPixel(x, y);
          }
        }
        if (iv > 0) GFX.swapRectangle(dx, dy , dx + px - 1, dy + py - 1); //swap Backcolor
        break;

      //****************************************************** PIC_E(X,Y,XX,YY,Filename.bmp) ******************************************
      case 'E':                                       //Export -> BMP
        if (Test_char('(')) return 1;
        dx = get_value();                             //x
        if (Test_char(',')) return 1;                 //Komma überspringen
        dy = get_value();                             //y
        if (Test_char(',')) return 1;
        px = get_value();                             //xx
        if (Test_char(',')) return 1;
        py = get_value();                             //yy
        if (Test_char(',')) return 1;                 //Komma überspringen
        get_value();                                  //Dateiname in tempstring
        if (Test_char(')')) return 1;
        export_pic(dx, dy, px, py, tempstring);
        break;

      //****************************************************** PIC_I(X,Y,Filename.bmp) ******************************************
      case 'I':                                         //Import <- BMP
        if (Test_char('(')) return 1;
        dx = get_value();                               //x
        if (Test_char(',')) return 1;                   //Komma überspringen
        dy = get_value();                               //y
        if (Test_char(',')) return 1;                   //Komma überspringen
        get_value();                                    //Dateiname in tempstring
        scal = 1;
        if (*txtpos == ',') {
          txtpos++;
          scal = get_value();
          if (scal > 1) scal = 1;                       //Skalierung auf 1 begrenzen
        }
        if (Test_char(')')) return 1;
        import_pic(dx, dy, tempstring, scal);
        break;

      //****************************************************** PIC_L(PIC_Nr,Filename) ******************************************
      case 'L':                                         //Load PIC_RAW-Data
        if (Test_char('(')) return 1;
        ad = get_value();                               //Adresse im Speicher
        if (ad > 4) ad = 4;
        ad = ad * FRAM_PIC_OFFSET;                      //0..4 Bildspeicherplatz
        if (Test_char(',')) return 1;                   //Komma überspringen
        get_value();                                    //Dateiname in tempstring
        if (Test_char(')')) return 1;
        load_pic(FRAM_OFFSET + ad, tempstring);
        break;

      //****************************************************** PIC_S(PIC_Nr,Filename) ******************************************
      case 'S':                                         //Save PIC_RAW-Data
        if (Test_char('(')) return 1;
        ad = get_value();                               //Adresse im Speicher
        if (ad > 4) ad = 4;
        ad = ad * FRAM_PIC_OFFSET;                      //0..4 Bildspeicherplatz
        if (Test_char(',')) return 1;                   //Komma überspringen
        get_value();                                    //Dateiname in tempstring
        if (Test_char(')')) return 1;
        spi_fram.read(FRAM_OFFSET + ad, buf, 4);         //Dimension lesen
        px = buf[0] + (buf[1] << 8);
        py = buf[2] + (buf[3] << 8);
        n_bytes = (px * py) + 4;                        //x*y=Biddaten + 4 Bytes der Dimension
        save_pic(FRAM_OFFSET + ad, n_bytes, tempstring);
        break;

      //****************************************************** PIC_P(PIC_Nr,X,Y,XX,YY) ******************************************
      case 'P':                                         //Grafikbildschirm in FRAM speichern
        if (Test_char('(')) return 1;
        ad = get_value();
        if (ad > 4) ad = 4;
        ad = ad * FRAM_PIC_OFFSET;                      //0..4 Bildspeicherplatz
        if (*txtpos == ',') {                           //Komma?, dann x,y-Position eingeben
          txtpos++;
          dx = get_value();                             //x
          if (Test_char(',')) return 1;                 //Komma überspringen
          dy = get_value();                             //y
          if (Test_char(',')) return 1;
          px = get_value();                             //xx
          if (Test_char(',')) return 1;
          py = get_value();                             //yy
        }
        if (Test_char(')')) return 1;
        ddx = px - dx;
        ddy = py - dy;

        buf[0] = lowByte(ddx);
        buf[1] = highByte(ddx);
        buf[2] = lowByte(ddy);
        buf[3] = highByte (ddy);
        SPI_RAM_write(FRAM_OFFSET + ad, buf, 4);       //XY-Dimension
        ad += 4;
        for (y = dy + ddy ; y > dy ; y--) {
          for (x = dx; x < (dx + ddx); x++) {
            if (x < vh && y < vv)
            {
              buf[0] = GFX.getPixel(x, y).R;
              buf[1] = GFX.getPixel(x, y).G;
              buf[2] = GFX.getPixel(x, y).B;
              //    B                  G                     R
              a = (buf[2] / 85) + ((buf[1] / 85) << 2) + ((buf[0] / 85) << 4); //einzelne Farbanteile in 64-Farbwert zurückwandeln
            }
            else a = 0;
            SPI_RAM_write8(FRAM_OFFSET + ad++, a);

          }
        }
        break;
      default:
        break;

    }//switch
    fcolor(Vordergrund);
    string_marker = false;
    return 0;
  }

  //****************************************************** PIC_E(X,Y,XX,YY,Filename.bmp) ******************************************

  int export_pic(long x, long y, long xx, long yy, char *file) {
    byte i, r, g, b, cl;
    uint32_t pic_size, pic, weite, hoehe;
    //                       0     1    2      3    4     5    6    7    8    9    10    11    12  13   14    15   16   17    18    19    20   21   22    23   24  25   26    27    28   29    30  31   32   33   34   35     36   37
    byte bmp_header[54] = {0x42, 0x4D, 0x36, 0x84, 0x03, 0x0, 0x0, 0x0, 0x0, 0x0, 0x36, 0x00, 0x0, 0x0, 0x28, 0x0, 0x0, 0x0, 0x40, 0x01, 0x0, 0x0, 0xF0, 0x0, 0x0, 0x0, 0x01, 0x0, 0x18, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2C, 0x01, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
    //                       B    M     |  ---  Size ---- |   |    Reseved     |   |   bfoffbits    |   |   bisize       |    |   Width        |    |   Height      |   |Planes| |BitCnt| |  Compress    |    |   size Img    |
    int rest, dx, dy;
    int durchlaeufe, tm;
    char k;

    pic = (xx - x) * (yy - y) * 3;    //Bildgrösse inkl.Header
    pic_size = pic + 54;              //Bildgrösse
    //Dateigrösse
    bmp_header[2] = pic_size;
    bmp_header[3] = pic_size >> 8;
    bmp_header[4] = pic_size >> 16;
    bmp_header[5] = pic_size >> 24;
    //Width
    weite = xx - x;
    bmp_header[18] = weite;
    bmp_header[19] = weite >> 8;
    bmp_header[20] = weite >> 16;
    bmp_header[21] = weite >> 24;
    //Height
    hoehe = yy - y;
    bmp_header[22] = hoehe;
    bmp_header[23] = hoehe >> 8;
    bmp_header[24] = hoehe >> 16;
    bmp_header[25] = hoehe >> 24;
    //Bildgrösse
    bmp_header[34] = pic;
    bmp_header[35] = pic >> 8;
    bmp_header[36] = pic >> 16;
    bmp_header[37] = pic >> 24;



    spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1
    // remove the old file if it exists
    if ( SD.exists( String(sd_pfad) + String(tempstring))) {
      printmsg("File exist, overwrite? (y/n)", 0);

      while (1)
      {
        k = wait_key(false);
        if (k == 'y' || k == 'n')
          break;
      }
      if (k == 'y') {
        SD.remove( String(sd_pfad) + String(tempstring));
        outchar(k);

      }
      else
      {
        outchar(k);
        line_terminator();
        sd_ende();                                             //SD-Card unmount
        warmstart();
        return 0;
      }
    }
    fp = SD.open( String(sd_pfad) + String(file), FILE_WRITE);
    for (i = 0; i < 54; i++) {
      fp.write(bmp_header[i]);
    }

    for (dy = y + yy - 1; dy > (y - 1); dy--) {
      for (dx = x; dx < x + xx; dx++) {
        r = GFX.getPixel(dx, dy).R;
        g = GFX.getPixel(dx, dy).G;
        b = GFX.getPixel(dx, dy).B;
        fp.write( b );
        fp.write( g );
        fp.write( r );
      }
    }
    fp.close();
    sd_ende();                                                //SD-Card unmount

    return 0;
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

    if ( !SD.exists(String(sd_pfad) + String(tempstring)))
    {
      syntaxerror(sdfilemsg);
      sd_ende();                                                  //SD-Card unmount
      return 0;
    }
    fp = SD.open( String(sd_pfad) + String(file), FILE_READ);
    fp.read(bmp_header, 54);                                      //BMP-Header einlesen
    skipx = 54;                                                   //nach dem Header geht's los mit Daten

    if (bmp_header[0] != 0x42 || bmp_header[1] != 0x4D)           //keine BMP-Datei, dann Abbruch!
    {
      syntaxerror(bmpfilemsg);
      sd_ende();                                                  //SD-Card unmount
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

    //Groesse auf Bildschirmauflösung skalieren
    if (xx >= vh && yy >= vv && sc <= 1) {
      xtmp = float(xx) / vh * (1 / sc);
      ytmp = float(yy) / vv * (1 / sc);
      restx = xx % vh;
    }
    else {
      xtmp = ytmp = 1;
      restx = 0;
    }

    stepx = xtmp;
    stepy = ytmp;

    if (ytmp > xtmp) {
      xtmp = ytmp;
    }
    else {
      ytmp = xtmp;
    }

    for (dy = yy - 1 ; dy > -1; dy -= stepy) {
      for (dx = 0; dx < xx; dx += stepx) {
        fp.read(buf, 3);                                     //Pixelfarben lesen (blau,grün,rot)
        sx = (dx / xtmp) + x;
        sy = (dy / ytmp) + y;

        if (sx < vh && sy < vv) {                            //nur im Bildschirmbereich pixeln
          GFX.setPenColor(buf[2], buf[1], buf[0]);
          GFX.setPixel(sx, sy);
        }
        skipx += stepx * 3;                                  //ist das Bild > Bildschirmbreite, Pixel*Skalierung überspringen
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
    sd_ende();                                               //SD-Card unmount
    return 0;
  }
  //******************************************************* PIC_S(PIC_NR,Filename) *************************************
  int save_pic(long adr, long n, char *file) {
    byte c[1024];
    char k;
    int rest;
    int durchlaeufe, tm;
    if (n > 1024) durchlaeufe = n / 1024;
    rest = n % 1024;

    spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1
    // remove the old file if it exists
    if ( SD.exists( String(sd_pfad) + String(tempstring))) {
      printmsg("File exist, overwrite? (y/n)", 0);
      while (1)
      {
        k = wait_key(false);
        if (k == 'y' || k == 'n')
          break;
      }
      if (k == 'y') {
        SD.remove( String(sd_pfad) + String(tempstring));
        outchar(k);
      }
      else
      {
        outchar(k);
        line_terminator();
        sd_ende();                                             //SD-Card unmount
        warmstart();
        return 0;
      }
    }

    fp = SD.open( String(sd_pfad) + String(file), FILE_WRITE);
    fp.close();
    sd_ende();                                                //SD-Card unmount

    for (int i = 0; i < durchlaeufe; i++) {
      spi_fram.read(adr, c, 1024);
      adr += 1024;
      spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1
      fp = SD.open( String(sd_pfad) + String(file), FILE_APPEND);
      for (int s = 0; s < 1024; s++) {
        fp.write( c[s] );
      }
      fp.close();
      sd_ende();                                                //SD-Card unmount
    }
    if (rest > 0) {
      spi_fram.read(adr, c, rest);
      spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1
      fp = SD.open( String(sd_pfad) + String(file), FILE_APPEND);
      for (int s = 0; s < rest; s++) {
        fp.write( c[s] );
      }
      fp.close();
      sd_ende();                                                //SD-Card unmount
    }
    return 0;
  }

  //******************************************* PIC_L(PIC_NR,Filename) ******************************************
  int load_pic(long adr, char *file) {
    byte c[1024];
    int rest, rx, ry;
    int durchlaeufe;
    long n, sc = 0;

    spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1
    if ( !SD.exists(String(sd_pfad) + String(tempstring)))
    {
      syntaxerror(sdfilemsg);
      sd_ende();                                                //SD-Card unmount
      return 1;
    }
    else {
      fp = SD.open( String(sd_pfad) + String(file), FILE_READ);
    }
    for (int s = 0; s < 4; s++) {
      c[s] = fp.read();
    }
    fp.close();
    sd_ende();                                             //SD-Card unmount
    rx = c[0] + (c[1] << 8);
    ry = c[2] + (c[3] << 8);
    n = (rx * ry) + 4;
    if (n > 1024) durchlaeufe = n / 1024;
    rest = n % 1024;

    for (int i = 0; i < durchlaeufe; i++) {
      spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1
      fp = SD.open( String(sd_pfad) + String(file), FILE_READ);
      fp.seek(sc);
      for (int s = 0; s < 1024; s++) {
        c[s] = fp.read();
      }
      sc = fp.position();
      fp.close();
      sd_ende();                                                //SD-Card unmount
      SPI_RAM_write(adr, c, 1024);
      adr += 1024;
    }
    if (rest > 0) {
      spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1
      fp = SD.open( String(sd_pfad) + String(file), FILE_READ);
      fp.seek(sc);
      for (int s = 0; s < rest; s++) {
        c[s] = fp.read();
      }
      fp.close();
      sd_ende();                                                //SD-Card unmount
      SPI_RAM_write(adr, c, rest);         //Dimension lesen
    }
    return 0;
  }
  //#######################################################################################################################################



  //#######################################################################################################################################
  //-----------------------------------------Befehl GRID_typ(x,y,x_zell,y_zell,x_pixel_step,y_pixelstep,frame_color,grid_color,pixel_raster,scale,arrow,frame) ----------------------
  //#######################################################################################################################################
  int make_grid(void) {
    int x_grid, y_grid, x_zell, y_zell, x_stp, y_stp;
    int i, a, gc, fc, pr, xdiff, ydiff, sc , arrow, frame;
    char typ;
    pr = 0;                                       //Pixelraster
    sc = 0;                                       //Skala
    if (Test_char('_')) return 1;
    typ = spaces();
    txtpos++;
    switch (typ) {
      case 'G':                                   //Gitter-Raster
        if (Test_char('R')) return 1;
        break;
      case 'K':                                   //Kartesisches Koordinatensystem
        if (Test_char('T')) return 1;
        break;
      case 'X':                                   //XY-Diagramm
        if (Test_char('Y')) return 1;
        break;
      case 'U':                                   //UI-Diagramm
        if (Test_char('I')) return 1;
        break;
      default:

        break;
    }

    if (Test_char('(')) return 1;
    x_grid = get_value();           //x-Position
    if (Test_char(',')) return 1;
    y_grid = get_value();           //y-Position
    if (Test_char(',')) return 1;
    x_zell = get_value();           //Anzahl Zellen in x-Richtung
    if (Test_char(',')) return 1;
    y_zell = get_value();           //Anzahl Zellen in y-Richtung
    if (Test_char(',')) return 1;
    x_stp = get_value();            //Rastergrösse in x-Richtung (pixel)
    if (Test_char(',')) return 1;
    y_stp = get_value();            //Rastergrösse in y-Richtung (pixel)
    if (Test_char(',')) return 1;
    fc = get_value();               //Farbe der Achsen und des Rahmens
    if (Test_char(',')) return 1;
    gc = get_value();               //Farbe des Rasters
    if (*txtpos == ',') {           //Pixelraster (Pixelabstand im Raster)
      txtpos++;
      pr = get_value();
      if (*txtpos == ',') {         //Skale hinzufügen
        txtpos++;
        sc = get_value();
        if (*txtpos == ',') {       //Pfeile anzeigen
          txtpos++;
          arrow = get_value();
          if (*txtpos == ',') {     //Rahmen darstellen
            txtpos++;
            frame = get_value();
          }
        }
      }
    }
    if (Test_char(')')) return 1;

    Grid[0] = x_grid;
    Grid[1] = y_grid;
    Grid[2] = x_grid + (x_stp * x_zell);
    Grid[3] = y_grid + (y_stp * y_zell);
    Grid[4] = x_zell;
    Grid[5] = y_zell;
    Grid[6] = x_stp;
    Grid[7] = y_stp;
    Grid[8] = fc;
    Grid[9] = gc;

    //-------------------------------- RS=Raster ---------------------------------------------------------------------------------

    i = x_grid;
    a = 0;
    //-------------------- Grid zeichnen ---------------------------------------------------
    fcolor(gc);
    while (a < x_zell + 1) {
      //------------- Raster zeichnen --------------------
      if (pr) pixel_line(i, y_grid, i, y_grid + (y_stp * y_zell), pr);
      else GFX.drawLine(i, y_grid, i, y_grid + (y_stp * y_zell));
      fcolor(fc);
      //---------- Skala zeichnen ------------------------
      if (sc) {
        if (typ == 'K' || typ == 'X') {
          ydiff = y_grid + ((Grid[3] - Grid[1]) / 2);
          GFX.drawLine(i, ydiff - 2, i, ydiff + 2);

        }
        else {
          ydiff = y_grid + Grid[3] - Grid[1];
          GFX.drawLine(i, ydiff - 2, i, ydiff + 2);

        }
      }
      fcolor(gc);
      i += x_stp;
      a++;
    }
    a = 0;
    i = y_grid;
    while (a < y_zell + 1) {
      //------------- Raster zeichnen --------------------
      if (pr) pixel_line(x_grid, i, x_grid + (x_stp * x_zell), i, pr);
      else GFX.drawLine(x_grid, i, x_grid + (x_stp * x_zell), i);
      //---------- Skala zeichnen ------------------------
      if (sc) {
        fcolor(fc);
        if (typ == 'K') {
          xdiff = x_grid + ((Grid[2] - Grid[0]) / 2);
          GFX.drawLine(xdiff - 2, i, xdiff + 2, i);

        }
        else if (typ == 'X' || typ == 'U') {
          GFX.drawLine(x_grid - 2, i, x_grid + 2, i);

        }

      }
      fcolor(gc);
      i += y_stp;
      a++;
    }
    //--------------------- Rahmen zeichnen ------------------------------------------------
    fcolor(fc);

    if (typ == 'R' || frame == 1) {
      //zweimal Rahmen (einmal um einen Pixel versetzt, damit er etwas breiter ist)
      GFX.drawRectangle(x_grid, y_grid, x_grid + (x_stp * x_zell), y_grid + (y_stp * y_zell));
      GFX.drawRectangle(x_grid - 1, y_grid - 1, x_grid + (x_stp * x_zell) + 1, y_grid + (y_stp * y_zell) + 1);
    }

    if (typ == 'K' || typ == 'X') {                 //x-Achse
      xdiff = x_grid + Grid[2] - Grid[0];
      ydiff = y_grid + ((Grid[3] - Grid[1]) / 2);
      GFX.drawLine(x_grid, ydiff, xdiff, ydiff);
      Grid[10] = ydiff;  //y-Position der x-Skale

      //----------- Pfeil zeichnen ---------------
      if (arrow) {
        bcolor(fc);
        Point points[3] = { {xdiff, ydiff - 3}, {xdiff + 6, ydiff}, {xdiff, ydiff + 3} };
        GFX.fillPath(points, 3);
        bcolor(Hintergrund);
      }

      if (typ == 'K') {                               //y-Achse
        xdiff = x_grid + ((Grid[2] - Grid[0]) / 2);
        ydiff = y_grid + Grid[3] - Grid[1];
        GFX.drawLine(xdiff, y_grid, xdiff, ydiff);
        Grid[11] = xdiff;  //x-Position der y-Skale
        //----------- Pfeil zeichnen ---------------
        if (arrow) {
          bcolor(fc);
          Point points[3] = { {xdiff - 3, y_grid}, {xdiff, y_grid - 6}, {xdiff + 3, y_grid} };
          GFX.fillPath(points, 3);
          bcolor(Hintergrund);
        }
      }
      else if (typ == 'X' ||  typ == 'U') {           //y-Achse
        ydiff = y_grid + Grid[3] - Grid[1];
        GFX.drawLine(x_grid, y_grid, x_grid, ydiff);
        Grid[11] = x_grid;  //x-Position der y-Skale
        //----------- Pfeil zeichnen ---------------
        if (arrow) {
          bcolor(fc);
          Point points[3] = { {x_grid - 3, y_grid}, {x_grid, y_grid - 6}, {x_grid + 3, y_grid} };
          GFX.fillPath(points, 3);
          bcolor(Hintergrund);
        }

      }

    }
    else if (typ  == 'U') {
      ydiff = y_grid + Grid[3] - Grid[1];
      xdiff = x_grid + Grid[2] - Grid[0];
      GFX.drawLine(x_grid, ydiff, xdiff, ydiff);          //x-Achse
      GFX.drawLine(x_grid, y_grid, x_grid, ydiff);        //y-Achse
      Grid[10] = ydiff;   //y-Position der x-Skale
      Grid[11] = x_grid;  //x-Position der y-Skale

      if (arrow) {
        bcolor(fc);
        Point points[3] = { {x_grid - 3, y_grid}, {x_grid, y_grid - 6}, {x_grid + 3, y_grid} };
        GFX.fillPath(points, 3);

        Point pointe[3] =  { {xdiff, ydiff - 3}, {xdiff + 6, ydiff}, {xdiff, ydiff + 3} };
        GFX.fillPath(pointe, 3);
        bcolor(Hintergrund);
      }

    }

    fcolor(Vordergrund);
    return 0;
  }

  //------------------------------------------ Pixellinie zeichnen ----------------------------------------------------------------
  void pixel_line(int x, int y, int xx, int yy, uint8_t pix) {
    for (int a = x; a < xx + 1; a) {
      for (int b = y; b < yy + 1; b) {
        GFX.setPixel(a, b);
        b += pix;
      }
      a += pix;
    }

  }
  //#######################################################################################################################################

  //#######################################################################################################################################
  //-------------------------------------------- Befehl TEXT(x,y,font,String)--------------------------------------------------------------
  int draw_text(void) {
    int x_text, y_text, fnt;

    if (Test_char('(')) return 1;
    x_text = get_value();
    if (Test_char(',')) return 1;
    y_text = get_value();
    if (Test_char(',')) return 1;
    fnt = get_value();
    if (Test_char(',')) return 1;
    get_value();                      //text in tempstring
    if (Test_char(')')) return 1;
    drawing_text(fnt, x_text, y_text);
    return 0;
  }

  void drawing_text(int fnt, int x_text, int y_text)
  {
    switch (fnt) {
      case 0:
        //GFX.drawTextWithEllipsis(&fabgl::FONT_8x8, x_text, y_text, tempstring, 200);
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
    string_marker = false;

  }
  //#######################################################################################################################################
  //------------------------------------------------ Befehl WIN(nr,x,y,xx,yy,color) -------------------------------------------------------
  //#######################################################################################################################################

  int win(void) {
    char c;
    int nr, a, vv, vh;

    vv = GFX.getHeight();
    vh = GFX.getWidth();
    if (*txtpos == NL || *txtpos == ':') {                   //WINDOW ohne Parameter setzt das Hauptfenster
      if (Frame_nr) {                                        //befinde ich mich in einem Fenster? dann Cursor-Positon merken
        Frame_curtmpx[Frame_nr] = tc.getCursorCol();
        Frame_curtmpy[Frame_nr] = tc.getCursorRow();
      }

      Frame_nr = 0;
      win_set_cursor(0);
      return 0;
    }

    if (Test_char('(')) return 1;                             //Window(nr) ->setze Fenster
    if (Frame_nr) {                                           //befinde ich mich in einem Fenster? dann Cursor-Positon merken
      Frame_curtmpx[Frame_nr] = tc.getCursorCol();
      Frame_curtmpy[Frame_nr] = tc.getCursorRow();
    }
    nr = abs(get_value());

    if (nr < 0) {
      syntaxerror(valmsg);
      return 1;
    }

    if (nr > 5) nr = 5;
    Frame_nr = nr;                                            //setze aktuelles Fenster
    if (*txtpos == ')') {
      txtpos++;

      if (nr == 0) {                                          //Window(0) setzt ebenfalls das Hauptfenster
        Frame_nr = 0;
        win_set_cursor(0);
        return 0;
      }

      Terminal.enableCursor(false);                           //Cursor ausschalten um Fehldarstellungen zu verhindern
      make_win(nr, Frame_col[nr]);                            //fenster neu zeichnen
      fbcolor(Frame_vcol[nr], Frame_hcol[nr]);                //Vordergrund und Hintergrundfarbe des Fensters setzen
      if (Frame_title[nr]) {
        strcpy(tempstring, Frame_ttext[nr]);
        drawing_text(fontsatz, Frame_x[nr] + x_char[fontsatz], Frame_y[nr] - 3);
      }
      tc.setCursorPos(Frame_curtmpx[nr], Frame_curtmpy[nr]);  //Cursorposition setzen
      Terminal.enableCursor(onoff);                           //Cursor in vorherigen Zustand setzen
      return 0;
    }

    if (Test_char(',')) return 1;
    a = get_value();
    Frame_x[nr] = abs(a * x_char[fontsatz]);
    if (Frame_x[nr] > vh) Frame_xx[nr] = vh;

    if (Test_char(',')) return 1;

    a = get_value();
    Frame_y[nr] = abs(a * y_char[fontsatz]);
    if (Frame_y[nr] > vv ) Frame_y[nr] = vv;

    if (Test_char(',')) return 1;                             //Fenster erstellen

    a = get_value();
    Frame_xx[nr] = abs(a * x_char[fontsatz]);
    if (Frame_xx[nr] > vh) Frame_xx[nr] = vh;

    if (Test_char(',')) return 1;

    a = get_value();
    Frame_yy[nr] = abs(a * y_char[fontsatz]);
    if (Frame_yy[nr] > vv ) Frame_y[nr] = vv;

    if (*txtpos == ',') {                                                //optionale Werte
      txtpos++;
      Frame_col[nr] = get_value();                                       //optionale Rahmen-Farbe

      if (*txtpos == ',') {
        txtpos++;
        get_value();                                                     //optionaler Fenstertitel
        Frame_title[nr] = true;
        strcpy(Frame_ttext[nr], tempstring);
      }
    }
    if (Test_char(')')) return 1;

    Frame_vcol[nr] = Vordergrund;                                        //Vordergrund und Hintergrundfarbe wie Hauptfenster
    Frame_hcol[nr] = Hintergrund;

    make_win(nr, Frame_col[Frame_nr]);                                   //Fenster erstellen
    win_cls(nr);                                                         //Fensterinhalt löschen
    fbcolor(Frame_vcol[nr], Frame_hcol[nr]);                             //Vordergrund und Hintergrundfarbe des Fensters setzen
    if (Frame_title[nr]) {
      drawing_text(fontsatz, Frame_x[nr] + x_char[fontsatz], Frame_y[nr] - (y_char[fontsatz] / 2) + 1);
    }

    win_dimension(nr);                                                   //Cursorposition errechnen
    win_set_cursor(Frame_nr);                                            //Cursor setzen

    return 0;

  }

  //----------------------------------------------- Window-Cursor-Initialwerte errechnen --------------------------------------------------

  void win_dimension(int nr)                                            //Cursor-Initial-Koordinaten errechnen
  {
    Frame_curx[nr] = (Frame_x[nr] / x_char[fontsatz]) + 2;
    Frame_cury[nr] = (Frame_y[nr] / y_char[fontsatz]) + 2;
  }

  //----------------------------------------------- Window-Rahmen erstellen ---------------------------------------------------------------
  void make_win(int nr, int col) {                                      //Fensterrahmen erstellen
    if (col > -1) {                                                     //Werte > -1 erzeugen einen farbigen Rahmen, -1=Rahmen unsichtbar
      fcolor(col);
      GFX.drawRectangle(Frame_x[nr], Frame_y[nr], Frame_xx[nr], Frame_yy[nr]);
      fcolor(Vordergrund);
    }
  }

  //----------------------------------------------- Window-Cursor setzen ------------------------------------------------------------------

  void win_set_cursor(int nr) {                                         //Cursor im Fenster setzen
    fbcolor(Frame_vcol[nr], Frame_hcol[nr]);
    Frame_curtmpx[nr] = Frame_curx[nr];
    Frame_curtmpy[nr] = Frame_cury[nr];
    tc.setCursorPos(Frame_curx[nr], Frame_cury[nr]);
  }

  //----------------------------------------------- Window-Parameter löschen --------------------------------------------------------------
  void del_window(void) {                                             //Fensterparameter löschen
    for (int i = 1; i < 6; i++) {
      Frame_x[i]        = 0;
      Frame_y[i]        = 0;
      Frame_xx[i]       = 0;
      Frame_yy[i]       = 0;
      Frame_curx[i]     = 0;              //X-Cursor Initialwert
      Frame_curtmpx[i]  = 0;              //X-Cursor temporärer Wert
      Frame_curtmpy[i]  = 0;              //Y-Cursor temporärer Wert
      Frame_cury[i]     = 0;              //Y-Cursor Initialwert
      Frame_col[i]      = 0;
      Frame_vcol[i]     = Vordergrund;
      Frame_hcol[i]     = Hintergrund;
      Frame_title[i]    = false;
      memset(Frame_ttext[i], '\0', sizeof(Frame_ttext[i]));  //Fenster-Titel-String
    }
  }

  //----------------------------------------------- Window-Fensterinhalt eine Zeile nach oben scrollen ------------------------------------
  void move_up(int nr) {
    int vx, vy, bx, by, cx, cy;
    fbcolor(Frame_vcol[nr], Frame_hcol[nr]);                                                                //Fensterfarben setzen
    Terminal.enableCursor(false);                                                                           //Cursor abschalten
    vx = Frame_x[nr] + x_char[fontsatz];
    vy = Frame_y[nr] + y_char[fontsatz] + y_char[fontsatz];
    bx = Frame_x[nr] + x_char[fontsatz];
    by = Frame_y[nr] + y_char[fontsatz];
    cx = Frame_xx[nr] - Frame_x[nr];
    cy = Frame_yy[nr] - Frame_y[nr] - y_char[fontsatz] - y_char[fontsatz];
    GFX.copyRect(vx, vy, bx, by, cx, cy);                                                                   //Bereich 2.Zeile bis letzte Zeile eine Zeile höher kopieren
    GFX.fillRectangle(Frame_x[nr] + 1, Frame_yy[nr] - y_char[fontsatz], Frame_xx[nr] - 1, Frame_yy[nr] - 1); //letzte Zeile löschen
    Terminal.enableCursor(onoff);                                                                           //Cursor wieder in vorherigen Zustand versetzen
  }

  //------------------------------------------------ CLS im Window ------------------------------------------------------------------------

  void win_cls(int nr) {
    int zeilen;
    fbcolor(Frame_vcol[nr], Frame_hcol[nr]);
    Terminal.enableCursor(false);                                                             //Cursor abschalten um Fehldarstellungen zu verhindern
    GFX.fillRectangle(Frame_x[nr] + 1, Frame_y[nr] + 1, Frame_xx[nr] - 1, Frame_yy[nr] - 1);  //Fensterbereich innerhalb des Rahmens löschen
    if (Frame_title[nr]) {                                                                    //Titel vorhanden?
      strcpy(tempstring, Frame_ttext[nr]);                                                    //Titeltext nach tempstring kopieren
      drawing_text(fontsatz, Frame_x[nr] + x_char[fontsatz], Frame_y[nr] - 3);                //Titeltext ausgeben
    }
    win_set_cursor(nr);                                                                       //initial Cursorposition im Fenster setzen
    Terminal.enableCursor(onoff);                                                             //Cursor wieder setzen
  }

  //#######################################################################################################################################
  //--------------------------------------------- Utility-Funktionstasten -----------------------------------------------------------------
  //#######################################################################################################################################
  //--------------------------------------------- ASCII-Zeichen ausgeben ------------------------------------------------------------------

  void char_out(int lo, int hi) {
    int z = 0;
    int i;
    char tx[10];

    for ( i = lo; i < hi ; i++ )
    {
      printnum(i, 0);
      outchar('=');
      outchar(char(i));
      outchar(' ');
      z++;
      if (z == 6) {
        z = 0;
        line_terminator();
      }
    }
    fbcolor(Vordergrund, Hintergrund);
    line_terminator();
    printmsg("OK>", 0);
  }
  //--------------------------------------------- Farbcodes ausgeben ---------------------------------------------------------------------

  void color_out(void) {
    int z = 0;
    char tx[10];

    for (int i = 0; i < 64 ; i++)
    {
      if (i == 0) fbcolor(63, 0);
      else
      {
        fbcolor(0, i);
        delay(5);                             //kleine Pause, sonst wird die Farbe nicht korrekt gesetzt
      }
      outchar(' ');
      printnum(i, 0);
      outchar(' ');
      z++;

      if (z == 8) {
        z = 0;
        line_terminator();
      }

    }
    fbcolor(Vordergrund, Hintergrund);
    line_terminator();
    printmsg("OK>", 0);
  }
  //***************************************************** Testbereich - Fill *****************************************************************

  //------------------------------------------ Befehl Fill --------------------------------------------------------------------------------------
  int fill_area(int x, int y) {
    int xx, yy, xl, xr, yo, yu;
    bool d, xl_m, xr_m = false;
    
    xl = x;
    xr = x;
    yo = y;
    yu = y;
    d = false;
/*
    while (!d) {
      
      if (Test_pixel(xl, yo, 0) == 0){//Hintergrund) {         //hat Pixel die Hintergrundfarbe?
        if (xl >= 0 && xl < GFX.getWidth() && yo >= 0) {   //innerhalb der Grenzen bleiben
          GFX.setPixel(xl, yo);                           //dann setze Pixel
          delay(1);
          xl--;                                           //ein Pixel nach links
        }
        else xl_m = true;                                 //ausserhalb der Grenzen - abbruch
      }
      else {                                              //Pixel gesetzt = rand links detektiert - abbruch
        xl_m = true;
      }

      if (Test_pixel(xr, yo, 0) == 0){//Hintergrund) {    //hat Pixel die Hintergrundfarbe?
        if (xr >= 0 && xr < GFX.getWidth() && yo >= 0) {  //innerhalb der Grenzen bleiben
          GFX.setPixel(xr, yo);                           //dann setze Pixel
          xr++;                                           //ein Pixxel nach rechts
        }
        else xr_m = true;                                 //Grenzen erreicht, dann abbruch

      }
      else {                                              //rand rechts detektiert, dann abbruch
        xr_m = true;
      }

      if (xr_m && xl_m)                                   //Grenzen rechtss und links erreicht
      {
        if (Test_pixel(x, yo - 1, 0) == 0)//Hintergrund)      //test Pixel eine Zeile höher
        {
          yo--;                                           //eine Zeile höher
          xr_m = false;                                   //rechten Grenzmarker zurücksetzen
          xl_m = false;                                   //linken Grenzmarker zurücksetzen

          //x = xr - xl ;                                 //mitte neu errechnen
          xr = x;
          xl = x;
        }
        else {
          d = true;
        }
      }


    }
    //x = xr - 1 - xl - 1;
    xl = x;
    xr = x;
    yu = y;
    d = false;
*/
    while (!d) {
      if (Test_pixel(xl, yu, 1) !=Vordergrund){//Hintergrund) {
        if (xl >= 0 && xl < GFX.getWidth() && yu < GFX.getHeight()) {
          GFX.setPixel(xl, yu);
          xl--;
        }
        else xl_m = true;

      }
      else {                        //rand links detektiert
        xl_m = true;
      }

      if (Test_pixel(xr, yu, 1) !=Vordergrund){//Hintergrund) {
        if (xr >= 0 && xr < GFX.getWidth() && yu < GFX.getHeight()) {
          GFX.setPixel(xr, yu);
          xr++;
        }
        else xr_m = true;

      }
      else {                        //rand rechts detektiert
        xr_m = true;
      }

      if (xr_m && xl_m)
      { //rand links und rechts erreicht dann eine Zeile tiefer
        if (Test_pixel(x, yu + 1, 0) == 0)//Hintergrund)
        {
          yu++;
          xr_m = false;
          xl_m = false;
          //x = xr - 1 - xl -1;
          xr = x;
          xl = x;
        }
        else {
          d = true;
        }
      }

    }
    fcolor(Vordergrund);
    return 0;
  }





  //------------------------------------------------- Prüfe, ob Pixel gesetzt ist --------------------------------------------------------------
  //->modes=0 - test Pixel gesetzt(1) oder nicht(0); modes=1 gibt die Farbe des Pixels zurück

  int Test_pixel(int x, int y, bool modes) {
    int buf[3], c;
    if (x >= 0 && x < GFX.getWidth() && y >= 0 && y < GFX.getHeight()) {
      buf[0] = GFX.getPixel(x, y).R;
      buf[1] = GFX.getPixel(x, y).G;
      buf[2] = GFX.getPixel(x, y).B;
    }
    c = (buf[2] / 85) + ((buf[1] / 85) << 2) + ((buf[0] / 85) << 4);
    if (!modes) {
      if (c == Hintergrund) //einzelne Farbanteile in 64-Farbwert zurückwandeln
        return 0;           //Pixel nicht gesetzt
      else
        return 1;           //Pixel gesetzt
    }
    else return c;          //Farbe des Pixels
  }



  //################################################## Systemparameter anzeigen ###########################################################
  void show_systemparameters(void) {
    Terminal.println();
    Terminal.write("BuiltTime : ");
    Terminal.write(BuiltTime);
    Terminal.println();
    Terminal.write("Keyboard  : ");
    Terminal.print(Keyboard_lang, DEC);
    Terminal.write("=");
    Terminal.write(Keylayout[Keyboard_lang]);
    Terminal.println();
    Terminal.write("Eeprom-Adr: #");
    Terminal.print(EEPROM.read(11), HEX);
    Terminal.println();
    Terminal.write("Workpath  : ");
    Terminal.print(sd_pfad);
    line_terminator();
    printmsg("OK>", 0);

  }
