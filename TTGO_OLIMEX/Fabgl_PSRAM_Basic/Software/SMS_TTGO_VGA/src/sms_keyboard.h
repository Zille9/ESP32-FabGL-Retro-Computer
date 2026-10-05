// sms_keyboard.h
#ifndef SMS_KEYBOARD_H
#define SMS_KEYBOARD_H

#include <Arduino.h>
#include <fabgl.h>

class SMSKeyboard {
public:
    static void init();
    static uint8_t read();
    
    // SMS Button bitmask values (Botón 1 y Botón 2)
    enum SMSButton : uint8_t {
        SMS_UP       = 0x01,
        SMS_DOWN     = 0x02,
        SMS_LEFT     = 0x04,
        SMS_RIGHT    = 0x08,
        SMS_BTN1     = 0x10,  // Button 1 (Z)
        SMS_BTN2     = 0x20,  // Button 2 (X)
        SMS_START    = 0x40,  // ENTER
        SMS_PAUSE    = 0x80  // SPACE
        
    };

private:
    static fabgl::PS2Controller ps2;
    static fabgl::Keyboard keyboard;
    static uint8_t currentState;
    static void updateState();
};

#endif