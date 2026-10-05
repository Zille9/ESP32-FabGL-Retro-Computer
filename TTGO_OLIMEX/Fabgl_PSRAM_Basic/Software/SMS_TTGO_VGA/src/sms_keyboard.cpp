// sms_keyboard.cpp
#include "sms_keyboard.h"
#include "../config.h"
bool backto = false;  //Marker fuer OTA-Funktion (F12)
fabgl::PS2Controller SMSKeyboard::ps2;
fabgl::Keyboard SMSKeyboard::keyboard;
uint8_t SMSKeyboard::currentState = 0;


void SMSKeyboard::init() {
    #ifdef DEBUG
        Serial.println("SMS Keyboard: Initializing PS/2...");
    #endif
    
    ps2.begin((gpio_num_t)PS2_CLK_PIN, (gpio_num_t)PS2_DAT_PIN);
    keyboard.begin(true, false, 0);
    
    #ifdef DEBUG
        Serial.println("SMS Keyboard: Ready");
        Serial.println("  Arrow Keys - D-Pad");
        Serial.println("  Z / ENTER - Button 1 / Start (select)");
        Serial.println("  X - Button 2");
        Serial.println("  SPACE - Pause");
        Serial.println("  ESC - Exit game / Reset system");
    #endif
}

void SMSKeyboard::updateState() {
    currentState = 0;
    
    // D-Pad
    if (keyboard.isVKDown(fabgl::VirtualKey::VK_UP))    currentState |= SMS_UP;
    if (keyboard.isVKDown(fabgl::VirtualKey::VK_DOWN))  currentState |= SMS_DOWN;
    if (keyboard.isVKDown(fabgl::VirtualKey::VK_LEFT))  currentState |= SMS_LEFT;
    if (keyboard.isVKDown(fabgl::VirtualKey::VK_RIGHT)) currentState |= SMS_RIGHT;
    
    // Button 1 (Z)
    if (keyboard.isVKDown(fabgl::VirtualKey::VK_Z) || 
        keyboard.isVKDown(fabgl::VirtualKey::VK_z)) currentState |= SMS_BTN1;
    
    // Button 2 (X)
    if (keyboard.isVKDown(fabgl::VirtualKey::VK_X) || 
        keyboard.isVKDown(fabgl::VirtualKey::VK_x)) currentState |= SMS_BTN2;
    
    // Start (ENTER)
    if (keyboard.isVKDown(fabgl::VirtualKey::VK_RETURN)) currentState |= SMS_START;
    
    // Pause (SPACE)
    if (keyboard.isVKDown(fabgl::VirtualKey::VK_SPACE))  currentState |= SMS_PAUSE;
    
    // F12 (Return to Basic)
    //if (keyboard.isVKDown(fabgl::VirtualKey::VK_F12))  currentState |= SMS_BACK;
    // F12 (Return to Basic) - Genau wie ESC als Aktion ausführen:
    static bool lastF12 = false;
    bool currentF12 = keyboard.isVKDown(fabgl::VirtualKey::VK_F12);
    if (currentF12 && !lastF12) {
        #ifdef DEBUG
            Serial.println("F12 pressed - Returning to Basic...");
        #endif
        
        // HIER DEINEN CODE EINFÜGEN, DER ZUM BASIC ZURÜCKKEHRT!
        // Beispiel: returnToBasic(); oder ein Flag setzen:
        backto = true; 
    }
    lastF12 = currentF12;


    // ESC - exit game (reset system)
    static bool lastEsc = false;
    bool currentEsc = keyboard.isVKDown(fabgl::VirtualKey::VK_ESCAPE);
    if (currentEsc && !lastEsc) {
        #ifdef DEBUG
            Serial.println("ESC pressed - Resetting system...");
        #endif
        ESP.restart();
    }
    lastEsc = currentEsc;
}

uint8_t SMSKeyboard::read() {
    updateState();
    return currentState;
}