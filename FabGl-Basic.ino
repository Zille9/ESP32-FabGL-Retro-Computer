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
//      SD-Card 14, 16, 17, 13 (SCK, MISO, MOSI, CS)    ESP32-Eigenboard                                                                          //
//      SD-Card 14, 2, 12, 13 (SCK, MISO, MOSI, CS)     TTGO                                                                                      //
//      FRAM-Board 14, 16, 17, 0 (SCK, MISO, MOSI, CS)  512kB FRAM am SD-SPI-BUS                                                                  //
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
// Die Version meiner Vor-Authoren wird die Grundlage für einen erweiterten Basic-Interpreter
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
//
// Author:Reinhard Zielinski <zille09@gmail.com>
// April 2021
//
//
#define BasicVersion "1.68b"
#define BuiltTime "Built:09.06.2023"
#pragma GCC optimize ("O2")
// siehe Logbuch.txt zum Entwicklungsverlauf
// v1.68b:09.06.2023          -Fehler in Print-Routine behoben, bei Printausgaben, welche in der gleichen Zeile durch : getrennte Befehle enthielt, wurde das Semikolon wirkungslos
//                            -jetzt werden alle Ausgaben nach einem Semikolon in der gleichen Zeile ausgeführt, erst ein NL (new Line) oder ein Print ohne ; führt zu einer neuen Zeile
//                            -am einfachsten ist ein Print einzufügen, um die Ausgabe in einer neuen Zeile beginnen zu lassen.
//                            -Unterprogramm get_value() geändert ->Fehlerausgabe entfernt, das führte zum Bsp. bei Input zum Programmabbruch, das ist ungewollt
//                            -ausserdem wurde damit das Problem der doppelten Fehlerausgabe behoben
//                            -Aussehen des Cursors geändert in Underline (m_emuState.cursorStyle= 3; in terminal.cpp)
//                            -Fehler in den Auswertungen von AND u. OR entdeckt, es wird nur eine Bedingung ausgewertet
//                            -18105 Zeilen/sek.
//
// v1.67b:01.06.2023          -Funktion GPIC(n) eingefügt PRINT GPIC(0)->Breite GPIC(1)->Höhe der letzten geladenen BMP-Datei
//                            -Versuch einen SD-Loader zu integrieren, CP/M kann geladen werden und funktioniert, nur komm ich nicht mehr zum Basic zurück :-(
//                            -in dieser Beziehung ist die Arduinowelt doof, alles muss geflasht werden :-(
//                            -mal sehen, ob ich eine Lösung finde, wäre cool, andere Programme zu laden und wieder zum Basic zurückzukehren.
//                            -18033 Zeilen/sek.
//
// v1.66b:25.05.2023          -Skalierung funktioniert jetzt besser
//                            -Routine verbessert, jetzt dauern auch sehr grosse Bilder nicht mehr ewig
//                            -krumme Formate führen allerdings zu Verzerrungen, Bilder sollten im Verhältnis dem Bildschirmformat entsprechen
//                            -Fehler in der Darstellungsroutine behoben, war das Bild kleiner als die Bildschirmauflösung, wurde das Bild nicht dargestellt
//                            -Bildschirmauflösung wird jetzt berücksichtigt damit ist die Darstellung auch bei 400x300 korrekt
//                            -18200 Zeilen/sek.
//
// v1.65b:23.05.2023          -Export des Bildschirminhaltes (oder eines Ausschnittes) im BMP-Format funktioniert
//                            -Import BMP-Datei funktioniert ebenfalls, Laden eines Bildes dauert ca.6.5sek.
//                            -Skalierung von größeren Bildern (>320x240) nicht perfekt aber funktionstüchtig
//                            -18099 Zeilen/sek.
//
// v1.64b:19.05.2023          -Laden (PIC_L(Adr,Filename)) und Speichern (PIC_S(Adr,Filename)) funktioniert
//                            -Datei-Sicherheitsabfragen für PIC_L und PIC_S hinzugefügt, Puffer auf 1024 Bytes erhöht ->Laden und Speichern erfolgt schneller
//                            -nächster Schritt: Daten von und ins BMP-Format wandeln und speichern/laden
//                            -18102 Zeilen/sek.
//
// v1.63b:12.05.2023          -erneut begonnen den PIC-Befehl zu integrieren, für erste Tests kann mit PIC_P(Adr) ein Grafikbildschirm
//                            -im FRAM abgelegt werden mit PIC_D(Adr) kann der gesicherte Bildinhalt wieder auf dem Bildschirm dargestellt werden
//                            -PIC_P(Adr<,x,y,xx,yy>) speichert den Bildausschnitt x,y,xx,yy PIC_P(Adr) speichert den gesamten Bildschirm
//                            -PIC_D(Adr,<x,y><,mode>) lädt die Bilddaten an Position x,y,mode=1 ->Hintergrund erhält die aktuelle Hintergrundfarbe
//                            -nächster Schritt: Laden und Speichern der Bilddaten auf SD-Karte
//                            -Unterprogramm FilenameWord eingespart - Umstellung der Dateioperationen auf die Stringfunktionen
//                            -damit ist die Übergabe des Dateinamens als Stringvariable möglich
//                            -17727 Zeilen/sek.
//
// v1.62b:07.05.2023          -COM-Befehl modifiziert ->COM_S(RX,TX,Baud) bzw. COM_S(0), COM_P(Zeichenkette,...), COM_W(Zeichenkette,...),
//                            -COM_T -> sendet das Programm im Speicher an die ser. Schnittstelle
//                            -Theme jetzt mit OPT THEME=x speicherbar, werden Farben oder Font über OPT gespeichert, wird beim Start das Theme ignoriert
//                            -Startbildschirm etwas angepasst ->Font_offset eingespart, wird jetzt berechnet
//                            -ein Farbbalken zeigt jetzt zusätzlich die Akku-Kapazität an ->mal sehen, ob das so bleibt
//                            -16575 Zeile/sek.
//
// v1.61b:04.05.2023          -begonnen serielle Funktionen zu integrieren, noch ist das Konzept nicht ganz klar
//                            -RX-Puffer der seriellen Schnittstelle auf 1024 Bytes erhöht, so können ganze Basic-Programme über die Com-Schnittstelle vom PC eingelesen werden,
//                            -ohne verlorene Zeichen (gibt ja keine Flusskontrolle)
//                            -bisherige Funktionen P=print W=write T=transfer (List)
//                            -COM P und COM W können jetzt verkettete Ausgaben (wie Print) durchführen (P mit, W ohne Zeilenumbruch)
//                            -17847 Zeilen/sek.
//
// v1.60b:29.04.2023          -DURCHBRUCH: es ist gelungen, den SPI-Port mit SD-Karte "und" FRAM zu betreiben ->FRAM_CS=Pin 0 (Sharing mit Flash-Taste);
//                            -diverse Aktivierungen und Deaktivierungen der einzelnen Treiber machte es möglich :D
//                            -somit können die vorherigen Einschränkungen zurückgenommen werden
//                            -mein BASIC-LAPTOP ist fertig :-) ->Befehl Akku erweitert Print Akku(0) zeigt die Spannung und Akku(1) die Akkukapazität in Prozent an
//                            -Akku-Interrupt-Routine ist jetzt aktiv und zeigt bei leerem Akku eine Warnung auf dem Bildschirm an
//                            -Funktion GPIX(x,y) zum ermitteln des Farbwertes eine Pixel an Position x,y hinzugefügt.
//                            -etwas schneller geworden 17379 Zeilen/sek. Mandel4.bas 14.71 Min
//
// v1.59b:29.04.2023          -bisher endgültige FRAM-PIN'S-> FRAM_CS  = 13, FRAM_MISO= 27 , FRAM_MOSI= 12 und FRAM_CLK = 0
//                            -damit wird Pin26 wieder frei für DAC,Video-Out
//                            -Port-Befehle auf die wenigen übrigen Pins reduziert (AREAD,DWRITE,PWM,PULSE)
//                            -17019 Zeilen/sek. Mandel4.bas 14.32 Min
//
// v1.58b:28.04.2023          -SPI_FRAM-Board von Adafruit eingetroffen, leider funktioniert das Board nur mit eigenen SPI-Pin's
//                            -an den Pin's für die SD-Karte (wie geplant) und eigenen CS-Pin funktioniert der Ram nicht
//                            -ärgerlich, das SPI nicht so funktioniert wie er sollte
//                            -mit eigenen Pin's (FRAM_CS  = 27, FRAM_MISO= 26 , FRAM_MOSI= 2 und FRAM_CLK = 12 funktioniert der Chip zwar aber verbraucht damit auch
//                            -alle IO-Pins für Ausgabe ,so ist z.Bsp.der LED-Strip nicht mehr nutzbar, nur I2C ist noch verwendbar
//                            -einzig 3 Analog Eingangs-Pins's sind noch verfügbar :-(
//                            -DOUT, PWM, DIN, PULSE, DAC ,TEMP, DHT usw.sind damit sinnlos, weil keine Pins mehr vorhanden sind.
//                            -nach mehrfachen Tests hat sich folgende Pin-Konfiguration als offensichtliches Optimum gezeigt
//                            -FRAM_CS=13 (SD-Card CS-Pin dauerhaft auf GND), FRAM_MISO=26 ,FRAM_MOSI=27, FRAM_CLK=0
//                            -so sind zumindest Pin 2 und 12 als IO frei für Anwendungen sowie 34, 35 und 36 als Analog-Eingänge, besser als nix
//                            -die FRAM-Geschwindigkeit ist mehr als 3mal so hoch gegenüber der I2C-Variante :D
//                            -16353 Zeilen/sek. ->Julia.bas 13.13Min, Mandel4.bas 14.62 Min
//
// v1.57b:25.04.2023          -Startbildschirm etwas farbig aufgepeppt, Anzeige der Bildschirmauflösung entfernt ->es gibt ja nur noch 320x240
//                            -Geschwindigkeit wieder eingebrochen ->15240 Zeilen/sek.!?
//                            -FPOKE und FPEEK hinzugefügt, ermöglicht das schreiben und lesen von float-Werten im Ram, FRam oder EEProm
//                            -FPOKE Ort,Adresse,Wert , A=FPEEK(Ort,Adresse) -> Ort 0=RAM, 1=FRAM, 2=EEPROM
//                            -Optimierungsoption jetzt im Programmkopf (siehe #pragma GCC optimize)
//                            -morgen kommt ein FRAM-Chip mit 512kb als SPI-Variante, mal sehen, ob der eingebunden werden kann, das wäre cool :-)
//                            -damit wäre eine umfangreiche Datenspeicherung usw.möglich, und das mit bis zu 20MHz (statt 400kHz mit I2C)
//                            -Pins sollen die gleichen, wie SD-Card sein, nur CS-Pin wird ein anderer ->mal sehen, obs klappt.
//                            -15165 Zeilen/sek.
//
// v1.56b:22.04.2023          -BEFEHL OPT geschaffen, damit können Pin-Konfigurationen, Farbeinstellungen usw. im Flash gespeichert werden
//                            -diese werden dann beim Start gelesen und entsprechend gesetzt (SD-CARD-Pins, I2C-Pins, Font, Vorder-und Hintergrundfarben)
//                            -THEME und FONT speichern jetzt nicht mehr dauerhaft, das macht OPT
//                            -Startbildschirm etwas geändert
//                            -OPT kann jetzt die SD-Karten- und die I2C-Konfiguration speichern und lesen ->muss noch getestet werden
//                            -18210 Zeilen/sek.
//
// v1.55b:20.04.2023          -Fehler der Print-Ausgabe gefunden, nicht der String-Marker sondern der Char-Marker wurde nicht rechtzeitig zurückgesetzt
//                            -dies wurde korrigiert, jetzt stimmt wieder alles (expr4()) :-)
//                            -Befehl Pic entfernt, stattdessen Befehl DRAW x,y,mode kreiert DRAW x,y,0 springt zur Position x,y
//                            -DRAW x,y,1 zeichnet eine Linie von der letzten Position (DRAW x,y,0) nach x,y, damit sind Vielecke o.ä. Strukturen zeichenbar
//                            -nach dem Turtle-Zeichnungs-Prinzip (von pos nach pos -> nach pos -> nach pos)
//                            -MODE-Befehl deaktiviert, ob man das braucht? 320x240 Pixel in 64 Farben ist für einen Retro-Computer ausreichend
//                            -außerdem ist der Basic-Interpreter besser für LCD-Displays portierbar
//                            -String-Fehldarstellungs-Ursache gefunden ->tempstring wurde nicht korrekt abgeschlossen (Nullterminator zu spät gesetzt)
//                            -STR$-Funktion umgebaut, ->STR$(12.34,n) wandelt einen numerischen Wert in einen String mit n - Nachkommastellen um
//                            -1.Array-Dimension auf Word-Größe erweitert, jetzt sind Arrays in der ersten Dimension über 255 möglich (DIM A(1000))
//                            -18255 Zeilen/sek. ->Font 25 Julia.bas 12.6Min, Mandel4.bas 13.9Min (Debug_lvl=Fehler)
//
// v1.54b:18.04.2023          -DOKE Befehl mit writeBuffer realisiert, ist schneller als 2 x writeEEPROM - 32kb dauern nur noch 5 statt 15 sek.
//                            -NEW-Befehl geändert, nur return führte dazu, dass neue Befehle/Programme den ESP aufhingen!?
//                            -etwas Code-Optimierung -> clear_var() und cmd_new() kombiniert - spart wieder einige Code-Zeilen
//                            -Core Debug-Level auf Info gesetzt ->18228 Zeilen/sek. ->Font 25, zur Zeit die beste Einstellung
//                            -MNT-Befehl funktioniert nicht, wie gewünscht, aus irgend welchen Gründen verliert der ESP die SD-Karte?
//                            -eine neue SD-Initialisierung funktioniert nicht, was ist das nun wieder?
//                            -sollte das der Editor sein?,nach dem Editieren von Code passiert diese Problematik am häufigsten, nur Reset hilft dann
//                            -überprüfe nochmal den Editor-Code
//                            -Fehler lag in sd_pfad-Variable ->in static char umgewandelt, jetzt scheint es richtig zu funktionieren
//                            -Addition von Strings und CHR$ erweitert -> jetzt verkettet möglich
//                            -allerdings wird für Print der String_marker nicht rechtzeitig zurückgesetzt, bis zur Lösung muss mit Print ein numerischer Wert ausgegeben werden
//                            -dann ist die Darstellung wieder korrekt -> muss noch die richtige Stelle zum zurücksetzen von string_marker finden
//                            -Mandel4.bas ->13.96 Min
//                            -17838 Zeilen/sek.
//
// v1.53b:16.04.2023          -String- und numerische Arrays bis zu 3 Dimensionen scheint zu funktionieren, ein großer Schritt für mich, bedeutungslos für die Menschheit :-)
//                            -weitere Test's werden zeigen, ob das wirklich so ist
//                            -die Verarbeitungsgeschwindigkeit ist natürlich erheblich langsamer, da die Arrays im FRAM gespeichert bzw. gelesen werden
//                            -Code muss noch optimert werden aber ein großer Schritt ist getan :-D !!!
//                            -CLEAR funtionierte nicht im Programm ,durch warmstart() und nachfolgendes continue in der Hauptschleife wurde das Programm unterbrochen
//                            -warmstart in clear_var() entfernt, continue in der Hauptschleife durch break ersetzt, jetzt funtioniert CLEAR auch im Programm korrekt!
//                            -write_array_value und read_array zu rw_array zusammengefasst, die beiden Programmteile waren fast identisch
//                            -VAR_TBL und STR_TBL könnten noch als RAM-Array realisiert werden, sollte etwas schneller sein
//                            -nach etwas Codeoptimierung ist die Geschwindigkeit von 14560 auf 17064 Zeilen/sek. gestiegen :-)
//                            -diverse Bit-shift Operationen durch highByte und lowByte ersetzt, das ist offensichtlich schneller
//                            -Ausführungszeit Julia.bas ->12.69 Min. Mandel4.bas ->14.29 Min.
//                            -aktuell 17418 Zeilen/sek. ->Font25 ->Core Debug Level=Debug
//
// v1.52b:12.04.2023          -etwas Code-Optimierung betrieben
//                            -Unterfunktion Test_char(char) zur Überprüfung auf erforderliche Zeichen erschaffen, dadurch etliche Zeilen, sich ständig wiederholenden
//                            -Abfrage-Codes eingespart (ca.100 Zeilen)
//                            -Befehl STOP entfernt, ohne Continue macht Stop keinen Sinn-> End macht das Gleiche
//                            -Unterprogramme Circ,Rect und Lines auf ein Unterprogramm reduziert (line_rec_circ(art,parameterzahl)), dadurch wieder ca.65 Zeilen Code
//                            -eingespart
//                            -Fehler im Sprite(D... Befehl behoben, der Funktionsstring musste mit trim() gekürzt werden sonst Fehlausgabe
//                            -Soundbefehl gefällt mir nicht, vielleicht wirds doch ne externe Soundkarte (Propeller mit SID-Sound!?)
//                            -18306 Zeilen/sek.
//
// v1.51b:10.04.2023          -Stringlänge auf 30 Zeichen gekürzt (Vorbereitung für Arrays)
//                            -dabei ist aufgefallen,das es keine Sicherheitsfunktion für das Schreiben zu langer Strings gegeben hat
//                            -bei sehr langen Strings wurde der Nachbarstring überschrieben, dies wurde behoben
//                            -Addition von Strings geändert, ist noch nicht perfekt ->am Ende einer Addition taucht ein '/' auf!?
//                            -die Übernahme des Gesamtstring nach Tempstring ist noch buggy
//                            -MNT Befehl zum Mounten der SD-Karte hinzugefügt, wenn die Karte entnommen wurde, konnte nicht mehr gelesen werden
//                            -Array-Dimensionierung begonnen - numerische und Stringvariablen
//                            -ist noch ein ganzes Stück Arbeit aber die Dimensionierung scheint zu funktionieren
//                            -es werden bis zu 3 Dimensionen möglich sein, warscheinlich geht der ganze FRAM dabei drauf
//                            -Verarbeitungsgeschwindigkeit ist eingebrochen ->
//                            -18225 Zeilen/sek. , es bleibt seltsam!
//
// v1.50b:07.04.2023          -PEEK und POKE wieder in ursprüngliche Form gebracht POKE Ort,Adresse,Wert
//                            -A=PEEK(Ort,Adresse) für Word Werte wurde DEEK und DOKE eingefügt ->DOKE Ort,Adresse,Wert, A=DEEK(Ort,Adresse)
//                            -das ist übersichtlicher - Long Werte können mit 2 Word Werten zusammengesetzt werden - siehe POKE.BAS
//                            -so konnte wieder auf float zurückgegangen werden - (Geschwindigkeit höher! siehe v1.49)
//                            -Beginn der Testphase für den Sound Befehl
//                            -Error-Sound hinzugefügt ->ertönt bei Fehlern
//                            -neuer Befehl BEEP(Note,Länge) geschaffen als mini-Soundmodul
//                            -20154 Zeilen/sek. Font25
//
// v1.49b:04.04.2023          -PEEK und POKE erweitert POKE Ort,Adresse,Wert<,2Word o. 4Long> ansonsten Byte
//                            -A=PEEK(Ort,Adresse<,2Word o.4Long> ansonsten Byte, dafür musste wieder auf die Verarbeitung der Zahlen im float-Format
//                            -umgestellt werden, da sonst der Wertebereich von float für die Darstellung von unsigned Long nicht ausreichte
//                            -kleiner Fehler in list_out-Routine behoben, es konnte vorkommen, das die letzte Zeilennumer nicht korrekt angezeigt wurde
//                            -Funktion SQR erweitert ->SQR(x <,n> )= n'te Wurzel aus x - n ist optional
//                            -17058 Zeilen/sek. Font1
//
// v1.48b:03.04.2023          -DMP-Befehl erweitert, jetzt sind alle drei verfügbaren Speichermedien anzeigbar
//                            -DMP 0<,Adresse> = interner RAM
//                            -DMP 1<,Adresse> = FRAM-Chip
//                            -DMP 2<,Adresse> = EEPROM-Chip
//                            -dabei ist die Angabe der Adresse optional, ohne Angabe der Adresse wird bei Adresse 0 begonnen
//                            -PEEK und POKE ebenfalls angepasst ->A=PEEK(0..2,ADRESSE) POKE(0..2,ADRESSE,VALUE)
//                            -Funktionstasten für DIR und LIST (CTRL+D, CTRL+L) hinzugefügt.
//                            -Befehl NEW mit memset-Funktion ergänzt, jetzt wird der gesamte Speicher gelöscht inklusive der Variablen, vorher waren die Variablen noch vorhanden
//                            -und im Speicher befanden sich Fragmente des alten Programms
//                            -20568 Zeilen/sek. Font25
//
// v1.47b:02.04.2023          -Input-Eingaben jetzt mit mehreren Variablen (auch gemischt) möglich, es erfolgt aber keine Typprüfung
//                            -d.h. das man auf die richtige (Zahl oder Zeichenkette) Variablentype bei der Eingabe achtet
//                            -das bedeutet, das eine Zahl auch als String akzeptiert wird, da keine Anführungszeichen abgefragt werden
//                            -mittels Lineeditor-Funktion von FabGl endlich einen funktionierenden Zeileneditor geschaffen
//                            -wird eine Zeile eingegeben, die nicht existiert, wird die nächst verfügbare Zeile in den Bearbeitungsspeicher geladen
//                            -und mit der falsch eingegebenen Zeilennummer gespeichert
//                            -ob das als Bug oder als Feature angesehen wird, muss sich noch zeigen - so könnte man Zeilen kopieren!?
//                            -19611 Zeilen/sek. Font25
//
//
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Feature option configuration...
//
//
//---------------------------------------------------------------- Verwendung der SD-Karte ---------------------------------------------------------
// Dies aktiviert die Befehle LOAD, SAVE, DIR über die Arduino SD Library
#define ENABLE_FILEIO 1
//---------------------------------------------------------------- Auswahl Bildschirmtreiber -------------------------------------------------------
//#define AVOUT                  //activate for AV
//#define VGA16                  //activate VGA 16 Color 320x240 Pixel Driver
#define VGA64                    //activate VGA 64 Color 320x240 Pixel Driver
#include "fabgl.h" //********************************************* Bibliotheken zur VGA-Signalerzeugung *********************************************
fabgl::Terminal         Terminal;
fabgl::LineEditor       LineEditor(&Terminal);
//---------------------------------------- die verschiedenen Grafiktreiber --------------------------------------------------------------------------
#ifdef AVOUT
fabgl::CVBS16Controller VGAController;    //AV-Variante
#define VIDEOOUT_GPIO GPIO_NUM_26         //Ausgabe auf GPIO25 oder 26 möglich ACHTUNG!:Soundausgabe erfolgt auf GPIO25
static const char * MODES_STD[]   = { "I-PAL-B", "P-PAL-B", "I-NTSC-M", "P-NTSC-M", "I-PAL-B-WIDE", "P-PAL-B-WIDE", "I-NTSC-M-WIDE", "P-NTSC-M-WIDE", "P-NTSC-M-EXT",};
#endif

#ifdef VGA16
fabgl::VGA16Controller  VGAController;      //VGA-Variante
#endif

#ifdef VGA64
fabgl::VGAController    VGAController;      //VGA-Variante
#endif

//------------------------------------------ Tastatur,GFX-Treiber- und Terminaltreiber -------------------------------------------------------------
fabgl::PS2Controller    PS2Controller;
fabgl::Canvas           GFX(&VGAController);
TerminalController      tc(&Terminal);

//---------------------------------------------------- verfügbare Themes ---------------------------------------------------------------------------
char * Themes[]    PROGMEM = {"C64", "C128", "CPC", "ATARI 800", "ZX-Spectrum", "KC87", "KC85", "VIC-20", "TRS-80", "ESP32+", "LCD", "User"}; //Theme-Namen
byte x_char[]      PROGMEM = {8, 5, 6, 8,  10, 8,  8,  8,  8,  8,  8,  8,  8,  6,  8,  4, 6,  7,  7,  8, 8, 8, 6, 9, 8, 6}; //x-werte der Fontsätze zur Berechnung der Terminalbreite
byte y_char[]      PROGMEM = {8, 8, 8, 14, 20, 14, 14, 16, 16, 14, 14, 14, 16, 10, 14, 6, 12, 13, 14, 9, 14, 14, 10, 15, 16, 8}; //y-werte der Fontsätze zur Berechnung der Terminalhöhe
//byte font_offset[] PROGMEM = { 0, 12, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 20, 6, 2, 2, 0, 0, 0, 6, 0, 0, 6};            //Fontoffset zur Berechnung der Überschrifts-Position

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//------------------------------------------------------------- Soundgenerator ----------------------------------------------------------------------------
unsigned int noteTable []  PROGMEM = {16350, 17320, 18350, 19450, 20600, 21830, 23120, 24500, 25960, 27500, 29140, 30870}; //Notentabelle für Soundausgabe
//------------------------------------------------------------- Soundgenerator ----------------------------------------------------------------------------

#define RAMEND 60928//----------------------------------- RAM increment for ESP32 ------------------------------------------------------------------ 

// --------- SD-Karten-Zugriff------------------------------
#include "FS.h"
#include <SD.h>
#include <SPI.h>
#include <Update.h>

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
//-------------- Konfiguration FRAM -----------------------

#include "Adafruit_FRAM_SPI.h"
byte FRAM_CS  = 0;//13;       //SPI_FRAM 512kB CS-Pin
word FRAM_OFFSET = 0x8000;    //Offset für Poke-Anweisungen, um zu verhindern, das in den Array-Bereich gepoked wird
word FRAM_PIC_OFFSET = 0x12C04; //Platz pro Bildschirm im Speicher

Adafruit_FRAM_SPI spi_fram = Adafruit_FRAM_SPI(kSD_CLK, kSD_MISO, kSD_MOSI, FRAM_CS);


//-------------- Konfiguration serielle Schnittstelle -----
uint8_t prx, ptx;             //RX- und TX-Pin
uint32_t pbd;                 //Baudrate
bool ser_marker = false;      //seriell-Marker, wenn gesetzt erfolgt jede Printausgabe auch auf die serielle Schnittstelle
bool list_send = false;
bool serout_marker = false;
#define SERIAL_SIZE_RX 1024

// --------- Dallas Temp-Sensor ----------------------------
#include <OneWire.h>
#include <DallasTemperature.h>
bool twire = false;

//---------- DHT-Sensor ------------------------------------
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
uint32_t delayMS;

//---------- BMP180-Sensor----------------------------------
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085.h>
// Store an instance of the BMP180 sensor.
Adafruit_BMP085 bmp;

// Store the current sea level pressure at your location in Pascals.
float seaLevelPressure = SENSORS_PRESSURE_SEALEVELHPA;

// --------- EEPROM Routinen für Parameter-Speicherung -----
#include <EEPROM.h>
#define EEPROM_SIZE 512  //2048 byte lesen/speichern

// --------- W2812-seriell LED-Treiber --------------------
#include <Adafruit_NeoPixel.h>
unsigned int LED_COUNT      = 255;
unsigned int LED_PIN        = 2;
unsigned int LED_BRIGHTNESS = 50;
unsigned int LED_TYP        = 2;
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);                //WS2812

//-------------- MCP23017 IO-Expander -----------------
#include <Adafruit_MCP23X17.h>   //Adafruit MCP23xx Treiber
Adafruit_MCP23X17 mcp;
short int MCP23017_ADDR = 0x20 ; //Adresse 32 (0x20) für eingebauten MCP23017
bool mcp_start_marker = false;


//------------- EEPROM o.FRAM-Chip --------------------
short int EEprom_ADDR = 0x50;
short int FRam_ADDR = 0x57;

File fp;

#include <Wire.h>           // for I2C 
#include "RTClib.h"         //to show time
TwoWire myI2C = TwoWire(0); //eigenen I2C-Bus erstellen
RTC_DS3231 rtc;

//diese Konstellation funktioniert mit ESP-Eigenboard und RTC-Modul oder 27 und 26 für die Freigabe des Seriellports
byte IIC_SET = 55;      // -steht 55 im EEprom-Platz 13, dann sind die Werte im EEprom gültig
// dies ist die Standard-Konfiguration
byte SDA_RTC = 3;
byte SCL_RTC = 1;

byte Keyboard_lang = 3;
byte KEY_SET = 66;      //-steht 66 im EEprom Platz 15, dann Nummer des Keyboard-Layouts aus dem EEProm laden
byte THEME_SET = 77;    //-steht 77 im EEPROM Platz 17, dann setze das gespeicherte Theme
//-------------- LCD-Treiber -----------------------------
#include "HD44780_LCD_PCF8574.h"
#define DISPLAY_DELAY_INIT 50 // mS
int LCD_SPALTEN, LCD_ZEILEN, LCD_ADRESSE, LCD_NACHKOMMA;
bool LCD_start_marker = false;
bool LCD_Backlight = true;

//---------- Akku-Überwachung ----------------------------
#define Batt_Pin 39
hw_timer_t *Akku_timer = NULL;      //Interrupt-Routine Akku-Überwachung

//---------- BMP-Info-Parameter für PIC-Befehl -------------------
uint32_t bmp_width, bmp_height;
/*
  //-------------------- Farbcodes für 16-Farbmodus ---------------------------
  #define  Black          0          //< Equivalent to RGB888(0,0,0)
  #define  Red            1          //< Equivalent to RGB888(128,0,0)
  #define  Green          2          //< Equivalent to RGB888(0,128,0)
  #define  Yellow         3          //< Equivalent to RGB888(128,128,0)
  #define  Blue           4          //< Equivalent to RGB888(0,0,128)
  #define  Magenta        5          //< Equivalent to RGB888(128,0,128)
  #define  Cyan           6          //< Equivalent to RGB888(0,128,128)
  #define  White          7          //< Equivalent to RGB888(128,128,128)
  #define  BrightBlack    8          //< Equivalent to RGB888(64,64,64)
  #define  BrightRed      9          //< Equivalent to RGB888(255,0,0)
  #define  BrightGreen    10         //< Equivalent to RGB888(0,255,0)
  #define  BrightYellow   11         //< Equivalent to RGB888(255,255,0)
  #define  BrightBlue     12         //< Equivalent to RGB888(0,0,255)
  #define  BrightMagenta  13         //< Equivalent to RGB888(255,0,255)
  #define  BrightCyan     14         //< Equivalent to RGB888(0,255,255)
  #define  BrightWhite    15         //< Equivalent to RGB888(255,255,255)
*/

// RAM Puffer-Größe für Programm und Benutzereingaben
// NOTE: Dieser Wert kann bei Verwendung anderer Librarys abweichen
#define kRamFileIO (1024)

#define kRamSize  (RAMEND - kRamFileIO)

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

#define STR_LEN 30
#define STR_SIZE 26*STR_LEN             //Stringspeicher = Stringlänge 26*30 Zeichen (A..Z * 30 Zeichen)


//------------------------------ hier wird der Funktionsstring gespeichert --------------------------------------------
#define FN_SIZE STR_LEN                 //Funktionsspeicher für benutzerdefinierte Funktionen mit bis zu vier Operatoren-> FN A(A,B,C,D)

//char * filenameWord(void);

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

//------------------------------ Array-Parameter ---------------------------------------
word Var_Neu_Platz =  0;                //Adresse nächstes Array-Feld Start bei 0x77e00
static word VAR_TBL = 0x7e00;           //Variablen-Array-Tabelle im FRAM
static word STR_TBL = 0x7f00;           //String-Array-Tabelle im FRAM
//--------------------------------------------------------------------------------------

// these will select, at runtime, where IO happens through for load/save
enum {
  kStreamTerminal = 0,
  kStreamFile,
  kStreamSerial
};
static char inStream = kStreamTerminal;
static char outStream = kStreamTerminal;

static char program[kRamSize];            //Basic-Programmspeicher
static char Stringtable[STR_SIZE];        //Stringvariablen mit 1 Buchstaben -> 26*30 = 780 Bytes

//------------------------------------- DEFN - FN ----------------------------------------------------------------------------------------------
static char Fntable[26][FN_SIZE];         // bytes 40 String = 40*26 ->1040 bytes ->Funktionsstring-Array
int Fnvar = 0;                            //Operatorenzähler
int Fnoperator[27 * 5];                   //DEFN A(a,b,c,d,e,f,g,h)-> Name 0-26,0-26=Operator1,0-26=operator2,0-26=Operator3,0-26=Operator4 + 1 Anzahl
bool fn_marker = false;

//------------------------------------ TRON ----------------------------------------------------------------------------------------------------
static byte tron_marker = 0;                           //TRON Aus
//------------------------------------ Editor --------------------------------------------------------------------------------------------------
char const * Edit_line = nullptr;        //Editor-Zeile

//------------------------------------ Interpreter ---------------------------------------------------------------------------------------------
char tempstring[STR_LEN];                //String Zwischenspeicher
static char *txtpos, *list_line, *tmptxtpos, *dataline;
static char expression_error;
static char *tempsp;
static char sd_pfad[STR_LEN];            //SD-Card Datei-Pfad

//int i_key = 0;                           //letzte Tasteneingabe
char path1[STR_LEN], path2[STR_LEN];     //Variablen für Dateioperationen
unsigned int Datum[4];                   //Datums-Array
unsigned int Zeit[4];                    //Zeit-Array

//---------------------- DATA Variablen ----------------------------------------
unsigned int datapointer = 0;       //data-Zeiger innerhalb des Datanfeldes
unsigned int restorepointer = 0;    //begin des Datanfeldes
unsigned int num_of_datalines = 0;  //Anzahl DATA-Zeilen
unsigned int current_dataline = 0;  //aktuelle DATA-Zeile
unsigned int data_numbers[300];     //Array zur speicherung von 300 DATA Zeilennummern


////////////////////////////////////////////////////////////////////////////////
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
  'R', 'E', 'N' + 0x80,               //--
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
  'C', 'I', 'R', 'C' + 0x80,        //--
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
  'T', 'R', 'O', 'N' + 0x80,
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
  'P', 'A', 'P', 'E', 'R' + 0x80,
  'I', 'N', 'K' + 0x80,
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
  KW_THEME,   //-------->Option
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
  KW_LCD,             //erfolgreich
  KW_MCPPORT,         //erfolgreich
  KW_MCPPIN,          //erfolgreich
  KW_CHDIR,
  KW_MKDIR,
  KW_RMDIR,
  KW_DEFFUNC, //60
  KW_TRON,
  KW_LED,
  KW_EDIT,
  KW_DOKE,
  KW_BEEP,
  KW_DIM,
  KW_OPTION,
  KW_FPOKE,
  KW_MOUNT,
  KW_COM,     //70
  KW_PIC,
  KW_PAPER,
  KW_INK,
  KW_DEFAULT  //74/* hier ist das Ende */
};


// Variablen zur Zwischenspeicherung von logischen Operationen (AND OR)

int logic_counter;
int logic_ergebnis[10];

//-> bis zu 5 AND oder OR Vergleiche können in einer Zeile vorkommen

struct stack_for_frame {
  char frame_type;
  int for_var;
  float to_var;
  float step;
  char *current_line;
  char *txtpos;
};

struct stack_gosub_frame {
  char frame_type;
  char *current_line;
  char *txtpos;
};

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
  FUNC_BMPREAD,         //ungetestet
  FUNC_FN,
  FUNC_PORT,
  FUNC_PIN,
  FUNC_PI,
  FUNC_LN,        //50
  FUNC_DEEK,
  FUNC_FPEEK,
  FUNC_GPIX,
  FUNC_PIC,
  FUNC_MAP,
  FUNC_CONSTRAIN,
  FUNC_STRING,
  FUNC_UNKNOWN   //56
};

//------------------------------- OPTION-Tabelle - alle Optionen, die dauerhaft gespeichert werden sollen ------------------------------
const static char options[] PROGMEM = {
  'S', 'D', 'C', 'A', 'R', 'D' + 0x80,      //Pin-Festlegung SD-Karte
  'I', 'I', 'C' + 0x80,                     //Pin-Festlegung I2C-Port
  'F', 'O', 'N', 'T' + 0x80,                //Font dauerhaft speichern
  'C', 'O', 'L' , 'O', 'R' + 0x80,          //Vordergrund- und Hintergrundfarbe dauerhaft speichern
  'K', 'E', 'Y' + 0x80,                     //Keyboardlayout 1=US,2=UK,3=GE,4=IT,5=ES,6=FR,7=BE,8=NO,9=JP
  'T', 'H', 'E', 'M', 'E' + 0x80,           //THEME
  0
};

enum {
  OPT_SDCARD = 0,
  OPT_IIC,
  OPT_FONT,
  OPT_COLOR,
  OPT_KEYBOARD,
  OPT_THEME,
  OPT_UNKNOWN
};
//---------------------------------------------------------------------------------------------------------------------------------------


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
#define VAR_SIZE sizeof(float)                           // Größe des Variablenpuffers in bytes (26*26*4)
//#define ARRAY_SIZE (sizeof(struct array_frame)*26*2)         // 26 numerische und 26 Stringarrays 

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
//#################################### Fehlermeldungen des Interpreters ####################

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
static const char indentmsg[]        PROGMEM = "  ";
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

/***************************************************************************/

/******************** Interpreter-Variablen ********************************/
char *pstart;
char *newEnd;
char linelen;
bool then_marker = false;
bool else_marker = false;
float val;
int Zahlenformat = 0;
int logica = 0;                          //logikzähler für IF abfragen
unsigned int ongosub = 0;                //ON-Gosub Goto marker
/******************** Interpreter-Variablen ********************************/

//############################################# Ende Deklarationsungsbereich ######################################################################

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
  switch (modes)
  {
    case 0:                                     //normale Zahlenausgabe
      if (num > 9999999 || num < -9999999)
      {
        if (serout_marker) {
          Serial1.printf("%E", num);              //Ausgabe auf Com-Port
        }
        else {
          Terminal.printf("%E", num);
        }

        //if(ser_marker)
      }
      else {
        tmp = num - int(num);                    //Nachkommastelle vorhanden oder 0 ?
        if (tmp == 0) {                          //keine Nachkommastelle
          if (serout_marker) {
            Serial1.print(int(num), DEC);        //Ausgabe auf Com-Port
          }
          else {
            Terminal.print(int(num), DEC);         //dann Integerausgabe
          }

          //if(ser_marker)
        }
        else {                                   //Nullen in der Nachkommastelle sollen abgeschnitten werden
          dtostrf(num, 1, Prezision, c);               //Nullen abschneiden
          stz = c;
          len = stz.length();
          stellen = Prezision;
          for (int i = len - 1; i > 0; i--) {
            if ((c[i] > '0') || (c[i] == '.')) break; //keine Null oder komma?
            if (c[i] == '0') stellen -= 1;
          }

          dtostrf(num, 1, stellen, c);                //formatierte Ausgabe ohne Nullen
          if (serout_marker) {
            Serial1.write(c);
          }
          else {
            Terminal.write(c);
          }

          //if(ser_marker)
        }
      }
      break;

    case 1:                                           //Ausgabe als Binärzahl

      hexnum = num;
      if (serout_marker) {
        Serial1.write('%');
        Serial1.print(hexnum, BIN);
      }
      else {
        Terminal.write('%');
        Terminal.print(hexnum, BIN);
      }
      Zahlenformat = 0;
      break;

    case 2:                                           //Ausgabe als Hexadezimalzahl
      hexnum = num;
      if (serout_marker) {
        Serial1.write('#');
        Serial1.print(hexnum, HEX);
      }
      else {
        Terminal.write('#');
        Terminal.print(hexnum, HEX);
      }

      Zahlenformat = 0;

      break;

    default:
      break;
  }//switch(modes)
}

/***************************************************************************/
static void pushb(char b)
{
  sp--;
  *sp = b;
}

/***************************************************************************/
static char popb()
{
  char b;
  b = *sp;
  sp++;
  return b;
}

//--------------------------------------------- DUMP - Befehl -------------------------------------------------------------------------------------

static int Memory_Dump() {                       //DMP Speichertyp 0..2 <,Adresse>
  int ex = 0, c, was;
  int ln = 11;                                   //Anzahl Zeilen
  int x_weite = VGAController.getScreenWidth() / x_char[fontsatz];

  word of = FRAM_OFFSET;
  long n;
  long adr = 0;
  was = abs(int(expression()));                  //nur ganze Zahlen
  if (*txtpos == ',')
  {
    txtpos++;
    adr = abs(expression());                      //nur ganze Zahlen
  }

  if (*txtpos == 'V') {
    of = 0;                                       //wird V angegeben, kann man den Variablenbereich ansehen
    txtpos++;
  }


  if (was > 2) {
    syntaxerror(syntaxmsg);
    return 1;
  }
  spi_fram.begin(3);
  while (!ex) {
    for (int i = 1; i < ln; i++)
    {
      if (adr < 0x1000)Terminal.write('0');
      if (adr < 0x100)Terminal.write('0');
      if (adr < 0x10)Terminal.write('0');
      n = adr;
      Terminal.print(adr, HEX);
      Terminal.write(' ');

      for (int f = 0; f < 8; f++) {                     //8 Speicherplätze lesen und anzeigen
        switch (was) {
          case 1:  //FRAM
            if (spi_fram.read8(of + n) < 16) Terminal.write('0');
            Terminal.print(spi_fram.read8(of + n++), HEX);
            if (x_weite > 39) Terminal.write(' ');                        //wenn genug Platz, dann Leerzeichen zwischen den Werten
            break;
          case 2:  //EEPROM
            if (readEEPROM(EEprom_ADDR, n ) < 16) Terminal.write('0');
            Terminal.print(readEEPROM(EEprom_ADDR, n++ ), HEX);
            if (x_weite > 39) Terminal.write(' ');
            break;
          default:  //interner RAM
            if (program[int(n)] < 16)Terminal.write('0');
            Terminal.print(program[int(n++)], HEX);
            if (x_weite > 39) Terminal.write(' ');
            break;
        }
      }
      if (x_weite < 40) Terminal.write(' ');                             //wenn nicht genug Platz, dann nur ein Leerzeichen nach 8Bytes


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
          Terminal.write(c);
        else
          Terminal.write('.');
      }
      Terminal.println();
    }

    Terminal.println("SPACE<Continue>/CTR+C<Exit>");
    if (wait_key() == 3) ex = 1;

  }//while (ex)
  return 0;
}

//--------------------------------------------- Unterprogramm teste auf gültige Zeilennummer ------------------------------------------------------

static unsigned short testnum(void)               // Überprüfung auf Zeilennummer -> Zeilennummern > 65535 erzeugen einen Überlauf (es fängt wieder bei 1 an)
{
  unsigned int num = 0;
  spaces();//ignore_blanks();

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
  fbcolor(Vordergrund, Hintergrund);          //Themen-Farben setzen ist nötig, sonst fällt der Interpreter in die Terminalfarben zurück
  while ( pgm_read_byte( msg ) != 0 )
  {
    outchar( pgm_read_byte( msg++ ) );
  };

  if (nl == 1) line_terminator();
}

//--------------------------------------------- Unterprogramm - Tastenabfrage (list-Ausgaben) -----------------------------------------------------

static unsigned short wait_key(void) {
  char c;
  while (1) {
    if (Terminal.available())
    {
      c = Terminal.read();
      break;
    }
  }//while (1)
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
        Terminal.write("\b\e[K");     //Backspace
        //if(ser_marker) Serial1.write("\b\e[K");
        break;

      case 0x03:       // ctrl+c
        line_terminator();
        printmsg(breaks, 1);
        current_line = 0;
        sp = program + sizeof(program);
        //printmsg(okmsg, 1);
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

void printline()
{ int digits = 0;
  int num;
  LINENUM line_num;

  line_num = *((LINENUM *)(list_line));
  list_line += sizeof(LINENUM) + sizeof(char);

  num = line_num;

  do {
    pushb(num % 10 + '0');
    num = num / 10;
    digits++;
  }
  while (num > 0);

  while (digits > 0)
  {
    outchar(popb());
    digits--;
  }
  //PrintUnum-Ersatz

  outchar(' ');
  while (*list_line != NL)
  {
    outchar(*list_line);
    list_line++;
  }

  list_line++;
  line_terminator();

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
  float map_var[4]={0,0,0,0};
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
    a = expression();
    if (*txtpos != ')')
      goto expr4_error;
    txtpos++;
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
        SPI_RAM_read(v_adr, buf, 4);
        //readBuffer(FRam_ADDR, v_adr, 4, buf);

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
        a = Terminal.read(5);
        if (a > 122 || a == 63 || a == 64) return 0;
        return a;                                             // INKEY - Taste abfragen
        break;

      case FUNC_NOT:                                          //NOT-Funktion
        a = expression();
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
    //Terminal.print(*txtpos);
    if (*txtpos != '(')                         //Klammer auf
      goto expr4_error;

    txtpos++;

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
      //Terminal.print(*txtpos);
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
        a = mcp.readGPIOA();                     //Port(R,A) liest Port A
        txtpos++;
      }
      else if (iic == 'B') {
        a = mcp.readGPIOB();                     //PORT(R,B) liest Port B
        txtpos++;
      }
      else
        a = mcp.readGPIOAB();                   //Port(R) liest Port A+B
    }


    else  {
      a = expression();                         //1.Zahl (bei Stringvariablen steht in tempstring die 1.Zeichenkette)
    }

    switch (fu)                                 //Rückgabe komplexer Werte (mit Klammer und 1-4 Operatoren)
    {
      case  FUNC_PEEK:                          //Peek Byte
      case  FUNC_DEEK:                          //Deek Word
      case  FUNC_FPEEK:                         //FPEEK float
      case  FUNC_GPIX:                          //GPIX(x,y)
      case  FUNC_MIN:
      case  FUNC_MAX:
        if (*txtpos != ',')
          goto expr4_error;
        txtpos++;
        b = expression();                       //2.Zahl
        break;

      case FUNC_SQR:                            //2.Zahl für n'te Wurzel ist optional
        b = 0;
        if (*txtpos == ',')
        {
          *txtpos++;
          b = expression();
        }
        break;


      case FUNC_FN:
        ((float *)variables_begin)[Fnoperator[charb]] = a;
        Fnvar = 0;
        b = Fnoperator[charb + 4];
        //Terminal.println(b);
        while (*txtpos == ',') {
          txtpos++;
          Fnvar += 1;
          //Terminal.println(Fnvar);
          if (Fnvar == b) {                                      //mehr Parameter als mit DEFN dimensioniert?
            syntaxerror(illegalmsg);
            goto expr4_error;
          }
          ((float *)variables_begin)[Fnoperator[charb + Fnvar]] = expression();
        }

        break;

      case FUNC_LEFT:
        if (*txtpos != ',')
          goto expr4_error;
        txtpos++;
        b = expression();                           //2.Parameter eine Zahl
        tempstring[int(b)] = 0;
        string_marker = true;
        break;

      case FUNC_RIGHT:
        if (*txtpos != ',')
          goto expr4_error;
        txtpos++;
        b = expression();                           //2.Parameter eine Zahl
        cbuf = String(tempstring);
        dbuf = cbuf.substring(cbuf.length() - b, cbuf.length());
        dbuf.toCharArray(tempstring, dbuf.length() + 1);
        string_marker = true;
        break;


      case FUNC_STR:                                //str$(12.34,n) ->Umwandlung Zahl nach String - n=Nachkommastellen
        if (*txtpos != ',')
          goto expr4_error;
        txtpos++;
        b = expression();                           //2.Parameter eine Zahl
        break;

      case FUNC_MID:
        if (*txtpos != ',')
          goto expr4_error;
        txtpos++;
        b = expression();                           //2.Parameter eine Zahl
        if (*txtpos != ',')
          goto expr4_error;
        txtpos++;
        c = expression();
        cbuf = String(tempstring);
        dbuf = cbuf.substring(b - 1, b - 1 + c);
        dbuf.toCharArray(tempstring, dbuf.length() + 1);
        string_marker = true;
        break;
        
      case FUNC_STRING:
        if(Test_char(',')) goto expr4_error;
        b=expression();                             //Zeichenkette in tempstring
        string_marker = true;
        func_string_marker = true;
        fstring = a;
        break;
          
      case FUNC_TEMP:
        if(Test_char(',')) goto expr4_error;
        b = expression();                           //2.Parameter Temperaturkanal
        break;

      case FUNC_DHT:                                // DHT-Sensor DHT(Port,Typ,temp/humi)
        if(Test_char(',')) goto expr4_error;
        b = expression();                           //2.Parameter Typ
        if(Test_char(',')) goto expr4_error;
        c = expression();                           //3.Parameter Messwert 0=Temp, 1=Humi
        break;


      case FUNC_AREAD:                              //Analog Read IO-Pin 2,26,34,35,36,39 - ein zusätzlicher Parameter bedeutet Ausgabe in Volt
        if (*txtpos == ',') {
          txtpos++;
          b = expression();                         //2.Parameter ->Ausgabe als Spannungswert in entsprechender physikalischer Grösse - Volt (bei SR04 in cm)
        }
        break;

      case FUNC_COMPARE:                            //COMP$(a$,b$)(0=beide Strings gleich, 1=a$>b$, -1=a$<b$)
      case FUNC_INSTR:                              //INSTR(Suchstring,Zeichenkette)
        if(Test_char(',')) goto expr4_error;
        //if (*txtpos != ',')
        //  goto expr4_error;
        quota = false;
        //txtpos++;
        if (!quota) cbuf = String(tempstring);     //ersten String sichern
        if (*txtpos == '"')                         //Zeichenkette in Anführungszeichen?
        {
          String_quoted_read();                     //zweite Zeichenkette lesen und nach dbuf kopieren
          dbuf = String(tempstring);
        }
        else
        {
          b = expression();                         //2.String
          dbuf = String(tempstring);
        }
        break;

      case FUNC_MAP:                                //x=map(value,fromLow, fromHigh, toLow, toHigh)
        if(Test_char(',')) goto expr4_error;
        map_var[0]=expression();                    
        if(Test_char(',')) goto expr4_error;
        map_var[1]=expression();
        if(Test_char(',')) goto expr4_error;
        map_var[2]=expression();
        if(Test_char(',')) goto expr4_error;
        map_var[3]=expression();                        
        break;

      case FUNC_CONSTRAIN:
        if(Test_char(',')) goto expr4_error;
        b=expression();
        if(Test_char(',')) goto expr4_error;
        c=expression();
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
          float reslt = *(float*)buf;
          return reslt;
        }
        else if (a == 1) {
          spi_fram.read(FRAM_OFFSET + b, buf, 4);         //FRAM float
          //readBuffer(FRam_ADDR, b, 4, buf);

          float reslt = *(float*)buf;
          return reslt;
        }
        else if (a == 2) {                                //EEPROM float
          readBuffer(EEprom_ADDR, b, 4, buf);
          float reslt = *(float*)buf;
          return reslt;
        }
        break;

      case FUNC_GPIX:
        //GFX.waitCompletion();                                            //warte bis eventuelle Draw-Anweisungen beendet sind
        buf[0] = GFX.getPixel(a, b).R;
        buf[1] = GFX.getPixel(a, b).G;
        buf[2] = GFX.getPixel(a, b).B;
        //    B                  G                     R
        a = (buf[2] / 85) + ((buf[1] / 85) << 2) + ((buf[0] / 85) << 4); //einzelne Farbanteile in 64-Farbwert zurückwandeln
        return a;
        break;

      case FUNC_GET:
        if (a > 0) return int(tc.getCursorRow());        //get(0)=x
        else    return int(tc.getCursorCol());           //get(1)=y
        break;

      case FUNC_VAL:                                     //VAL("numerische Zeichenkette")
        dbuf = String(tempstring);
        a = dbuf.toFloat();
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
          Terminal.write(' ');
          //if(ser_marker) Serial1.write(' ');
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
          Terminal.print("no BMP-Sensor connected!");
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
        a = expression();                                 //Formel ausführen
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
        return map(a,map_var[0],map_var[1],map_var[2],map_var[3]);
        break;
      
      case FUNC_CONSTRAIN:
        return constrain(a,b,c);
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
static float expression(void)
{
  float a, b;
  unsigned long c;

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
//--------------------------------------------- SDT - Befehl "SDT Tag,Monat,Jahr,Stunde,Minute,Sekunde"--------------------------------------------

static int set_TimeDate(void)
{ int tagzeit[7];
  expression_error = 0;
  tagzeit[0] = abs(int(expression()));         //nur ganze Zahlen
  if (expression_error)
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  for (int i = 1; i < 6; i++)
  {
    if (Test_char(',')) return 1;
    tagzeit[i] = abs(int(expression()));         //nur ganze Zahlen
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
  }
  rtc.adjust(DateTime(tagzeit[2], tagzeit[1], tagzeit[0], tagzeit[3], tagzeit[4], tagzeit[5]));
  return 0;
}
//--------------------------------------------- PRZ - Befehl --------------------------------------------------------------------------------------

static int set_prezision(void)
{
  if (Test_char('(')) return 1;
  expression_error = 0;
  Prezision = abs(int(expression()));         //nur ganze Zahlen
  if (expression_error)
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  if (Test_char(')')) return 1;
  return 0;
}

//--------------------------------------------- LIST - Befehl -------------------------------------------------------------------------------------

void list_out()
{
  int l = 0;
  int ex = 0;
  linenum = testnum(); // Retuns 0 if no line found.
  // Should be EOL
  if (txtpos[0] != NL)
  {
    syntaxerror(syntaxmsg);
    return;
  }

  // Find the line
  list_line = findline();
  while (list_line != program_end) {
    if (ex == 1)                            //Abbruch, dann raus
      break;
    printline();                            //Zeile ausgeben
    l++;
    if (!list_send) {
      if (l == 12) {                          //nach 12 Zeilen auf Tastatur warten
        l = 0;
        if (wait_key() == 3) break;
      }
    }
  }
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

//--------------------------------------------- GOSUB - Befehl ------------------------------------------------------------------------------------

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

//--------------------------------------------- INPUT - Befehl ------------------------------------------------------------------------------------
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
    a = expression();                         //1.Zahl (bei Stringvariablen steht in tempstring die 1.Zeichenkette)
    printstring(tempstring);
    tempstring[0] = 0;
    string_marker = false;                     //String-Marker zurücksetzen, für korrekte Printausgabe/Werteübergabe
  }
  if (*txtpos != ';') {
    syntaxerror(syntaxmsg);
    return 1;
  }
  txtpos++;



  while (1)
  {
    c = spaces();
    if (c < 'A' || c > 'Z')
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
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

//--------------------------------------------- DATA - Befehl ------------------------------------------------------------------------------------

static float data_get(void)
{
  float value;
  float *var;
  char *st;
  int tmp, stmp, svar, i;
  char c;

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
    return 1;//continue;
  }

  var = (float *)variables_begin + *txtpos - 'A';
  stmp = (int) (*txtpos - 'A') * STR_LEN;                                      //Strings nur als einbuchstabige Variablen erlaubt, deshalb Variablenadresse sichern
  txtpos++;
  if (*txtpos >= 'A' && *txtpos <= 'Z') {                                 //zweiter Variablenbuchstabe
    tmp = (int) ((*txtpos - 'A' + 1) * 26);
    var = var + tmp;
    //svar = (int)((*txtpos - 'A' + 1)* 40 * 26);                           //zweiter Variablenbuchstabe für Strings
    //stmp = stmp+svar;
    txtpos++;

  }

  //------------------------------------------------------------------------------------------------------------------------------------------------------------
  while (*txtpos >= 'A' && *txtpos <= 'Z') txtpos++;  //so sind auch lange Variablennamen möglich ->siehe auch expr4()
  //------------------------------------------------------------------------------------------------------------------------------------------------------------

  if (spaces() == '$')
  { //String?
    txtpos++;
    Data_String_quoted_read();
    i = 0;
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
        else {
          Stringtable[stmp + i] = '\0';
        }

      }
    }

    return 0;
  }
  else
  {
    spaces();
    value = data_expr();
  }
  //if (expression_error) return 1;//continue;
  // Check that we are at the end of the statement
  if (*txtpos == NL)
  {
    *var = value;

    return 0;//continue;
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

//--------------------------------------------- Wert aus DATA - Anweisung lesen ------------------------------------------------------------------------


static float data_expr(void)
{
  float a = 0;
  float b = 0;
  float c = 0;
  unsigned long t = 0;
  unsigned long f = 0;
  char g = 0;
  int pointmarker = 0;
  int i;
  bool quota = false;
  String cbuf, dbuf;
  char *st;
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

//--------------------------------------------- Zeichenkette aus DATA - Anweisung lesen ---------------------------------------------------------------


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


//---------------------------------------- Zeile ans Ende des Speichers verschieben -----------------------------------
void move_line() {

  toUppercaseBuffer();                              //Zeile in Großbuchstaben umwandeln

  //convert();

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
//-------------------------------------------- neue Zeile einfügen ----------------------------------------------------
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
  spi_fram.begin(3);                                     //Fram select
  cmd_new();                                             //alles löschen
  int a, e;
  tron_marker = 0;

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

    if (breakcheck())                                              //Programmabbruch mit Ctrl-C
    {

      line_terminator();

      if (current_line != NULL)
      {
        printmsg(breakmsg, 0);
        linenum = *((LINENUM *)(current_line));
        Terminal.print(linenum);
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
        if (*txtpos == '_') {
          txtpos++;
          if (*txtpos == 'B') {
            txtpos++;
            if (load_binary())
              continue;
          }
        }
        else load_file();
        string_marker = false;
        continue;
        break;

      case KW_NEW:                                        // NEW
        cmd_new();
        continue;

      case KW_RUN:                                        // RUN
        clear_var();
        find_data_line();                                 //Data-Zeilen finden
        current_line = program_start;                     //beginn mit erster Zeile
        sp = program + sizeof(program);
        goto execline;
        break;

      case KW_SAVE:                                       // SAVE filename (/filename.bas)
        save_file();
        string_marker = false;
        continue;
        break;

      case KW_NEXT:                                       // NEXT
        goto next;
        break;

      case KW_REN:                                        // RENAME
        renameWord();
        renameFile(SD, path1, path2);
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

      case KW_ON:                                         //ON (GOTO/GOSUB)
        ongosub = 0;
        ongosub = get_value();
        if (ongosub == 0) goto execnextline;              //ist der Wert der Variable=0 dann wird mit der nächsten Zeile weitergemacht
        break;

      case KW_GOTO ... KW_GOSUB:                          // GOTO/GOSUB
        expression_error = 0;
        linenum = expression();
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
            syntaxerror(syntaxmsg);
            continue;
          }
        }
        if (keyword_index == KW_GOSUB)
        {
          gosub();
        }

        current_line = findline();
        goto execline;

      case KW_RETURN:                                     // RETURN
        goto gosub_return;
        break;

      case KW_REM:                                        // REM
        goto execnextline;                                // Ignoriere die komplette Zeile
        break;

      case KW_FOR:
        goto forloop;

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
        tc.setCursorPos(1, 1);
        GFX.clear();
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
        load_file();
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
            //Terminal.print(data_numbers[i],DEC);
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
        for (int i = 0; i < logic_counter+1; i++)
        {
          logica += logic_ergebnis[i];
        }
        val = logica - logic_counter;                     //sind alle Bedingungen erfüllt lautet das Ergebnis 0
        break;

      case KW_OR:
        expression_error = 0;
        val = get_value();
        
        logic_ergebnis[logic_counter++] = int(val);
        for (int i = 0; i < logic_counter+1; i++)         //alle Ergebnisse einlesen und auswerten
        {
          //Terminal.print(logic_ergebnis[i]);
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
          syntaxerror(syntaxmsg);
          continue;
        }
        break;

      case KW_SPRITE:
        if (Test_char('(')) continue;
        pa = *txtpos;
        if (pa == 'C' || pa == 'D' || pa == 'S') {
          txtpos++;
          if (sprite(pa)) {
            syntaxerror(syntaxmsg);
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
          syntaxerror(syntaxmsg);
          continue;
        }
        break;

      case KW_PEN:
        if (set_pen()) {
          syntaxerror(syntaxmsg);
          continue;
        }
        break;


      case KW_LCD:
        if (LCD_Set()) {
          syntaxerror(syntaxmsg);
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

      case KW_TRON:                                   //TRON(delay)
        if (Test_char('(')) continue;
        expression_error = 0;
        tron_marker = byte(get_value());
        if (Test_char(')')) continue;
        break;

      case KW_LED:
        if (LED_Set()) {
          syntaxerror(syntaxmsg);
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
        spi_fram.begin(3);                                     //Fram select
        break;

      case KW_COM:
        if (cmd_serial())
          continue;
        break;
        
      case KW_PIC:
        if (show_Pic())
          continue;
        break;

      case KW_PAPER:
        Hintergrund=get_value();
        bcolor(Hintergrund);
        break;

      case KW_INK:
        Vordergrund=get_value();
        fcolor(Vordergrund);
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
    if (tron_marker>0) {
      Terminal.print('<' + String(*((LINENUM *)(current_line))) + '>');
      delay(tron_marker);                                                  //Verzögerung
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

      initial = get_value();//expression();
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
        continue;//
      }

      if (!expression_error && (*txtpos == NL || *txtpos == ':'))
      {
        string_marker = false;
        struct stack_for_frame *f;
        if (sp + sizeof(struct stack_for_frame) < stack_limit) {
          printmsg(fornextmsg, 1);
          Terminal.print(linenum, DEC);
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
    syntaxerror(syntaxmsg);
    continue;


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
    continue;

  }//while(1)
} //Basic_interpreter

//#################################################################################################################################################
//############################################# Ende Basic_Interpreter ############################################################################
//#################################################################################################################################################





//--------------------------------------------- PRINT - Befehl ------------------------------------------------------------------------------------
static int command_Print(void)
{
  int k = 0, at = 0, sm = 0;
  int xp, yp, tx, ty;
  while (!k)
  { char c = spaces();

    switch (c)
    {

      case ';':
        if (skip_spaces() == NL)
        {
          semicolon = true;
          k = 1;

        }
        break;

      case '"':
        print_quoted_string();
        break;

      case ':':
        if(!semicolon) Terminal.println();
        //if(ser_marker) Serial1.println();
        txtpos++;
        k = 1;
        break;

      case NL:
        Terminal.println();
        semicolon=false;
        //if(ser_marker) Serial1.println();
        k = 1;
        break;

      case ',':
        xp = tc.getCursorCol();             //Tabulator ausgeben
        yp = tc.getCursorRow();
        if (xp < 37)
          tc.setCursorPos(xp + 8, yp);
        else {
          Terminal.println();
          //if(ser_marker) Serial1.println();
        }
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
          //else k=2;
        }
        else {
          txtpos = tmptxtpos;
        }

      default:
        float e;

        e = get_value();                    //Zahl oder Variable lesen

        if (expression_error) k = 2;
        if(func_string_marker == true){
            for(int i=0; i< fstring;i++){
               printstring(tempstring);
            }
            //Terminal.print(tempstring);
            func_string_marker=false;
            string_marker = false;
            chr = false;
        }
        else if (string_marker == true)                  //String?
        {
          printstring(tempstring);
          func_string_marker=false;
          string_marker = false;
          chr = false;
          }
        else if (chr == true)
        {
          Terminal.write(int(e));            //CHR$
          //if(ser_marker) Serial1.write(int(e));
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

//--------------------------------------------- Variablen - Eingabe -------------------------------------------------------------------------------

static float var_get(void)
{
  float value;
  float *var;
  char *st;
  int tmp, stmp, svar, i, var_pos, array_art ;
  char c;
  word arr_adr;
  //byte p_data[8], str_data[STR_LEN];

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

    expression();                                                         //Stringvariable?, Zeichenkette oder numerische Variable

    i = 0;
    while (1)
    {
      c = tempstring[i];
      if (c == '\0') {
        if (array_art == 2) {
          SPI_RAM_write8(arr_adr + i, '\0');
          //writeEEPROM(FRam_ADDR, arr_adr + i, '\0' );
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
            //writeEEPROM(FRam_ADDR, arr_adr + i++, c);
          }
          else Stringtable[stmp + i++] = c;
        }
        else {
          if (array_art == 2) {
            SPI_RAM_write8(arr_adr + i, '\0');
            //writeEEPROM(FRam_ADDR, arr_adr + i, '\0' );
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
    //WriteBuffer(FRam_ADDR, arr_adr, 4, bytes);
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
  SPI_RAM_read(ort, p_data, 6);
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
{ long w_ert = 0;
  byte* bytes = (byte*)&w_ert;

  for (int i = 0; i < VAR_SIZE * 26 * 27; i++)                    //Variablen löschen
  {
    variables_begin[i] = 0;
  }
  for (int i = 0; i < STR_SIZE; i++)                              //Strings löschen
  {
    Stringtable[i] = '\0';
  }
  for (int i = 0x7e00; i < 0x7fff; i += 4) SPI_RAM_write(i, bytes, 4);   //Array-Tabelle löschen
  Var_Neu_Platz = 0;                                              //Array-Zeiger zurücksetzen
  num_of_datalines = 0;                                           //Datazeilenzähler zurücksetzen
  return;
}

//--------------------------------------------- NEW-Befehl -------------------------------------------------------------
void cmd_new(void) {
  long w_ert = 0;
  byte* bytes = (byte*)&w_ert;
  program_start = program;
  program_end = program_start;
  sp = program + sizeof(program);                                    // Needed for printnum
  stack_limit = program + sizeof(program) - STACK_SIZE;// - ARRAY_SIZE;
  variables_begin = stack_limit - (26 * 27 * VAR_SIZE) ;             //26*27 (2)Buchstaben als Variablen
  memset(program, 0, 1000);                                          //die ersten 1000 Bytes des Speichers löschen
  clear_var();                                                       //Variablen und Array-Tabelle löschen
  for (int i = 0x0; i < 0x7fff; i += 4) SPI_RAM_write(i, bytes, 4);   //Array-Bereich löschen

  print_info();                                                      //Start-Bildschirm anzeigen

}

//--------------------------------------------- einzelnen Wert eingeben ---------------------------------------------------------------------------

static float get_value()
{
  float value;

  // Work out where to put it
  expression_error = 0;
  value = expression();
  /*
    if (expression_error)
    {
    //syntaxerror(syntaxmsg);
    return value;
    }
  */
  return value;
}

//--------------------------------------------- THEME - Befehl ------------------------------------------------------------------------------------

static int set_theme(int value)
{
  int fn;
#ifdef VGA16
switch (value)
{
case 0: //C64
Vordergrund = 12;
Hintergrund = 4;
set_font(0);
break;
case 1: //C128
Vordergrund = 10;
Hintergrund = 8;
set_font(0);
break;
case 2: //CPC
Vordergrund = 11;
Hintergrund = 4;
set_font(19);
break;
case 3: //ATARI-800
Vordergrund = 14;
Hintergrund = 6;
set_font(21);
break;
case 4: //ZX-SPECTRUM
Vordergrund = 0;
Hintergrund = 7;
set_font(16);
break;
case 5: //KC87
Vordergrund = 15;
Hintergrund = 0;
set_font(16);
break;
case 6: //KC 85
Vordergrund = 15;
Hintergrund = 4;
set_font(19);
break;
case 7: //VIC20
Vordergrund = 4;
Hintergrund = 15;
set_font(17);
break;
case 8: //TRS80
Vordergrund = 2;
Hintergrund = 0;
set_font(17);
break;
case 9: //Basic32+
Vordergrund = 11;
Hintergrund = 1;
set_font(7);
break;
case 10:  //LCD
Vordergrund = 8;
Hintergrund = 3;
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

#else
switch (value)
{
case 0: //C64
Vordergrund = 27;
Hintergrund = 22;
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
  set_font(16);
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
#endif
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

//--------------------------------------------- POKE - Befehl -------------------------------------------------------------------------------------

static int poke(int fn)             //POKE WAS,ADRESSE,WERT
{
  byte value;
  unsigned long address;
  int was, weite;
  unsigned long wert;
  byte p_data[2];
  float w_ert;

  was = abs(get_value());                                       //Speicherort 0..2 ->0-RAM, 1-FRAM, 2-EEPROM
  if (was > 2) was = 2;
  // check for a comma
  if (Test_char(',')) return 1;

  address = abs(get_value());                                  //Speicheradresse
  // check for a comma
  if (Test_char(',')) return 1;

  // Wert eingeben
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
    if (fn == KW_POKE)  SPI_RAM_write8(FRAM_OFFSET + address, byte(wert)); //FRAM Byte
    else if (fn == KW_DOKE)
    {
      p_data[0] = highByte(wert);
      p_data[1] = lowByte(wert);
      SPI_RAM_write(FRAM_OFFSET + address, p_data, 2);                       //FRAM Word
    }
    else if (fn == KW_FPOKE) {
      byte* bytes = (byte*)&w_ert;
      SPI_RAM_write(FRAM_OFFSET + address, bytes, 4);                       //FRAM float
    }
    return 0;
  }
  /*
    else if (was == 1) {
    if (fn == KW_POKE)  writeEEPROM(FRam_ADDR, address, byte(wert));   //FRAM Byte
    else if (fn == KW_DOKE)
    {
      p_data[0] = highByte(wert);
      p_data[1] = lowByte(wert);
      WriteBuffer(FRam_ADDR, address, 2, p_data);                       //FRAM Word
    }
    else if (fn == KW_FPOKE){
      byte* bytes = (byte*)&w_ert;
      WriteBuffer(FRam_ADDR, address, 4, bytes);                       //FRAM float
    }
    return 0;
    }
  */
  //----------------------------- EEPROM -------------------------------------------
  else if (fn == KW_POKE)  writeEEPROM(EEprom_ADDR, address, byte(wert));  //EEPROM Byte
  else if (fn == KW_DOKE)
  {
    p_data[0] = highByte(wert);
    p_data[1] = lowByte(wert);
    WriteBuffer(EEprom_ADDR, address, 2, p_data);                       //EEPROM Word
  }
  else if (fn == KW_FPOKE) {                                              //EEPROM float
    byte* bytes = (byte*)&w_ert;
    WriteBuffer(EEprom_ADDR, address, 4, bytes);
  }
  return 0;

}

//--------------------------------------------- STYLE - Befehl ------------------------------------------------------------------------------------

static int set_style(void)
{
  int st, mst;
  bool tmst;
again:
  expression_error = 0;
  st = abs(int(expression()));         //nur ganze Zahlen
  if (expression_error)
  {
    syntaxerror(syntaxmsg);
    return 1;
  }

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
  if (spaces() == ',')
  {
    txtpos++;
    goto again;
  }
  return 0;
}

//--------------------------------------------- SCROLL - Befehl -----------------------------------------------------------------------------------

static int scroll_xy(void)
{
  short int i, par[6];
  i = 0;
  expression_error = 0;
  par[0] = expression();
  if (expression_error)
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  if (Test_char(',')) return 1;

  expression_error = 0;
  par[1] = expression();
  if (expression_error)
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  // Check that we are at the end of the statement
  if (*txtpos != NL && *txtpos != ':')
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  GFX.scroll(par[0], par[1]);

  return 0;
}


//--------------------------------------------- Lines/CIRCLE/RECT - Befehl --------------------------------------------------------------------------

static int line_rec_circ(int circ_or_rect, int param)
{
  //#################### Linie, Rechteck oder Kreis zeichnen #################

  short int i, par[6];
  i = 0;

  while (i < param) {                 //3 o.4 Parameter eingeben
    expression_error = 0;
    par[i] = expression();
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }

    // check for a comma
    if (Test_char(',')) return 1;
    i++;
  }

  expression_error = 0;
  par[param] = expression();            //param bei circ und rect=4 -> 5.Parameter , lines=3 -> 4.Parameter
  if (expression_error)
  {
    syntaxerror(syntaxmsg);
    return 1;
  }

  // Check that we are at the end of the statement
  if (*txtpos != NL && *txtpos != ':')
  {
    syntaxerror(syntaxmsg);
    return 1;
  }

  //----------------- abhängig vom Parameter circ_or_rect wird zwischen Circle,Rect und Lines ausgewählt -------
  switch (circ_or_rect) {
    case 1:
      if (par[4] == 0) GFX.drawEllipse(par[0], par[1], par[2], par[3]);       //Circle circ x,y,xx,yy,fill=0
      else GFX.fillEllipse(par[0], par[1], par[2], par[3]);                   //Circle circ x,y,xx,yy,fill=1
      break;
    case 2:
      if (par[4] == 0) GFX.drawRectangle(par[0], par[1], par[2], par[3]);     //Rectangle rect x,y,xx,yy,fill=0
      else GFX.fillRectangle(par[0], par[1], par[2], par[3]);                 //Rectangle rect x,y,xx,yy,fill=1
      break;
    default:
      GFX.drawLine(par[0], par[1], par[2], par[3]);                           //Line line x,y,xx,yy
      break;
  }
  return 0;
}

//************************************************************* DRAW-Befehl ******************************************************************
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

//************************************************** Sprite-Befehl *********************************************************************************
static int sprite(char cm) {
  short int cnt;
  String myString, abuf;
  short int nr, i, par[8];

  if (Test_char(',')) return 1;
  switch (cm) {
    case 'C':
      expression_error = 0;
      cnt = expression();
      if (expression_error) return 1;
      if (cnt > 16)                                        //16 Sprites sind erlaubt
      {
        syntaxerror(valmsg);                               // falscher wert
        return 1;
      }

      break;
    case 'D':
      expression_error = 0;
      nr = expression();                                   //Sprite-Nr
      if (expression_error) return 1;
      if (Test_char(',')) return 1;
      for (i = 1; i < 4; i++)
      {
        expression_error = 0;
        par[i] = expression();                             //Frame
        if (expression_error) return 1;
        if (Test_char(',')) return 1;
      }
      //par[3]=farbe des Sprites
      par[4] = (bitRead(par[3], 5) * 2 + bitRead(par[3], 4)) * 64;
      par[5] = (bitRead(par[3], 3) * 2 + bitRead(par[3], 2)) * 64;
      par[6] = (bitRead(par[3], 1) * 2 + bitRead(par[3], 0)) * 64;

      i = expression(); //String_quoted_read();
      break;

    case 'S':
      expression_error = 0;
      nr = expression();                                  //Sprite-Nr
      if (expression_error) return 1;
      if (Test_char(',')) return 1;
      String_quoted_read();
      if (Test_char(',')) return 1;
      for (i = 1; i < 3; i++)
      {
        expression_error = 0;
        par[i] = expression();
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
      myString = "\e_GSPRITECOUNT" + String(cnt, DEC) + "$";
      Terminal.print(myString);
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

//----------------------------------------------------- SND-Befehl ----------------------------------------------------------
static int Sound(void) {
  short int  nr, i, par[7];

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
    par[i] = expression();
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



//--------------------------------------------- PSET - Befehl -------------------------------------------------------------------------------------

static int pset(void)
{

  short int xp, yp, pc;
  // Work out where to put it
  expression_error = 0;
  xp = expression();
  if (expression_error)
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  // check for a comma
  if (Test_char(',')) return 1;

  // Now get the value to assign
  expression_error = 0;
  yp = expression();
  if (expression_error)
  {
    syntaxerror(syntaxmsg);
    return 1;
  }

  if (*txtpos == ',') {                  //optional Angabe der Farbe
    txtpos++;
    expression_error = 0;
    pc = expression();
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
    fcolor(pc);
    //#ifdef VGA16
    //GFX.setPenColor((bitRead(pc, 5) * 2 + bitRead(pc, 4)) * 64, (bitRead(pc, 3) * 2 + bitRead(pc, 2)) * 64, (bitRead(pc, 1) * 2 + bitRead(pc, 0)) * 64);
    //#else
    //GFX.setPenColor((bitRead(pc, 5) * 2 + bitRead(pc, 4)) * 64, (bitRead(pc, 3) * 2 + bitRead(pc, 2)) * 64, (bitRead(pc, 1) * 2 + bitRead(pc, 0)) * 64);
    //#endif
  }
  // Check that we are at the end of the statement
  if (*txtpos != NL && *txtpos != ':')
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  GFX.setPixel(xp, yp);

  return 0;
}

//--------------------------------------------- COLOR - Befehl ------------------------------------------------------------------------------------

static int color(void)
{

  short int fc, bc;
  // Work out where to put it
  expression_error = 0;
  fc = expression();
  if (expression_error)
  {
    syntaxerror(syntaxmsg);
    return 1;
  }
  // check for a comma
  if (Test_char(',')) return 1;

  // Now get the value to assign
  expression_error = 0;
  bc = expression();
  if (expression_error)
  {
    syntaxerror(syntaxmsg);
    return 1;
  }

  // Check that we are at the end of the statement
  if (*txtpos != NL && *txtpos != ':')
  {
    syntaxerror(syntaxmsg);
    return 1;
  }

  fbcolor(fc & 63, bc & 63);

  return 0;
}

//--------------------------------------------- Unterrogramm - Farben setzen ----------------------------------------------------------------------

void fbcolor(int fc, int bc)
{
  fcolor(fc);
  bcolor(bc);
}

void fcolor(int fc) {
#ifdef VGA16

  fc = fc & 0xF;

  switch (fc) {
    case 0:
      GFX.setPenColor(Color::Black);
      break;
    case 1:
      GFX.setPenColor(Color::Red);
      break;
    case 2:
      GFX.setPenColor(Color::Green);
      break;
    case 3:
      GFX.setPenColor(Color::Yellow);
      break;
    case 4:
      GFX.setPenColor(Color::Blue);
      break;
    case 5:
      GFX.setPenColor(Color::Magenta);
      break;
    case 6:
      GFX.setPenColor(Color::Cyan);
      break;
    case 7:
      GFX.setPenColor(Color::White);
      break;
    case 8:
      GFX.setPenColor(Color::BrightBlack);
      break;
    case 9:
      GFX.setPenColor(Color::BrightRed);
      break;
    case 10:
      GFX.setPenColor(Color::BrightGreen);
      break;
    case 11:
      GFX.setPenColor(Color::BrightYellow);
      break;
    case 12:
      GFX.setPenColor(Color::BrightBlue);
      break;
    case 13:
      GFX.setPenColor(Color::BrightMagenta);
      break;
    case 14:
      GFX.setPenColor(Color::BrightCyan);
      break;
    case 15:
      GFX.setPenColor(Color::BrightWhite);
      break;
  }
#else
GFX.setPenColor((bitRead(fc, 5) * 2 + bitRead(fc, 4)) * 64, (bitRead(fc, 3) * 2 + bitRead(fc, 2)) * 64, (bitRead(fc, 1) * 2 + bitRead(fc, 0)) * 64); //setPenColor(RGB888(color));
#endif
}

  void bcolor(int bc) {

#ifdef VGA16

    bc = bc & 0xF;


    switch (bc) {
      case 0:
        GFX.setBrushColor(Color::Black);
        break;
      case 1:
        GFX.setBrushColor(Color::Red);
        break;
      case 2:
        GFX.setBrushColor(Color::Green);
        break;
      case 3:
        GFX.setBrushColor(Color::Yellow);
        break;
      case 4:
        GFX.setBrushColor(Color::Blue);
        break;
      case 5:
        GFX.setBrushColor(Color::Magenta);
        break;
      case 6:
        GFX.setBrushColor(Color::Cyan);
        break;
      case 7:
        GFX.setBrushColor(Color::White);
        break;
      case 8:
        GFX.setBrushColor(Color::BrightBlack);
        break;
      case 9:
        GFX.setBrushColor(Color::BrightRed);
        break;
      case 10:
        GFX.setBrushColor(Color::BrightGreen);
        break;
      case 11:
        GFX.setBrushColor(Color::BrightYellow);
        break;
      case 12:
        GFX.setBrushColor(Color::BrightBlue);
        break;
      case 13:
        GFX.setBrushColor(Color::BrightMagenta);
        break;
      case 14:
        GFX.setBrushColor(Color::BrightCyan);
        break;
      case 15:
        GFX.setBrushColor(Color::BrightWhite);
        break;
    }
#else
GFX.setBrushColor((bitRead(bc, 5) * 2 + bitRead(bc, 4)) * 64, (bitRead(bc, 3) * 2 + bitRead(bc, 2)) * 64, (bitRead(bc, 1) * 2 + bitRead(bc, 0)) * 64);
#endif

  }
  //--------------------------------------------- PEN - Befehl --------------------------------------------------------------------------------------

  static int set_pen(void)
  {

    short int pc, pw;

    expression_error = 0;                         //Pen-Farbe
    pc = int(expression());
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }

    if (*txtpos == ',')                             //wurde die PEN-Weite angegeben?
    {
      txtpos++;
      expression_error = 0;
      pw = int(expression());
      if (expression_error)
      {
        syntaxerror(syntaxmsg);
        return 1;
      }
      GFX.setPenWidth(pw);                           //Pen-Weite
    }
    // Check that we are at the end of the statement
    else if (*txtpos != NL && *txtpos != ':')
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
    fcolor(pc);
    //GFX.setPenColor((bitRead(pc, 5) * 2 + bitRead(pc, 4)) * 64, (bitRead(pc, 3) * 2 + bitRead(pc, 2)) * 64, (bitRead(pc, 1) * 2 + bitRead(pc, 0)) * 64);

    return 0;

  }



  //---------------------------------------------- DEF_FN --------------------------------------------------
  static int def_func(void)
  {
    int fname, fnpos, fault, i;

    if (*txtpos < 'A' || *txtpos > 'Z')                                     //Funktionsname
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
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

  //--------------------------------------------- Unterprogramm - String ausgeben -------------------------------------------------------------------

  void printstring(const char *msg)
  { int i = 0;
    while ( 1 ) {
      if (msg[i] == '\0') break;
      outchar( msg[i++] );
    };
  }

  //--------------------------------------------- PULSE - Befehl ------------------------------------------------------------------------------------
  static int set_pulse(void)
  {
    int p, x, y, pl, i;
    if (Test_char('(')) return 1;

    expression_error = 0;
    p = expression();             //IO-Port
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
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
    pl = expression();                   //Anzahl-Pulse
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }

    if (Test_char(',')) return 1;

    expression_error = 0;
    x = expression();                   //Pause1-Zeit
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }

    if (Test_char(',')) return 1;

    expression_error = 0;
    y = expression();                   //Pause2-Zeit
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }

    if (Test_char(')')) return 1;

    if (*txtpos != NL && *txtpos != ':')
    {
      syntaxerror(syntaxmsg);
      return 1;
    }


    for (i = 0; i < pl; i++) {                    //Anzahl pl-Impulse
      digitalWrite(p, HIGH);                      //setze Port - High
      delay(x);                                   //Pause x
      digitalWrite(p, LOW);                       //Low
      delay(y);
    }

    return 0;
  }

  //--------------------------------------------- Dwrite-Befehl -------------------------------------------------------------------------------------
  static int set_port(void)
  {
    int p, x;

    if (Test_char('(')) return 1;

    expression_error = 0;
    p = expression();
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
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
    x = expression();
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
    if (*txtpos != ')') {
      syntaxerror(syntaxmsg);
      return 1;
    }
    txtpos++;
    if (*txtpos != NL && *txtpos != ':')
    {
      syntaxerror(syntaxmsg);
      return 1;
    }

    if (x > 0) digitalWrite(p, HIGH);                //setze Port - alles ausser 0 ist High
    else digitalWrite(p, LOW);                       //sonst Low

    return 0;
  }

  //--------------------------------------------- PWM-Befehl -------------------------------------------------------------------------------------
  static int set_pwm(void)
  {
    int p, x, chan;

    if (*txtpos != '(') {
      syntaxerror(syntaxmsg);
      return 1;
    }
    txtpos++;

    expression_error = 0;
    p = expression();
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
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
    x = expression();
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }

    if (*txtpos != ')') {
      syntaxerror(syntaxmsg);
      return 1;
    }
    txtpos++;
    if (*txtpos != NL && *txtpos != ':')
    {
      syntaxerror(syntaxmsg);
      return 1;
    }

    ledcWrite(chan, x);                //PWM-Wert setzen (pwm-channel,wert)

    return 0;
  }
  //--------------------------------------------- CUR - Befehl --------------------------------------------------------------------------------------

  static int cursor_onoff(void)
  {
    short int onoff;
    // Work out where to put it
    expression_error = 0;
    onoff = expression();
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
    if (onoff == 0) Terminal.enableCursor(false);
    else Terminal.enableCursor(true);
    return 0;

  }

  //--------------------------------------------- POS - Befehl --------------------------------------------------------------------------------------

  static int set_pos(void)
  {
    int xp, yp;

    expression_error = 0;
    xp = expression();
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }

    // check for a comma
    if (Test_char(',')) return 1;

    // Now get the value to assign
    expression_error = 0;
    yp = expression();
    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }

    if (*txtpos != NL && *txtpos != ':')
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
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

    // make sure there are no quotes or spaces, search for valid characters
    int i = 0;
    while (*txtpos >= ' ' || *txtpos <= 'Z' || *txtpos >= 'a' || *txtpos <= 'z')//&& (*txtpos != '"'))
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
    //string_marker=true;
    //Terminal.println(String(*txtpos));
    return c;
  }

  //--------------------------------------------- Unterprogramm - Zeilenabschluss -------------------------------------------------------------------
  static void line_terminator(void)
  {
    outchar(NL);
    outchar(CR);
  }

  //--------------------------------------------- FONT - Befehl -------------------------------------------------------------------------------------

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


  //--------------------------------------------- Unterprogramm - Startbildchirm  -------------------------------------------------------------------

  void print_info()
  { int c, d, e, f;
    int y_pos = y_char[fontsatz] / 2;
    int x_pos = VGAController.getScreenWidth() / x_char[fontsatz];

    float g = 3.3 / 4095 * analogRead(Batt_Pin);
    g = g / 0.753865;                           //(Umess/(R2/(R1+R2)) R1=3.327kohm R2=10.19kohm
    int   h = 100 - ((4.2 - g) * 100);                //Akkuwert in Prozent
    if (h > 100) h = 100;


    fbcolor(Vordergrund, Hintergrund);
    Terminal.enableCursor(false);
    GFX.clear();
    c = random(64);
    d = random(64);
    e = random(64);
    f = random(64);
    fcolor(c);  //48,60,3,28
    GFX.drawLine(100 - h, 1 + (2 * y_char[fontsatz]), 219 + h, 1 + (2 * y_char[fontsatz]));
    fcolor(d);
    GFX.drawLine(100 - h, 2 + (2 * y_char[fontsatz]), 219 + h, 2 + (2 * y_char[fontsatz]));
    fcolor(e);
    GFX.drawLine(100 - h, 3 + (2 * y_char[fontsatz]), 219 + h, 3 + (2 * y_char[fontsatz]));
    fcolor(f);
    GFX.drawLine(100 - h, 4 + (2 * y_char[fontsatz]), 219 + h, 4 + (2 * y_char[fontsatz]));
    fcolor(e);
    GFX.drawLine(100 - h, 5 + (2 * y_char[fontsatz]), 219 + h, 5 + (2 * y_char[fontsatz]));
    fcolor(d);
    GFX.drawLine(100 - h, 6 + (2 * y_char[fontsatz]), 219 + h, 6 + (2 * y_char[fontsatz]));
    fcolor(c);
    GFX.drawLine(100 - h, 7 + (2 * y_char[fontsatz]), 219 + h, 7 + (2 * y_char[fontsatz]));

    fcolor(Vordergrund);

    tc.setCursorPos((x_pos - 26) / 2, 1);
    Terminal.write("Basic32+ V ");
    Terminal.write(BasicVersion);
    Terminal.write(" Zille-Soft\r\n");
    tc.setCursorPos((x_pos - 16) / 2 , 2);
    // memory free
    Terminal.print(int(variables_begin - program_end), DEC);
    printmsg(memorymsg, 1);
    /*
      tc.setCursorPos(1, 5);
      Terminal.write("Screen-Size  :");
      Terminal.print(VGAController.getScreenWidth());
      Terminal.write("x");
      Terminal.print(VGAController.getScreenHeight());
    */
    tc.setCursorPos(1, 4);
    Terminal.write("Terminal-Size:");
    Terminal.print(VGAController.getScreenWidth() / x_char[fontsatz]);
    Terminal.write("x");
    Terminal.print(VGAController.getScreenHeight() / y_char[fontsatz]);

    tc.setCursorPos(1, 5);
    Terminal.write("ESP-Memory   :");
    Terminal.print(ESP.getFreeHeap());

    tc.setCursorPos(1, 6);
    //Terminal.print("Fontset      :");
    Terminal.print("Fontset      :");
    Terminal.print(fontsatz);

    tc.setCursorPos(1, 7);
    Terminal.print("Theme        :");
    Terminal.print(Theme_state);
    Terminal.print("=");
    Terminal.print(Themes[Theme_state]);

    Terminal.enableCursor(true);
    tc.setCursorPos(1, 9);
  }




  //--------------------------------------------- Unterprogramm - Üerprüfung auf Abbruch-Taste (Ctrl-C) ---------------------------------------------

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
      Terminal.print(linenum);
    }
    line_terminator();
    warmstart();
    return;
  }
  //--------------------------------------------- Unterprogramm Zeichen von Tastatur oder aus Datei lesen -------------------------------------------

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

      case ( kStreamTerminal ):

      default:

        while (1)
        {

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
            return c;
          }//if(Terminal.available)

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

  //--------------------------------------------- Unterprogramm Zeichen zum Bildschirm oder in Datei schreiben --------------------------------------
  static void outchar(char c)
  {
    if ( inhibitOutput ) return;

    if ( outStream == kStreamFile ) {
      // output to a file
      fp.write( c );
    }
    else {
      if (ser_marker && list_send) Serial1.write(c);       //User-Seriellschnittstelle
      Terminal.write(c);       //------------------------ auf FabGl VGA-Terminal schreiben----------------------------------
    }
  }
  //############################################# Dateioperationen auf der SD-Karte #################################################################
  //--------------------------------------------- Unterprogramm SD-Karte initialisieren -------------------------------------------------------------
  static int initSD( void )
  {

    spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1
    //spiSD.begin(14, 2, 12, kSD_CS);                         //TTGO Board
    if ( !SD.begin( kSD_CS, spiSD )) {                        //SD-Card starten
      // failed
      spiSD.end();                                            //unmount
      syntaxerror(sderrormsg);
      return kSD_Fail;
    }

    // file redirection flags
    outStream = kStreamTerminal;
    inStream = kStreamTerminal;
    inhibitOutput = false;
    sd_pfad[0] = '/';                                         //setze Root-Verzeichnis
    sd_pfad[1] = 0;
    Terminal.println("SD-Card OK");
    spiSD.end();                                              //unmount
    return kSD_OK;
  }

  //--------------------------------------------- SPI-Bus umschalten -------------------------------------------------------------------------------------

  void sd_ende(void) {
    spiSD.end();                                              //SD-Card unmount
    spi_fram.begin(3);                                        //FRAM aktivieren

  }
  //--------------------------------------------- LOAD - Befehl -------------------------------------------------------------------------------------

  static int load_file(void)
  {

    // clear the program
    program_end = program_start;

    // load from a file into memory


    bool a_st = false;

    // Work out the filename
    expression_error = 0;
    get_value();

    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }

    spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

    if ( !SD.exists(String(sd_pfad) + String(tempstring)))
    {
      syntaxerror(sdfilemsg);
      sd_ende();
    }
    else {
      fp = SD.open(String(sd_pfad) + String(tempstring));
      inStream = kStreamFile;
      inhibitOutput = true;
    }


    warmstart();
    return 0;
  }

  //--------------------------------------------- SAVE - Befehl -------------------------------------------------------------------------------------

  static int save_file()
  {

    char c;

    // Work out the filename
    expression_error = 0;
    get_value();


    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
    spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

    // remove the old file if it exists
    if ( SD.exists( String(sd_pfad) + String(tempstring))) {
      Terminal.print("File exist, overwrite? (y/n)");
      while (1)
      {
        c = wait_key();
        if (c == 'y' || c == 'n')
          break;
      }
      if (c == 'y') {
        SD.remove( String(sd_pfad) + String(tempstring));
        Terminal.print(c);
      }
      else
      {
        Terminal.print(c);
        Terminal.println();
        sd_ende();                                             //SD-Card unmount
        warmstart();
        return 0;
      }

    }


    // open the file, switch over to file output
    fp = SD.open( String(sd_pfad) + String(tempstring), FILE_WRITE);
    if (!fp) {
      Terminal.println("Open File-Error!");
    }
    outStream = kStreamFile;

    // copied from "List"
    list_line = findline();
    while (list_line != program_end)
      printline();

    // go back to standard output, close the file
    outStream = kStreamTerminal;

    fp.close();
    Terminal.println();
    sd_ende();                                             //SD-Card unmount
    warmstart();
    return 0;

  }


  //--------------------------------------------- DEL - Befehl --------------------------------------------------------------------------------------

  static int cmd_delFiles(void)
  {

    char c;
    int n = 0;

    // eingabe Dateiname
    expression_error = 0;
    get_value();

    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
    spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

    // Datei löschen, wenn sie existiert
    if ( SD.exists(String(sd_pfad) + String(tempstring))) {
      Terminal.print("delete File? (y/n)");
      while (1)
      {
        c = wait_key();
        if (c == 'y' || c == 'n')
          break;
      }
      if (c == 'y') {
        SD.remove( String(sd_pfad) + String(tempstring));
        Terminal.print(c);
      }
    }
    else n = 1;
    line_terminator();
    fp.close();
    sd_ende();                                             //SD-Card unmount

    warmstart();
    return n;

  }

  //--------------------------------------------- CHDIR - Befehl --------------------------------------------------------------------------------------

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
      printmsg(sderrormsg, 1);
    }
    sd_ende();                                             //SD-Card unmount

  }
  //------------------------------------------- Befehl MD und RD (MKDIR und Remove Dir -----------------------------------
  static int cmd_mkdir(int mod)
  {

    // eingabe Verzeichnisname
    expression_error = 0;
    get_value();

    if (expression_error)
    {
      syntaxerror(syntaxmsg);
      return 1;
    }
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
  //--------------------------------------------- DIR - Befehl --------------------------------------------------------------------------------------

  void cmd_Dir(void)
  { int ln = 1;
    int ex = 0;
    int Dateien = 0;

    spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

    File dir = SD.open(String(sd_pfad));

    dir.seek(0);

    while ( !ex ) {
      File entry = dir.openNextFile();
      if ( !entry ) {
        entry.close();
        break;
      }

      // common header
      printmsg( indentmsg, 0 );
      printmsg( (const char *)entry.name() , 0);
      if ( entry.isDirectory() ) {
        printmsg( slashmsg, 0 );
      }

      if ( entry.isDirectory() ) {
        // directory ending
        for ( int i = strlen( entry.name()) ; i < 16 ; i++ ) {
          printmsg( spacemsg, 0 );
        }
        printmsg( dirextmsg , 0);
      }
      else {
        // file ending
        for ( int i = strlen( entry.name()) ; i < 17 ; i++ ) {
          printmsg( spacemsg , 0);
        }
        Terminal.print(" ");
        Terminal.print(int(entry.size()), DEC);
        Dateien++;
      }
      line_terminator();
      entry.close();
      ln++;

      if (ln == 15)
      { //nach 15 Zeilen auf Tastatur warten ->SPACE,ENTER=weiter, CTRL+C=EXIT
        if (wait_key() == 3) ex = 1;
        ln = 1;
      }
    }
    Terminal.println();
    printmsg( indentmsg, 0 );
    Terminal.print(Dateien, DEC);
    Terminal.println(" Files on SD-Card");
    printmsg( indentmsg, 0 );
    Terminal.printf("Total space: %lluMB\r\n", SD.totalBytes() / (1024 * 1024));
    printmsg( indentmsg, 0 );
    Terminal.printf("Used  space: %lluMB\r\n", SD.usedBytes() / (1024 * 1024));

    dir.close();
    sd_ende();                                             //SD-Card unmount
  }

  //--------------------------------------------- RENAME - Befehl -----------------------------------------------------------------------------------

  void renameFile(fs::FS &fs, const char * path1, const char * path2) {
    Terminal.printf("Renaming file %s to %s\n", path1, path2);
    line_terminator();
    spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

    if (fs.rename(path1, path2)) {
      Terminal.println("File renamed");
    } else {
      Terminal.println("Rename failed");
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
  //--------------------------------------------- Unterprogramm - Dateinamen extrahieren für RENAME - Befehl ---------------------------------------
  char * renameWord(void)
  {
    // SDL - I wasn't sure if this functionality existed above, so I figured i'd put it here
    char * ret = txtpos;
    int i = 0;
    expression_error = 0;

    if ( *ret == '\0' ) {                 //Leerstring
      expression_error = 1;
      return ret;
    }

    while (*ret != ',')                   //alter Dateiname
      path1[i++] = *ret++;
    path1[i] = 0;
    *ret++;
    i = 0;

    while (*ret != NL)                    //neuer Dateiname
      path2[i++] = *ret++;
    path2[i] = 0;
  }
  //--------------------------------------------- Unterprogramm - Dateiname extrahieren -------------------------------------------------------------
  /*
    char * filenameWord(void)
    {
      // SDL - I wasn't sure if this functionality existed above, so I figured i'd put it here
      char * ret = txtpos;
      expression_error = 0;

      // make sure there are no quotes or spaces, search for valid characters
      if (*txtpos != '"')   //Anführungszeichen ist Pflicht
      {
        expression_error = 1;
      }

      while ( !isValidFnChar( *txtpos )) txtpos++;
      ret = txtpos;
      // now, find the next nonfnchar
      txtpos++;
      while ( isValidFnChar( *txtpos )) txtpos++;
      if ( txtpos != ret ) *txtpos = '\0';
      if(*txtpos=='"') {
         txtpos = '\0';
        ret=txtpos;
        return ret;
      }
      // set the error code if we've got no string
      if ( *ret == '\0' ) {
        expression_error = 1;
      }

      return ret;
    }
  */
  //--------------------------------------------- Timer-Interrupt für Akku-Überwachung --------------------------------------------------------------
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
      Terminal.print("   * AKKU LOW!!! * ");
      Terminal.print(batterie, 2);
      Terminal.print(" V");
    }
  }


  //--------------------------------------------- SETUP-Datei ---------------------------------------------------------------------------------------

  void setup()
  {

    setCpuFrequencyMhz(240);                                                           //mit dieser Option gibt's Startschwierigkeiten

    EEPROM. begin ( EEPROM_SIZE ) ;
    delay(200);
    //################ Farbschema aus dem EEPROM lesen ############################
    Vordergrund = EEPROM.read(0) ;   //512 Byte Werte im EEPROM speicherbar
    Hintergrund = EEPROM.read(1);
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
      kSD_CS   = EEPROM.read(9);    //CS-Pin der SD-Karte ist fest auf GND
    }

    //--- ist der IIC_Marker (55) auf Platz 13 gesetzt, dann sind die folgenden Werte zu verwenden
    if (EEPROM.read(13) == 55) {
      SDA_RTC = EEPROM.read(11);
      SCL_RTC = EEPROM.read(12);
    }

    VGAController.queueSize = 400;
    PS2Controller.begin(PS2Preset::KeyboardPort0);

    //--- ist der KEY_Marker (66) auf Platz 15 gestzt, dann ist das gespeicherte Layout zu wählen
    if (EEPROM.read(15) == 66) {
      Keyboard_lang = EEPROM.read(14);
    }
    Set_Layout();           //Keyboard-Layout setzen

    // --- ist der Theme_marker (77) auf Platz 17 gesetzt, dann das gespeicherte Theme setzen
    if (EEPROM.read(17) == 77) {
      Theme_state = EEPROM.read(16);
      Theme_marker = true;
    }
    else Theme_state = 11;
    //************************************************************ welcher Bildschirmtreiber? *********************************************************
    // 64 colors
#ifdef AVOUT                                                                          //AV-Variante
VGAController.begin(VIDEOOUT_GPIO);
VGAController.setHorizontalRate(2);                                                   //320x240
VGAController.setResolution(MODES_STD[5]);                                            //5 scheint optimal
#else
VGAController.begin();                                                                //VGA-Variante //64 Farben
VGAController.setResolution(QVGA_320x240_60Hz);
//VGAController.setResolution(VGA_400x300_60Hz);
#endif
//***************************************************************************************************************************************************

    Terminal.begin(&VGAController);
    Terminal.connectLocally();                                                           // für Terminal Komandos

    set_font(fontsatz);                             // Fontsatz laden (1 Byte)
    Terminal.enableCursor(true);
    fbcolor(Vordergrund, Hintergrund);
    tc.setCursorPos(1, 1);
    GFX.clear();
    if (Theme_marker) set_theme(Theme_state);      //Theme setzen, wenn im EEprom gespeichert

    PS2Controller.keyboard()-> onVirtualKey = [&](VirtualKey * vk, bool keyDown) {
      if (*vk == VirtualKey::VK_ESCAPE) {
        if (keyDown) {
          Terminal.write(0x03);                   //ESC-Taste abfangen
        }
        *vk = VirtualKey::VK_NONE;
      }
    };
    //Serial.begin(kConsoleBaud);                                                           // open serial port

    // ein I2C-Interface definieren
    myI2C.begin(SDA_RTC, SCL_RTC, 400000); //400kHz

    rtc.begin(&myI2C);


    //-------------------------------- Akku-Überwachung per Timer0-Interrupt --------------------------------------------

    Akku_timer = timerBegin(0, 80, true);
    timerAttachInterrupt(Akku_timer, &onTimer, true);
    timerAlarmWrite(Akku_timer, 60000000, true);         //ca.60sek bis Interrupt ausgelöst wird
    //timerAlarmEnable(Akku_timer); //wenn alles fertig aufgebaut ist, diese Zeile aktivieren
    //-------------------------------------------------------------------------------------------------------------------

  }


  //################################################## Testbereich Ultraschall-Sensor ##################################################
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

  //############################################### Testbereich Dallas Temp-Sensor ######################################################
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

  //############################################### Testbereich DHT Temp/Humidity-Sensor ################################################

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

    //############################################### Testbereich MCP23017 ##############################################################
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
          for (i = 0; i < 8; i++) mcp.pinMode(i, INPUT_PULLUP); //alle pins von Port A auf Ausgang
        }
      }
      if (Port == 'B' || Port == 'C') {
        if (dir == 0) {
          for (i = 8; i < 16; i++) mcp.pinMode(i, OUTPUT); //alle pins von Port A auf Ausgang
        }
        else if (dir == 1) {
          for (i = 8; i < 16; i++) mcp.pinMode(i, INPUT_PULLUP); //alle pins von Port A auf Ausgang
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


    //############################################### Testbereich LCD ###################################################################

    static int LCD_Set(void) {
      char c;
      int bl, x, y, p, z;
      float a;

      HD44780LCD myLCD(LCD_ZEILEN, LCD_SPALTEN, LCD_ADRESSE, &myI2C);  // LCD object.rows ,cols ,PCF8574 I2C addr, Interface)
      myLCD.PCF8574_LCDBackLightSet(LCD_Backlight);
      c = spaces();

      if (c != '(')
      {
        return 1;
      }
      txtpos++;

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
            a = expression();
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

    //############################################### Testbereich Neopixel-LED ###############################################################
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

      //strip.setBrightness(LED_BRIGHTNESS);

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
          //Terminal.print(md);
          if (Test_char(',')) return 1;
          cnt = get_value();                //2.Parameter
          //Terminal.print(cnt);
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

    //############################################### externer EEPROM/FRAM #################################################################
    void writeEEPROM(int deviceaddress, word eeaddress, byte dat )
    {
      myI2C.beginTransmission(deviceaddress);
      myI2C.write((int)(highByte(eeaddress)));   // MSB
      myI2C.write((int)(lowByte(eeaddress)));    // LSB
      myI2C.write(dat);
      myI2C.endTransmission();
      if (deviceaddress == EEprom_ADDR)                           //bei EEprom muss etwas gewartet werden
        delay(10);
    }

    byte readEEPROM(int deviceaddress, word eeaddress )
    {
      myI2C.beginTransmission(deviceaddress);
      myI2C.write((int)(highByte(eeaddress)));   // MSB
      myI2C.write((int)(lowByte(eeaddress)));    // LSB
      myI2C.endTransmission();
      if (deviceaddress == EEprom_ADDR)
        delay(2);
      myI2C.requestFrom(deviceaddress, 1);
      while (myI2C.available() == 0);
      return myI2C.read();
      //Serial.println(myI2C.read(), HEX);
    }

    void WriteBuffer(int deviceaddress, word address, byte ln, byte* p_data)
    {
      myI2C.beginTransmission(deviceaddress);
      myI2C.write(highByte(address));
      myI2C.write(lowByte(address));
      //myI2C.write(p_data,ln);

      for (byte i = 0; i < ln; i++)
      {
        myI2C.write(p_data[i]);
      }

      myI2C.endTransmission();

      // Write cycle time (tWR). See EEPROM memory datasheet for more details.
      if (deviceaddress == EEprom_ADDR)                                         //bei EEprom muss etwas gewartet werden
        delay(10);
    }

    void readBuffer(int deviceaddress, word address, byte ln, byte* p_data)
    {
      myI2C.beginTransmission(deviceaddress);
      myI2C.write(highByte(address));
      myI2C.write(lowByte(address));
      myI2C.endTransmission();
      myI2C.requestFrom(deviceaddress, ln);
      for (byte i = 0; i < ln; i++)
      {
        if (myI2C.available())
        {
          p_data[i] = myI2C.read();
        }
      }
    }


    //############################################### Zeileneditor ###################################################################
    void Editor(int lnr) {

      int l = 0;
      int ln = 0;
      int dez = 0;

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
    { int digits = 0;
      int num, i;
      LINENUM line_num;

      line_num = *((LINENUM *)(list_line));
      list_line += sizeof(LINENUM) + sizeof(char);
      num = line_num;

      i = 0;
      do {
        pushb(num % 10 + '0');
        num = num / 10;
        digits++;
      }
      while (num > 0);

      while (digits > 0)
      {
        outchar(popb());
        digits--;
      }
      outchar(' ');

      while (*list_line != NL)
      {
        tempstring[i] = *list_line;
        list_line++;
        i++;
      }
      tempstring[i] = '\0';

    }

    //********************************************************** DIM-Befehl **************************************************************
    int Array_Dim(void) {

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
        x = abs(expression());
        if (*txtpos == ',') {
          txtpos++;
          y = abs(expression());
          if (*txtpos == ',') {
            txtpos++;
            z = abs(expression());
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
        //WriteBuffer(FRam_ADDR, ort, 6, p_data);
        Var_Neu_Platz += (grenze * len);

        return 0;
      }

      syntaxerror(syntaxmsg);
      return 1;
    }

    //----------------------------------------------------------- OPTION-Befehl -------------------------------------------------------------
    int Option(void) {
      byte p[6];
      scantable(options);                                                  //Optionstabelle lesen
      char fu = table_index;

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

        case OPT_IIC:
          p[0] = get_value();
          if (Test_char(',')) return 1;
          p[1] = get_value();
          EEPROM.write(11, p[0]);          //SDA im Flash speichern
          EEPROM.write(12, p[1]);          //SCL im Flash speichern
          EEPROM.write(13, IIC_SET);       //Marker, das Pinkonfiguration im EEprom abgelegt wurde
          EEPROM.commit () ;
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

    void SPI_RAM_write8(uint32_t addr, uint8_t value) {
      spi_fram.begin(3);
      spi_fram.writeEnable(true);
      spi_fram.write8(addr, value);
      spi_fram.writeEnable(false);
    }

    void SPI_RAM_write(uint32_t addr, const uint8_t *values, int count) {
      spi_fram.begin(3);
      spi_fram.writeEnable(true);
      spi_fram.write(addr, values, count);
      spi_fram.writeEnable(false);
    }

    uint8_t SPI_RAM_read8(uint32_t addr) {
      byte c;
      spi_fram.begin(3);
      c = spi_fram.read8(addr);
      return c;
    }
    void SPI_RAM_read(uint32_t addr, uint8_t *values, int count) {
      spi_fram.begin(3);
      spi_fram.read(addr, values, count);
    }

    void SPI_FRAM_init(void) {
      if (spi_fram.begin(3)) {
        Terminal.println("Found SPI FRAM");
      } else {
        Terminal.println("No SPI FRAM found\r\n");
      }
    }

    //**************************************************************** Seriell-Funktionen *****************************************************************************

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
          return 0;
          break;

        case 'T':                     //Transfer Programm zum PC
          if (ser_marker) {
            list_send = true;
            list_out();
            list_send = false;
            return 0;
          }
          syntaxerror(commsg);
          //return 0;
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

    //********************************************************** PIC-Befehl ****************************************************
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
          if (ad > pn - 1) ad = pn - 1;
          ad = ad * FRAM_PIC_OFFSET;                      //0..5 Bildspeicherplatz (320x240) bzw.0..2 (400x300)
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
          SPI_RAM_read(FRAM_OFFSET + ad, buf, 4);         //Dimension lesen
          px = buf[0] + (buf[1] << 8);
          py = buf[2] + (buf[3] << 8);
          ad += 4;
          for (y = dy + py - 1 ; y > dy - 1; y--) {
            for (x = dx; x < (dx + px); x++) {
              w = SPI_RAM_read8(FRAM_OFFSET + ad++);
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
          if (ad > 5) ad = 5;
          ad = ad * FRAM_PIC_OFFSET;                      //0..5 Bildspeicherplatz
          if (Test_char(',')) return 1;                   //Komma überspringen
          get_value();                                    //Dateiname in tempstring
          if (Test_char(')')) return 1;
          load_pic(FRAM_OFFSET + ad, tempstring);
          break;

        //****************************************************** PIC_S(PIC_Nr,Filename) ******************************************
        case 'S':                                         //Save PIC_RAW-Data
          if (Test_char('(')) return 1;
          ad = get_value();                               //Adresse im Speicher
          if (ad > 5) ad = 5;
          ad = ad * FRAM_PIC_OFFSET;                      //0..5 Bildspeicherplatz
          if (Test_char(',')) return 1;                   //Komma überspringen
          get_value();                                    //Dateiname in tempstring
          if (Test_char(')')) return 1;
          SPI_RAM_read(FRAM_OFFSET + ad, buf, 4);         //Dimension lesen
          px = buf[0] + (buf[1] << 8);
          py = buf[2] + (buf[3] << 8);
          n_bytes = (px * py) + 4;                        //x*y=Biddaten + 4 Bytes der Dimension
          save_pic(FRAM_OFFSET + ad, n_bytes, tempstring);
          break;

        //****************************************************** PIC_P(PIC_Nr,X,Y,XX,YY) ******************************************
        case 'P':                                         //Grafikbildschirm in FRAM speichern
          if (Test_char('(')) return 1;
          ad = get_value();
          if (ad > 5) ad = 5;
          ad = ad * FRAM_PIC_OFFSET;                      //0..5 Bildspeicherplatz
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
    /*
        void diashow_pic(long ad, int dx, int dy, int iv) {
          byte buf[4], w;
          int x, y, px, py, vv, vh;
          vv = GFX.getHeight();
          vh = GFX.getWidth();

          SPI_RAM_read(FRAM_OFFSET + ad, buf, 4);         //Dimension lesen
          px = buf[0] + (buf[1] << 8);
          py = buf[2] + (buf[3] << 8);

          ad += 4;
          switch (iv) {
            case 0:                                           //von unten nach oben
              for (y = dy + py - 1 ; y > dy - 1; y--) {
                for (x = dx; x < (dx + px); x++) {
                  w = SPI_RAM_read8(FRAM_OFFSET + ad++);
                  fcolor(w);
                  if (x < vh && y < vv) GFX.setPixel(x, y);
                }
              }
              break;

            case 1:                                           //von unten nach oben + swap Backcolor
              for (y = dy + py - 1 ; y > dy - 1; y--) {
                for (x = dx; x < (dx + px); x++) {
                  w = SPI_RAM_read8(FRAM_OFFSET + ad++);
                  fcolor(w);
                  if (x < vh && y < vv) GFX.setPixel(x, y);
                }
              }
              GFX.swapRectangle(dx, dy + 1, dx + px - 1, dy + py); //swap Backcolor
              break;

            case 2:                                           //von oben nach unten + swap Backcolor
              for (y = dy; y < (dy + py); y++) {
                for (x = dx + px - 1 ; x > dx-1 ; x--) {
                  w = SPI_RAM_read8(FRAM_OFFSET + FRAM_PIC_OFFSET - ad++);
                  fcolor(w);
                  if (x < vh && y < vv) GFX.setPixel(x, y);
                }

              }
              GFX.swapRectangle(dx, dy, dx + px, dy + py); //swap Backcolor

              break;

            default:                                          //von oben nach unten
              for (y = dy; y < (dy + py); y++) {
                for (x = dx + px - 1 ; x > dx-1 ; x--) {
                  w = SPI_RAM_read8(FRAM_OFFSET + FRAM_PIC_OFFSET - ad++);
                  fcolor(w);
                  if (x < vh && y < vv) GFX.setPixel(x, y);
                }
              }
              break;
          }
        }

    */
    //****************************************************** PIC_E(X,Y,XX,YY,Filename.bmp) ******************************************

    int export_pic(long x, long y, long xx, long yy, char *file) {
      byte i, r, g, b, cl;
      uint32_t pic_size, pic, weite, hoehe;
      //                    0     1    2      3    4     5    6    7    8    9    10    11    12  13   14    15   16   17    18    19    20   21   22    23   24  25   26    27    28   29    30  31   32   33   34   35     36   37
      byte bmp_header[54] = {0x42, 0x4D, 0x36, 0x84, 0x03, 0x0, 0x0, 0x0, 0x0, 0x0, 0x36, 0x00, 0x0, 0x0, 0x28, 0x0, 0x0, 0x0, 0x40, 0x01, 0x0, 0x0, 0xF0, 0x0, 0x0, 0x0, 0x01, 0x0, 0x18, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x2C, 0x01, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
      //                     B    M     |  ---  Size ---- |   |    Reseved     |   |   bfoffbits    |   |   bisize       |    |   Width        |    |   Height      |   |Planes| |BitCnt| |  Compress    |    |   size Img    |
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
        Terminal.print("File exist, overwrite? (y/n)");
        while (1)
        {
          k = wait_key();
          if (k == 'y' || k == 'n')
            break;
        }
        if (k == 'y') {
          SD.remove( String(sd_pfad) + String(tempstring));
          Terminal.print(k);
        }
        else
        {
          Terminal.print(k);
          Terminal.println();
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
        Terminal.print("File exist, overwrite? (y/n)");
        while (1)
        {
          k = wait_key();
          if (k == 'y' || k == 'n')
            break;
        }
        if (k == 'y') {
          SD.remove( String(sd_pfad) + String(tempstring));
          Terminal.print(k);
        }
        else
        {
          Terminal.print(k);
          Terminal.println();
          sd_ende();                                             //SD-Card unmount
          warmstart();
          return 0;
        }
      }

      fp = SD.open( String(sd_pfad) + String(file), FILE_WRITE);
      fp.close();
      sd_ende();                                                //SD-Card unmount

      for (int i = 0; i < durchlaeufe; i++) {
        SPI_RAM_read(adr, c, 1024);
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
        SPI_RAM_read(adr, c, rest);
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



    //------------------------------------- Testbereich SD-Update -----------------------------------------------------------------------------
    int load_binary(void) {

      expression_error = 0;
      get_value();

      if (expression_error)
      {
        syntaxerror(syntaxmsg);
        return 1;
      }

      spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

      if ( !SD.exists(String(sd_pfad) + String(tempstring)))
      {
        syntaxerror(sdfilemsg);
        sd_ende();
        return 1;
      }

      File updateBin = SD.open(String(sd_pfad) + String(tempstring));

      if (updateBin) {
        if (updateBin.isDirectory()) {
          Terminal.println("Error, " + String(tempstring) + " is not a file");
          updateBin.close();
          return 1;
        }

        size_t updateSize = updateBin.size();

        if (updateSize > 0) {
          Terminal.println("load " + String(tempstring));
          performUpdate(updateBin, updateSize);
        }
        else {
          Terminal.println("Error, file is empty");
        }
        updateBin.close();
      }
      else {
        Terminal.println("Could not load Binary from sd");
      }
    }

    // perform the actual update from a given stream
    void performUpdate(Stream &updateSource, size_t updateSize) {
      if (Update.begin(updateSize)) {
        size_t written = Update.writeStream(updateSource);
        if (written == updateSize) {
          Terminal.println("Written : " + String(written) + " successfully");
        }
        else {
          Terminal.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
        }
        if (Update.end()) {
          Terminal.println("OTA done!");
          if (Update.isFinished()) {
            Terminal.println("successfully completed. Now Rebooting.");
            delay(1000);
            ESP.restart();
          }
          else {
            Terminal.println("not finished? Something went wrong!");
          }
        }
        else {
          Terminal.println("Error Occurred. Error #: " + String(Update.getError()));
        }

      }
      else
      {
        Terminal.println("Not enough space to begin OTA");
      }
    }
