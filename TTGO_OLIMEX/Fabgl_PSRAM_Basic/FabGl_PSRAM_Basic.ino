///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//             Basic32+ with FabGL VGA library + PS2 PS2Controller                                                                                //
//               for VGA monitor output - May 2019                                                                                                //
//          Variante mit PSRAM statt FRAM für TTGO1.4 und OLIMEX SBC                                                                              //
//      Ursprungsversion von: Rob Cai <rocaj74@gmail.com>                                                                                         //
//      erweitert/modifiziert von:Reinhard Zielinski <zille09@gmail.com>                                                                          //
//                                                                                                                                                //
//      Connections:                                                                                                                              //
//      PS2Controller Data to ESP32 pin 32;                                                                                                       //
//      PS2Controller IRQ (clock) to ESP32 pin 33;                                                                                                //
//      VGA RGB to ESP32 pin 21,22, 18,19 und 4,5                                                                                                 //
//      VGA Hsync und Vsync am ESP32 pins 23 und 15                                                                                               //
//      SD-Card 14, 16, 35, 13 (SCK, MISO, MOSI, CS)             OLIMEX-SBC   siehe cfg.h                                                         //
//      SD-Card 14, 2, 12, 13 (SCK, MISO, MOSI, CS)              TTGO 1.4                                                                         //
//                                                                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Authors: Mike Field <hamster@snap.net.nz>
//	        Scott Lawrence <yorgle@gmail.com>
//          Brian O'Dell <megamemnon@megamemnon.com>
//
// How to compile:-Arduino IDE 1.8.19
//                -ESP32 Core 1.0.6 ... 2.0.8 ... 2.0.17
//                -copy lib Files in Arduino lib Directory
//                -Partition Scheme 1,9MB Minimal SPIFFS with OTA
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
//                                          -Zeileneditor
//                                          -Syntax-Hervorhebung
//                                          -integrierte Kurzhilfe
//
// Author:Reinhard Zielinski <zille09@gmail.com>
// August 2026
//
//
//
//
#define BasicVersion "2.22"
#define BuiltTime "26.08.2026"
// siehe Logbuch.txt zum Entwicklungsverlauf
// V2.22:26.08.2026           -im Explorer sind jetzt BAS, BIN, BMP und PIC-Dateien ladbar
//                            -dies erweitert auch die Load-funktion um dieses Feature, da alle Dateien über load_file anhand der
//                            -Dateierweiterung identifiziert und geladen werden
//
// V2.21:21.08.2026           -Variablenanzeige mit MENU-Taste realisiert, zeigt die belegten Variablen und Strings im RAM an
//                            -Arrays wären noch cool, aber das ist noch etwas aufwendig
//                            -DMP Befehl um den Memorybereich 3 (Options-EEPROM) erweitert
//                            -einen Dateiexplorer eingebaut, wird über DIR oder Taste F4 aufgerufen
//                            -Scroll-Funktion innerhalb des Explorers verbessert -> es wird nur am oberen oder unteren Bildrand gescrollt
//                            -Page_up und Page_down in Explorer eingebaut
//
// V2.20:19.08.2026           -Variablenbereich im PSRAM gekapselt in einem eigenen 256kb grossen Bereich
//                            -1MB User-RAM im PSRAM eingerichtet, Bilder,loadram,saveram und Renumber benutzen diesen Bereich
//                            -dadurch ist der Variablenbereich sicher vor Überschreiben
//                            -DMP-Befehl erlaubt den Zugriff auf 0=Programmspeicher 1=Variablen-RAM 2=User-RAM ->DMP2,<adresse>
//                            -POKE,DOKE,FPOKE sowie PEEK,DEEK und FPEEK ist nur noch im User-Ram möglich/erlaubt
//                            -diese Befehle sind um die Angabe des Speicherortes gekürzt POKE 1,#1c00,Wert -> POKE #1c00,Wert
//                            -Adresse für save und load (Programm im PSRAM) auf 0x0 geändert und Berechnung der im PSRAM speicherbaren
//                            -Bilder angepasst (user_groesse - 0x10000) damit wird ein Programm im RAM nicht durch den PIC Befehl überschrieben
//                            -insgesamt sind so 12 Bilder 320x240 und ein 64kb Programm im Speicher ablegbar (FRAM_OFFSET wieder auf 0x10000)
//                            -Fehler in der OPTION Funktion behoben ->Font wurde nicht geladen
//
// V2.19:17.08.2026           -FRAM_OFFSET auf 0x20000 erhöht, so wird verhindert, das Arrays überschrieben werden, da der gleiche Speicher
//                            -verwendet wird, die ersten 64kB des Array-Bereiches bleiben unangetastet
//                            -load_address für das Ablegen eines Programms ebenfalls auf 0x20000 geändert
//                            -47031 Zeilen/sek.
//
// V2.18:15.08.2026           -Variante für TTGO1.4 und OLIMEX SBC mit PSRAM statt FRAM
//                            -ESP32-Core 2.0.8 installiert
//                            -Basic-Basisfunktionen (bis auf IO-Funktionen) funktionsfähig
//                            -47842 Zeilen/sek.
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Konfiguration Grafiktreiber und Akku-Überwachung
#include "cfg.h"   //********************************************* Konfigurations-Datei *************************************************************
#include "fabgl.h" //********************************************* Bibliotheken zur VGA-Signalerzeugung *********************************************

fabgl::Terminal         Terminal;
fabgl::LineEditor       LineEditor(&Terminal);

//---------------------------------------- die verschiedenen Grafiktreiber --------------------------------------------------------------------------
fabgl::VGAController    VGAController;      //VGA-Variante
//-------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------------ Tastatur,GFX-Treiber- und Terminaltreiber -------------------------------------------------------------
fabgl::PS2Controller    PS2Controller;
fabgl::Keyboard Keyboard;
fabgl::Canvas           GFX(&VGAController);
TerminalController      tc(&Terminal);
fabgl::SoundGenerator SoundGenerator;

const int MAX_DATEIEN = 30;
EXT_RAM_ATTR char dateiListe[MAX_DATEIEN][32]; // Platz für 30 Dateinamen mit je 31 Zeichen
int gesamtDateien = 0;
int ausgewaehlterIndex = 0;
int letzterStartSchnitt = -1;
int letzterAusgewaehlterIndex = -1;

const uint8_t colorTable[4] = {0, 85, 170, 255};    //Farbtabelle für Umwandlung Farbwerte
//-------------------------------------------------------------------------------------------------------------------------------------------------

#define erststart_marker 131                //dieser Marker steht im EEprom an Position 100 - wird der ESP32 zum ersten mal mit dem Basic gestartet werden standard-Werte gesetzt
//damit eine benutzbare Version gestartet wird
//---------------------------------------------------- verfügbare Themes ---------------------------------------------------------------------------
const char * Themes[]    PROGMEM = {"C64", "C128", "CPC", "ATARI 800", "ZX-Spectrum", "KC87", "KC85", "VIC-20", "TRS-80", "TI99", "LCD", "User"}; //Theme-Namen
const char * Keylayout[] PROGMEM = {" ", "US", "UK", "GE", "IT", "ES", "FR", "BE", "NO", "JP"};
byte x_char[]      PROGMEM = {8, 5, 6, 8,  10, 8,  8,  8,  8,  8,  8,  8,  8,  6,  8,  4, 6,  7,  7,  8, 8, 8, 6, 9, 8, 8, 6}; //x-werte der Fontsätze zur Berechnung der Terminalbreite
byte y_char[]      PROGMEM = {8, 8, 8, 14, 20, 14, 14, 16, 16, 14, 14, 14, 16, 10, 14, 6, 12, 13, 14, 9, 14, 14, 13, 15, 16, 8, 8}; //y-werte der Fontsätze zur Berechnung der Terminalhöhe

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------- Soundgenerator ----------------------------------------------------------------------------
unsigned int noteTable []  PROGMEM = {16350, 17320, 18350, 19450, 20600, 21830, 23120, 24500, 25960, 27500, 29140, 30870}; //Notentabelle für Soundausgabe
//------------------------------------------------------------- Soundgenerator ----------------------------------------------------------------------------
uint32_t startZeit;        // Variable für abbrechbaren Pause-Befehl
//----------------------------------- Editor -Marker ----------------------------------------------------------------------------------------------
bool Editor_ende = true;
//-------------------------------------------------------------------------------------------------------------------------------------------------

// ---------------------------------- SD-Karten-Zugriff--------------------------------------------------------------------------------------------
#include <SD.h>
#include <SPI.h>
//SPI CLASS FOR REDEFINED SPI PINS !
SPIClass spiSD(HSPI);
File fp;
#include <vector>
#include <algorithm>
//-------------------------------------------------------------------------------------------------------------------------------------------------
//------------------------------------- OTA-Update-Lib --------------------------------------------------------------------------------------------
#include <Update.h>
//-------------------------------------------------------------------------------------------------------------------------------------------------


//------------------------------------- ESP32-Time-Lib fuer Datei-zeitstempel ---------------------------------------------------------------------
#include <ESP32Time.h>
ESP32Time e_rtc(0);  // offset in seconds GMT+1
//-------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------- Mathematische Funktionen fuer printnum --------------------------------------------------------------------
#include "MathHelpers.h"
//-------------------------------------------------------------------------------------------------------------------------------------------------

// -------------------------- EEPROM Routinen für Parameter-Speicherung ---------------------------------------------------------------------------
#include <EEPROM.h>
#define EEPROM_SIZE 512  //512 byte lesen/speichern
//-------------------------------------------------------------------------------------------------------------------------------------------------

//######################################### Anfang Konfiguration SPI-RAM ##########################################################################
//------------------------------------------- SPI-RAM-Lib -----------------------------------------------------------------------------------------
float* var_table_psram = nullptr;
uint8_t* user_psram = nullptr;
uint32_t user_groesse = 0x100000;     //User-RAM 1MB
uint32_t variablen_groesse = 0x40000; //Variablen-Ram 256kb (Platz für 65435 Variablen)

uint32_t SPI_memSize;                 //ermittelte Variablen-Ram-Grösse
uint32_t renum_addr = 0x10004;        //Adresse ab der Renumber arbeitet
uint32_t fram_ptr;                    //Pointer für Renumber
unsigned int zeilen_anzahl;           //für Renumber

//---------------------------------------- spezielle SPI-Ram-Adressen -----------------------------------------------------------------------------
word FRAM_OFFSET      = 0x10000;      //Offset für Poke-Anweisungen, um zu verhindern, das in den Array-Bereich gepoked wird
word FRAM_PIC_OFFSET  ;               //Platz pro Bildschirm im Speicher 320x240=76800 + 4Byte für die Dimension = 76804 --> siehe Memory_RW
long load_adress      = 0x0;          //ab hier kann ein Basicprogramm abgelegt werden (Eingabe: LOAD oder SAVE ohne Parameter)

//---------------------------------------- Array-Parameter ----------------------------------------------------------------------------------------
//Der Arraybereich befindet sich 0x0..0x7fff
word Var_Neu_Platz =  0;            //Adresse nächstes Array-Feld Start bei 0x0
static word VAR_TBL = 0xC000;       //Variablen-Array-Tabelle im SPI-RAM 4kb
static word STR_TBL = 0xE000;       //String-Array-Tabelle im SPI-RAM 4kb
static word VAR_MAX = 0xBFFF;       //Variablengrenze bei 0xBFFF 49kb
//-------------------------------------------------------------------------------------------------------------------------------------------------
//######################################### Ende Konfiguration SPI-RAM ############################################################################

//------------------------------- Konfiguration serielle Schnittstelle ----------------------------------------------------------------------------
uint8_t prx, ptx;             //RX- und TX-Pin
uint32_t pbd;                 //Baudrate
bool ser_marker = false;      //seriell-Marker, wenn gesetzt erfolgt jede Printausgabe auch auf die serielle Schnittstelle
bool list_send = false;
bool serout_marker = false;
#define SERIAL_SIZE_RX 1024
//-------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------ Startparameterauswahl --------------------------------------------------------------------------
byte Keyboard_lang = KLayout; //Tastatur-Layout (cfg.h) - Standardeinstellung=German
byte THEME_SET = 77;    //-steht 77 im EEPROM Platz 17, dann setze das gespeicherte Theme
byte PATH_SET = 88;     //-steht 88 im EEPROM Platz 19, dann setze Arbeits-Pfad
//-------------------------------------------------------------------------------------------------------------------------------------------------

int currentIndent = 0;        //Einrückungsmerker für For-Next

//------------------------------------- Akku-Überwachung ------------------------------------------------------------------------------------------
/*
  #ifdef Akkualarm_enabled            // Akku-Überwachung für batteriebetriebene Geräte
  hw_timer_t *Akku_timer = NULL;      //Interrupt-Routine Akku-Überwachung
  #endif
  #define Batt_Pin 39                 //Pin wird in jedem Fall definiert
*/
//-------------------------------------------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------------------------------------------
#define COL_RESET  "\e[0m"
#define COL_KEYW   "\e[33m" // Gelb für Keywords (PRINT, IF...)
#define COL_FUNC   "\e[36m" // Cyan für Funktionen (ABS, SIN...)
#define COL_VAR    "\e[32m" // Grün für Variablen
#define COL_STR    "\e[31m" // Rot für Strings "..."
#define COL_NUM    "\e[35m" // Magenta für Zahlen

//------------------------------- BMP-Info-Parameter für PIC-Befehl -------------------------------------------------------------------------------
uint32_t bmp_width, bmp_height;
//-------------------------------------------------------------------------------------------------------------------------------------------------

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

bool inhibitOutput = false;             //Sichtbarkeit an/aus (für outchar auf sd-karte oder terminal)
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

static bool break_marker = false;       //Abbruch-Marker
static bool function_key = false;       //Funktionstasten-Marker
static bool show_vars    = false;       //Anzeige der Variablen über Menue-Taste
static bool cursor_up    = false;
static bool cursor_down  = false;
static bool page_up      = false;
static bool page_down    = false;

//------------------------------ Grid-Parameter ---------------------------------------------------------------------------------------------------
int Grid[15];                          //0=x, 1=y, 2=xx, 3=yy, 4=zell_x, 5=zell_y, 6=pix_x, 7=pix_y, 8=frame-col, 9=grid_col
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

// Ausgabe-Stream Terminal, Datei, Seriell oder FRAM
enum {
  kStreamTerminal = 0,
  kStreamFile,
  kStreamSerial,
};
static char inStream = kStreamTerminal;    //Eingabe an Terminal senden
static char outStream = kStreamTerminal;   //Ausgabe an Terminal senden
static char Stringtable[STR_SIZE];         //Stringvariablen mit 1 Buchstaben -> 26*40 = 1040 Bytes
//-------------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------- DEFN - FN -------------------------------------------------------------------------------------------------
static char Fntable[26][FN_SIZE];         // bytes 40 String = 40*26 ->1040 bytes ->Funktionsstring-Array
int Fnvar = 0;                            //Operatorenzähler
int Fnoperator[27 * 5];                   //DEFN A(a,b,c,d,e,f,g,h)-> Name 0-26,0-26=Operator1,0-26=operator2,0-26=Operator3,0-26=Operator4 + 1 Anzahl
bool fn_marker = false;                   //Funktions-Marker
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

// Variablen zur Zwischenspeicherung von logischen Operationen (AND OR)

int logic_counter;
int logic_ergebnis[10];

//-> bis zu 5 AND oder OR Vergleiche können in einer Zeile vorkommen



bool useColor = true;         //Text-Highlightning on

//#define User_Ram (kRamSize-STACK_SIZE-(26 * 27 * VAR_SIZE))

/***************************Basic-Befehle ********************************/
const static char keywords[] PROGMEM = {
  'A', 'N', 'D' + 0x80,
  'A', 'N', 'G', 'L', 'E' + 0x80,
  'A', 'R', 'C' + 0x80,
  'B', 'E', 'E', 'P' + 0x80,
  'C', 'H', 'D' + 0x80,
  'C', 'I', 'R', 'C' + 0x80,
  'C', 'L', 'E', 'A', 'R' + 0x80,
  'C', 'L', 'O', 'S', 'E' + 0x80,
  'C', 'L', 'S' + 0x80,
  'C', 'O', 'L' + 0x80,
  'C', 'O', 'M' + 0x80,
  'C', 'O', 'P', 'Y' + 0x80,
  'C', 'U', 'R' + 0x80,
  'D', 'A', 'T', 'A' + 0x80,
  'D', 'E', 'F', 'N' + 0x80,
  'D', 'E', 'L' + 0x80,
  'D', 'I', 'M' + 0x80,
  'D', 'I', 'R' + 0x80,
  'D', 'M', 'P' + 0x80,
  'D', 'O', 'K', 'E' + 0x80,
  'D', 'R', 'A', 'W' + 0x80,
  'E', 'D', 'I', 'T' + 0x80,
  'E', 'L', 'S', 'E' + 0x80,
  'E', 'N', 'D' + 0x80,
  'F', 'I', 'L', 'E' + 0x80,
  'F', 'O', 'N', 'T' + 0x80,
  'F', 'O', 'R' + 0x80,
  'F', 'P', 'O', 'K', 'E' + 0x80,
  'F', 'R', 'A', 'M', 'E' + 0x80,
  'G', 'O', 'S', 'U', 'B' + 0x80,
  'G', 'O', 'T', 'O' + 0x80,
  'G', 'R', 'I', 'D' + 0x80,
  'H', 'E', 'L', 'P' + 0x80,
  'I', 'F' + 0x80,
  'I', 'N', 'P', 'U', 'T' + 0x80,
  'L', 'I', 'N', 'E' + 0x80,
  'L', 'I', 'S', 'T' + 0x80,
  'L', 'O', 'A', 'D' + 0x80,
  'L', 'O', 'C', 'A', 'T', 'E' + 0x80,
  'M', 'K', 'D' + 0x80,
  'M', 'N', 'T' + 0x80,
  'N', 'E', 'W' + 0x80,
  'N', 'E', 'X', 'T' + 0x80,
  'O', 'N' + 0x80,
  'O', 'P', 'E', 'N' + 0x80,
  'O', 'P', 'T' + 0x80,
  'O', 'R' + 0x80,
  'P', 'A', 'T', 'H' + 0x80,
  'P', 'A', 'U', 'S', 'E' + 0x80,
  'P', 'E', 'N' + 0x80,
  'P', 'I', 'C' + 0x80,
  'P', 'O', 'K', 'E' + 0x80,
  'P', 'O', 'S' + 0x80,
  'P', 'R', 'I', 'N', 'T' + 0x80,
  'P', 'R', 'Z' + 0x80,
  'P', 'S', 'E', 'T' + 0x80,
  'R', 'E', 'A', 'D' + 0x80,
  'R', 'E', 'C', 'T' + 0x80,
  'R', 'E', 'M' + 0x80,
  'R', 'E', 'N', 'A', 'M', 'E' + 0x80,
  'R', 'E', 'N', 'U', 'M' + 0x80,
  'R', 'E', 'S', 'T', 'O', 'R', 'E' + 0x80,
  'R', 'E', 'T', 'U', 'R', 'N' + 0x80,
  'R', 'M', 'D' + 0x80,
  'R', 'T', 'C' + 0x80,
  'R', 'U', 'N' + 0x80,
  'S', 'A', 'V', 'E' + 0x80,
  'S', 'C', 'R', 'O', 'L', 'L' + 0x80,
  'S', 'N', 'D' + 0x80,
  'S', 'P', 'R', 'T' + 0x80,
  'S', 'T', 'Y', 'L', 'E' + 0x80,
  'S', 'W', 'A', 'P' + 0x80,
  'T', 'E', 'X', 'T' + 0x80,
  'T', 'H', 'E', 'M', 'E' + 0x80,
  'T', 'H', 'E', 'N' + 0x80,
  'T', 'Y', 'P', 'E' + 0x80,
  'W', 'I', 'N', 'D', 'O', 'W' + 0x80,
  0
};


enum {
  KW_AND = 0,
  KW_ANGLE,
  KW_ARC,
  KW_BEEP,
  KW_CHD,
  KW_CIRC,
  KW_CLEAR,
  KW_CLOSE,
  KW_CLS,
  KW_COL,
  KW_COM,       //10
  KW_COPY,
  KW_CUR,
  KW_DATA,
  KW_DEFN,
  KW_DEL,
  KW_DIM,
  KW_DIR,
  KW_DMP,
  KW_DOKE,
  KW_DRAW,      //20
  KW_EDIT,
  KW_ELSE,
  KW_END,
  KW_FILE,
  KW_FONT,
  KW_FOR,
  KW_FPOKE,
  KW_FRAME,
  KW_GOSUB,
  KW_GOTO,      //30
  KW_GRID,
  KW_HELP,
  KW_IF,
  KW_INPUT,
  KW_LINE,
  KW_LIST,
  KW_LOAD,
  KW_LOCATE,
  KW_MKD,
  KW_MNT,       //40
  KW_NEW,
  KW_NEXT,
  KW_ON,
  KW_OPEN,
  KW_OPT,
  KW_OR,
  KW_PATH,
  KW_PAUSE,
  KW_PEN,
  KW_PIC,       //50
  KW_POKE,
  KW_POS,
  KW_PRINT,
  KW_PRZ,
  KW_PSET,
  KW_READ,
  KW_RECT,
  KW_REM,
  KW_RENAME,
  KW_RENUM,     //60
  KW_RESTORE,
  KW_RETURN,
  KW_RMD,
  KW_RTC,
  KW_RUN,
  KW_SAVE,
  KW_SCROLL,
  KW_SND,
  KW_SPRT,
  KW_STYLE,     //70
  KW_SWAP,
  KW_TEXT,
  KW_THEME,
  KW_THEN,
  KW_TYPE,
  KW_WINDOW,
  KW_COUNT // Ergibt 77
};

int KW_WORDS = KW_COUNT;  //77

static uint16_t kw_offsets[KW_COUNT];
const uint8_t kw_id_map[] PROGMEM = {
  KW_AND,      // 0: AND
  KW_ANGLE,    // 1: ANGLE
  KW_ARC,      // 2: ARC
  KW_BEEP,     // 3: BEEP
  KW_CHD,      // 4: CHD
  KW_CIRC,     // 5: CIRC
  KW_CLEAR,    // 6: CLEAR
  KW_CLOSE,    // 7: CLOSE
  KW_CLS,      // 8: CLS
  KW_COL,      // 9: COL
  KW_COM,      // 10: COM
  KW_COPY,     // 11: COPY
  KW_CUR,      // 12: CUR
  KW_DATA,     // 13: DATA
  KW_DEFN,     // 14: DEFN
  KW_DEL,      // 15: DEL
  KW_DIM,      // 16: DIM
  KW_DIR,      // 17: DIR
  KW_DMP,      // 18: DMP
  KW_DOKE,     // 19: DOKE
  KW_DRAW,     // 20: DRAW
  KW_EDIT,     // 21: EDIT
  KW_ELSE,     // 22: ELSE
  KW_END,      // 23: END
  KW_FILE,     // 24: FILE
  KW_FONT,     // 25: FONT
  KW_FOR,      // 26: FOR
  KW_FPOKE,    // 27: FPOKE
  KW_FRAME,    // 28: FRAME
  KW_GOSUB,    // 29: GOSUB
  KW_GOTO,     // 30: GOTO
  KW_GRID,     // 31: GRID
  KW_HELP,     // 32: HELP
  KW_IF,       // 33: IF
  KW_INPUT,    // 34: INPUT
  KW_LINE,     // 35: LINE
  KW_LIST,     // 36: LIST
  KW_LOAD,     // 37: LOAD
  KW_LOCATE,   // 38: LOCATE
  KW_MKD,      // 39: MKD
  KW_MNT,      // 40: MNT
  KW_NEW,      // 41: NEW
  KW_NEXT,     // 42: NEXT
  KW_ON,       // 43: ON
  KW_OPEN,     // 44: OPEN
  KW_OPT,      // 45: OPT
  KW_OR,       // 46: OR
  KW_PATH,     // 47: PATH
  KW_PAUSE,    // 48: PAUSE
  KW_PEN,      // 49: PEN
  KW_PIC,      // 50: PIC
  KW_POKE,     // 51: POKE
  KW_POS,      // 52: POS
  KW_PRINT,    // 53: PRINT
  KW_PRZ,      // 54: PRZ
  KW_PSET,     // 55: PSET
  KW_READ,     // 56: READ
  KW_RECT,     // 57: RECT
  KW_REM,      // 58: REM
  KW_RENAME,   // 59: RENAME
  KW_RENUM,    // 60: RENUM
  KW_RESTORE,  // 61: RESTORE
  KW_RETURN,   // 62: RETURN
  KW_RMD,      // 63: RMD
  KW_RTC,      // 64: RTC
  KW_RUN,      // 65: RUN
  KW_SAVE,     // 66: SAVE
  KW_SCROLL,   // 67: SCROLL
  KW_SND,      // 68: SND
  KW_SPRT,     // 69: SPRT
  KW_STYLE,    // 70: STYLE
  KW_SWAP,     // 71: SWAP
  KW_TEXT,     // 72: TEXT
  KW_THEME,    // 73: THEME
  KW_THEN,     // 74: THEN
  KW_TYPE,     // 75: TYPE
  KW_WINDOW    // 76: WINDOW
};


//**************************** Basic-Funktionen **********************************************************************************************************************************************
//****** HINWEIS: nicht benötigte Befehle können nicht einfach gelöscht oder deaktiviert werden (eher umbenennen), da die nachfolgenden Befehls-ID's sonst nicht mehr stimmen.****************
//*************** umbenannte Befehle müssen in allen Func_tabellen exakt in der gleichen alphabetischen Reihenfolge eingefügt werden,                                         ****************
//*************** damit der Interpreter die richtige Funktion aufruft !!!                                                                                                     ****************
//*************** Neue Befehle werden ebenso behandelt - die Reihenfolge in der enum-Tabelle entspricht der ID in der id_map                                                  ****************
//********************************************************************************************************************************************************************************************
const static char func_tab[] PROGMEM = {
  '!' + 0x80,                       // NOT (0)
  'A', 'B', 'S' + 0x80,             // ABS
  'A', 'I', 'N' + 0x80,             // AREAD
  'A', 'S', 'C' + 0x80,             // ASC
  'A', 'T', 'N' + 0x80,             // ATAN
  'B', 'I', 'N' + 0x80,             // BIN
  'C', 'H', 'R', '$' + 0x80,        // CHR
  'C', 'O', 'L' + 0x80,             // GETCOL
  'C', 'O', 'M', 'P', '$' + 0x80,   // COMPARE
  'C', 'O', 'N', 'S' + 0x80,        // CONSTRAIN
  'C', 'O', 'S' + 0x80,             // COS   (10)
  'D', 'A', 'T', 'E' + 0x80,        // GDATE
  'D', 'E', 'E', 'K' + 0x80,        // DEEK
  'E', 'X', 'P' + 0x80,             // EXP
  'F', 'I', 'L', 'E' + 0x80,        // FILE
  'F', 'N' + 0x80,                  // FN
  'F', 'O', 'N', 'T' + 0x80,        // FONT
  'F', 'P', 'E', 'E', 'K' + 0x80,   // FPEEK
  'G', 'E', 'T' + 0x80,             // GET
  'G', 'P', 'I', 'C' + 0x80,        // PIC
  'G', 'P', 'X' + 0x80,             // GPIX  (20)
  'G', 'R', 'I', 'D' + 0x80,        // GRID
  'H', 'E', 'X' + 0x80,             // HEX
  'I', 'N', 'K', 'E', 'Y' + 0x80,   // INKEY
  'I', 'N', 'S', 'T', 'R' + 0x80,   // INSTR
  'I', 'N', 'T' + 0x80,             // INT
  'L', 'C', '$' + 0x80,             // LCASE
  'L', 'E', 'F', 'T', '$' + 0x80,   // LEFT
  'L', 'E', 'N' + 0x80,             // LEN
  'L', 'N' + 0x80,                  // LN
  'L', 'O', 'G' + 0x80,             // LOG   (30)
  'M', 'A', 'P' + 0x80,             // MAP
  'M', 'A', 'X' + 0x80,             // MAX
  'M', 'E', 'M' + 0x80,             // MEM
  'M', 'I', 'D', '$' + 0x80,        // MID
  'M', 'I', 'N' + 0x80,             // MIN
  'P', 'E', 'E', 'K' + 0x80,        // PEEK
  'P', 'I' + 0x80,                  // PI
  'R', 'I', 'G', 'H', 'T', '$' + 0x80, // RIGHT
  'R', 'N', 'D' + 0x80,             // RND
  'S', 'G', 'N' + 0x80,             // SGN  (40)
  'S', 'I', 'N' + 0x80,             // SIN
  'S', 'P', 'C' + 0x80,             // SPC
  'S', 'Q', 'R' + 0x80,             // SQR
  'S', 'T', 'R', '$' + 0x80,        // STR
  'S', 'T', 'R', 'I', 'N', 'G', '$' + 0x80, // STRING
  'T', 'A', 'B' + 0x80,             // TAB
  'T', 'A', 'N' + 0x80,             // TAN
  'T', 'I', 'M', 'E' + 0x80,        // GTIME
  'T', 'I', 'M', 'E', 'R' + 0x80,   // TIMER
  'U', 'C', '$' + 0x80,             // UCASE (50)
  'V', 'A', 'L' + 0x80,             // VAL   (51)
  0
};

enum {
  FUNC_NOT = 0,   // !
  FUNC_ABS,       // ABS
  FUNC_AREAD,     // AIN
  FUNC_ASC,       // ASC
  FUNC_ATAN,      // ATAN
  FUNC_BIN,       // BIN
  FUNC_CHR,       // CHR$
  FUNC_GETCOL,    // COL
  FUNC_COMPARE,   // COMP$
  FUNC_CONSTRAIN, // CONS
  FUNC_COS,       // COS
  FUNC_GDATE,     // DATE
  FUNC_DEEK,      // DEEK
  FUNC_EXP,       // EXP
  FUNC_FILE,      // FILE
  FUNC_FN,        // FN
  FUNC_FONT,      // FONT
  FUNC_FPEEK,     // FPEEK
  FUNC_GET,       // GET
  FUNC_PIC,       // GPIC
  FUNC_GPIX,      // GPIX
  FUNC_GRID,      // GRID
  FUNC_HEX,       // HEX
  FUNC_INKEY,     // INKEY
  FUNC_INSTR,     // INSTR
  FUNC_INT,       // INT
  FUNC_LCASE,     // LC$
  FUNC_LEFT,      // LEFT$
  FUNC_LEN,       // LEN
  FUNC_LN,        // LN
  FUNC_LOG,       // LOG
  FUNC_MAP,       // MAP
  FUNC_MAX,       // MAX
  FUNC_MEM,       // MEM
  FUNC_MID,       // MID$
  FUNC_MIN,       // MIN
  FUNC_PEEK,      // PEEK
  FUNC_PI,        // PI
  FUNC_RIGHT,     // RIGHT$
  FUNC_RND,       // RND
  FUNC_SGN,       // SGN
  FUNC_SIN,       // SIN
  FUNC_SPC,       // SPC
  FUNC_SQR,       // SQR
  FUNC_STR,       // STR$
  FUNC_STRING,    // STRING$
  FUNC_TAB,       // TAB
  FUNC_TAN,       // TAN
  FUNC_GTIME,     // TIME
  FUNC_TIMER,     // TIMER
  FUNC_UCASE,     // UC$
  FUNC_VAL,       // VAL
  FUNC_UNKNOWN    // 52
};

int FUNC_WORDS = FUNC_UNKNOWN;
static uint16_t func_offsets[FUNC_UNKNOWN]; // 53 Funktionen laut deinem Enum

const uint8_t func_id_map[] PROGMEM = {
  FUNC_NOT,       // !
  FUNC_ABS,       // ABS
  FUNC_AREAD,     // AIN
  FUNC_ASC,       // ASC
  FUNC_ATAN,      // ATAN
  FUNC_BIN,       // BIN
  FUNC_CHR,       // CHR$
  FUNC_GETCOL,    // COL
  FUNC_COMPARE,   // COMP$
  FUNC_CONSTRAIN, // CONS
  FUNC_COS,       // COS
  FUNC_GDATE,     // DATE
  FUNC_DEEK,      // DEEK
  FUNC_EXP,       // EXP
  FUNC_FILE,      // FILE
  FUNC_FN,        // FN
  FUNC_FONT,      // FONT
  FUNC_FPEEK,     // FPEEK
  FUNC_GET,       // GET
  FUNC_PIC,       // GPIC
  FUNC_GPIX,      // GPIX
  FUNC_GRID,      // GRID
  FUNC_HEX,       // HEX
  FUNC_INKEY,     // INKEY
  FUNC_INSTR,     // INSTR
  FUNC_INT,       // INT
  FUNC_LCASE,     // LC$
  FUNC_LEFT,      // LEFT$
  FUNC_LEN,       // LEN
  FUNC_LN,        // LN
  FUNC_LOG,       // LOG
  FUNC_MAP,       // MAP
  FUNC_MAX,       // MAX
  FUNC_MEM,       // MEM
  FUNC_MID,       // MID$
  FUNC_MIN,       // MIN
  FUNC_PEEK,      // PEEK
  FUNC_PI,        // PI
  FUNC_RIGHT,     // RIGHT$
  FUNC_RND,       // RND
  FUNC_SGN,       // SGN
  FUNC_SIN,       // SIN
  FUNC_SPC,       // SPC
  FUNC_SQR,       // SQR
  FUNC_STR,       // STR$
  FUNC_STRING,    // STRING$
  FUNC_TAB,       // TAB
  FUNC_TAN,       // TAN
  FUNC_GTIME,     // TIME
  FUNC_TIMER,     // TIMER
  FUNC_UCASE,     // UC$
  FUNC_VAL        // VAL
};

//------------------------------- OPTION-Tabelle - alle Optionen, die dauerhaft gespeichert werden sollen -----------------------------------------
static uint16_t opt_offsets[5]; // RAM-Speicher für die Startpositionen

const static char options_tab[] PROGMEM = {
  'C', 'O', 'L', 'O', 'R' + 0x80,      // OPT_COLOR
  'F', 'O', 'N', 'T' + 0x80,           // OPT_FONT
  'K', 'E', 'Y' + 0x80,                // OPT_KEYBOARD
  'P', 'A', 'T', 'H' + 0x80,           // OPT_PATH
  'T', 'H', 'E', 'M', 'E' + 0x80,      // OPT_THEME
  0
};

#define OPT_COLOR 0
#define OPT_FONT 1
#define OPT_KEYBOARD 2
#define OPT_PATH 3
#define OPT_THEME 4
#define OPT_COUNT 5

// Die IDs passend zur alphabetischen Sortierung
const uint8_t opt_id_map[] PROGMEM = {
  OPT_COLOR,
  OPT_FONT,
  OPT_KEYBOARD,
  OPT_PATH,
  OPT_THEME,
};

//-------------------------------------------------------------------------------------------------------------------------------------------------

// Sortierung: Erst nach 1. Zeichen (ASCII), dann Länge (2-Zeichen vor 1-Zeichen)
const char relop_tab[] PROGMEM = {
  '%', 0,   // MOD   (ID 9)
  '&', 0,   // AND   (ID 10)
  '<', '<', // SHL   (ID 6)
  '<', '=', // LE    (ID 5)
  '<', '>', // NE    (ID 1)
  '<', 0,   // LT    (ID 8)
  '=', 0,   // EQ    (ID 4)
  '>', '>', // SHR   (ID 2)
  '>', '=', // GE    (ID 0)
  '>', 0,   // GT    (ID 3)
  '^', 0,   // POW   (ID 12)
  '|', '|', // XOR   (ID 7)
  '|', 0    // OR    (ID 11)
};

#define RELOP_GE 0
#define RELOP_NE 1
#define RELOP_SHR 2
#define RELOP_GT 3
#define RELOP_EQ 4
#define RELOP_LE 5
#define RELOP_SHL 6
#define RELOP_XOR 7
#define RELOP_LT 8
#define RELOP_MOD 9
#define RELOP_AND 10
#define RELOP_OR 11
#define RELOP_POW 12
#define RELOP_UNKNOWN  13

// Die IDs exakt nach deiner Definition
const uint8_t relop_id[] PROGMEM = {
  9,  // MOD
  10, // AND
  6,  // SHL
  5,  // LE
  1,  // NE
  8,  // LT
  4,  // EQ
  2,  // SHR
  0,  // GE
  3,  // GT
  12, // POW
  7,  // XOR
  11  // OR
};

struct stack_for_frame {
  char frame_type;    // 1 Byte (wird vom Compiler oft auf 4 Byte aufgefüllt/Padding)
  int for_var;        // 4 Byte (Index der Variable A-Z)
  float to_var;       // 4 Byte (Zielwert)
  float step;         // 4 Byte (Schrittweite)
  char *current_line; // 4 Byte (Pointer auf den Zeilenanfang)
  char *txtpos;       // 4 Byte (Pointer auf die Position nach dem FOR)
}; // Gesamtgröße: ca. 24 Byte pro Frame

struct stack_gosub_frame {
  char frame_type;
  char *current_line;
  char *txtpos;
};


#define STACK_SIZE (sizeof(struct stack_for_frame)*26)   // 26 verschachtelte For-Next-Schleifen erlaubt (32bytes pro frame) 
uint8_t stack_memory[STACK_SIZE];                        // Der eigentliche Speicherblock für den Stack
char *stack_top = (char *)(stack_memory + STACK_SIZE);   // Zeiger auf das Ende des Blocks (da der Stack nach UNTEN wächst)
char *stack_limit = (char *)stack_memory;                // Die untere Grenze (darf nicht unterschritten werden)
char *sp = stack_top;                                    // Der aktuelle Stackpointer startet ganz oben

#define VAR_SIZE sizeof(float)                           // Variablengrösse für float 4Bytes
static char program[kRamSize];
static char *program_start;
static char *program_end;
static char *stack; // Software stack for things that should go on the CPU stack
static char *variables_begin;
static char *current_line;
static char *data_line;
//static char *sp;
#define STACK_GOSUB_FLAG 'G'
#define STACK_FOR_FLAG 'F'
static char table_index;
static char keyword_index;
static char key_command;
static LINENUM linenum;

//---------------------------------- Fehlermeldungen des Interpreters -----------------------------------------------------------------------------

static const char syntaxmsg[]        PROGMEM = "Syntax Error! ";                //1
static const char mathmsg[]          PROGMEM = "Math Error!";                   //2
static const char gosubmsg[]         PROGMEM = "Gosub Error!";                  //3
static const char fornextmsg[]       PROGMEM = "For-Next Error!";               //4
static const char memorymsg[]        PROGMEM = " bytes free";
static const char missing_then[]     PROGMEM = "Missing THEN!";                 //5
static const char breakmsg[]         PROGMEM = "Break! in Line:";
static const char breaks[]           PROGMEM = "Break!";
static const char datamsg[]          PROGMEM = "Out of DATA!";                  //6
static const char invalidmsg[]       PROGMEM = "Invalid comparison!";           //7
static const char sderrormsg[]       PROGMEM = "SD card Error.";                //8
static const char sdfilemsg[]        PROGMEM = "SD file Error.";                //9
static const char dirextmsg[]        PROGMEM = "(dir)";
static const char slashmsg[]         PROGMEM = "/";
static const char spacemsg[]         PROGMEM = " ";
static const char notexistmsg[]      PROGMEM = "File not exist!";               //10
static const char portmsg[]          PROGMEM = "Wrong Port-Number!";            //11
static const char valmsg[]           PROGMEM = "Invalid Value!";                //12
static const char dirmsg[]           PROGMEM = "Dir not empty!";                //13
static const char illegalmsg[]       PROGMEM = "Illegal quantity!";             //14
static const char zeroerror[]        PROGMEM = "Div/0-Error!";                  //15
static const char outofmemory[]      PROGMEM = "Out of Memory!";                //16
static const char mountmsg[]         PROGMEM = "SD-Card mounted";               //17
static const char notmount[]         PROGMEM = "SD-Card can't mount";           //18
static const char dimmsg[]           PROGMEM = "Array-Dimension!";              //19
static const char commsg[]           PROGMEM = "No COM-Port defined!";          //20
static const char comsetmsg[]        PROGMEM = "Wrong COM-Port Definition!";    //21
static const char bmpfilemsg[]       PROGMEM = "No BMP-File!";                  //22
static const char no_prg_msg[]       PROGMEM = "No Program in Memory!";         //23
static const char no_command_msg[]   PROGMEM = "Keyword not found!";            //24
static const char not_openmsg[]      PROGMEM = "File not open!";                //25
static const char dirnotfound[]      PROGMEM = "DIR not found !";               //26
static const char extension_error[]  PROGMEM = "invalid File-Extension !";      //27
static const char stringtolong[]     PROGMEM = "String to long!";               //28
static const char wronglinenr[]      PROGMEM = "Wrong Line-Number!";            //29

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
static void syntaxerror(const char *msg)
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

static char skip_spaces() {
  if (*txtpos)
  {
    txtpos++;
    return spaces();
  }
}

//--------------------------------------------- Unterprogramm - Leerzeichen überspringen ----------------------------------------------------------
//--------------------------------------------- erstes gültiges Zeichen zurückgeben ---------------------------------------------------------------

static char spaces() {
  while (*txtpos == SPACE || *txtpos == TAB) {
    txtpos++;
  }
  return *txtpos; // Das erste Zeichen, das kein Leerzeichen/Tab ist
}

//--------------------------------------------- Unterprogramm - Zahlenausgabe ---------------------------------------------------------------------

void printnum(float num, int modes) {
  char c[32];

  switch (modes) {
    case 0: // Normale Zahlenausgabe
      if (num > 9999999.0f || num < -9999999.0f) {
        printmsg(sci(num, Prezision), 0); // Exponentiell
      }
      else if (num == (float)((long)num)) {
        // Saubere Integer-Ausgabe ohne tmp-Variable
        snprintf(c, sizeof(c), "%ld", (long)num);
        printmsg(c, 0);
      }
      else {
        // Fließkomma mit variabler Präzision
        snprintf(c, sizeof(c), "%.*f", Prezision, num);

        // Nullen am Ende abschneiden (In-place statt String-Objekt)
        char* p = c + strlen(c) - 1;
        while (p > c && *p == '0') *p-- = '\0';
        if (*p == '.') *p = '\0'; // Punkt entfernen, falls nichts folgt

        printmsg(c, 0);
      }
      break;

    case 1: // Binär
      outchar('%');
      // itoa ist auf ESP32 vorhanden, Basis 2 funktioniert
      itoa((long)num, c, 2);
      printmsg(c, 0);
      break;

    case 2: // Hex mit #
      outchar('#');
    // Fall-through zu Case 3
    case 3: // Hex ohne # für Memory_Dump
      snprintf(c, sizeof(c), "%lx", (long)num); // %lx für unsigned long/hex
      printmsg(c, 0);
      break;
  }
  Zahlenformat = 0; // Einmalig am Ende setzen spart Code-Duplikate
}


//#######################################################################################################################################
//--------------------------------------------- DUMP - Befehl ---------------------------------------------------------------------------
//#######################################################################################################################################

static int Memory_Dump() {                       //DMP Speichertyp 0..2 <,Adresse>
  int ex = 0, c, was, tpm;
  int ln = (VGAController.getScreenHeight() / y_char[fontsatz]) - 3; //Anzahl Zeilen abhängig vom Fontsatz
  int x_weite = VGAController.getScreenWidth() / x_char[fontsatz];
  byte rdbyte[8];

  long n;
  long adr = 0;
  was = abs(int(get_value()));                  //nur ganze Zahlen
  if (*txtpos == ',')
  {
    txtpos++;
    adr = abs(get_value());                      //nur ganze Zahlen
  }

  if (was > 3) {
    syntaxerror(syntaxmsg);
    return 1;
  }
  while (!ex) {
    for (int i = 1; i < ln; i++)
    {
      if (adr < 0x10000)outchar('0');
      if (adr < 0x1000)outchar('0');
      if (adr < 0x100)outchar('0');
      if (adr < 0x10)outchar('0');
      n = adr;
      printnum(adr, 3);                                 //Hexausgabe
      outchar(' ');

      for (int f = 0; f < 8; f++) {                     //8 Speicherplätze lesen und anzeigen
        switch (was) {
          case 1:  //Variablen-PSRAM
            if (spi_fram_read8(n) < 16) outchar('0');
            printnum(spi_fram_read8(n++), 3);
            if (x_weite > 39) outchar(' ');                       //wenn genug Platz, dann Leerzeichen zwischen den Werten
            if (n > SPI_memSize) n = 0;                           //letzte Speicherstelle erreicht?, dann von vorn
            break;
          case 2:  //USER-PSRAM
            if (user_fram_read8(n) < 16) outchar('0');
            printnum(user_fram_read8(n++), 3);
            if (x_weite > 39) outchar(' ');                       //wenn genug Platz, dann Leerzeichen zwischen den Werten
            if (n > user_groesse) n = 0;                          //letzte Speicherstelle erreicht?, dann von vorn
            break;
          case 3:  //OPTION EEPROM
            c = EEPROM.read(n++);
            if (c < 16) outchar('0');
            printnum(c, 3);
            if (x_weite > 39) outchar(' ');                       //wenn genug Platz, dann Leerzeichen zwischen den Werten
            if (n > 512) n = 0;
            break;
          default:  //interner RAM
            if (program[int(n)] < 16) outchar('0');
            printnum(program[int(n++)], 3);
            if (x_weite > 39) outchar(' ');
            if (n > kRamSize) n = 0;                              //letzte Speicherstelle erreicht?, dann von vorn
            break;
        }
      }
      if (x_weite < 40) outchar(' ');                             //wenn nicht genug Platz, dann nur ein Leerzeichen nach 8Bytes


      for (int i = 0; i < 8; i++) {
        switch (was) {
          case 1:  //Variablen-PSRAM
            c = spi_fram_read8(adr++);
            if (adr > SPI_memSize) adr = 0;                       //letzte Speicherstelle erreicht?, dann von vorn
            break;
          case 2:  //USER-PSRAM
            c = user_fram_read8(adr++);
            if (adr > user_groesse) adr = 0;                      //letzte Speicherstelle erreicht?, dann von vorn
            break;
          case 3:
            c = EEPROM.read(adr++);
            if (adr > 512) adr = 0;                      //letzte Speicherstelle erreicht?, dann von vorn
            break;
          default:  //interner RAM
            c = program[int(adr++)];
            if (adr > kRamSize) adr = 0;                          //letzte Speicherstelle erreicht?, dann von vorn
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
    delay(1);
  }//while (ex)
  return 0;
}

//--------------------------------------------- Unterprogramm teste auf gültige Zeilennummer ------------------------------------------------------

static uint16_t testnum() {
  uint32_t num = 0; // Intern mit 32-Bit rechnen für Überlauf-Check
  spaces();
  while (*txtpos >= '0' && *txtpos <= '9') {
    // Optimierte Berechnung: (num << 3) + (num << 1) ist num * 10
    num = num * 10 + (*txtpos - '0');
    txtpos++;
    if (num > 65535) num %= 65536;
  }
  return (uint16_t)num;
}
//--------------------------------------------- Unterprogramm - Fehler/Nachrichtausgabe -----------------------------------------------------------

void printmsg(const char *msg, int nl) {

  while (*msg) {
    outchar(*msg++);
  }

  if (nl == 1) {
    line_terminator();
  }
}
//--------------------------------------------- Unterprogramm - Tastenabfrage (list-Ausgaben) -----------------------------------------------------

static uint16_t wait_key(bool modes) {
  if (modes) {
    if (function_key) function_key = false;
    line_terminator();
    printmsg("SPACE<Continue>/CTR+C <Exit>", 1);
  }
  while (1) {
    // 1. Terminal-Check
    if (Terminal.available()) {
      return (uint16_t)Terminal.read();
    }
    if (cursor_up) {
      cursor_up = false;
      return 0x06;
    }
    if (cursor_down) {
      cursor_down = false;
      return 0x05;
    }
    if (page_up) {
      page_up = false;
      return 0x14;
    }
    if (page_down) {
      page_down = false;
      return 0x15;
    }
    if (break_marker) {
      break_marker = false;
      return 0x03;
    }
  }
}

//--------------------------------------------- Unterprogramm - Zeile eingeben --------------------------------------------------------------------

static void getln(int m)
{ int chpos = -1;
  if (m)
  {
    printmsg("READY.", 1);
  }
  char *input_start = program_end + sizeof(LINENUM);
  txtpos = input_start;


  while (1)
  {
    char c = inchar();
    if (c == 27 && Frame_nr) continue;

    switch (c)
    {
      case NL:
      case CR:
        line_terminator();
        txtpos[0] = NL;
        return;

      case 0x7F:
        if (txtpos == input_start)
          break;

        if (Frame_nr) {                   //im Fenster kein Backspace, um das Fenster nicht zu beschädigen
          Cursor_x = tc.getCursorCol();
          Cursor_y = tc.getCursorRow();
          tc.setCursorPos(tc.getCursorCol() - 1, Cursor_y);
          tc.setChar(' ');
          tc.setCursorPos(tc.getCursorCol() - 1, Cursor_y);
        }
        else if (chpos > -1) {
          chpos--;
          txtpos--;
          Terminal.write("\b\e[K");      //nicht im Fenster, dann Backspace
        }
        break;

      case 0x03:       // ctrl+c
        line_terminator();
        printmsg(breaks, 1);
        current_line = 0;
        if (program != nullptr) {
          sp = program + kRamSize;
        }
        txtpos[0] = NL;
        return;
        break;

      default:
        // Wir müssen mindestens ein Leerzeichen lassen, damit wir die Zeile in die richtige Reihenfolge bringen können
        if (txtpos == variables_begin - 2) {
          outchar(CTRLH);
          syntaxerror(outofmemory);
        }
        else
        {
          *txtpos++ = c;
          chpos++;
          outchar(c);
        }
    }
  }

}


//--------------------------------------------- Unterprogramm - Programmzeile finden ---------------------------------------------------------------
static char *findline()
{
  char *line = program_start;
  while (line != program_end)
  {
    if (((LINENUM *)line)[0] >= linenum)
      return line;
    // Add the line length onto the current address, to get to the next line;
    line += line[sizeof(LINENUM)];
  }
  return line;
}

//--------------------------------------------- Unterprogramm - Data-zeile finden -----------------------------------------------------------------

static char *find_data_line()
{
  char *line = program_start;
  while (line < program_end)
  {

    dataline = line + sizeof(LINENUM) + sizeof(char);         //Programmzeile übergeben
    //Terminal.println(dataline);
    if (dataline[0] == 'D' && dataline[1] == 'A' && dataline[2] == 'T' && dataline[3] == 'A')
    {
      data_numbers[num_of_datalines++] = *((LINENUM *)line);
    }
    line += line[sizeof(LINENUM)];
  }
  num_of_datalines -= 1;
  return 0;
}

//--------------------------------------------- Unterprogramm - Grossbuchstabenumwandlung ---------------------------------------------------------

static void toUppercaseBuffer()
{
  char *c = program_end + sizeof(LINENUM);
  for (char q = 0; *c != NL; c++) {
    if (*c == '"' || *c == '\'') q = (q == *c) ? 0 : (q == 0 ? *c : q);
    if (!q && *c >= 'a' && *c <= 'z') *c -= 32;
  }
}

//##################################################################################################################################################
//################################## Programmzeile ausgeben auf dem Bildschirm in Farbe, auf SD-Karte ohne Farbe ###################################
//##################################################################################################################################################

void setSyntaxColor(const char* ansiCode) {
  if (useColor) {
    Terminal.print(ansiCode);
  }
}



int printline() {
  LINENUM line_num;
  memcpy(&line_num, list_line, sizeof(LINENUM));
  list_line += sizeof(LINENUM) + sizeof(char);
  bool isStartOfLine = true;

  printnum((int)line_num, 0);
  outchar(' ');


  while (*list_line != NL && *list_line != '\0') {
    char c = *list_line;
    int matched_len = 0;

    // --- LEERZEICHEN ---
    if (isspace(c)) {
      outchar(*list_line++);
      continue;
    }
    // --- 1. RELOPS (Höchste Priorität) ---
    // Wir prüfen nur, wenn c ein Operator-Startzeichen ist
    if (strchr("<>=!&|^%+-/*", c) != NULL) {
      if (peekRelop(list_line, matched_len) != -1 && matched_len > 0) {
        setSyntaxColor("\e[94m"); // Blau
        for (int k = 0; k < matched_len; k++) {
          outchar(*list_line++);
        }
        setSyntaxColor("\e[0m");
        isStartOfLine = false;
        continue; // Sofort zum nächsten Zeichen im Quelltext
      }
    }
    // --- STRINGS (Rot) ---
    if (c == '"') {
      setSyntaxColor("\e[31m");
      outchar(*list_line++);
      while (*list_line != '"' && *list_line != NL && *list_line != '\0') outchar(*list_line++);
      if (*list_line == '"') outchar(*list_line++);
      setSyntaxColor("\e[0m");
      isStartOfLine = false;
    }
    // --- ZAHLEN (Magenta) ---
    else if (isdigit(c)) {
      setSyntaxColor("\e[35m");
      while (isdigit(*list_line)) outchar(*list_line++);
      setSyntaxColor("\e[0m");
      isStartOfLine = false;
    }
    // --- WORTER (Befehle, Funktionen, Variablen) ---
    else if (isalpha(c)) {
      int kwort = peekInTable(list_line, keywords, kw_id_map, kw_offsets, KW_COUNT, matched_len);

      // --- 1. EINRÜCKUNG (Gilt für ALLE Zeilenanfänge) ---
      if (isStartOfLine) {
        // Spezial-Logik: NEXT rückt sich selbst schon ein Stück zurück (Ausrückung)
        int displayIndent = currentIndent;
        if (kwort == KW_NEXT) {
          displayIndent--;
          if (displayIndent < 0) displayIndent = 0;
        }

        for (int i = 0; i < displayIndent; i++) outchar(' ');
        isStartOfLine = false; // Flag für den Rest der Zeile löschen
      }

      // --- 2. TOKEN-UNTERSCHEIDUNG ---

      // A) KEYWORD (Gelb)
      if (kwort != -1) {
        // Logik für den Zähler der folgenden Zeilen
        if (kwort == KW_NEXT) {
          currentIndent--;
          if (currentIndent < 0) currentIndent = 0;
        }

        setSyntaxColor("\e[33m");
        for (int k = 0; k < matched_len; k++) outchar(*list_line++);
        setSyntaxColor("\e[0m");

        if (kwort == KW_FOR) {
          currentIndent++;
        }
      }
      // B) FUNKTION (Cyan)
      else if (peekInTable(list_line, func_tab, func_id_map, func_offsets, 60, matched_len) != -1) {
        setSyntaxColor("\e[36m");
        for (int k = 0; k < matched_len; k++) outchar(*list_line++);
        setSyntaxColor("\e[0m");
      }
      // C) VARIABLE (Grün)
      else {
        setSyntaxColor("\e[32m");
        while (isalnum(*list_line) || *list_line == '$' || *list_line == '_') {
          outchar(*list_line++);
        }
        setSyntaxColor("\e[0m");
      }
    }

    /*

        else if (isalpha(c)) {
          // 1. Prüfen ob Keyword (Gelb)
          int kwort = peekInTable(list_line, keywords, kw_id_map, kw_offsets, KW_COUNT, matched_len);
          if (kwort != -1)
          {
            if ((kwort == KW_FOR) || (kwort == KW_NEXT)) {

              if (isStartOfLine) {                               // Zeileneinrückung und -ausrückung bei FOR-NEXT
                if (kwort == KW_FOR) currentIndent ++;


                for (int i = 0; i < currentIndent; i++) outchar(' ');

                if (kwort == KW_NEXT) {
                  currentIndent --;
                  if (currentIndent < 0)
                    currentIndent = 0;
                }
                isStartOfLine = false; // Danach für den Rest der Zeile sperren
              }
            }
            setSyntaxColor("\e[33m");
            for (int k = 0; k < matched_len; k++) outchar(*list_line++);
            setSyntaxColor("\e[0m");
          }


          // 2. Prüfen ob Funktion (Cyan)
          else if (peekInTable(list_line, func_tab, func_id_map, func_offsets, 60, matched_len) != -1) {
            setSyntaxColor("\e[36m");
            for (int k = 0; k < matched_len; k++) outchar(*list_line++);
            setSyntaxColor("\e[0m");
            isStartOfLine = false;
          }
          // 3. Sonst Variable (Grün)
          else {
            setSyntaxColor("\e[32m");
            while (isalnum(*list_line) || *list_line == '$' || *list_line == '_') outchar(*list_line++);
            setSyntaxColor("\e[0m");
            isStartOfLine = false;
          }
        }*/
    // --- OPERATOREN & REST ---
    else {
      outchar(*list_line++);
    }
    yield();
  }

  if (*list_line == NL) list_line++;
  line_terminator();
  return (int)line_num;
}
//######################################################################################################################################################################

//--------------------------------------------- Unterprogramm - RTC auslesen ----------------------------------------------------------------------

void getdatetime()
{
  Zeit[2] = e_rtc.getSecond();
  Zeit[1]  = e_rtc.getMinute();
  Zeit[0]  = e_rtc.getHour(true); // true = 24-Stunden-Format
  Datum[0] = e_rtc.getDay();
  Datum[1] = e_rtc.getMonth() + 1; // Achtung: Manche Bibliotheken starten bei 0 (Januar)
  Datum[2] = e_rtc.getYear();
  Datum[3] = e_rtc.getDayofWeek();
}

//--------------------------------------------- Unterprogramm - Hexadezimalzahl in Dezimalzahl konvertieren ---------------------------------------

static int hexDigit(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}


//#################################################################################################################################################
//--------------------------------------------- Start - Werteausgabe ------------------------------------------------------------------------------
//--------------------------------------------- Zahlen,Variablen oder Strings auslesen ------------------------------------------------------------
//--------------------------------------------- logische Abhängigkeiten auswerten -----------------------------------------------------------------
//--------------------------------------------- mathematische Funtionen ausführen -----------------------------------------------------------------
//#################################################################################################################################################


static float expr4()
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
  //Terminal.println(*txtpos);
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
  else if (isdigit(*txtpos) || *txtpos == '.')
  {
    char *endptr;
    float a = strtof(txtpos, &endptr);                        // strtof liest automatisch Vorkomma, Punkt, Nachkomma und E+/- ab.

    if (txtpos == endptr) {                                   // Falls keine Zahl dann fehler
      goto expr4_error;
    }
    txtpos = endptr;                                          // Wir verschieben den txtpos-Zeiger direkt an das Ende der gelesenen Zahl
    string_marker = false;
    return a;
  }
  //----------------------------------------- Hexadezimalzahlen -------------------------------------------------------------------------------------
  else if (*txtpos == '#') {
    string_marker = false;                          //Stringmarker zurücksetzen sonst falsche Ausgabe nach Stringzuweisung
    txtpos++;
    f = 0;
    while (isxdigit(*txtpos))
    {
      g = *txtpos;
      f = hexDigit(g);
      t = (t << 4) | f;     // Schiebe 4 Bits nach links und füge neue Stelle ein
      txtpos++;
    }
    // Wenn wir hier ankommen, war das Zeichen kein Hex mehr (z.B. Leerzeichen oder Zeilenende)
    return t;
  }
  //----------------------------------------- Binärzahlen -------------------------------------------------------------------------------------------
  else if (*txtpos == '%') {
    string_marker = false;
    txtpos++;

    if (*txtpos < '0' || *txtpos > '1') {
      goto expr4_error;
    }
    t = 0;
    while (*txtpos == '0' || *txtpos == '1') {
      t = (t << 1) | (*txtpos - '0');             // Bitweise nach links schieben und 0/1 addieren
      txtpos++;
    }
    return t;
  }
  //***************************************************** ein- oder zweibuchstabige variablen ***************************************************
  // Funktion oder Variable
  //--------------------------------------------------------------------------------------------------------------------------------------------------------------
  table_index = findFunction();                                         //Funktionstabelle lesen

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
        v_name = tmp + tmo;
      }

      while (*txtpos >= 'A' && *txtpos <= 'Z') txtpos++;                //lange Variablennamen

      //----------------------------------------- Stringvariablen -------------------------------------------------------------------------------------
      if (*txtpos == '$') {                                             //Stringvariable

        txtpos++;

        if (*txtpos == '(') {                                           //Stringarray?
          txtpos++;
          var_art = 2;
          expression_error = 0;
          v_adr = rw_array(v_name, STR_TBL);
          if (expression_error) goto expr4_error;
        }

        // String-Lesen
        i = 0;
        while (i < STR_LEN - 1) {
          char c;
          if (var_art == 2) {
            c = spi_fram_read8(v_adr + i);
          } else {
            c = Stringtable[stmp + i];
          }
          tempstring[i] = c;
          if (c == '\0') break; // Ende gefunden
          i++;
        }
        tempstring[i] = '\0'; // Sicherstellen, dass terminiert ist
        string_marker = true;
        return a;
      }
      else if (*txtpos == '(') {                                        //Numerisches Array?
        txtpos++;
        var_art = 1;                                                    //numerisches Array
        expression_error = 0;
        v_adr = rw_array(v_name, VAR_TBL);
        if (expression_error) goto expr4_error;
        spi_fram_read(v_adr, buf, 4);

        //a = *((float *)buf);
        memcpy(&a, buf, 4);                                             //byte-array des Wertes nach float konvertieren
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
    int fu = table_index, iic, cc;
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
        if (*txtpos == ' ') txtpos++;                         //Leerzeichen überspringen
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

    else if (fu == FUNC_NOT)                     //NOT-Funktion
      a = get_value();

    else if (fu == FUNC_PEEK || fu == FUNC_DEEK || fu == FUNC_FPEEK)  //byte, word oder float aus dem USER-Ram lesen
      a = get_value();

    else  {
      a = get_value();                         //1.Zahl (bei Stringvariablen steht in tempstring die 1.Zeichenkette)
    }


    switch (fu)                                 //Rückgabe komplexer Werte (mit Klammer und 1-4 Operatoren)
    {
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

      case  FUNC_GPIX:                          //GPIX(x,y,<mode>)
        if (Test_char(',')) goto expr4_error;
        b = get_value();                        //2.Zahl
        if (*txtpos == ',') {
          txtpos++;
          c = get_value();                      //optional 3.wert für modus
        }
        else c = 0;
        break;

      case FUNC_FN:
        ((float *)variables_begin)[Fnoperator[charb]] = a;
        Fnvar = 0;
        b = Fnoperator[charb + 4];
        while (*txtpos == ',') {
          txtpos++;
          Fnvar += 1;
          if (Fnvar >= b) {                                      //mehr Parameter als mit DEFN dimensioniert?
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

      case FUNC_NOT:                                //NOT-Funktion
        return int(!a);
        break;

      /*
            case FUNC_BATT:                               // Akku abfragen
              b = 3.3 / 4095 * analogRead(Batt_Pin);
              b = b / 0.753865;                           //(Umess/(R2/(R1+R2)) R1=3.327kohm R2=10.19kohm
              c = 100 - ((4.2 - b) * 100);                //Akkuwert in Prozent
              if (c > 100) c = 100;
              if (a == 0) return b;                       //Spannungswert zurückgeben
              else return int(c);                         //Ladung in Prozent
              break;
      */
      case FUNC_PEEK:
        return user_fram_read8(a);                         //User-RAM
        break;

      case FUNC_DEEK:
        user_fram_read(a, buf, 2);
        return ((word)buf[0] << 8) | buf[1];
        break;

      case FUNC_FPEEK:
        user_fram_read(a, buf, 4);                           //FRAM float
        return *(float*)buf;
        break;


      case FUNC_GPIX:
        return Test_pixel(a, b, c);
        break;

      case FUNC_GET:
        if (a == 0) return int(tc.getCursorRow());             //get(0)=x
        else return int(tc.getCursorCol());                    //get(1)=y
        break;

      case FUNC_VAL:                                           //VAL("numerische Zeichenkette")
        //dbuf = String(tempstring);
        a = atof(cbuf.c_str());//cbuf.toFloat();
        string_marker = false;
        return a;
        break;

      case FUNC_ABS:                                          //ABS(x)
        if (a < 0)
          return -a;
        return a;
        break;

      case FUNC_CHR:                                          //CHR$(x)
        chr = true;                                           //merker für chr setzen (für Print-Befehl)
        tempstring[0] = int(a);                               //Char-Zeichen übergeben
        tempstring[1] = 0;
        string_marker = true;                                 //String-marker setzen
        return int(a);
        break;

      case FUNC_SIN:                                          //SIN(x)
        return sinf(a);
        break;

      case FUNC_COS:                                          //COS(x)
        return cosf(a);
        break;

      case FUNC_TAN:                                          //TAN(x)
        return tanf(a);
        break;

      case FUNC_ATAN:                                         //ATN(x)
        return atanf(a);
        break;

      case FUNC_LOG:                                          //LOG(x) Logarithmus zur Basis 10 (X>0)
        if (a < 0)
        {
          printmsg(mathmsg, 1);
          return (a);
        }
        return log10f(a);
        break;

      case FUNC_LN:                                           //LN(x) natürlicher Logarithmus (X>0)
        if (a < 0)
        {
          printmsg(mathmsg, 1);
          return (a);
        }
        return logf(a);
        break;

      case FUNC_LEN:                                          //LEN(a$) -> Rückgabe Stringlänge
        a = (float)strlen(tempstring);
        string_marker = false;                                //String-Marker zurücksetzen, für korrekte Printausgabe/Werteübergabe
        return a;
        break;

      case FUNC_UCASE:
        dbuf = String(tempstring);
        dbuf.toUpperCase();                                   //String in Grossbuchstaben umwandeln
        dbuf.toCharArray(tempstring, dbuf.length() + 1);      //und nach tempstring zurückschreiben
        return a;
        break;

      case FUNC_LCASE:
        dbuf = String(tempstring);
        dbuf.toLowerCase();                                  //String in Kleinbuchstaben umwandeln
        dbuf.toCharArray(tempstring, dbuf.length() + 1);     //und nach tempstring zurückschreiben
        return a;
        break;

      case FUNC_SGN:                                         //SGN(x)
        return (a > 0) - (a < 0);
        break;

      case FUNC_SQR:                                        //SQR(x)
        if (a < 0) {
          printmsg(mathmsg, 1);
          return (a);
        }
        if (b == 0) return sqrtf(a);                        //Quadratwurzel aus a
        else return powf(a, 1 / b);                         //N'te Wurzel aus a
        break;

      case FUNC_MIN:                  //MIN(x,y)
        return min(a, b);
        break;

      case FUNC_MAX:                  //MAX(x,y)
        return max(a, b);
        break;

      case FUNC_EXP:                  //EXP(x)
        return expf(a);
        break;

      case FUNC_INT:                  //INT()
        return (int)a;
        break;

      case FUNC_RND:                  //Random-Funktion RND(x)
        return random( a + 1 );
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

      case FUNC_SPC:
        if (a > 0) {
          for (int i = 0; i < int(a) ; i++) {
            outchar(' ');
          }
          tab_marker = true;
        }
        return a;
        break;

      case FUNC_STR:
        dtostrf(a, 0, (int)b, tempstring);
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

      case FUNC_GETCOL:
        if (a == 0) return Vordergrund;
        else return Hintergrund;
        break;

      case FUNC_FN:                                       //hier muss der gespeicherte Funktionsstring zurückgeholt und ausgeführt werden
        a = call_user_function(fname);
        return a;
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

float call_user_function(int fname) {
  // 1. Sichere den aktuellen globalen txtpos LOKAL auf dem Stack
  char *old_txtpos = txtpos;

  // 2. Setze den Parser auf den Funktions-String aus der Fntable
  txtpos = Fntable[fname];

  // 3. Berechne den Ausdruck der Funktion
  // get_value() wird nun den String in Fntable[fname] abarbeiten
  float result = get_value();

  // 4. Stelle den ursprünglichen txtpos wieder her
  // Egal ob get_value() fertig ist oder einen Fehler hatte
  txtpos = old_txtpos;

  // 5. Gib das Ergebnis der Funktion zurück
  return result;
}

/***************************************************************************/

static float expr3()
{
  float a, b;
  a = expr4();

  while (1)
  {
    spaces(); // Leerzeichen VOR der Operator-Prüfung erlauben
    char op = *txtpos;

    if (op == '*' || op == '/')
    {
      txtpos++;
      b = expr4();

      if (op == '*') {
        a *= b;
      } else {
        if (b != 0.0f) {
          a /= b;
        } else {
          printmsg(zeroerror, 1);
          expression_error = 1;
          return 0; // Sofortiger Abbruch bei Fehler
        }
      }
    }
    else {
      return a;
    }
  }
}

/***************************************************************************/
static float expr2()
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

static float get_value()
{
  float a, b;

  expression_error = 0;

  a = expr2();

  // Check if we have an error
  if (expression_error)  return a;

  table_index = findRelopBinary();                //Operatortabelle scannen
  if (table_index == RELOP_UNKNOWN)
    return a;


  switch (table_index)
  {
    case RELOP_GE:
      b = expr2();
      return (a >= b);
    case RELOP_NE:
      b = expr2();
      return (a != b);
    case RELOP_GT:
      b = expr2();
      return (a > b);
    case RELOP_EQ:
      b = expr2();
      return (a == b);
    case RELOP_LE:
      b = expr2();
      return (a <= b);
    case RELOP_LT:
      b = expr2();
      return (a < b);
    case RELOP_MOD:              //Modulo  x mod y
      b = expr2();
      return int(a) % int(b);
    case RELOP_SHL:              //Bit-wise SHL  x shl y
      b = expr2();
      return (int(a) << int(b));
    case RELOP_SHR:              //Bit-wise SHR  x >> y
      b = expr2();
      return (int(a) >> int(b));
    case RELOP_XOR:               //Bit-wise XOR   x || y
      b = expr2();
      return (int(a) ^ int(b));
    case RELOP_AND:              //Bit-wise AND  x & y
      b = expr2();
      return (int(a)&int(b));
    case RELOP_OR:                //Bit-wise OR   x | y
      b = expr2();
      return (int(a) | int(b));
    case RELOP_POW:               //x^y
      b = expr2();
      return pow(a, b);

    default:
      break;
  }

  return 0;
}



//--------------------------------------------- ende - Werteausgabe -------------------------------------------------------------------------------
//--------------------------------------------- ende - Werteausgabe -------------------------------------------------------------------------------
//--------------------------------------------- ende - Werteausgabe -------------------------------------------------------------------------------


//#######################################################################################################################################
//--------------------------------------------- RTC - Befehl "RTC Tag,Monat,Jahr,Stunde,Minute,Sekunde"----------------------------------
//#######################################################################################################################################

static int set_TimeDate()
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
  // Parameter-Reihenfolge der RTC: Sekunde, Minute, Stunde, Tag, Monat, Jahr
  e_rtc.setTime(tagzeit[5], tagzeit[4], tagzeit[3], tagzeit[2], tagzeit[1], tagzeit[0], 0);

  return 0;
}

//#######################################################################################################################################
//--------------------------------------------- PRZ - Befehl ----------------------------------------------------------------------------
//#######################################################################################################################################

static int set_prezision()
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
  currentIndent = 0;

  linenum = testnum();                                       // wenn nicht LIST-Taste, dann überprüfe Zeilennummer und gibt 0 zurück, wenn keine Zeilennummer angegeben wird
  if (program_start == program_end) return;

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

static int direct() {
  txtpos = program_end + sizeof(LINENUM);
  if (*txtpos == NL) return 0;                //Zeilenende?
  else return 1;  //                          //nein?, nächster Befehl
}

//--------------------------------------------- Unterprogramm - gehe zu nächsten Befehl -----------------------------------------------------------

static int run_next() {
  if (spaces() == ':') txtpos++;
  if (spaces() == NL ) return 1;              //nächste Zeile
  return 0;                                   //nächster Befehl
}

//#######################################################################################################################################
//--------------------------------------------- GOSUB - Befehl --------------------------------------------------------------------------
//#######################################################################################################################################
/*
  static int gosub()
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
*/
// Stelle sicher, dass die Frame-Größe für den ESP32 optimiert ist
#define GOSUB_FRAME_SIZE ((sizeof(struct stack_gosub_frame) + 3) & ~3)

static int gosub()
{
  // 1. Sicherstellen, dass wir eine Zielzeile haben, BEVOR wir den Stack anfassen
  char *next_line_ptr = findline();
  if (next_line_ptr == NULL) {
    // Fehler: Zeile nicht gefunden, wir brechen ab, bevor der Stack korrumpiert wird
    return 1;
  }

  // 2. Stack-Berechnung (32-Bit Alignment erzwingen)
  const size_t frame_size = (sizeof(struct stack_gosub_frame) + 3) & ~3;

  // 3. Strengere Prüfung gegen den Array-Anfang
  if ((char *)sp - frame_size < (char *)stack_limit)
  {
    printmsg(gosubmsg, 1);
    warmstart();
    return 1;
  }

  // 4. Den Stack-Pointer erst jetzt bewegen
  sp = (char *)sp - frame_size;
  struct stack_gosub_frame *f = (struct stack_gosub_frame *)sp;

  // 5. Daten sichern
  f->frame_type = STACK_GOSUB_FLAG;
  f->txtpos = txtpos;           // Wo im Text wir gerade sind
  f->current_line = current_line; // Pointer auf den Anfang der aktuellen Zeile

  // 6. Den Interpreter auf die neue Zeile setzen
  current_line = next_line_ptr;
  txtpos = current_line; // Meistens muss txtpos an den Anfang der neuen Zeile

  return 0;
}
/*
  static int gosub()
  {
  struct stack_gosub_frame *f;

  // 1. Korrekte Prüfung: Reicht der Platz nach unten?
  if ((uintptr_t)sp - sizeof(struct stack_gosub_frame) < (uintptr_t)stack_limit)
  {
    printmsg(gosubmsg, 1);
    warmstart();
    return 1;
  }

  // 2. Stack-Pointer senken
  sp -= sizeof(struct stack_gosub_frame);

  // 3. Daten sichern
  f = (struct stack_gosub_frame *)sp;
  f->frame_type = STACK_GOSUB_FLAG;
  f->txtpos = txtpos;
  f->current_line = current_line;

  // 4. Neue Zeile suchen
  char *newline = findline();
  if (newline == NULL) {
      // Fehler: Zielzeile nicht gefunden!
      // Stack wieder aufräumen oder Error ausgeben
      sp += sizeof(struct stack_gosub_frame);
      return 1;
  }

  current_line = newline;
  return 0;
  }
*/
//#######################################################################################################################################
//--------------------------------------------- INPUT - Befehl --------------------------------------------------------------------------
//#######################################################################################################################################

static char Input_String()
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

static int input()
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
  outchar('?');
  getln(0);
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

static float data_get()
{
  float value;
  float *var;
  int tmp, stmp, stmp_b, i, var_pos, array_art ;
  char c;
  word arr_adr;
  array_art = 0;

  if (current_dataline <= num_of_datalines)                //DATA-Zeile gültig?
  {
    if (datapointer == 0)
    {
      linenum = data_numbers[current_dataline++];                         //Zeilennummer übergeben
      if (linenum > 0)
      {
        dataline = findline() + sizeof(LINENUM) + sizeof(char);           //entsprechende Zeile nach dataline laden
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
  stmp = (int) (*txtpos - 'A') * STR_LEN;                                 //Strings Variablenadresse sichern
  txtpos++;
  if (*txtpos >= 'A' && *txtpos <= 'Z') {                                 //zweiter Variablenbuchstabe
    var = var + ((*txtpos - 'A' + 1) * 26);                               //einfache Variable (2Buchstaben)
    tmp = var_pos + ((*txtpos - 'A' + 1) * 26);
    var_pos = tmp;                                                        //Array-Adresse (2Buchstaben)
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


static char data_spaces() {
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

static float data_expr()
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

static char Data_String_quoted_read()
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
  // Sicherheitscheck: Ist der PSRAM bereit?
  if (program == nullptr) return;

  toUppercaseBuffer();                              // Zeile in Großbuchstaben umwandeln

  // Startpunkt der Eingabezeile im freien Speicher festlegen
  char *start_pos = program_end + sizeof(unsigned short);
  txtpos = start_pos;

  // 1. Das Ende der Eingabezeile SICHER finden (mit Schutz vor Endlosschleife)
  char *max_limit = program + kRamSize;
  while (txtpos < max_limit && *txtpos != NL) {
    txtpos++;
  }

  // Falls kein NL gefunden wurde oder der Speicher voll ist, abbrechen
  if (txtpos >= max_limit) {
    syntaxerror(outofmemory);
    return;
  }

  // 2. Länge der Zeile berechnen (inklusive des NL-Zeichens)
  uint32_t len = txtpos - start_pos + 1;

  // 3. Zieladresse berechnen (die Zeile wird bündig vor variables_begin geschoben)
  char *dest = variables_begin - len;

  // Sicherheits-Check: Überschneidet sich das Ziel mit dem Programmende?
  if (dest < program_end + sizeof(unsigned short)) {
    syntaxerror(outofmemory);
    return;
  }
  memmove(dest, start_pos, len);

  // 5. txtpos auf die neue Startadresse der verschobenen Zeile setzen
  txtpos = dest;
}
//#######################################################################################################################################
//--------------------------------------------------- neue Zeile einfügen ---------------------------------------------------------------
//#######################################################################################################################################
int insert_line() {
  char *pstart;
  char *newEnd;
  unsigned int linelen = 0;

  while (txtpos[linelen] != NL)                                 // Zeilenlänge ermitteln
    linelen++;
  linelen++; // Include the NL in the line length
  linelen += sizeof(unsigned short) + sizeof(char);             // Leerzeichen einfügen für Zeilennummer + Zeile

  txtpos -= 3;                                                  // Zeilennummer ermitteln
  *((unsigned short *)txtpos) = linenum;
  txtpos[sizeof(LINENUM)] = linelen;
  pstart = findline();                                          // Zeile ins Programm einfügen/anhängen

  if (pstart != program_end && *((LINENUM *)pstart) == linenum) // 1. WENN ZEILENNUMMER EXISTIERT: Vorherige Zeile löschen / überschreiben
  {
    char *dest, *from;
    unsigned int tomove;
    from = pstart + pstart[sizeof(LINENUM)];
    dest = pstart;
    tomove = program_end - from;

    if (tomove > 0 && from >= program && program_end <= (program + kRamSize)) {
      memmove(dest, from, tomove);
      program_end = dest + tomove;
    } else if (from == program_end) {
      program_end = dest;
    }
    pstart = findline();
  }
  if (txtpos[sizeof(LINENUM) + sizeof(char)] == NL)             // Wenn die neue Zeile keinen Text hat (nur NL), war das ein LÖSCH-Befehl -> fertig!
    return 1;


  while (linelen > 0)                                           // 2. PLATZ SCHAFFEN UND NEUE ZEILE EINFÜGEN
  {
    unsigned int tomove;
    unsigned int space_to_make;

    if (txtpos < program_end) {                                 // Sicherheits-Check gegen unzulässige Pointer-Arithmetik
      syntaxerror(outofmemory);
      return 1;
    }

    space_to_make = txtpos - program_end;

    if (space_to_make > linelen)
      space_to_make = linelen;

    newEnd = program_end + space_to_make;
    if (newEnd >= (program + kRamSize)) {
      syntaxerror(outofmemory);
      return 1;
    }

    tomove = program_end - pstart;
    if (tomove > 0) {
      memmove(pstart + space_to_make, pstart, tomove);
    }

    // Zeilen-Metadaten und Text an die freigewordene Stelle kopieren
    for (tomove = 0; tomove < space_to_make; tomove++)
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

void reset_program() {
  clear_var();
  find_data_line();
  sp = program + kRamSize;
  current_line = program_start;
  txtpos = program_start;
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
  uint8_t cmd;
  cmd_new();                                             //alles löschen
  print_info();                                          //Start-Bildschirm anzeigen
  initSD();                                              //SD-Karte initialisieren
  SPI_FRAM_info();                                       //SPI-Ram ermitteln
  int a, e;

  //################################################# Hauptprogrammschleife ######################################################
  while (1)
  {
    expression_error = 0;                               //alle Errors zurücksetzen

    if ( triggerRun ) {                                 // AUTO-START-FUNKTION
      triggerRun = false;
      reset_program();
      goto execline;
    }

    else {
      getln(1);
      //-------------------------------- Start Zeile einfügen ---------------------------------------------
      if (function_key) {                                           //Funktionstaste gedrückt? Befehl ausführen
        goto fnkey;
      }

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

    if (breakcheck() || break_marker)                              //Programmabbruch mit Ctrl-C oder ESC
    {
      break_marker = false;

      line_terminator();

      if (current_line != NULL)
      {
        printmsg(breakmsg, 0);
        linenum = *((LINENUM *)(current_line));
        printnum(linenum, 0);
      }

      line_terminator();
      warmstart();
      Terminal.enableCursor(true);                                //Cursor einschalten
      continue;

    }
    table_index = findCommand();
    keyword_index = table_index;

fnkey:                                                            //Funktionstaste wurde gedrückt -> Befehl ausführen

    if (function_key) {                                           //Funktions-Befehl nur ausführen, wenn kein Programm läuft
      if (show_vars) zeige_variablen();                           //Variablen über LALT+v anzeigen
      keyword_index = key_command;
      function_key = false;
      show_vars = false;
    }
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

        if (load_file(0)) {
          continue;
        }
        string_marker = false;
        continue;
        break;

      case KW_NEW:                                        // NEW
        cmd_new();
        continue;
        break;

      case KW_RUN:                                        // RUN
        if (*txtpos != NL && *txtpos != '*') {            //RUN"/Filename" / RUN X$ lädt und startet das Programm
          if (load_file(0)) continue;
          string_marker = false;
          autorun = true;
        }
        if (*txtpos == '*') {
          load_ram();                                     //lädt und startet ein Programm aus dem SPI-RAM
        }
        reset_program();
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

      case KW_RENAME:                                     // RENAME
        expression_error = 0;
        get_value();
        strcpy(filestring, tempstring);                   //Tempstring nach filestring kopieren
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

      case KW_GOSUB ... KW_GOTO:                          // GOTO/GOSUB
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
            syntaxerror(wronglinenr);
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
        continue;
        break;

      case KW_CHD:                                      // SD-Card Change-Directory CD
        cmd_chdir();
        break;

      case KW_MKD:                                      // SD-Card Make-Dir MD
        if (cmd_mkdir(1));
        continue;
        break;

      case KW_RMD:                                      // SD-Card Remove-Dir RD
        if (cmd_mkdir(0));
        continue;
        break;

      case KW_CLS:                                      // CLS
        if (Frame_nr) {
          win_cls(Frame_nr);
        }
        else {
          tc.setCursorPos(1, 1);
          GFX.clear();
        }
        break;

      case KW_POS:                                      // POS x,y
      case KW_LOCATE:                                   //LOCATE x,y
        if (set_pos())
          continue;
        break;

      case KW_COL:                                      // COLOR v,b
        if (color())
          continue;
        break;

      case KW_PSET:                                       // PSET x,y
        if (pset())
          continue;
        break;

      case KW_CIRC:                                     // CIRCLE x,y,w,h,fill
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

      case KW_FRAME:                                      // FRAME x,y,w,h,r
        if (line_rec_circ(3, 4))                          // 3=Round, 0..4 Parameter
          continue;
        break;

      case KW_ARC:                                        // ARC x,y,rmin,rmax,gstart,gend,fill
        if (line_rec_circ(4, 6))
          continue;
        break;

      case KW_PATH:                                       // Path x,y,xx,yy,...xn,yn
        if (draw_path())
          continue;
        break;

      case KW_SWAP:
        if (line_rec_circ(5, 3))                          //SWAP x,y,xx,yy ->vertauscht vorder und Hintergrundfarbe eines rechtecks
          continue;
        break;

      case KW_COPY:
        if (line_rec_circ(6, 5))                          //Copy x,y,x_dest,y_dest, w, h ->Kopiert ein Rechteck von x,y nach x_dest,y_dest mit der Größe w,h
          continue;
        break;

      case KW_ANGLE:                                      //eine Linie im Winkel zeichnen
        if (line_rec_circ(9, 3))
          continue;
        break;

      case KW_FONT:                                       // FONT f
        expression_error = 0;
        val = int(get_value());
        set_font(val);
        break;

      case KW_PAUSE:                                       //PAUSE - kann mit ESC oder Ctrl+C unterbrochen werden (wie beim KC)
        expression_error = 0;
        val = get_value();
        startZeit = millis();
        while ((millis() - startZeit < val) && (!break_marker)) yield();      // Wichtig, um den Watchdog-Timer zu beruhigen!
        break_marker = false;
        //delay(val);
        break;

      case KW_END:
        current_line = program_end;
        goto execline;
        break;

      case KW_CLEAR:
        clear_var();                                     //Variablen- und Array-Ram löschen
        break;

      case KW_THEN:                                      // THEN
        then_marker = false;
        if ( val == 0 ) {                                //Überprüfung, ob alle If Bedingungen war sind
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

      case KW_CUR:                                        //Cursor On/Off
        if (cursor_onoff())
          continue;
        break;

      case KW_PRZ:                                      // PREZ 6 ->Nachkommastellen
        if (set_prezision())
          continue;
        break;

      case KW_DMP:                                       // DUMP adresse
        if (Memory_Dump())
          continue;
        break;

      case KW_STYLE:                                      //Textstyle setzen
        if (set_style())
          continue;
        break;

      case KW_SCROLL:                                     //Bildschirmbereich scrollen
        if (line_rec_circ(8, 1))
          continue;
        break;

      case KW_THEME:                                      //Farb-und Fontschema setzen
        int a;
        a = int(get_value());
        set_theme(a, -1);
        print_info();
        break;

      case KW_DATA:                                       // DATA - wird ignoriert
        goto execnextline;
        break;

      case KW_READ:                                       //DATA Read
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

      case KW_DEL:                                        // DEL File
        if (cmd_delFiles())
        {
          syntaxerror(notexistmsg);
        }
        continue;
        break;

      case KW_AND:                                        //Logisch UND
        expression_error = 0;
        val = get_value();
        logic_ergebnis[logic_counter++] = int(val);       //alle Ergebnisse einlesen und auswerten
        for (int i = 0; i < logic_counter + 1; i++)
        {
          logica += logic_ergebnis[i];
        }
        val = logica - logic_counter;                     //sind alle Bedingungen erfüllt lautet das Ergebnis 0
        break;

      case KW_OR:                                         //Logisch OR
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

      case KW_RTC:                                     //RTC stellen
        if (set_TimeDate())
          continue;
        break;

      case KW_DRAW:                                       //Draw x,y,0..1
        if (line_rec_circ(7, 2))
          continue;
        break;

      case KW_SPRT:                                     //SPRITE-Befehl
        if (Test_char('(')) continue;
        pa = *txtpos;
        if (pa == 'C' || pa == 'D' || pa == 'S') {
          txtpos++;
          if (sprite(pa)) {
            continue;
          }
        }
        else {
          syntaxerror(syntaxmsg);
          continue;
        }
        break;

      case KW_SND:                                      //Sound-Ausgabe
        if (Sound())
          continue;
        break;

      case KW_PEN:                                        //Stiftfarbe und Breite setzen

        if (set_pen()) {
          continue;
        }
        break;


      case KW_DEFN:                                    //Funktion definieren
        if (def_func()) {
          continue;
        }
        break;

      case KW_EDIT:
        val = int(get_value());                           //Edit <Zeilennummer> startet den Zeileneditor
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

      case KW_OPT:
        if (Option())
          continue;
        break;

      case KW_MNT:
        initSD();                                              //SD-Karte initialisieren
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
        type_file(1);
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

      case KW_RENUM:
        renum();
        continue;
        break;

      default:
        if (var_get())
          continue;
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

      if (*txtpos != 'T' && *txtpos + 1 != 'O') {
        syntaxerror(syntaxmsg);
        continue;
      }
      txtpos += 2;

      to_var = get_value();
      if (expression_error) continue;

      //if (initial > to_var) goto run_next_statement;

      if (txtpos[0] == 'S' && txtpos[1] == 'T' && txtpos[2] == 'E' && txtpos[3] == 'P')//if (isStep())//table_index == 0)
      {
        txtpos += 4;
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


next:
    // Variable ermitteln
    spaces();
    if (*txtpos < 'A' || *txtpos > 'Z')
    {
      printmsg(fornextmsg, 1);
      continue;
    }
    txtpos++;
    spaces();

gosub_return:
    // Nun durchlaufen wir die Frame-Stapel und suchen den gewünschten Frame, falls vorhanden.
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
          // Das ist nicht die Schleife, die wir suchen... also gehen wir den Stapel wieder nach oben.
          tempsp += sizeof(struct stack_gosub_frame);
          break;

        case STACK_FOR_FLAG:
          // Flag, Var, Final, Step
          if (table_index == KW_NEXT)
          { int tmp;
            struct stack_for_frame *f = (struct stack_for_frame *)tempsp;
            // Is the the variable we are looking for?

            if (txtpos[-1] >= 'A' && txtpos[-1] <= 'Z')                     //erster Variablenbuchstabe
            {
              int ef, xf;
              tmp = int(txtpos[-1] - 'A');

              if (*txtpos >= 'A' && *txtpos <= 'Z')                         //zweiter Variablenbuchstabe
              {
                ef = int(*txtpos - 'A' + 1) * 26;
                tmp = tmp + ef;
                txtpos++;
              }

              if (tmp <= f->for_var)
              {
                float *varaddr = ((float *)variables_begin) + tmp;
                *varaddr = *varaddr + f->step;
                //benutze unterschiedliche tests abhängig vom Vorzeichen für STEP
                if ((f->step > 0 && *varaddr <= f->to_var) || (f->step < 0 && *varaddr >= f->to_var))
                {
                  // Wir müssen eine Schleife durchlaufen, damit kein Element vom Stapel entfernt wird.
                  txtpos = f->txtpos;
                  current_line = f->current_line;
                  goto run_next_statement;
                }
                // Wir haben das Ende der Schleife erreicht. Wir verlassen die Schleife und entfernen das Element vom Stapel.
                sp = tempsp + sizeof(struct stack_for_frame);
                goto run_next_statement;
              }
            }
          }
          // Das ist nicht die Schleife, die wir suchen... also gehen wir den Stapel wieder nach oben.
          tempsp += sizeof(struct stack_for_frame);
          break;

        default:
          warmstart();
          continue;
      }
    }
    // Didn't find the variable we've been looking for

    syntaxerror(syntaxmsg);
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

  semicolon = false; // Initialisierung

  while (!k)
  {
    char c = spaces(); // Nächstes Zeichen holen

    // Ende der Print-Liste prüfen
    if (c == ':' || c == NL || c == CR || c == '\0') {
      if (!semicolon) line_terminator();
      if ( c == ':' ) txtpos++;
      k = 1;
      break;
    }

    switch (c)
    {
      case ';':
        semicolon = true;
        txtpos++; // Semikolon überspringen
        break;

      case '"':
        print_quoted_string(); // Verarbeitet Anführungszeichen intern
        semicolon = false;
        break;

      case ',':
        {
          // Tabulator-Logik: Springe zur nächsten 10er Spalte
          int nextTab = ((tc.getCursorCol() / 10) + 1) * 10;
          if (nextTab >= 40) { // Angenommen 40 Spalten Breite
            line_terminator();
          } else {
            tc.setCursorPos(nextTab, tc.getCursorRow());
          }
          semicolon = true;
          txtpos++;
        }
        break;

      case 'A': // Prüfung auf AT(x,y)
        if (txtpos[1] == 'T' && txtpos[2] == '(')
        {
          txtpos += 3; // "AT(" überspringen
          xp = (int)get_value();
          if (*txtpos == ',') {
            txtpos++;
            yp = (int)get_value();
            tc.setCursorPos(xp, yp);
            if (*txtpos == ')') txtpos++; // Klammer zu überspringen
          }
          semicolon = true;
          break;
        }
        // Falls kein AT, falle gehe zu default (Ausdruck/Variable beginnend mit A)
        [[fallthrough]];

      default:
        float e = get_value(); // Zahl, Variable oder Funktion (z.B. CHR$, STR$)

        if (expression_error) return 1;

        // Zentrale Marker-Abfrage
        if (func_string_marker == true) {               //String$ funktion
          for (int i = 0; i < fstring; i++) {           //in fstring steht die Anzahl der Wiederholungen
            printmsg(tempstring, 0);
          }
          func_string_marker = false;
          string_marker = false;
          chr = false;
        }
        else if (string_marker)
        {
          // Für Funktionen die Strings zurückgeben oder direkte String-Variablen
          printmsg(tempstring, 0);
          func_string_marker = false;
          string_marker = false;
        }
        else if (chr)
        {
          outchar((int)e);
          chr = false;
        }
        else if (tab_marker)
        {
          // TAB/SPC wurde bereits intern durch tc.setCursorPos in get_value erledigt
          tab_marker = false;
        }
        else
        {
          printnum(e, Zahlenformat);
        }
        semicolon = false; // Nach einer Zahl/String wird standardmäßig ein CR erwartet
        break;
    }
  }

  return 0;
}

//#######################################################################################################################################
//--------------------------------------------- Variablen - Eingabe ---------------------------------------------------------------------
//#######################################################################################################################################

static float var_get()
{
  float value;
  float *var;
  //char *st;
  int tmp, stmp, stmp_b, i, var_pos, array_art ;
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
  if (*txtpos >= 'A' && *txtpos <= 'Z') {                               //zweiter Variablenbuchstabe
    tmp = (int) ((*txtpos - 'A' + 1) * 26);
    var_pos = var_pos + tmp;                                            //pos falls array
    var = var + tmp;                                                    //pos normale variable
    txtpos++;
  }

  //------------------------------------------------------------------------------------------------------------------------------------------------------------
  while (*txtpos >= 'A' && *txtpos <= 'Z') txtpos++;  //so sind auch lange Variablennamen möglich ->siehe auch expr4()
  //------------------------------------------------------------------------------------------------------------------------------------------------------------
  if (*txtpos == '(') {
    txtpos++;
    expression_error = 0;
    arr_adr = rw_array(var_pos, VAR_TBL);                                  //numerische Array? Adresse im Zahlen-Arrayfeld
    //Terminal.print("Hier bin ich");
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

  //Terminal.print(*txtpos);
  if (Test_char('=')) {
    return 1;
  }

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
  int x, y, z, xx, yy, zz;
  word vadresse, ort;
  byte p_data[6], len;

  // 1. Dimensionen einlesen
  x = (int)get_value();
  y = (*txtpos == ',') ? (txtpos++, (int)get_value()) : 0;
  z = (*txtpos == ',') ? (txtpos++, (int)get_value()) : 0;

  if (Test_char(')')) {
    expression_error = 1;
    return 0;
  }

  // 2. Metadaten aus dem FRAM laden (6 Bytes: Startadresse, XX, YY, ZZ)
  ort = table + (num * 6);
  len = (table == VAR_TBL) ? 4 : STR_LEN; // float = 4 Bytes

  spi_fram_read(ort, p_data, 6);

  vadresse = word(p_data[0], p_data[1]); // Start des Datenfelds
  xx = word(p_data[2], p_data[3]);       // Max Index X
  yy = p_data[4];                        // Max Index Y
  zz = p_data[5];                        // Max Index Z

  // 3. Bounds-Check (Wichtig!)
  if (x > xx || y > yy || z > zz) {
    expression_error = 1;
    syntaxerror(dimmsg); // optional, falls du sofort abbrechen willst
    return 0;
  }

  // 4. Korrekte Adressberechnung für 1D, 2D und 3D
  // Wir nutzen (xx + 1), weil BASIC-Arrays meist von 0 bis inclusive XX gehen
  uint32_t element_index = (uint32_t)x +
                           ((uint32_t)y * (xx + 1)) +
                           ((uint32_t)z * (xx + 1) * (yy + 1));

  ort = vadresse + (element_index * len);

  return (float)ort; // Rückgabe der berechneten Speicheradresse im FRAM
}


//#######################################################################################################################################
//----------------------------------------------------- CLEAR-Befehl --------------------------------------------------------------------
//#######################################################################################################################################

void clear_var()
{
  memset(tempstring, '\0', sizeof(tempstring));
  for (int i = 0; i < VAR_SIZE * 26 * 27; i++)                     //Variablen löschen
  {
    variables_begin[i] = 0;
  }
  memset(Stringtable, '\0', sizeof(Stringtable));
  if (Var_Neu_Platz > 0) {                                          //SPI-Ram nur löschen, wenn Arrays definiert wurden
    SPI_RAM_fill(0x0, 0x1000, 0);                                   //die ersten 4kb Array-Variablen im Ram löschen
    SPI_RAM_fill(VAR_TBL, 0x4000, 0);                               //Array-Tabellen löschen
  }
  Var_Neu_Platz = 0;                                               //Array-Zeiger zurücksetzen
  num_of_datalines = 0;                                            //Datazeilenzähler zurücksetzen
  del_window();                                                    //Fensterparameter löschen
  Frame_nr = 0;                                                    //Hauptfenster setzen
  return;
}

//#######################################################################################################################################
//--------------------------------------------- NEW-Befehl ------------------------------------------------------------------------------
//#######################################################################################################################################
void cmd_new() {
  // Hinweis: Der nullptr-Check fällt weg, da 'program' ein festes internes Array ist

  program_start = program;
  program_end = program_start;
  sp = program + kRamSize;                                            // Zeigt an das absolute Ende des internen RAMs (0x10000)
  stack_limit = program + kRamSize - STACK_SIZE;                      // Stack-Limit im internen RAM berechnen
  variables_begin = stack_limit - (26 * 27 * VAR_SIZE);
  uint32_t bytes_to_clear = variables_begin - program;

  if (bytes_to_clear <= kRamSize) {
    // Löscht exakt den reinen Programmspeicherbereich im internen RAM
    memset(program, 0, bytes_to_clear);
  } else {
    // Sicherheits-Fallback: Löscht den gesamten internen Block bis variables_begin (ca. 62 KB)
    memset(program, 0, 0x10000 - STACK_SIZE - (26 * 27 * VAR_SIZE));
  }
  clear_var();
}
//#######################################################################################################################################
//--------------------------------------------- THEME - Befehl --------------------------------------------------------------------------
//#######################################################################################################################################

static int set_theme(int value, int fnt)
{
  //int fn;

  switch (value)
  {
    case 0: //C64
      Vordergrund = 11;
      Hintergrund = 1;
      set_font(25);
      break;

    case 1: //C128
      Vordergrund = 46;
      Hintergrund = 21;
      set_font(25);
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

    case 6: //KC85, AMIGA
      Vordergrund = 63;
      Hintergrund = 2;
      set_font(19);
      break;

    case 7: //VIC20
      Vordergrund = 2;
      Hintergrund = 63;
      set_font(2);
      break;

    case 8: //TRS80
      Vordergrund = 0;
      Hintergrund = 24;
      set_font(22);
      break;

    case 9: //TI-99
      Vordergrund = 0;
      Hintergrund = 30;
      set_font(22);
      break;

    case 10: //LCD
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
  if (fnt > -1) {
    set_font(fnt);
  }
  //Terminal.println(fnt);
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
  unsigned long address;
  float w_ert;
  word wert;
  int was, weite;
  byte value, p_data[2];
  /*
    was = abs(get_value());                                       //Speicherort 0..2 ->0-RAM, 1-FRAM, 2-EEPROM
    if (was > 2) was = 2;
    if (Test_char(',')) return 1;
  */
  address = abs(get_value());                                  //Speicheradresse
  if (Test_char(',')) return 1;

  if (fn == KW_FPOKE) {
    w_ert = get_value();                                        //floatwert poken
  }
  else
    wert = abs(get_value());                                   //zu speichernder Wert in byte oder word

  // Testen auf Zeilenende oder ':'
  if (*txtpos != NL && *txtpos != ':')
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  //---------------------------- RAM -----------------------------------------------

  /*
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
    }*/
  //---------------------------- USER-PSRAM ---------------------------------------------
  //  else if (was == 1) {
  if (fn == KW_POKE)  USER_RAM_write8(address, byte(wert));   //FRAM Byte
  else if (fn == KW_DOKE)
  {
    p_data[0] = highByte(wert);
    p_data[1] = lowByte(wert);
    USER_RAM_write(address, p_data, 2);                       //FRAM Word
  }
  else if (fn == KW_FPOKE) {
    byte* bytes = (byte*)&w_ert;
    USER_RAM_write(address, bytes, 4);                        //FRAM float
  }
  return 0;
  // }
  //----------------------------- EEPROM -------------------------------------------
  /*
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
  */
}

//#######################################################################################################################################
//--------------------------------------------- STYLE - Befehl --------------------------------------------------------------------------
//#######################################################################################################################################

static int set_style()
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
//--------------------------------------------- Lines/CIRCLE/RECT - Befehl --------------------------------------------------------------
//#######################################################################################################################################

static int line_rec_circ(int circ_or_rect, int param)
{
  //#################### Linie, Rechteck oder Kreis zeichnen #################
  //int x,y;
  short int i, par[7];
  //float w[4];

  i = 0;

  while (i < param) {                 //die ersten Parameter eingeben
    expression_error = 0;
    par[i] = get_value();
    if (expression_error) return 1;

    // check for a comma
    if (Test_char(',')) return 1;
    i++;
  }

  expression_error = 0;

  par[param] = get_value();            //letzter Parameter - in tempstring steht ein eventueller string

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
      //GFX.waitCompletion();
      break;

    case 2:
      if (par[4] == 0) GFX.drawRectangle(par[0], par[1], par[2], par[3]);     //Rectangle rect x,y,xx,yy,fill=0
      else {
        bcolor(Hintergrund);
        GFX.fillRectangle(par[0], par[1], par[2], par[3]);                    //Rectangle rect x,y,xx,yy,fill=1
      }
      //GFX.waitCompletion();
      break;

    case 3:                                                                   //Frame frame x,y,xx,yy,r
      GFX.drawLine(par[0] + 1.6 * par[4], par[1] - 1, par[0] + par[2] - 1.6 * par[4], par[1] - 1);
      GFX.drawLine(par[0] + 1.6 * par[4], par[1] + par[3], par[0] + par[2] - 1.6 * par[4], par[1] + par[3]);
      GFX.drawLine(par[0] - 1, par[1] + par[4], par[0] - 1, par[1] + par[3] - par[4]);
      GFX.drawLine(par[0] + par[2], par[1] + par[4], par[0] + par[2], par[1] + par[3] - par[4]);
      for (int i = 0; i <= 25; i++) {
        GFX.setPixel(par[0] + par[2] - par[4] * 1.6 * (1 - cos(i / 25.*M_PI / 2.)), par[1] + par[4] * (1 - sin(i / 25.*M_PI / 2.)));
        GFX.setPixel(par[0] + par[4] * 1.6 * (1 - cos(i / 25.*M_PI / 2.)), par[1] + par[4] * (1 - sin(i / 25.*M_PI / 2.)));
        GFX.setPixel(par[0] + par[2] - par[4] * 1.6 * (1 - cos(i / 25.*M_PI / 2.)), par[1] + par[3] - par[4] * (1 - sin(i / 25.*M_PI / 2.)));
        GFX.setPixel(par[0] + par[4] * 1.6 * (1 - cos(i / 25.*M_PI / 2.)), par[1] + par[3] - par[4] * (1 - sin(i / 25.*M_PI / 2.)));
      }
      //GFX.waitCompletion();
      break;

    case 4:
      drawArc(par[0], par[1], par[2], par[3], par[4], par[5], par[6]);        //Arc x,y,rmin,rmax,g_start,g_end,fill
      break;

    case 5:
      GFX.swapRectangle(par[0], par[1], par[2], par[3]);                      //Swap x,y,xx,yy
      break;

    case 6:
      GFX.copyRect(par[0], par[1], par[2], par[3], par[4], par[5]);          //Copy x, y, x_dest, y_dest, w, h
      break;

    case 7:
      if (par[2] == 0) GFX.moveTo(par[0], par[1]);                           //Draw x,y,1=draw/0=move
      else GFX.lineTo(par[0], par[1]);
      break;

    case 8:                                                                  //Scroll x,y
      GFX.scroll(par[0], par[1]);
      break;

    case 9:
      GFX.drawLine(par[0], par[1], par[0] + par[3]*cos(par[2]*M_PI / 180), par[1] + par[3]*sin(par[2]*M_PI / 180)); //Angle x,y,winkel,länge
      break;

    default:
      GFX.drawLine(par[0], par[1], par[2], par[3]);                          //Line line x,y,xx,yy
      break;
  }
  return 0;
}

//#######################################################################################################################################
//----------------------------------------------------- PATH-Befehl ---------------------------------------------------------------------
//#######################################################################################################################################

static int draw_path() {
  // FabGL nutzt den Typ 'Point' (enthält int16_t X und Y)
  Point points[16];
  uint8_t ptCount = 0;
  bool fill = false;
  // Parameter einsammeln
  while (ptCount < 16) {
    expression_error = 0;

    // X-Koordinate
    points[ptCount].X = (int16_t)get_value();
    if (expression_error || *txtpos != ',') break;
    txtpos++;

    // Y-Koordinate
    points[ptCount].Y = (int16_t)get_value();
    if (expression_error) break;

    ptCount++;

    // Check für das nächste Paar
    if (*txtpos == ',') txtpos++;
    else break;

    // oder auf FILL
    if (*txtpos == '1') {                           //das Ende von Fill_Path wird mit einer 1 gekennzeichnet
      fill = true;
      txtpos++;
      break;
    }

  }

  // Zeichnen mit FabGL
  if (ptCount >= 2) {
    if (fill) {
      // Syntax: Canvas.fillPath(Point* points, int numPoints)
      GFX.fillPath(points, ptCount);
    } else {
      GFX.drawPath(points, ptCount);
    }
    return 0;
  }

  return 1; // Fehler: Zu wenig Punkte
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
      Terminal.print("\e_GSPRITECOUNT" + String(cnt, DEC) + "$");
      break;
    case 'D':
      abuf = String(tempstring);                          //Stringkopie für D-Befehl nach abuf kopieren
      abuf.trim();
      Terminal.print("\e_GSPRITEDEF" + String(nr, DEC) + ";" + String(par[1], DEC) + ";" + String(par[2], DEC) + ";M;" + String(par[4], DEC) + ";" + String(par[5], DEC) + ";" + String(par[6], DEC) + ";" + abuf + "$"); //sprtd(n,"H;W;R;G;B;Data")
      break;
    case 'S':
      Terminal.print("\e_GSPRITESET" + String(nr, DEC) + ";" + tempstring + ";0;" + String(par[1], DEC) + ";" + String(par[2], DEC) + "$"); //sprts(n,"V..H;Frame;x;y");
      break;
  }

  return 0;
}

//#######################################################################################################################################
//----------------------------------------------------- SND-Befehl ----------------------------------------------------------------------
//#######################################################################################################################################
static int Sound(void) {
  short int  i, par[7];
  char c[30];
  String seq;

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
  Terminal.print("\e_S" + String(par[1], DEC) + ";" + String(par[2], DEC) + ";" + String(par[3], DEC) + ";" + String(par[4], DEC) + "$");


  if (Test_char(')')) return 1;                           //Klammer-zu vorhanden?

  // Check that we are at the end of the statement
  if (*txtpos != NL && *txtpos != ':' )  return 1;
  return 0;

}



//#######################################################################################################################################
//--------------------------------------------- PSET - Befehl ---------------------------------------------------------------------------
//#######################################################################################################################################

static int pset()
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

static int color()
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
  GFX.setPenColor(colorTable[(fc >> 4) & 0x03], colorTable[(fc >> 2) & 0x03], colorTable[fc & 0x03]);
}

void bcolor(int bc) {
  GFX.setBrushColor(colorTable[(bc >> 4) & 0x03], colorTable[(bc >> 2) & 0x03], colorTable[bc & 0x03]);
}


//#######################################################################################################################################
//--------------------------------------------- PEN - Befehl --------------------------------------------------------------------------------------
//#######################################################################################################################################

static int set_pen()
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
static int def_func()
{
  int fname, fnpos, i;

  // 1. Funktionsname prüfen (A-Z)
  if (*txtpos < 'A' || *txtpos > 'Z') return 1;
  fname = (*txtpos - 'A');
  fnpos = fname * 5; // Jede Funktion belegt 5 Slots (4 Operatoren + 1 Zähler)
  txtpos++;

  // 2. Klammer auf prüfen
  if (Test_char('(')) return 1;

  i = 0;
  // 3. Operatoren (Parameter) einlesen
  while (1) {
    if (*txtpos < 'A' || *txtpos > 'Z') {
      syntaxerror(syntaxmsg);
      return 1;
    }

    // Operator-ID (0-25) speichern
    Fnoperator[fnpos + i] = *txtpos - 'A';
    i++;
    txtpos++;

    if (*txtpos == ',') {
      txtpos++;
    } else if (*txtpos == ')') {
      txtpos++; // Klammer zu überspringen
      break;
    } else {
      syntaxerror(syntaxmsg); // Weder Komma noch Klammer zu
      return 1;
    }

    if (i > 4) { // Du hast Platz für 4 Parameter (0,1,2,3), der 5. Slot ist für die Anzahl
      syntaxerror(illegalmsg);
      return 1;
    }
  }

  Fnoperator[fnpos + 4] = i; // Anzahl der tatsächlichen Operatoren speichern

  // 4. Erwartete Trenner prüfen
  if (Test_char('=')) return 1;
  if (Test_char('[')) return 1; // Start des Funktions-Ausdrucks

  // 5. Funktions-Ausdruck in Fntable kopieren
  i = 0;
  while (*txtpos != ']' && *txtpos != '\0' && *txtpos != NL) {
    // Sicherheitscheck: Verhindert Überlauf von Fntable[fname]
    if (i < (sizeof(Fntable[0]) - 2)) {
      Fntable[fname][i++] = *txtpos++;
    } else {
      syntaxerror(illegalmsg); // Ausdruck zu lang für den Puffer
      return 1;
    }
  }

  // Korrekte Terminierung des Strings
  Fntable[fname][i] = NL;
  Fntable[fname][i + 1] = '\0';

  if (*txtpos == ']') txtpos++; // Schließende Klammer überspringen
  return 0;
}

//--------------------------------------------- Unterprogramm - String in Anführungszeichen ausgeben ----------------------------------------------

static char print_quoted_string()
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

static int set_pulse()
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
static int set_port()
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

static int set_pwm()
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

static int cursor_onoff()
{

  // Work out where to put it
  expression_error = 0;
  onoff = get_value();
  if (expression_error) return 1;

  if (onoff == 0) Terminal.enableCursor(false);
  else Terminal.enableCursor(true);
  return 0;

}


//#######################################################################################################################################
//--------------------------------------------- POS - Befehl --------------------------------------------------------------------------------------
//#######################################################################################################################################

static int set_pos()
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

static char String_quoted_read()
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
    if (i < (STR_LEN - 1)) tempstring[i++] = *txtpos++; //Tempstring füllen
    else {
      syntaxerror(stringtolong);
      expression_error = 1;
      return 0;
    }
  }
  tempstring[i] = '\0';       //tempstring abschliessen

  txtpos++;                   //Anführungszeichen überspringen
  return c;
}

//--------------------------------------------- Unterprogramm - Zeilenabschluss -------------------------------------------------------------------
static void line_terminator()
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
    case 22: Terminal.loadFont(&fabgl::FONT_6x13); //(siehe Ordner Fonts)
      break;
    case 23: Terminal.loadFont(&fabgl::FONT_9x15); //(siehe Ordner Fonts)
      break;
    case 24: Terminal.loadFont(&fabgl::FONT_8x16); //(siehe Ordner Fonts)
      break;
    case 25: Terminal.loadFont(&fabgl::FONT_8x8_PET); //(siehe Ordner Fonts)
      break;
    default: Terminal.loadFont(&fabgl::FONT_6x8);
      fnt = 26;
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
  char l, r;
  int y_pos = VGAController.getViewPortHeight() / y_char[fontsatz]; //VGAController.getScreenHeight() / y_char[fontsatz];
  int x_pos = VGAController.getViewPortWidth() / x_char[fontsatz];  //VGAController.getScreenWidth() / x_char[fontsatz];
  /*
      #ifdef Akkualarm_enabled
      float g = 3.3 / 4095 * 4000;//analogRead(Batt_Pin);
      g = g / 0.753865;                                 //(Umess/(R2/(R1+R2)) R1=3.327kohm R2=10.19kohm
      int   h = 100 - ((4.2 - g) * 100);                //Akkuwert in Prozent
      if (h > 100) h = 100;
      #endif
  */

  Terminal.enableCursor(false);
  GFX.clear();
  delay(100);
  fbcolor(Vordergrund, Hintergrund);

  l = 42;
  r = 42;


  tc.setCursorPos((x_pos - 28) / 2, 2);
  Terminal.write("*Basic32+ V");
  Terminal.write(BasicVersion);
  Terminal.write(" Zille-Soft*");

  /*
    #ifdef Akkualarm_enabled                                  //Akku in Prozent anzeigen
    Terminal.write("  ");
    Terminal.print(int(h), DEC);
    Terminal.write("%");
    #endif
  */
  tc.setCursorPos((x_pos - 16) / 2 , 4);
  // memory free
  uint32_t freier_speicher = kRamSize - (program_end - program);
  Terminal.print(freier_speicher, DEC);
  printmsg(memorymsg, 1);

  Terminal.enableCursor(true);
  tc.setCursorPos(1, 6);

}


//#######################################################################################################################################
//--------------------------------------------- Unterprogramm - Üerprüfung auf Abbruch-Taste (Ctrl-C) -----------------------------------
//#######################################################################################################################################

static char breakcheck() {
  // 1. Schneller Check, ob Daten im Puffer liegen
  if (Terminal.available()) {
    char c = Terminal.read();

    // 2. Prüfen auf CTRL+C (ASCII 3) oder ESC (ASCII 27)
    if (c == CTRLC || c == 27) {
      return 1;
    }
  }
  return 0;
}

void break_program()
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
  char d;


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


    case ( kStreamTerminal ):

    default:

      while (1)
      {
        //if(Serial.available()){
        //  c=Serial.read();
        if (Terminal.available()) {
          c = Terminal.read();          //Standard-Tasteneingabe

          switch (c) {

            case 0x03:       // ESC        -> BREAK
              current_line = 0;
              sp = program + kRamSize;
              break;

            default:

              break;
          }

          if (Graph_char && c != 13 && c != 32 && c != 0x7F) return c + 121; //alle Tasten außer Enter und Space und Backspace umwandeln in Grafik-chars
          return c;
        }//if(Terminal.available)

        if (break_marker) {                     //ESC-Abfrage -> Break
          break_marker = false;
          triggerRun == false;
          current_line = 0;
          sp = program + kRamSize;

          return 0x03;
        }
        if (function_key) {                     //Funktionsstaste - LIST, RUN etc.
          current_line = 0;
          sp = program + kRamSize;
          return CR;
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
    fp.write( c );                       //Char in Datei schreiben
  }
  else {
    if (ser_marker && list_send) {
      Serial1.write(c);                 //User-Seriellschnittstelle
      delay(2);                         //kurzes Delay nach jeder Zeile, sonst läuft der RX-Buffer über
    }

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

          move_up(Frame_nr);                                              //Window scrollen

        }
        tc.setCursorPos(Frame_curx[Frame_nr], y_pos);
        return;
      }
    }                                                    //************************** im Fenster schreiben ******************
    Terminal.write(c);                                   //auf FabGl VGA-Terminal schreiben----------------------------------
  }
}



//############################################# Dateioperationen auf der SD-Karte #######################################################
//--------------------------------------------- Unterprogramm SD-Karte initialisieren ---------------------------------------------------
//#######################################################################################################################################
static int initSD()
{
  int c;
  int adr, i;

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

  if (!SD.begin( kSD_CS, spiSD))
  {
    spiSD.end();
    syntaxerror(sderrormsg);
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

  printmsg("SD-Card: OK", 1);
  sd_ende();                                               //unmount
  return 1;                                                //OK
}

//#######################################################################################################################################
//--------------------------------------------- SPI-Bus umschalten ----------------------------------------------------------------------
//#######################################################################################################################################

void sd_ende() {
  spiSD.end();                                              //SD-Card unmount
  //spi_fram.begin(3);                                        //FRAM aktivieren
  string_marker == false;                                   //Stringmarker für Dateioperationen löschen
}


//#######################################################################################################################################
//--------------------------------------------- LOAD - Befehl ---------------------------------------------------------------------------
//#######################################################################################################################################

static int load_file(int modes)
{
  int fcheck;
  // Programmspeicher löschen
  program_end = program_start;
  expression_error = 0;

  if (!modes) {
    get_value();                                              //in tempstring steht der Dateiname
  }

  if (expression_error) return expression_error;

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);
  while (!SD.begin( kSD_CS, spiSD)) {
    syntaxerror(sderrormsg);
    delay(3000);
  }
  //Serial.println(String(sd_pfad) + String(tempstring));
  
  if ( !SD.exists(String(sd_pfad) + String(tempstring)))    //Datei vorhanden?
  {
    syntaxerror(sdfilemsg);                                 //Datei nicht vorhanden -> Fehlerausgabe
    sd_ende();
    expression_error = 1;
    return expression_error;
  }
  else {

    //******* hier Überprüfung auf BAS bzw. BIN Erweiterung ************************
    fcheck = check_extension();
    switch (fcheck) {
      case 0:
        hex_monitor(0);                                         //alle unbekannten Dateien werden in hexform angezeigt
        break;
      case 1:
        fp = SD.open(String(sd_pfad) + String(tempstring));     // Datei zum Laden öffnen
        inStream = kStreamFile;
        inhibitOutput = true;
        break;
      case 2:
        load_binary();                                          // Bin-Dateien laden
        break;
      case 3:
        import_pic(0, 0, tempstring, 1);                        // BMP-Dateien
        break;
      case 4:
        {
          load_pic(FRAM_OFFSET, tempstring);                    // PIC-Dateien
          static char picd[] = "_D(0)\n";                       // Befehlssequenz für die Darstellung
          txtpos = picd;                                        // Befehlssequenz an txtpos übergeben
          show_Pic();                                           // PIC auf Speicherplatz 0 anzeigen
        }
        break;
      case 5:
        type_file(0);                                           // TXT-Dateien -> fehlt noch
        break;

      default:
        break;
    }
  }
  warmstart();
  return expression_error;
}

int check_extension() {
  String dbuf = String(tempstring);
  int len = dbuf.length();
  if (len < 4) return 0;
  // Extrahiert die letzten 4 Zeichen
  String ext = dbuf.substring(len - 4);
  // equalsIgnoreCase ist auf dem ESP32 effizient und spart toUpperCase()
  if (ext.equalsIgnoreCase(".BAS")) return 1;
  if (ext.equalsIgnoreCase(".BIN")) return 2;
  if (ext.equalsIgnoreCase(".BMP")) return 3;
  if (ext.equalsIgnoreCase(".PIC")) return 4;
  if (ext.equalsIgnoreCase(".TXT")) return 5;
  return 0;
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
  while (!SD.begin( kSD_CS, spiSD)) {
    syntaxerror(sderrormsg);
    delay(3000);
  }

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

  fp = SD.open( String(sd_pfad) + String(tempstring), FILE_WRITE);  //Datei wird zum Schreiben geöffnet
  if (!fp) {                                                        //Fehler?
    printmsg("Open File-Error!", 1);
  }
  outStream = kStreamFile;
  useColor = false;                                                 //Farbausgabe unterdrücken
  // copied from "List"
  list_line = findline();
  while (list_line != program_end)                                  //Zeile für Zeile des Programms in die Datei schreiben
    printline();

  outStream = kStreamTerminal;                                      // zurück zum standard output, Datei schließen
  useColor = true;                                                  //Farbausgabe unterdrücken
  fp.close();

  line_terminator();
  sd_ende();                                                        //SD-Card unmount
  warmstart();
  return 0;
}
//#######################################################################################################################################
//---------------------------------------- Save ohne Parameter speichert das Programm ab 0x7000 im FRAM ---------------------------------
//#######################################################################################################################################
static int save_ram() {
  uint32_t address = (uint32_t)load_adress;

  // Berechnung der Programmlänge (Abstand zwischen Start und Ende)
  uint32_t n_bytes = (uint32_t)(program_end - program_start);

  // Sicherheitscheck: Programm zu kurz oder leer?
  if (n_bytes < 2) {
    syntaxerror(no_prg_msg);
    return 1;
  }

  // 1. Header vorbereiten (4 Bytes: 'B', 'S', HighByte, LowByte)
  uint8_t header[4];
  header[0] = 'B';
  header[1] = 'S';
  header[2] = (n_bytes >> 8) & 0xFF;
  header[3] = n_bytes & 0xFF;

  // 2. Header im Block schreiben
  USER_RAM_write(address, header, 4);
  address += 4;

  // 3. Das gesamte Programm im Block (Burst-Mode) schreiben
  USER_RAM_write(address, (uint8_t*)program_start, n_bytes);

  return 0;
}

//#######################################################################################################################################
//----------------------------------------------- Load ohne Parameter lädt das Programm aus dem FRAM ab 0x7000 --------------------------
//#######################################################################################################################################

static int load_ram() {
  uint32_t address = (uint32_t)load_adress;
  uint16_t n_bytes;
  uint8_t header[4];

  memset(program, 0, kRamSize);

  // Header (BS + Länge) in einem Rutsch lesen (4 Bytes)
  user_fram_read(address, header, 4);
  address += 4;

  // Kennung prüfen: 'B' 'S'
  if (header[0] == 'B' && header[1] == 'S') {
    // Anzahl der Bytes aus dem Header (Big Endian)
    n_bytes = (header[2] << 8) | header[3];

    if (n_bytes > 0x10000) n_bytes = 0x10000; // Sicherheits-Check (64KB Limit)
    user_fram_read(address, (uint8_t*)program, n_bytes);
    program_end = program_start + n_bytes;

    warmstart();
    return 0;
  }
  else {
    syntaxerror(no_prg_msg);
    return 0;
  }
}
//#######################################################################################################################################
//--------------------------------------------- DEL - Befehl ----------------------------------------------------------------------------
//#######################################################################################################################################

static int cmd_delFiles()
{

  char c;
  int n = 0;

  // eingabe Dateiname
  expression_error = 0;
  get_value();

  if (expression_error) return 1;

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1
  while (!SD.begin( kSD_CS, spiSD)) {
    syntaxerror(sderrormsg);
    delay(3000);
  }

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

void cmd_chdir()
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
  while (!SD.begin( kSD_CS, spiSD)) {
    syntaxerror(sderrormsg);
    delay(3000);
  }
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
  while (!SD.begin( kSD_CS, spiSD)) {
    syntaxerror(sderrormsg);
    delay(3000);
  }

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

int key_press(int current_row) {

  int screen_height = VGAController.getScreenHeight();

  // 2. Berechne, wie viele Textzeilen auf den Schirm passen
  // Die Höhe einer Zeile kommt aus y_char[fontsatz]
  int max_rows = (screen_height / y_char[fontsatz]) - 3;

  // 3. Prüfen, ob die aktuelle Zeile das Limit erreicht hat
  if (current_row >= max_rows) {
    if (wait_key(true) == 3) {
      return 1; // Abbrechen (User will Exit)
    }
    return 0;   // Weiter (User hat Taste gedrückt)
  }
  return 2;     // Noch Platz auf dem Schirm, kein Warten nötig
}


bool search_file(const char* names) {
  String cbuf;
  cbuf = String(names);
  cbuf.toUpperCase();                               //String in Grossbuchstaben umwandeln
  if (cbuf.indexOf(filestring) > -1) return true;
  else return false;
}

void cmd_Dir()
{
  int ln = 1;
  int ex = 0;
  String cbuf;
  const char hi[] = "._";
  const char ho[] = ".";
  int was;
  int wd = GFX.getWidth() / x_char[fontsatz];
  int Dateien = 0;
  bool ext = false;       // Sucherweiterung?
  bool found = false;
  char c = *txtpos;
  memset(filestring, 0, sizeof(filestring));                  //Puffer für Suchbegriff leeren

  if (c == char(34)) {                                        // Anführungszeichen erkannt
    expression_error = 0;
    get_value();                                              // in tempstring steht die Dateinamens-Erweiterung
    cbuf = String(tempstring);
    cbuf.toUpperCase();                                       // String in Grossbuchstaben umwandeln
    cbuf.toCharArray(filestring, cbuf.length() + 1);          // und nach filestring schreiben
    if (expression_error) return;
    ext = true;                                               // Ausgabe mit Sucherweiterung
  }
  zeichneGeruest();

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);
  delay(5);
  if (!SD.begin(kSD_CS, spiSD)) {
    syntaxerror(sderrormsg);
    sd_ende();
    return;
  }

  File dir = SD.open(String(sd_pfad));
  dir.seek(0);                                                  // zum Verzeichnis-Anfang

  // Zwei getrennte Listen für Ordner und Dateien
  std::vector<String> folderList;
  std::vector<String> fileList;

  int maxNameLength = 12; // Standard-Mindestbreite für die Namensspalte (z.B. 8.3 Format)

  // Schritt 1: Alle Einträge trennen, sammeln und maximale Länge ermitteln
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      entry.close();
      break;
    }

    cbuf = String(entry.name());
    cbuf.toCharArray(tempstring, cbuf.length() + 1);

    // Versteckte Dateien ausblenden
    if (strstr(tempstring, ho)) { //strstr(tempstring, hi)||
      entry.close();
      continue;
    }

    // Sucherweiterung filtern
    if (ext == true) {
      found = search_file(entry.name());
      if (!found) {
        entry.close();
        continue;
      }
    }

    String nameStr = String(entry.name());
    // Ermittle die Länge für die dynamische Spaltenbreite
    if (nameStr.length() > maxNameLength) {
      maxNameLength = nameStr.length();
    }
    // Einsortieren je nachdem, ob es ein Verzeichnis oder eine Datei ist
    if (entry.isDirectory()) {
      folderList.push_back(nameStr);
    } else {
      fileList.push_back(nameStr);
    }
    entry.close();
    yield();
  }

  int maxAllowedWidth = wd - 23;
  if (maxAllowedWidth < 10) maxAllowedWidth = 10; // Untere Grenze absichern
  if (maxNameLength > maxAllowedWidth) {
    maxNameLength = maxAllowedWidth;
  }
  // Lambda-Funktion für case-insensitive alphabetische Sortierung
  auto compCaseInsensitive = [](const String & a, const String & b) {
    String a_upper = a; a_upper.toUpperCase();
    String b_upper = b; b_upper.toUpperCase();
    return a_upper < b_upper;
  };
  // Schritt 2: Beide Listen separat alphabetisch sortieren
  std::sort(folderList.begin(), folderList.end(), compCaseInsensitive);
  std::sort(fileList.begin(), fileList.end(), compCaseInsensitive);
  // Schritt 3: Ordner-Liste mit Datei-Liste zusammenführen
  std::vector<String> combinedList = std::move(folderList);
  combinedList.insert(combinedList.end(), fileList.begin(), fileList.end());
  starteGrafischenExplorer(filestring);
}

void zeichneGeruest() {
  bcolor(1);
  fcolor(63);

  GFX.fillRectangle(20, 20, 300, 220);  // Fensterfläche
  GFX.drawRectangle(20, 20, 300, 220);  // Fensterrahmen

  // 2. Titel-Trennlinie zeichnen
  fcolor(51);
  GFX.drawLine(21, 45, 299, 45);

  fcolor(63);
  GFX.drawText(30, 26, "SD-Card EXPLORER");
  GFX.drawText(160, 26, "...wait");
  if (filestring[0] != '\0') {
    GFX.drawText(&fabgl::FONT_6x8, 30, 35, "search for:");
    GFX.drawText(&fabgl::FONT_6x8, 99, 35, filestring);
  }
  fcolor(60);
  GFX.drawLine(21, 202, 299, 202);
  GFX.drawText(&fabgl::FONT_6x8, 30, 206, "[Cursor/Page]=Scroll [Enter]=Run [ESC]=Break");

}


void zeichneCustomExplorer(const std::vector<String>& dateiListe, int ausgewaehlterIndex, int startSchnitt) {
  int maxSichtbar = 16;

  // 1. Hintergrund löschen, wenn gescrollt wurde ODER der Explorer frisch geöffnet wurde
  if (startSchnitt != letzterStartSchnitt || letzterStartSchnitt == -1) {
    GFX.fillRectangle(22, 60, 298, 188);
    letzterStartSchnitt = startSchnitt;
    letzterAusgewaehlterIndex = -1; // Erzwingt das Neuzeichnen aller Zeilen
  }

  if (dateiListe.empty()) {
    fcolor(48);
    GFX.drawText(35, 60, "Keine Dateien gefunden!");
  } else {
    int zeile = 0;
    for (size_t i = startSchnitt; i < dateiListe.size() && zeile < maxSichtbar; i++) {
      int yPos = 60 + (zeile * 8);
      // Alten Auswahlbalken entfernen (nur wenn sich der Bildausschnitt NICHT verschoben hat)
      if (letzterStartSchnitt == startSchnitt && letzterAusgewaehlterIndex != -1) {
        if ((int)i == letzterAusgewaehlterIndex && ausgewaehlterIndex != letzterAusgewaehlterIndex) {
          GFX.swapRectangle(25, yPos, 295, yPos + 7);
        }
      }
      // Text zeichnen
      if (letzterAusgewaehlterIndex == -1 || (int)i == ausgewaehlterIndex || (int)i == letzterAusgewaehlterIndex) {
        GFX.drawText(&fabgl::FONT_6x8, 35, yPos, dateiListe[i].c_str());
      }
      // Neuen Auswahlbalken setzen
      if ((int)i == ausgewaehlterIndex && ausgewaehlterIndex != letzterAusgewaehlterIndex) {
        GFX.swapRectangle(25, yPos, 295, yPos + 7);
      }
      zeile++;
    }
  }

  letzterAusgewaehlterIndex = ausgewaehlterIndex;
  GFX.swapBuffers();
}

bool starteGrafischenExplorer(char ext[]) {
  String cbuf;
  const char hi[] = "._";
  bool erfolg = false;

  // --- SD-Karten-Initialisierung ---
  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);
  delay(5);
  if (!SD.begin(kSD_CS, spiSD)) {
    syntaxerror(sderrormsg);
    sd_ende();
    return false;
  }

  // Diese Variablen müssen außerhalb des Verzeichnis-Ladens deklariert sein
  std::vector<String> combinedList;
  int aktuellerIndex = 0;
  int maxSichtbar = 16;
  int startSchnitt = 0;

  // Haupt-Label für das (Neu-)Laden des aktuellen Arbeitsordners (sd_pfad)
verzeichnis_laden:
  GFX.drawText(160, 26, "...wait");
  combinedList.clear();
  aktuellerIndex = 0;
  startSchnitt = 0;
  letzterStartSchnitt = -1;
  letzterAusgewaehlterIndex = -1;

  // Öffne den aktuellen Arbeitsordner
  File dir = SD.open(String(sd_pfad));
  if (!dir || !dir.isDirectory()) {
    // Falls der Ordner nicht existiert, versuchen wir ins Root zu retten
    strcpy(sd_pfad, "/");
    dir = SD.open(String(sd_pfad));
    if (!dir) {
      syntaxerror(sdfilemsg);
      sd_ende();
      return false;
    }
  }
  dir.seek(0);

  std::vector<String> folderList;
  std::vector<String> fileList;

  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      entry.close();
      break;
    }

    cbuf = String(entry.name());
    if (cbuf.startsWith(".")) {
      entry.close();
      continue;
    }

    String nameStr = cbuf;
    nameStr.toUpperCase();

    // Ordner werden immer angezeigt, Dateien nach Sucherweiterung gefiltert
    if (!entry.isDirectory() && nameStr.indexOf(ext) == -1) {
      entry.close();
      continue;
    }

    if (entry.isDirectory()) {
      folderList.push_back("[" + cbuf + "]"); // Ordner optisch kennzeichnen
    } else {
      fileList.push_back(cbuf);
    }

    entry.close();
    yield();
  }

  auto compCaseInsensitive = [](const String & a, const String & b) {
    String a_upper = a; a_upper.toUpperCase();
    String b_upper = b; b_upper.toUpperCase();
    return a_upper < b_upper;
  };

  std::sort(folderList.begin(), folderList.end(), compCaseInsensitive);
  std::sort(fileList.begin(), fileList.end(), compCaseInsensitive);

  combinedList = std::move(folderList);
  combinedList.insert(combinedList.end(), fileList.begin(), fileList.end());

  dir.close();

  // Erstes Zeichnen
  zeichneCustomExplorer(combinedList, aktuellerIndex, startSchnitt);
  GFX.drawText(160, 26, "       ");

  while (1) {
    char c = wait_key(0);

    // PFEIL RUNTER
    if (c == 0x05 && aktuellerIndex < (int)combinedList.size() - 1) {
      aktuellerIndex++;
      if (aktuellerIndex >= startSchnitt + maxSichtbar) {
        startSchnitt = aktuellerIndex - maxSichtbar + 1;
      }
      zeichneCustomExplorer(combinedList, aktuellerIndex, startSchnitt);
    }

    // PFEIL HOCH
    else if (c == 0x06 && aktuellerIndex > 0) {
      aktuellerIndex--;
      if (aktuellerIndex < startSchnitt) {
        startSchnitt = aktuellerIndex;
      }
      zeichneCustomExplorer(combinedList, aktuellerIndex, startSchnitt);
    }

    // PAGE DOWN (0x15)
    else if (c == 0x15 && aktuellerIndex < (int)combinedList.size() - 1) {
      aktuellerIndex += maxSichtbar;
      if (aktuellerIndex >= (int)combinedList.size()) {
        aktuellerIndex = (int)combinedList.size() - 1;
      }
      if (aktuellerIndex >= startSchnitt + maxSichtbar) {
        startSchnitt = aktuellerIndex - maxSichtbar + 1;
      }
      if (startSchnitt + maxSichtbar > (int)combinedList.size()) {
        startSchnitt = (int)combinedList.size() - maxSichtbar;
        if (startSchnitt < 0) startSchnitt = 0;
      }
      zeichneCustomExplorer(combinedList, aktuellerIndex, startSchnitt);
    }

    // PAGE UP (0x14)
    else if (c == 0x14 && aktuellerIndex > 0) {
      aktuellerIndex -= maxSichtbar;
      if (aktuellerIndex < 0) {
        aktuellerIndex = 0;
      }
      if (aktuellerIndex < startSchnitt) {
        startSchnitt = aktuellerIndex;
      }
      zeichneCustomExplorer(combinedList, aktuellerIndex, startSchnitt);
    }

    // BACKSPACE (0x7F=127) - Einen Ordner nach oben springen
    else if (c == 0x7F) {
      String pfadStr = String(sd_pfad);
      if (pfadStr != "/" && pfadStr.length() > 1) {
        if (pfadStr.endsWith("/")) {
          pfadStr.remove(pfadStr.length() - 1);
        }
        int letzterSlash = pfadStr.lastIndexOf('/');
        if (letzterSlash >= 0) {
          pfadStr = pfadStr.substring(0, letzterSlash + 1);
        }
        // Aktualisiere den globalen Arbeitsordner
        pfadStr.toCharArray(sd_pfad, pfadStr.length());
        goto verzeichnis_laden;
      }
    }

    // ENTER (Datei laden ODER Ordner öffnen)
    else if (c == 13 && !combinedList.empty()) {
      String auswahl = combinedList[aktuellerIndex];
      
      if (auswahl.startsWith("[") && auswahl.endsWith("]")) {
        // Ordnername extrahieren
        String ordnerName = auswahl.substring(1, auswahl.length() - 1);
        String neuerPfad = String(sd_pfad);
        
        if (!neuerPfad.endsWith("/")) {
          neuerPfad += "/";
        }
        neuerPfad += ordnerName ;
        // Aktualisiere den globalen Arbeitsordner
        neuerPfad.toCharArray(sd_pfad, neuerPfad.length() + 1);        
        goto verzeichnis_laden;
      } 
      else {
        // Datei ausgewählt
        
        erfolg = true;
        break;
      }
    }
    
    // ESCAPE (Abbrechen)
    else if (c == 0x03) {
      sd_ende();
      erfolg = false;
      GFX.clear();
      break;
    }
    yield();
  }

  if (erfolg) {
    // Kopiere den echten Dateinamen ohne Ordner-Klammern nach tempstring
    cbuf = String(combinedList[aktuellerIndex]);
    cbuf.toCharArray(tempstring, cbuf.length() + 1);    
    // --- PFAD-REPARATUR FÜR DIE LADE-FUNKTIONEN ---
    String vollerPfad = String(sd_pfad);
    // Falls wir nicht im Root "/" sind, MUSS ein Slash zwischen Ordner und Datei
    if (vollerPfad != "/") {
      vollerPfad += "/";
    }
    vollerPfad += String(tempstring); // Jetzt steht hier z.B. "/ORDNER/DATEI.BAS"    
    if (String(sd_pfad) != "/") {
      // Wenn wir in einem Unterordner sind, fügen wir den Slash direkt vor dem Dateinamen ein
      String dateiMitSlash = "/" + String(tempstring);
      dateiMitSlash.toCharArray(tempstring, dateiMitSlash.length() + 1);
    }
    load_file(1);
    string_marker = false;
    autorun = true;
  }
  letzterStartSchnitt = -1;
  letzterAusgewaehlterIndex = -1;
  return erfolg;
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
  while (!SD.begin( kSD_CS, spiSD)) {
    syntaxerror(sderrormsg);
    delay(3000);
  }
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

//void IRAM_ATTR onTimer()
//{
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
/*
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
*/

//#######################################################################################################################################
//############################################### Tabellensuche Befehle,Funkt. u.Optionen ###############################################
//#######################################################################################################################################
// Für Befehle
int findCommand() {
  return findInTable(keywords, kw_offsets, kw_id_map, KW_COUNT, KW_COUNT);
}

// Für Funktionen
int findFunction() {
  return findInTable(func_tab, func_offsets, func_id_map, FUNC_UNKNOWN, FUNC_UNKNOWN);
}

// Für Optionen
int findOption() {
  return findInTable(options_tab, opt_offsets, opt_id_map, OPT_COUNT, OPT_COUNT);
}

int8_t findInTable(const char* table, const uint16_t* offsets, const uint8_t* id_map, uint8_t count, int8_t error_val) {
  int low = 0;
  int high = count - 1;

  while (low <= high) {
    int mid = (low + high) / 2;
    uint16_t offset = offsets[mid];
    int i = 0;
    int res = 0;

    // Vergleichsschleife (Direkt integriert statt Hilfsfunktion spart Flash)
    while (true) {
      uint8_t table_byte = pgm_read_byte(&table[offset + i]);
      uint8_t table_char = table_byte & 0x7F;
      uint8_t input_char = (uint8_t)txtpos[i];

      if (input_char != table_char) {
        res = input_char - table_char;
        break;
      }

      if (table_byte & 0x80) { // Wortende in Tabelle erreicht
        // WORTGRENZEN-CHECK:
        // kommt nach dem Wort ein Buchstabe, wird weiter gesucht zBsp. TIME vs. TIMER
        if (isalpha((uint8_t) txtpos[i + 1])) {
          res = 1;
          break;
        }

        // Wort gefunden!
        txtpos += (i + 1); // Zeiger vorrücken
        spaces();                                                               //eingefügt nach Umstellung auf findcommand()
        return pgm_read_byte(&id_map[mid]);
      }
      i++;
    }

    if (res < 0) high = mid - 1;
    else low = mid + 1;
  }
  return error_val;
}

//#######################################################################################################################################
//############################################### Hilfsprogramme für die Syntaxhervorhebung #############################################
//#######################################################################################################################################
int8_t peekRelop(const char* p, int &matched_len) {
  int8_t low = 0;
  int8_t high = 12;
  char r1 = *p;
  char r2 = *(p + 1);

  while (low <= high) {
    int8_t mid = (low + high) >> 1;
    char c1 = pgm_read_byte(&relop_tab[mid << 1]);

    if (c1 == r1) {
      // Zum Anfang der Gruppe mit diesem Startzeichen springen
      while (mid > 0 && pgm_read_byte(&relop_tab[(mid - 1) << 1]) == r1) mid--;

      int8_t found_id = -1;
      matched_len = 0;

      // Die Gruppe durchsuchen
      while (mid <= 12 && pgm_read_byte(&relop_tab[mid << 1]) == r1) {
        char c2 = pgm_read_byte(&relop_tab[(mid << 1) + 1]);

        if (c2 != 0 && c2 == r2) {
          // Volltreffer 2-Zeichen (z.B. "<=") -> Sofort fertig!
          matched_len = 2;
          return pgm_read_byte(&relop_id[mid]);
        }

        if (c2 == 0) {
          matched_len = 1;
          found_id = pgm_read_byte(&relop_id[mid]);
        }
        mid++;
      }
      return found_id; // Wenn kein 2-Zeichen Match kam, nimm das 1-Zeichen Match
    }
    if (c1 < r1) low = mid + 1;
    else high = mid - 1;
  }
  return -1;
}

int8_t peekInTable(const char* start_p, const char* table, const uint8_t* id_map, const uint16_t* offsets, uint8_t count, int &matched_len) {
  int8_t low = 0;
  int8_t high = count - 1;

  while (low <= high) {
    int8_t mid = (low + high) >> 1;
    uint16_t offset = offsets[mid];
    int8_t i = 0;
    int8_t res = 0;

    while (true) {
      uint8_t t_byte = pgm_read_byte(&table[offset + i]);
      uint8_t t_char = t_byte & 0x7F;
      uint8_t i_char = (uint8_t)start_p[i];

      if (i_char != t_char) {
        res = i_char - t_char;
        break;
      }

      if (t_byte & 0x80) { // Wortende in der Tabelle
        char next = start_p[i + 1];
        if (isalnum(next) || next == '_' || next == '$') {
          res = 1;
        } else {
          matched_len = i + 1; // Wie viele Zeichen hat das Wort?
          return pgm_read_byte(&id_map[mid]);
        }
        break;
      }
      i++;
    }
    if (res < 0) high = mid - 1;
    else low = mid + 1;
  }
  return -1;
}

//#######################################################################################################################################
//############################################### Indextabellen für die div.Befehlstabellen erstellen ###################################
//#######################################################################################################################################
void setupTableIndex(const char* table, uint16_t* offsets, int count) {
  uint16_t pos = 0;
  for (int i = 0; i < count; i++) {
    offsets[i] = pos;
    while (!(pgm_read_byte(&table[pos++]) & 0x80));
  }
}

//#######################################################################################################################################
//############################################### Operatortabelle lesen #################################################################
//#######################################################################################################################################
int findRelopBinary() {
  int8_t low = 0;
  int8_t high = 12;
  char r1 = *txtpos;
  char r2 = *(txtpos + 1);

  while (low <= high) {
    int8_t mid = (low + high) >> 1;
    char c1 = pgm_read_byte(&relop_tab[mid << 1]);

    if (c1 == r1) {
      // 1. Finde den absolut ersten Eintrag mit diesem Startzeichen in der Tabelle
      while (mid > 0 && pgm_read_byte(&relop_tab[(mid - 1) << 1]) == r1) {
        mid--;
      }

      // 2. Prüfe alle Einträge, die mit r1 beginnen (max. 4 Stück)
      while (mid <= 12) {
        uint16_t addr = mid << 1;
        if (pgm_read_byte(&relop_tab[addr]) != r1) break;

        char c2 = pgm_read_byte(&relop_tab[addr + 1]);

        if (c2 != 0) {
          // 2-Zeichen-Check (z.B. <=, <>, <<)
          if (c2 == r2) {
            txtpos += 2;
            return pgm_read_byte(&relop_id[mid]);
          }
        } else {
          // 1-Zeichen-Check (z.B. <) -> Immer das letzte in der r1-Gruppe
          txtpos += 1;
          return pgm_read_byte(&relop_id[mid]);
        }
        mid++;
      }
      break;
    }
    if (c1 < r1) low = mid + 1;
    else high = mid - 1;
  }
  return RELOP_UNKNOWN;
}


//#######################################################################################################################################
//--------------------------------------------- SETUP -----------------------------------------------------------------------------------
//#######################################################################################################################################

void setup()
{
  setCpuFrequencyMhz(240);                                                           //mit dieser Option gibt's Startschwierigkeiten
  Serial.begin(115200);

  // Im Programm-Start:
  setupTableIndex(keywords, kw_offsets, KW_COUNT);                                  // Basic-Befehls-Index-Tabelle erstellen
  setupTableIndex(func_tab, func_offsets, FUNC_UNKNOWN);                            //Funktions-Index-Tabelle erstellen
  setupTableIndex(options_tab, opt_offsets, OPT_COUNT);                             //Index-Tabelle für Options erstellen

  pinMode(kSD_CS, OUTPUT);
  digitalWrite(kSD_CS, HIGH);

  SPI.begin();

  // PSRAM des ESP32 starten
  if (!psramInit()) {
    Serial.println("PSRAM nicht gefunden!");
    while (1);
  }

  //------------------- 1MB Variablen-Speicher ------------------------------------------

  SPI_memSize = variablen_groesse;
  var_table_psram = (float*)ps_malloc(variablen_groesse);

  if (var_table_psram == nullptr) {
    Serial.println("Kritischer Fehler: PSRAM-Zuweisung fehlgeschlagen!");
    while (1);
  } else {
    memset(var_table_psram, 0, SPI_memSize);          // 1MB Speicher löschen (float 4byte * 256kb)
  }
  //-------------------------------------------------------------------------------------
  //------------------- 1MB User-Speicher ------------------------------------------

  user_psram = (uint8_t*)ps_malloc(user_groesse);
  if (user_psram == nullptr) {
    Serial.println("Kritischer Fehler: User-PSRAM-Zuweisung fehlgeschlagen!");
    while (1);
  } else {
    memset(user_psram, 0, user_groesse);          // 1MB Speicher löschen
  }
  //-------------------------------------------------------------------------------------

  EEPROM. begin ( EEPROM_SIZE ) ;
  delay(200);
  if (EEPROM.read(100) == erststart_marker) {                                         //auf jungfräulichkeit prüfen

    //################ Farbschema aus dem internen EEPROM lesen ##################
    Vordergrund = EEPROM.read(0) ;   //512 Byte Werte im EEPROM speicherbar
    Hintergrund = EEPROM.read(1);
    user_vcolor = Vordergrund;       //User-Vordergrundfarbe merken
    user_bcolor = Hintergrund;       //User-Hintergrundfarbe merken
    //#############################################################################
    fontsatz = EEPROM.read(2);
    user_font = fontsatz;         //User-Fontsatz merken

    // --- auf Platz 15 im EEPROM steht das Keyboard-Layout
    byte k = EEPROM.read(15);
    if (k > 0 && k < 10) Keyboard_lang = k;
    Theme_marker = false;
    // --- ist der Theme_marker (77) auf Platz 17 gesetzt, dann das gespeicherte Theme setzen
    if (EEPROM.read(17) == THEME_SET) {
      Theme_state = EEPROM.read(16);
      Theme_marker = true;
    }
    else Theme_state = 0;
  }
  else                                                  //der ESP ist noch jungfräulich, also standard-Werte setzen
  {

    Vordergrund = 60;                                     //CPC Theme
    Hintergrund = 1;
    user_font   = 2;
    Theme_state = 2;                                      //CPC Theme
  }

  delay(1000);                                              //eine sek warten, damit die CardKB-Tastatur starten kann

  Keyboard.begin(GPIO_NUM_33, GPIO_NUM_32);
  Set_Layout();                                             //Keyboard-Layout setzen

  delay(200);


  //************************************************************ welcher Bildschirmtreiber? *********************************************************
  // 64 colors

  VGAController.begin();                                                                //VGA-Variante //64 Farben
  VGAController.setResolution(QVGA_320x240_60Hz);                                    //Standard-Auflösung

  //***************************************************************************************************************************************************

  Terminal.begin(&VGAController);
  Terminal.activate(TerminalTransition::None); // Sofort aktivieren ohne Effekt
  Terminal.connectLocally();                                                           // für Terminal Komandos

  FRAM_PIC_OFFSET = (VGAController.getViewPortWidth() * VGAController.getViewPortHeight()) + 4;     //Bildoffset im Speicher X*Y Dimension + 4 Byte für Dimensionsdaten

  Terminal.enableCursor(true);
  fbcolor(Vordergrund, Hintergrund);
  tc.setCursorPos(1, 1);
  GFX.clear();

  if (Theme_marker) set_theme(Theme_state, user_font);                                           //Theme setzen, wenn im EEprom gespeichert
  else set_font(user_font);

  PS2Controller.keyboard()-> onVirtualKey = [&](VirtualKey * vk, bool keyDown) {
    if (keyDown) {

      if (*vk == VirtualKey::VK_ESCAPE) {
        break_marker = true;                                                            //ESC abfangen und in Ctrl-C wandeln
        *vk = VirtualKey::VK_NONE;
      }
      else if (*vk == VirtualKey::VK_DOWN) {
        cursor_down = true;
        *vk = VirtualKey::VK_NONE;
      }
      // PFEIL HOCH
      else if (*vk == VirtualKey::VK_UP) {
        cursor_up = true;
        *vk = VirtualKey::VK_NONE;
      }
      else if (*vk == VirtualKey::VK_PAGEDOWN) {
        page_down = true;
        *vk = VirtualKey::VK_NONE;
      }
      // PFEIL HOCH
      else if (*vk == VirtualKey::VK_PAGEUP) {
        page_up = true;
        *vk = VirtualKey::VK_NONE;
      }
      else if (*vk == VirtualKey::VK_APPLICATION) {                                     //Anzeige der belegten Variablen
        if (current_line == NULL) {
          key_command = KW_PRINT;
          show_vars   = true;
          function_key = true;
        }
        *vk = VirtualKey::VK_NONE;
      }

      else if (*vk == VirtualKey::VK_F1) {                                              //Anzeige der Funktionstastenbelegung
        if (current_line == NULL) {
          function_key = true;
          key_command = KW_PRINT;
          show_function_key();
        }
        *vk = VirtualKey::VK_NONE;
      }

      else if (*vk == VirtualKey::VK_F2) {                                              //LIST - Befehl
        if (current_line == NULL) {
          key_command = KW_LIST;
          function_key = true;
          Terminal.println();
        }
        *vk = VirtualKey::VK_NONE;
      }

      else if (*vk == VirtualKey::VK_F3) {                                              //RUN - Befehl
        if (current_line == NULL) {
          key_command = KW_RUN;
          function_key = true;
          Terminal.println();
        }
        *vk = VirtualKey::VK_NONE;
      }

      else if (*vk == VirtualKey::VK_F4) {                                              //Dir - Befehl
        if (current_line == NULL) {
          key_command = KW_DIR;
          function_key = true;
          Terminal.println();
        }
        *vk = VirtualKey::VK_NONE;
      }

      else if (*vk == VirtualKey::VK_F5) {                                               //TRON/TROFF
        if (current_line == NULL) {
          tron_marker = !tron_marker;
          if (tron_marker) printmsg("TRON", 1);
          else printmsg("TROFF", 1);
          line_terminator();
          printmsg("READY.", 1);
        }
        *vk = VirtualKey::VK_NONE;
      }
      else if (*vk == VirtualKey::VK_F6) {                                              //Ausgabe Char-Table 32..127
        if (current_line == NULL) {
          char_out(32, 128);
        }
        *vk = VirtualKey::VK_NONE;
      }
      else if (*vk == VirtualKey::VK_F7) {                                              //Ausgabe Char-Table 128..255
        if (current_line == NULL) {
          char_out(128, 256);
        }
        *vk = VirtualKey::VK_NONE;
      }
      else if (*vk == VirtualKey::VK_F8) {                                              //Ausgabe Color-Tabelle
        if (current_line == NULL) {
          color_out();
        }
        *vk = VirtualKey::VK_NONE;
      }

      else if (*vk == VirtualKey::VK_F9) {                                               //Grafiksymbole on/off
        Graph_char = !Graph_char;
        PS2Controller.keyboard()->setLEDs(false, false, Graph_char);
        *vk = VirtualKey::VK_NONE;
      }

      else if (*vk == VirtualKey::VK_F10) {                                              //Anzeige Systemparameter
        if (current_line == NULL) {
          show_systemparameters();
        }
        *vk = VirtualKey::VK_NONE;
      }

      else if (*vk == VirtualKey::VK_F11) {                                              //SPI-RAM-Löschen (Test)
        if (current_line == NULL) {
          Terminal.print("erase SPI-RAM, please wait...");
          memset(var_table_psram, 0, SPI_memSize);                                       //Variablen-Speicher löschen
          memset(user_psram, 0, user_groesse);                                           //User-Speicher löschen
          line_terminator();
          printmsg("READY.", 1);
        }
        *vk = VirtualKey::VK_NONE;
      }

      else if (*vk == VirtualKey::VK_F12) {                                               //F12 = Reboot
        Terminal.print("Now reboot ...");
        delay(1500);
        ESP.restart();
        *vk = VirtualKey::VK_NONE;
      }

      yield();
    }
    yield();
  };

  //--------------- ESP32 RTC starten und stellen --------------------
  char const *compileDate = __DATE__;
  char const *compileTime = __TIME__;

  // Monate konvertieren
  char monthStr[4];
  int day, year, hour, minute, second;
  sscanf(compileDate, "%s %d %d", monthStr, &day, &year);
  sscanf(compileTime, "%d:%d:%d", &hour, &minute, &second);

  int month = 1;
  const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  for (int i = 0; i < 12; i++) {
    if (strcmp(monthStr, months[i]) == 0) {
      month = i + 1;
      break;
    }
  }
  e_rtc.setTime(second, minute, hour, day, month, year);

  //------------------------------------------------------------------
  //-------------------------------- Akku-Überwachung per Timer0-Interrupt --------------------------------------------
  /*
    #ifdef Akkualarm_enabled
    Akku_timer = timerBegin(0, 80, true);
    timerAttachInterrupt(Akku_timer, &onTimer, true);
    timerAlarmWrite(Akku_timer, 60000000, true);         //ca.60sek bis Interrupt ausgelöst wird
    timerAlarmEnable(Akku_timer);                        //Interrupt-Routine
    #endif
  */
  //----------------------------------------------------------------------------------------------------------------------
}

//#######################################################################################################################################
//############################################### Zeileneditor ##########################################################################
//#######################################################################################################################################

void Editor(int lnr) {
  int next_lnr = lnr;

  //--------------------------------- Sicherheitsabfrage, falls die Zeilennummer nicht existiert ----------------------------------------
  list_line = findline();
  if (!list_line) {
    printmsg("END OF PROGRAM", 1);
    return;
  }
  //-------------------------------------------------------------------------------------------------------------------------------------

  while (next_lnr > 0  && !break_marker) {
    linenum = next_lnr;
    list_line = findline();

    // 1. Falls Ende des Programms erreicht, Editor verlassen
    if (list_line == program_end) {
      printmsg("END OF PROGRAM", 1);
      break;
    }

    // 2. Zeile laden
    edit_getline();

    // 3. Editor starten
    LineEditor.setText(tempstring);
    LineEditor.edit();

    // 4. Editierte Zeile abholen
    Edit_line = LineEditor.get();

    // 5. In Puffer kopieren und speichern
    txtpos = (char*)(program_end + sizeof(LINENUM));
    while (*Edit_line) {
      *txtpos++ = *Edit_line++;
    }
    *txtpos = NL;

    move_line();
    insert_line();

    // 6. NÄCHSTE ZEILE FINDEN
    list_line = findline();                                       // Aktuelle Position neu bestimmen
    if (list_line < program_end) {

      uint8_t current_len = *(list_line + sizeof(LINENUM));       // Springe zum Ende der aktuellen Zeile
      char* next_ptr = list_line + current_len;

      if (next_ptr < program_end) {
        next_lnr = *((LINENUM *)next_ptr);
      } else {
        next_lnr = 0;                                             // Keine weitere Zeile vorhanden
      }
    } else {
      next_lnr = 0;
    }

  }
  outchar(CR);
  outchar(NL);
  break_marker = 0; // Marker zurücksetzen
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
//********************************************************** DIM-Befehl *****************************************************************
//#######################################################################################################################################
int Array_Dim(void) {
  int res;
  //spaces();                                         //eingefügt nach Umstellung auf find_function()
  while (1) {
    if (*txtpos >= 'A' && *txtpos <= 'Z') {
      uint32_t tmp;
      int x = 0, y = 0, z = 0;
      uint32_t grenze, ort;
      uint8_t p_data[6], len;
      bool str = false;

      // 1. Variablenname parsen (Unterstützt AA, AB...)
      tmp = (*txtpos - 'A');
      txtpos++;
      if (*txtpos >= 'A' && *txtpos <= 'Z') {
        tmp += ((*txtpos - 'A' + 1) * 26);
        txtpos++;
      }
      while (*txtpos >= 'A' && *txtpos <= 'Z') txtpos++; // Restliche Buchstaben überspringen

      // 2. Typ bestimmen ($ für Strings)
      if (*txtpos == '$') {
        len = STR_LEN;
        str = true;
        txtpos++;
      } else {
        len = sizeof(float);
      }

      // 3. Dimensionen parsen (X, Y, Z)

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

      if (Test_char(')')) return 1;
      // 4. Speicherbedarf berechnen (32-Bit für ESP32)
      grenze = (uint32_t)(z + 1) * (y + 1) * (x + 1) * len;

      if (Var_Neu_Platz + grenze > VAR_MAX) {                           //Überprüfung auf Variablen-Grenze
        syntaxerror(outofmemory);
        return 1;
      }

      // 5. Adresse in der Symboltabelle berechnen
      ort = (str ? STR_TBL : VAR_TBL) + (tmp * 6);
      //Terminal.print(ort,HEX);
      // 6. Metadaten schreiben (Adresse im RAM, Dimensionen)
      p_data[0] = (Var_Neu_Platz >> 8) & 0xFF; // High
      p_data[1] = Var_Neu_Platz & 0xFF;        // Low
      p_data[2] = (x >> 8) & 0xFF;
      p_data[3] = x & 0xFF;
      p_data[4] = (uint8_t)y;
      p_data[5] = (uint8_t)z;

      // Auf dem ESP32: Nutze entweder SPI_RAM oder ein internes Array
      SPI_RAM_write(ort, p_data, 6);
      Var_Neu_Platz += grenze;
    }

    if (*txtpos != ',') return 0; // Ende der DIM-Anweisung
    txtpos++; // Nächstes Array in derselben Zeile
  }
}

//#######################################################################################################################################
//----------------------------------------------------------- OPTION-Befehl -------------------------------------------------------------
//#######################################################################################################################################
int Option(void) {
  byte p[6];
  table_index = findOption();         //Optionstabelle lesen
  char fu = table_index;
  int i, adr;

  switch (fu) {

    case OPT_FONT:
      p[0] = get_value();
      EEPROM.write(2, p[0]);          //Font-Nummer im Flash speichern                        Platz 2
      EEPROM.write(17, 0);            //THEME-Marker löschen
      EEPROM.commit () ;
      set_font(p[0]);                 //setze Font
      if (EEPROM.read(100) != erststart_marker) {                                           //marker-setzen, das werte im EEprom stehen
        EEPROM.write ( 100, erststart_marker) ;                                             //Platz 100
        EEPROM.commit () ;
      }
      break;

    case OPT_KEYBOARD:
      p[0] = get_value();
      EEPROM.write(15, p[0]);         //Keyboard-Layout im Flash speichern                    Platz 15
      EEPROM.commit () ;
      Terminal.println("For take effect now reboot!");
      delay(1000);
      ESP.restart();
      break;

    case OPT_COLOR:
      p[0] = get_value();
      if (Test_char(',')) return 1;
      p[1] = get_value();
      EEPROM.write(0, p[0]);          //Vordergrundfarbe im Flash speichern                   Platz 0
      EEPROM.write(1, p[1]);          //Hintergrundfarbe im Flash speichern                   Platz 1
      EEPROM.write(17, 0);            //THEME-Marker löschen
      EEPROM.commit () ;
      Vordergrund = p[0];
      Hintergrund = p[1];
      fbcolor(Vordergrund, Hintergrund); //Farben setzen
      if (EEPROM.read(100) != erststart_marker) {                                           //marker-setzen, das werte im EEprom stehen
        EEPROM.write ( 100, erststart_marker) ;                                             //Platz 100
        EEPROM.commit () ;
      }
      break;

    case OPT_THEME:
      p[0] = get_value();
      EEPROM.write(16, p[0]);          //Nummer des Themes                                      Platz 16
      EEPROM.write(17, THEME_SET);     //THEME-Marker                                           Platz 17
      EEPROM.commit();
      set_theme(p[0], fontsatz);
      EEPROM.write(2, byte(fontsatz));  //Font-Nummer im Flash speichern                        Platz 2
      EEPROM.commit () ;
      if (EEPROM.read(100) != erststart_marker) {                                           //marker-setzen, das werte im EEprom stehen
        EEPROM.write ( 100, erststart_marker) ;
        EEPROM.commit () ;
      }
      break;

    case OPT_PATH:                    //Arbeits-Pfad im EEPROM-Platz 20-50 (max. 30 Zeichen)
      cmd_chdir();
      adr = 20;
      i = 0;
      EEPROM.write(19, PATH_SET);                                                              //Platz 19 - 99
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


  if (fu == OPT_COUNT)                                              //am ende angekommen, Option nicht gefunden
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  return 0;
}

//#######################################################################################################################################
//############################################### Tastatur - LAYOUT #####################################################################
//#######################################################################################################################################

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
//********************************************************** PIC-Befehl *****************************************************************
//#######################################################################################################################################
int show_Pic(void) {
  long ad, n_bytes;
  int x, y, iv;
  int dx, dy, ddx, ddy, px, py, vv, vh;
  float scal;
  byte w[400], a, buf[4];
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
  
  
  if (Test_char('_')) return 1;                       //Unterstrich für folgenden Befehlsbuchstaben
  c = spaces();                                       //Befehlsbuchstabe lesen
  txtpos++;
  switch (c) {

    //****************************************************** PIC_D(PIC_Nr<,swap Backcolor><,X,Y>) ******************************************
    case 'D':                                         //Grafik im FRAM auf dem Bildschirm ausgeben
      {
        if (Test_char('(')) return 1;
        ad = get_value();
        if (ad > ((user_groesse - 0x10000) / FRAM_PIC_OFFSET) - 1) {
          syntaxerror(outofmemory);
          return 1;
        }

        ad = ad * FRAM_PIC_OFFSET;                      //Bildspeicherplatz (320x240)

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

        user_fram_read(FRAM_OFFSET + ad, buf, 4);         //Dimension lesen
        px = buf[0] + (buf[1] << 8);
        py = buf[2] + (buf[3] << 8);
        ad += 4;
        for (y = dy + py - 1 ; y > dy - 1; y--) {
          user_fram_read(FRAM_OFFSET + ad, w, px);
          for (int i = 0; i < px; i++) {
            fcolor(w[i]);
            if ((dx + i) < vh && y < vv) GFX.setPixel(dx + i, y);
          }
          ad += px;
          x = 0;
        }
        if (iv > 0) GFX.swapRectangle(dx, dy , dx + px - 1, dy + py - 1); //swap Backcolor
      }
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

    //****************************************************** PIC_I(X,Y,Filename.bmp<,scal>) **********************************
    case 'I':                                         //Import <- BMP
      if (Test_char('(')) return 1;
      dx = get_value();                               //x
      if (Test_char(',')) return 1;                   //Komma überspringen
      dy = get_value();                               //y
      if (Test_char(',')) return 1;                   //Komma überspringen
      get_value();                                    //Dateiname in tempstring
      scal = 1;                                       //Skalierung auf 1 begrenzen

      if (Test_char(')')) return 1;
      import_pic(dx, dy, tempstring, scal);
      break;

    //****************************************************** PIC_L(PIC_Nr,Filename) ******************************************
    case 'L':                                         //Load PIC_RAW-Data
      {
        if (Test_char('(')) return 1;
        ad = get_value();                               //Adresse im Speicher
        if (ad > ((user_groesse - 0x10000) / FRAM_PIC_OFFSET) - 1) {
          syntaxerror(outofmemory);
          return 1;
        }
        ad = ad * FRAM_PIC_OFFSET;                      //Bildspeicherplatz
        if (Test_char(',')) return 1;                   //Komma überspringen
        get_value();                                    //Dateiname in tempstring
        if (Test_char(')')) return 1;
        load_pic(FRAM_OFFSET + ad, tempstring);
      }
      break;

    //****************************************************** PIC_S(PIC_Nr,Filename) ******************************************
    case 'S':                                         //Save PIC_RAW-Data
      {
        if (Test_char('(')) return 1;
        ad = get_value();                               //Adresse im Speicher
        if (ad > ((user_groesse - 0x10000) / FRAM_PIC_OFFSET) - 1) {//Überprüfung auf max.Anzahl der Bilder
          syntaxerror(outofmemory);
          return 1;
        }
        ad = ad * FRAM_PIC_OFFSET;                      //Bildspeicherplatz

        if (Test_char(',')) return 1;                   //Komma überspringen
        get_value();                                    //Dateiname in tempstring
        if (Test_char(')')) return 1;
        user_fram_read(FRAM_OFFSET + ad, buf, 4);         //Dimension lesen
        px = buf[0] + (buf[1] << 8);
        py = buf[2] + (buf[3] << 8);
        n_bytes = (px * py) + 4;                        //x*y=Biddaten + 4 Bytes der Dimension
        save_pic(FRAM_OFFSET + ad, n_bytes, tempstring);
      }
      break;

    //****************************************************** PIC_P(PIC_Nr,X,Y,XX,YY) ******************************************
    case 'P':                                         //Grafikbildschirm in FRAM speichern
      if (Test_char('(')) return 1;
      ad = get_value();
      if (ad > ((user_groesse - 0x10000) / FRAM_PIC_OFFSET) - 1) {
        syntaxerror(outofmemory);
        return 1;
      }
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
      USER_RAM_write(FRAM_OFFSET + ad, buf, 4);       //XY-Dimension
      ad += 4;
      for (y = dy + ddy ; y > dy ; y--) {
        for (x = dx; x < (dx + ddx); x++) {
          if (x < vh && y < vv)
          {
            uint8_t r = GFX.getPixel(x, y).R & 0xC0; // Bits 7 und 6
            uint8_t g = GFX.getPixel(x, y).G & 0xC0; // Bits 7 und 6
            uint8_t b = GFX.getPixel(x, y).B & 0xC0; // Bits 7 und 6
            // R wandert auf Bits 5-4, G auf Bits 3-2, B auf Bits 1-0
            a = (r >> 2) | (g >> 4) | (b >> 6);
          }
          else a = 0;
          USER_RAM_write8(FRAM_OFFSET + ad++, a);

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
  while (!SD.begin( kSD_CS, spiSD)) {
    syntaxerror(sderrormsg);
    delay(3000);
  }
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
  while (!SD.begin( kSD_CS, spiSD)) {
    syntaxerror(sderrormsg);
    delay(3000);
  }
  if ( !SD.exists(String(sd_pfad) + String(file)))
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
  if (xx >= vh && yy >= vv ) { //&& sc <= 1) {
    xtmp = float(xx) / vh ;
    ytmp = float(yy) / vv ;
    restx = xx % vh;
  }
  else {
    xtmp = ytmp = sc;
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
  while (!SD.begin( kSD_CS, spiSD)) {
    syntaxerror(sderrormsg);
    delay(3000);
  }

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

  for (int i = 0; i < durchlaeufe; i++) {
    user_fram_read(adr, c, 1024);
    adr += 1024;
    for (int s = 0; s < 1024; s++) {
      fp.write( c[s] );
    }
  }
  if (rest > 0) {
    user_fram_read(adr, c, rest);
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

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);           //SCK,MISO,MOSI,SS 13 //HSPI1
  while (!SD.begin( kSD_CS, spiSD)) {
    syntaxerror(sderrormsg);
    delay(3000);
  }

  if ( !SD.exists(String(sd_pfad) + String(tempstring)))
  {
    syntaxerror(sdfilemsg);
    sd_ende();                                                //SD-Card unmount
    return 1;
  }
  else {
    fp = SD.open( String(sd_pfad) + String(file), FILE_READ);
  }
  for (int s = 0; s < 4; s++) {                               //Dimension lesen
    c[s] = fp.read();
  }
  fp.seek(0);                                                 //Dateizeiger wieder zurück auf Anfang
  rx = c[0] + (c[1] << 8);
  ry = c[2] + (c[3] << 8);
  n = (rx * ry) + 4;
  if (n > 1024) durchlaeufe = n / 1024;
  rest = n % 1024;

  for (int i = 0; i < durchlaeufe; i++) {
    for (int s = 0; s < 1024; s++) {
      c[s] = fp.read();
    }
    sc = fp.position();
    USER_RAM_write(adr, c, 1024);
    adr += 1024;
  }
  if (rest > 0) {
    fp.seek(sc);
    for (int s = 0; s < rest; s++) {
      c[s] = fp.read();
    }
    fp.close();
    sd_ende();                                                //SD-Card unmount
    USER_RAM_write(adr, c, rest);                             //Dimension lesen
  }
  return 0;
}

//#######################################################################################################################################
//------------Befehl GRID_typ(x,y,x_zell,y_zell,x_pixel_step,y_pixelstep,frame_color,grid_color,pixel_raster,scale,arrow,frame) ---------
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
  // Sicherheits-Check gegen Endlosschleife (Intervall muss min. 1 sein)
  uint8_t step = (pix < 1) ? 1 : pix;

  // Bereichsprüfung, um außerhalb des Displays liegende Pixel zu ignorieren
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (xx >= GFX.getWidth()) xx = GFX.getWidth() - 1;
  if (yy >= GFX.getHeight()) yy = GFX.getHeight() - 1;

  for (int a = x; a <= xx; a += step) {
    for (int b = y; b <= yy; b += step) {
      GFX.setPixel(a, b);
      // Hinweis: Falls GFX eine Farbe erwartet, hier fbcolor(Vordergrund) setzen
    }
  }

  // Wenn du viele Punkte zeichnest, entlastet ein kurzes delay(0) den Watchdog
  delay(0);
}

//#######################################################################################################################################
//############################################### Befehl TEXT ###########################################################################
//#######################################################################################################################################
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
      //GFX.drawTextWithEllipsis(&fabgl::FONT_8x8, x_text, y_text, tempstring, 100);
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
    case 25:
      GFX.drawText(&fabgl::FONT_8x8_PET, x_text, y_text, tempstring);
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
  nr = abs(get_value());                                    //Fensternummer empfangen

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
//############################################### Char-Tabelle ausgeben F6,7 ############################################################
//#######################################################################################################################################
void char_out(int lo, int hi) {
  int z = 0;
  char buf[16]; // Buffer für formattierte Ausgabe

  for (int i = lo; i < hi; i++) {
    // Formattierung: 3-stellige Zahl (rechtsbündig) + '=' + Zeichen
    // %3d sorgt dafür, dass die Spalten bei 1, 10 und 100 immer gleich breit bleiben
    snprintf(buf, sizeof(buf), "%3d=%c ", i, (char)i);
    printmsg(buf, 0);

    z++;
    if (z == 6) { // Nach 6 Einträgen eine neue Zeile
      z = 0;
      line_terminator();

    }
    delay(5);
    yield();
  }

  // Abschlussmeldung
  fbcolor(Vordergrund, Hintergrund);
  line_terminator();
  printmsg("READY.", 1);
}

//#######################################################################################################################################
//############################################### Farbcodes ausgeben F8 #################################################################
//#######################################################################################################################################

void color_out(void) {
  int z = 0;
  char tx[10];
  Terminal.enableCursor(false);                                 //Cursor ausschalten
  for (int i = 0; i < 64 ; i++)
  {
    if (i == 0) fbcolor(63, 0);
    else
    {
      fbcolor(0, i);
      delay(5);                             //kleine Pause, sonst wird die Farbe nicht korrekt gesetzt
    }
    outchar(' ');
    if (i < 10) outchar(' ');
    printnum(i, 0);
    outchar(' ');
    z++;

    if (z == 8) {
      z = 0;
      fbcolor(Hintergrund, Hintergrund);
      line_terminator();
    }
    yield();
  }
  fbcolor(Vordergrund, Hintergrund);
  line_terminator();
  printmsg("READY.", 1);
  Terminal.enableCursor(true);                                 //Cursor einschalten
}


//#######################################################################################################################################
//############################################### Funktion GPX ##########################################################################
//#######################################################################################################################################

//->modes=0 - test Pixel gesetzt(1) oder nicht(0); modes=1 gibt die Farbe des Pixels zurück
int Test_pixel(int x, int y, bool modes) {
  // Bereichsprüfung am Anfang
  if (x < 0 || x >= GFX.getWidth() || y < 0 || y >= GFX.getHeight()) {
    return 0;
  }

  // Pixel nur EINMAL lesen
  auto pixel = GFX.getPixel(x, y);

  // Umrechnung: 0-255 auf 0-3 (85er Schritte)
  // Division durch 85 ist langsam -> Multiplikation/Shift oder einfache IFs sind schneller
  // Hier nutzen wir die kompakte Bit-Logik aus deinem Original:
  int r = pixel.R / 85;
  int g = pixel.G / 85;
  int b = pixel.B / 85;

  // 6-Bit Wert berechnen: RRR GGG BBB (je 2 Bit)
  int c = b + (g << 2) + (r << 4);

  if (!modes) {
    // Falls nur geprüft werden soll, ob der Pixel "gesetzt" ist (ungleich Hintergrund)
    return (c != Hintergrund) ? 1 : 0;
  }

  return c; // Farbewert zurückgeben
}

//#######################################################################################################################################
//############################################### ARC-Befehl ############################################################################
//#######################################################################################################################################
void drawArc(int x, int y, int r_min, int r_max, int gr_start, int gr_end, int filled) {
  const int max_pnts = (gr_end - gr_start + 1) * 2;
  Point pnt[max_pnts]; // Auf dem ESP32 sind lokale Arrays dieser Größe ok (Stack ~8KB)
  int npnt = 0;

  // Vorfaktor für Umrechnung Grad -> Radiant
  const float deg2rad = M_PI / 180.0f;

  // Innere Kurve (r_min)
  for (int n = gr_start; n <= gr_end; n++) {
    float rad = n * deg2rad;
    int px = x + (cosf(rad) * r_min * 1000) / 800;
    int py = y + (sinf(rad) * r_min);

    // Nur hinzufügen, wenn Punkt sich vom letzten unterscheidet (Inlined drawArcP)
    if (npnt == 0 || pnt[npnt - 1].X != px || pnt[npnt - 1].Y != py) {
      pnt[npnt].X = px;
      pnt[npnt].Y = py;
      npnt++;
    }
  }

  // Äußere Kurve (r_max)
  for (int n = gr_end; n >= gr_start; n--) {
    float rad = n * deg2rad;
    int px = x + (cosf(rad) * r_max * 1000) / 800;
    int py = y + (sinf(rad) * r_max);

    if (pnt[npnt - 1].X != px || pnt[npnt - 1].Y != py) {
      pnt[npnt].X = px;
      pnt[npnt].Y = py;
      npnt++;
    }
  }

  if (filled == 1) GFX.fillPath(pnt, npnt);
  else GFX.drawPath(pnt, npnt);

  GFX.waitCompletion();
}

//#######################################################################################################################################
//############################################### Renum Befehl ##########################################################################
//#######################################################################################################################################

void renum()
{
  int l = 0;
  int bis, num;
  bool b_bis = false;
  unsigned int adr = 0x0;                                                          //Bereich in dem die Zeilennummern-Tabelle steht
  unsigned int startnum;                                                           //Startzeilennummer
  unsigned int schritt;                                                            //Schrittweite

  byte p_data[2];
  LINENUM line_num;
  schritt = 10;
  zeilen_anzahl = 0;                                                               //merker für die anzahl der Zeilen im SPI_RAM

  USER_RAM_fill(0x0, 0x20000, 0);                                                  //Bearbeitungsspeicher löschen sonst gibt's fehler

  if (*txtpos != NL) {                                                             //Parameter für Startnummer und Schrittweite
    startnum = int(get_value());
    if (*txtpos == ',') {
      txtpos++;
      schritt = int(get_value());
    }
    else schritt = 10;
  }
  else {
    startnum = 10;
    schritt = 10;
  }

  if (txtpos[0] != NL)                                                             //Renum darf nur im Kommandomodus benutzt werden
  {
    syntaxerror(syntaxmsg);
    return;
  }


  //********************************** erster Durchlauf - Zeilennummern lesen und in Ram schreiben + neue Zeilennummer  *************************************************
  inhibitOutput = true;                                                           //Bildschirm-Ausgabe unterdrücken

  list_line = findline();                                                         // Finde Zeile
  while (list_line != program_end) {

    num = printline();                                                            //Zeilennummer alt ermitteln
    p_data[0] = highByte(num);
    p_data[1] = lowByte(num);
    USER_RAM_write(adr, p_data, 2);                                                //FRAM Word

    p_data[0] = highByte(startnum);
    p_data[1] = lowByte(startnum);
    USER_RAM_write(adr + 2, p_data, 2);                                    //Zeilennumer +
    adr += 4;
    startnum += schritt;                                                  //Schrittweite addieren
    zeilen_anzahl++;
  }

  inhibitOutput = false;                                                  //Bildschirm-Ausgabe zulassen
  line_terminator();

  renum2();                                                               //jede Zeilen lesen, nach ON Gosub On Goto durchsuchen und Zeilennummern ändern

  //*********************************************** Jetzt nur noch zurück in den Speicher *****************************************************************
  load_ram();
  line_terminator();
  warmstart();
  return;
}
//************************************* zweiter Durchlauf - jede Zeile lesen und nach ON Gosub On Goto durchsuchen und die Zeilennummern neu schreiben ****************

void renum2() {
  int i, str_ln;
  int num_neu, num, num_alt, tmp;
  unsigned int adr = 0x0;
  unsigned int startnum = 10;                                                      //Startzeilennummer
  unsigned int schritt = 10;                                                       //Schrittweite
  String neuzeile;

  fram_ptr = 0;                                                                   //Adress-Pointer für das zurückschreiben der geänderten Zeilen
  linenum = testnum();                                                            // überprüfe Zeilennummer und gibt 0 zurück, wenn keine Zeilennummer angegeben wird
  list_line = findline();                                                         // Finde Zeile

  while (list_line != program_end) {
    num = read_line();
    adr = 0;
    for (int i = 0; i < zeilen_anzahl; i++) {
      num_alt = user_fram_read8(adr) << 8;
      num_alt = num_alt + user_fram_read8(adr + 1);
      if (num_alt == num)
      {
        num_neu = user_fram_read8(adr + 2) << 8;
        num_neu = num_neu + user_fram_read8(adr + 3);
        break;
      }

      adr += 4;
    }

    // ***************************************************************** Schritt 3 - suche Goto und Gosub ***********************************************
    String content = tempstring;
    int onPos = content.indexOf("ON ");
    int gotoPos = content.indexOf(" GOTO ");
    if (gotoPos == -1) gotoPos = content.indexOf(" GOSUB ");
    if (onPos != -1 && gotoPos > onPos) {
      // Prüfen, ob an der Stelle GOSUB oder GOTO steht
      int keyLen = 6; // Standard für " GOTO "
      if (content.indexOf(" GOSUB ", gotoPos) != -1) {
        keyLen = 7; // Falls GOSUB gefunden wurde
      }

      // Alles vor der Nummernliste (z.B. "ON X GOTO ")
      String prefix = content.substring(0, gotoPos + keyLen);

      // Die Liste der Zielnummern ab dem korrekten Index
      String list = content.substring(gotoPos + keyLen);

      content = prefix + processOnList(list);
      neuzeile = String(num_neu) + " " + content;
    }
    else {
      String keys[] = {"GOTO ", "GOSUB ", "RESTORE "};
      for (String k : keys) {
        int p = content.indexOf(k);
        if (p != -1) {
          int startOfNum = p + k.length();

          // 1. Ende der Zahl finden (scannt bis zum ersten Nicht-Ziffer-Zeichen)
          int endOfNum = startOfNum;
          while (endOfNum < content.length() && isDigit(content[endOfNum])) {
            endOfNum++;
          }

          if (startOfNum < endOfNum) {
            // 2. Die alte Nummer extrahieren
            int oldT = content.substring(startOfNum, endOfNum).toInt();

            // 3. String neu zusammenbauen:[Anfang bis GOTO ] + [Neue Nummer] + [Rest ab endOfNum (z.B. : PRINT)]
            content = content.substring(0, startOfNum) + String(getNewNum(oldT)) + content.substring(endOfNum);
          }
        }
      }
      neuzeile = String(num_neu) + " " + content;

    }
    write_zeile_fram(neuzeile);           //neue geänderte Zeile mit neuer Zeilennummer in den Ram schreiben
  }


  uint8_t header[4];
  header[0] = 'B';
  header[1] = 'S';
  header[2] = (fram_ptr >> 8) & 0xFF;
  header[3] = fram_ptr & 0xFF;
  USER_RAM_write(load_adress, header, 4);

}
//*********************************************** schreibe die geänderten Zeilen in PSRam **************************************************************

void write_zeile_fram(String zeile) {
  // 1. Zeilennummer am Anfang des Strings finden und umwandeln
  zeile += "\n";
  int len = zeile.length();
  char* endPtr;
  uint16_t line_num = (uint16_t)strtol(zeile.c_str(), &endPtr, 10);

  // 2. Den Rest der Zeile (nach der Nummer) finden
  // überspringe die Nummer und eventuelle Leerzeichen danach
  while (*endPtr == ' ') endPtr++;
  String content = String(endPtr);

  // 3. Länge berechnen: 2 Bytes (Nr) + 1 Byte (Längen-Byte) + Textlänge
  uint8_t payload_len = content.length();
  uint8_t total_line_len = payload_len + 3;

  // 4. Temporären Puffer für den Header erstellen
  uint8_t header[3];
  header[0] = line_num & 0xFF;         // Low Byte der Zeilennummer
  header[1] = (line_num >> 8) & 0xFF;  // High Byte der Zeilennummer
  header[2] = total_line_len;          // Gesamtgröße dieser Zeile im FRAM

  // 5. In den FRAM schreiben - Erst den 3-Byte Header
  USER_RAM_write(renum_addr + fram_ptr, header, 3);
  fram_ptr += 3;

  // Dann den restlichen Text (ohne die Zeilennummer)
  if (payload_len > 0) {
    USER_RAM_write(renum_addr + fram_ptr, (const uint8_t*)content.c_str(), payload_len);
    fram_ptr += payload_len;
  }

  // 6. Optional: Ein Null-Byte als "Ende-Markierung" für den gesamten Programmspeicher
  uint8_t eof = 0;
  USER_RAM_write(renum_addr + fram_ptr, &eof, 1);
  // (wird beim nächsten Aufruf an dieser Stelle einfach überschrieben)
}

int read_line(void)
{ int digits = 0;
  int num;
  int i = 0;
  LINENUM line_num;

  line_num = *((LINENUM *)(list_line));
  list_line += sizeof(LINENUM) + sizeof(char);

  num = line_num;

  while (*list_line != NL)
  {
    tempstring[i++] = *list_line;
    list_line++;

  }
  tempstring[i] = '\0';
  list_line++;

  return line_num;
}

String processOnList(String list) {
  String result = "";
  String restCode = "";

  // 1. Doppelpunkt abfangen
  int colonPos = list.indexOf(':');
  if (colonPos != -1) {
    restCode = list.substring(colonPos);
    list = list.substring(0, colonPos);
  }

  int start = 0;
  int commaPos = list.indexOf(',');

  // 2. Liste verarbeiten
  while (commaPos != -1) {
    String part = list.substring(start, commaPos);
    part.trim();
    if (part.length() > 0) {
      int n = getNewNum(part.toInt());
      result += (n != -1 ? String(n) : "???" + part) + ", ";    // Wenn n == -1, schreibe ??? und die alte Nummer
    }
    start = commaPos + 1;
    commaPos = list.indexOf(',', start);
  }

  // 3. Letzte Nummer verarbeiten
  String lastPart = list.substring(start);
  lastPart.trim();
  if (lastPart.length() > 0) {
    int n = getNewNum(lastPart.toInt());
    result += (n != -1 ? String(n) : "???" + lastPart);
  }

  return result + restCode;
}


int getNewNum(int oldTarget) {
  int num_alt;
  unsigned int adr = 0x0;

  for (int i = 0; i < zeilen_anzahl; i++) { // zeilen_anzahl reicht meist aus
    num_alt = (user_fram_read8(adr) << 8) | user_fram_read8(adr + 1);

    if (num_alt == oldTarget) {
      int num_neu = (user_fram_read8(adr + 2) << 8) | user_fram_read8(adr + 3);
      return num_neu; // Sofort beenden, wenn gefunden
    }
    adr += 4;
  }

  return -1; // Rückgabe, wenn die Nummer NICHT im RAM existiert
}

void zeige_variablen() {
  Terminal.println("--- Variables & Strings ---");
  int gefundene = 0;
  int zeilen_zaehler = 1;
  int max_zeilen = 22;
  bool ist_rechte_spalte = false;

  // ==========================================
  // PART 1: EIN- UND ZWEI-BUCHSTABIGE FLOAT-VARIABLEN
  // ==========================================
  for (int i = 0; i < 702; i++) {
    float wert = ((float *)variables_begin)[i];

    if (wert != 0.0f) {
      String name = "";
      if (i < 26) {
        name += (char)('A' + i);
      } else {
        int erster_index = i % 26;
        int zweiter_index = (i / 26) - 1;
        name += (char)('A' + erster_index);
        name += (char)('A' + zweiter_index);
      }

      String wert_text = String(wert, 4);
      while (wert_text.indexOf('.') != -1 && (wert_text.endsWith("0") || wert_text.endsWith("."))) {
        wert_text.remove(wert_text.length() - 1);
      }

      String block = "  " + name;
      if (name.length() == 1) block += "   ";
      else                     block += "  ";
      block += "= " + wert_text;

      // Scroll-Pause
      if (!ist_rechte_spalte && zeilen_zaehler >= max_zeilen) {
        if (wait_key(true) == 3) return;                          //Abbruch mit ESC
        Terminal.print("\r                               \r");
        zeilen_zaehler = 0;
      }

      if (!ist_rechte_spalte) {
        Terminal.print(block);
        int rest_leerzeichen = 20 - block.length();
        for (int l = 0; l < rest_leerzeichen; l++) Terminal.print(" ");
        ist_rechte_spalte = true;
      } else {
        Terminal.println(block);
        ist_rechte_spalte = false;
        zeilen_zaehler++;
      }
      gefundene++;
    }
  }

  // ==========================================
  // PART 2: STRINGS AUS DER STRINGTABLE (A$ - Z$)
  // ==========================================
  // STR_LEN sollte der Puffergröße im Interpreter entsprechen (z.B. 40)
  const int STR_LEN_VAL = STR_LEN;

  for (int i = 0; i < 26; i++) {
    // Berechne die Startadresse des jeweiligen Strings im flachen Array
    int offset = i * STR_LEN_VAL;

    // Prüfen, ob der String Inhalt hat (erstes Byte ist nicht 0)
    if (Stringtable[offset] != '\0') {

      // String-Namen erzeugen (z.B. A$)
      String name = String((char)('A' + i)) + "$";

      // Den Text sicher aus dem Array extrahieren (stoppt automatisch bei \0)
      String string_inhalt = String(&Stringtable[offset]);

      // Block-Formatierung (z.B. "  A$  = Text")
      String block = "  " + name + "  = " + string_inhalt;

      // Scroll-Pause
      if (!ist_rechte_spalte && zeilen_zaehler >= max_zeilen) {
        if (wait_key(true) == 3) return;                          //Abbruch mit ESC
        Terminal.print("\r                               \r");
        zeilen_zaehler = 0;
      }

      if (!ist_rechte_spalte) {
        Terminal.print(block);
        int rest_leerzeichen = 20 - block.length();
        // Falls der String-Inhalt sehr lang ist, erzwingen wir mindestens ein Trenn-Leerzeichen
        if (rest_leerzeichen < 1) rest_leerzeichen = 1;

        for (int l = 0; l < rest_leerzeichen; l++) Terminal.print(" ");
        ist_rechte_spalte = true;
      } else {
        Terminal.println(block);
        ist_rechte_spalte = false;
        zeilen_zaehler++;
      }
      gefundene++;
    }
  }

  // Zeilenabschluss, falls am Ende ein Eintrag links hängen geblieben ist
  if (ist_rechte_spalte) {
    Terminal.println();
  }

  if (gefundene == 0) {
    Terminal.println("No Variables or Strings.");
  }
}

//#########################################################################################################################################################################
//########################################################################## Testbereich - neue Funktionen ################################################################
