 /*!
	@file     HelloWorld.ino
	@author   Gavin Lyons
	@brief 
		 Hello World for HD44780_LCD_PCF8574 arduino library 
*/

// Section: Included library
#include "HD44780_LCD_PCF8574.h"

// Section: Defines
#define DISPLAY_DELAY_INIT 50 // mS

HD44780LCD myLCD(2, 16, 0x27, &Wire); // instantiate an object

// Section: Setup

void setup() {
  delay(DISPLAY_DELAY_INIT);
  myLCD.PCF8574_LCDInit(myLCD.LCDCursorTypeOn);
  myLCD.PCF8574_LCDClearScreen();
  myLCD.PCF8574_LCDBackLightSet(true);
  myLCD.PCF8574_LCDGOTO(myLCD.LCDLineNumberOne, 0);
}

// Section: Main Loop

void loop() {
  char testString[] = "Hello World";
  myLCD.PCF8574_LCDSendString(testString);
  myLCD.PCF8574_LCDSendChar('!');  // Display a single character
  while (true) {};
}

// EOF
