# ESP32-Basic
ESP-Basic+
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
//      SD-Card 14, 16, 17, 13 (SCK, MISO, MOSI, CS)    ESP32-Eigenboard   ->CS dauerhaft auf GND                                                 //
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
