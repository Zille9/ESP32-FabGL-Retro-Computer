#pragma once

#include <Arduino.h>

// --- 1. Keyboard Types ---
enum class KeyboardLayout {
    JAPANESE,
    US_ENGLISH,
    PORTUGUESE, // Brazil / Portugal
    SPANISH,     // Spain / Latin America
    GERMAN
};

// Global key_map array
inline uint8_t key_map[256];

// --- 2. Function Prototypes ---
void initKeyMap(KeyboardLayout layout);
void setJapaneseKeyMap();
void setUSKeyMap();
void setPortugueseKeyMap();
void setSpanishKeyMap();
void setGermanKeyMap();
// --- 3. Implementations ---

/**
 * Initialize key map based on the selected layout.
 */
inline void initKeyMap(KeyboardLayout layout) {
    memset(key_map, 0xFF, 256);

    switch (layout) {
        case KeyboardLayout::JAPANESE:
            setJapaneseKeyMap();
            break;
        case KeyboardLayout::US_ENGLISH:
            setUSKeyMap();
            break;
        case KeyboardLayout::PORTUGUESE:
            setPortugueseKeyMap();
            break;
    	case KeyboardLayout::SPANISH:
            setSpanishKeyMap();
            break;
      case KeyboardLayout::GERMAN:
            setGermanKeyMap();
            break;
        default:
            setJapaneseKeyMap();
            break;
    }
}

/**
 * Japanese MSX Keyboard Layout (JP)
 */
inline void setJapaneseKeyMap() {
    // Row 0 (0-7)
    key_map[0x45] = 0x00;  // 0
    key_map[0x16] = 0x01;  // 1
    key_map[0x1E] = 0x02;  // 2
    key_map[0x26] = 0x03;  // 3
    key_map[0x25] = 0x04;  // 4
    key_map[0x2E] = 0x05;  // 5
    key_map[0x36] = 0x06;  // 6
    key_map[0x3D] = 0x07;  // 7

    // Row 1 (8, 9, -, ^, \, @, [, ;)
    key_map[0x3E] = 0x10;  // 8
    key_map[0x46] = 0x11;  // 9
    key_map[0x4E] = 0x12;  // -
    key_map[0x55] = 0x13;  // ^
    key_map[0x6A] = 0x14;  // \ (Ro)
    key_map[0x54] = 0x15;  // @
    key_map[0x5B] = 0x16;  // [
    key_map[0x4C] = 0x17;  // ;

    // Row 2 (:, ], ,, ., /, _, A, B)
    key_map[0x52] = 0x20;  // :
    key_map[0x5D] = 0x21;  // ]
    key_map[0x41] = 0x22;  // ,
    key_map[0x49] = 0x23;  // .
    key_map[0x4A] = 0x24;  // /
    key_map[0x51] = 0x25;  // _ (Not 0x51, needs fix)
    key_map[0x1C] = 0x26;  // A
    key_map[0x32] = 0x27;  // B

    // Row 3-5 (C-Z)
    key_map[0x21] = 0x30; key_map[0x23] = 0x31; key_map[0x24] = 0x32;
    key_map[0x2B] = 0x33; key_map[0x34] = 0x34; key_map[0x33] = 0x35;
    key_map[0x43] = 0x36; key_map[0x3B] = 0x37; // Row 3
    key_map[0x42] = 0x40; key_map[0x4B] = 0x41; key_map[0x3A] = 0x42;
    key_map[0x31] = 0x43; key_map[0x44] = 0x44; key_map[0x4D] = 0x45;
    key_map[0x15] = 0x46; key_map[0x2D] = 0x47; // Row 4
    key_map[0x1B] = 0x50; key_map[0x2C] = 0x51; key_map[0x3C] = 0x52;
    key_map[0x2A] = 0x53; key_map[0x1D] = 0x54; key_map[0x22] = 0x55;
    key_map[0x35] = 0x56; key_map[0x1A] = 0x57; // Row 5

    // Row 6 (System Keys)
    key_map[0x12] = 0x60;  // L-SHIFT
    key_map[0x59] = 0x60;  // R-SHIFT
    key_map[0x14] = 0x61;  // CTRL
    key_map[0x11] = 0x62;  // L-ALT (GRAPH)
    key_map[0x58] = 0x63;  // CAPS
    key_map[0x13] = 0x64;  // KANA
    key_map[0x05] = 0x65;  // F1
    key_map[0x06] = 0x66;  // F2
    key_map[0x04] = 0x67;  // F3

    // Row 7
    key_map[0x0C] = 0x70;  // F4
    key_map[0x03] = 0x71;  // F5
    key_map[0x76] = 0x72;  // ESC
    key_map[0x0D] = 0x73;  // TAB
    key_map[0x0E] = 0x74;  // STOP
    key_map[0x66] = 0x75;  // BS
    key_map[0x5A] = 0x77;  // RETURN

    // Row 8 (Space)
    key_map[0x29] = 0x80;  // SPACE

    // Numeric Keypad
    key_map[0x70] = 0x93; key_map[0x69] = 0x94; key_map[0x72] = 0x95;
    key_map[0x7A] = 0x96; key_map[0x6B] = 0x97; key_map[0x73] = 0xA0;
    key_map[0x74] = 0xA1; key_map[0x6C] = 0xA2; key_map[0x75] = 0xA3;
    key_map[0x7D] = 0xA4; 
    key_map[0x7C] = 0x20; // KP *
    key_map[0x7B] = 0xA5; // KP -
    key_map[0x79] = 0x21; // KP +
    key_map[0x71] = 0xA6; // KP .
}

/**
 * US MSX Layout (Placeholder)
 * Symbols like @, [, ], ;, : are in different positions.
 */
inline void setUSKeyMap() {
    setJapaneseKeyMap(); // Start with base
	// --- Row 1 Re-mapping ---
    key_map[0x4E] = 0x12;  // -
    key_map[0x55] = 0x13;  // = (Matches MSX US Row 1, Bit 3)
    key_map[0x5D] = 0x14;  // \ (Matches MSX US Row 1, Bit 4)
    key_map[0x54] = 0x15;  // [
    key_map[0x5B] = 0x16;  // ]
    key_map[0x4C] = 0x17;  // ;

    // --- Row 2 Re-mapping ---
    key_map[0x52] = 0x20;  // ' (Single quote)
    key_map[0x0E] = 0x21;  // ` (Backtick)
    key_map[0x41] = 0x22;  // ,
    key_map[0x49] = 0x23;  // .
    key_map[0x4A] = 0x24;  // /

    // --- Functionality tweaks ---
    key_map[0x13] = 0xFF;  // Disable KANA for US layout
}

inline void setGermanKeyMap() {
    setJapaneseKeyMap(); // Start mit der Basis (oder setUSKeyMap, falls nötig)

    // --- Buchstaben-Tausch (QWERTY -> QWERTZ) ---
    key_map[0x1A] = 0x35;  // Z wird zu Y
    key_map[0x35] = 0x1A;  // Y wird zu Z

    // --- Reihe 1 Re-mapping (Zahlen & Sonderzeichen oben) ---
    key_map[0x4E] = 0x12;  // ß / ?
    key_map[0x55] = 0x13;  // ` / ´ (Akzent-Taste neben Backspace)
    key_map[0x5D] = 0x14;  // ü / Ü
    key_map[0x54] = 0x15;  // + / *

    // --- Reihe 2 Re-mapping (Mittlere Reihe / Umlaute) ---
    key_map[0x4C] = 0x17;  // ö / Ö
    key_map[0x52] = 0x20;  // ä / Ä
    key_map[0x5B] = 0x16;  // # / '

    // --- Reihe 3 Re-mapping (Untere Reihe / Satzzeichen) ---
    key_map[0x0E] = 0x21;  // < / > (Taste neben Shift)
    key_map[0x41] = 0x22;  // , / ;
    key_map[0x49] = 0x23;  // . / :
    key_map[0x4A] = 0x24;  // - / _

    // --- Funktionstasten-Tweaks ---
    key_map[0x13] = 0xFF;  // KANA-Taste für deutsches Layout deaktivieren
}

/**
 * Portuguese/Brazil Layout (Placeholder)
 * Popular in Brazil (Gradiente/Sharp). Needs Accents support.
 */
inline void setPortugueseKeyMap() {
   setUSKeyMap(); // Start with US base as they share many similarities

    // --- Row 1 Re-mapping ---
    key_map[0x4E] = 0x12;  // -
    key_map[0x55] = 0x13;  // = 
    key_map[0x5D] = 0x14;  // [ (On many BR MSX, this is the accent key row)
    key_map[0x54] = 0x15;  // @
    key_map[0x5B] = 0x16;  // ]
    key_map[0x4C] = 0x17;  // ;

    // --- Row 2 Re-mapping ---
    key_map[0x52] = 0x20;  // ~ / ^ (Dead key on BR layout)
    key_map[0x0E] = 0x21;  // ' / " (Dead key)
    key_map[0x41] = 0x22;  // ,
    key_map[0x49] = 0x23;  // .
    key_map[0x4A] = 0x24;  // /
    
    // --- Row 9 (Special Brazilian Keys) ---
    // In MSX Matrix, Row 9 is used for the Ç and accented characters
    key_map[0x4D] = 0x90;  // P -> Bit 0 (usually standard)
    key_map[0x33] = 0x91;  // Ç (On PS/2, usually the ';' or '[' key depending on region)
    
    // Note: PS/2 scancode for Ç on a Brazilian ABNT2 keyboard is 0x27
    key_map[0x27] = 0x91;  // Ç (Cedilha) mapped to Row 9, Bit 1
    
    key_map[0x13] = 0xFF;  // Ensure KANA is disabled setJapaneseKeyMap(); // Start with base
}

/**
 * Spanish MSX Layout (Spain / Latin America)
 * Includes mapping for the 'Ñ' key and specific punctuation.
 * Based on the standard Spanish MSX keyboard matrix.
 */
inline void setSpanishKeyMap() {
    setUSKeyMap(); // Start with US base

    // --- Row 1 Re-mapping ---
    key_map[0x4E] = 0x12;  // -
    key_map[0x55] = 0x13;  // =
    key_map[0x5D] = 0x14;  // \ (Often used for 'Ç' or accents in Spain)
    key_map[0x54] = 0x15;  // [
    key_map[0x5B] = 0x16;  // ]
    key_map[0x4C] = 0x17;  // ;

    // --- Row 2 Re-mapping ---
    key_map[0x52] = 0x20;  // ' (Apostrophe / Accent)
    key_map[0x0E] = 0x21;  // ` (Backtick)
    key_map[0x41] = 0x22;  // ,
    key_map[0x49] = 0x23;  // .
    key_map[0x4A] = 0x24;  // /

    // --- Row 9 (Special Spanish Keys) ---
    // The 'Ñ' key is the heart of the Spanish layout.
    // On a physical Spanish PS/2 keyboard, 'Ñ' is at scancode 0x4C (where ';' is on US).
    key_map[0x4C] = 0x91;  // Ñ mapped to MSX Row 9, Bit 1

    // Additional symbols often found on Spanish MSX
    key_map[0x61] = 0x92;  // < > key (if present on the keyboard)

    key_map[0x13] = 0xFF;  // Ensure KANA is disabled
}
