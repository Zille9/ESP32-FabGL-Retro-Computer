// Classic99 for ESP32 - PS/2 keyboard to TI-99/4A key matrix
// Matrix layout from Classic99 Tiemul.cpp KEYS[1] (C) Mike Brent aka Tursi.
// See the original licence there. Not for distribution without the author's permission.
//
// FabGL already resolves shift and layout (VK_a vs VK_A, VK_AT, ...), so each PC key is
// mapped to one TI key plus the SHIFT or FCTN it needs. Physical Ctrl/Alt pass through
// as TI CTRL/FCTN.

#include <Arduino.h>
#include "keyboard_ti.h"
#include "bus.h"

using fabgl::VirtualKey;

// TI keys as (column << 3) | row, in Classic99's column numbering (see rcru)
#define K(col, row) (uint8_t)(((col) << 3) | (row))
enum : uint8_t {
    TI_M = K(1,0), TI_J = K(1,1), TI_U = K(1,2), TI_7 = K(1,3), TI_4 = K(1,4), TI_F = K(1,5), TI_R = K(1,6), TI_V = K(1,7),
    TI_SLASH = K(2,0), TI_SEMI = K(2,1), TI_P = K(2,2), TI_0 = K(2,3), TI_1 = K(2,4), TI_A = K(2,5), TI_Q = K(2,6), TI_Z = K(2,7),
    TI_PERIOD = K(3,0), TI_L = K(3,1), TI_O = K(3,2), TI_9 = K(3,3), TI_2 = K(3,4), TI_S = K(3,5), TI_W = K(3,6), TI_X = K(3,7),
    TI_COMMA = K(5,0), TI_K = K(5,1), TI_I = K(5,2), TI_8 = K(5,3), TI_3 = K(5,4), TI_D = K(5,5), TI_E = K(5,6), TI_C = K(5,7),
    TI_N = K(6,0), TI_H = K(6,1), TI_Y = K(6,2), TI_6 = K(6,3), TI_5 = K(6,4), TI_G = K(6,5), TI_T = K(6,6), TI_B = K(6,7),
    TI_EQUALS = K(7,0), TI_SPACE = K(7,1), TI_ENTER = K(7,2), TI_FCTN = K(7,4), TI_SHIFT = K(7,5), TI_CTRL = K(7,6),
    TI_NONE = 0xff
};

enum : uint8_t { MOD_NONE = 0, MOD_SHIFT = 1, MOD_FCTN = 2, MOD_ARROW = 4 };

struct TiKey { uint8_t key; uint8_t mod; };

static const uint8_t letters[26] = {
    TI_A, TI_B, TI_C, TI_D, TI_E, TI_F, TI_G, TI_H, TI_I, TI_J, TI_K, TI_L, TI_M,
    TI_N, TI_O, TI_P, TI_Q, TI_R, TI_S, TI_T, TI_U, TI_V, TI_W, TI_X, TI_Y, TI_Z
};
static const uint8_t digits[10] = { TI_0, TI_1, TI_2, TI_3, TI_4, TI_5, TI_6, TI_7, TI_8, TI_9 };

static TiKey mapKey(VirtualKey vk) {
    if (vk >= VirtualKey::VK_a && vk <= VirtualKey::VK_z) return { letters[vk - VirtualKey::VK_a], MOD_NONE };
    if (vk >= VirtualKey::VK_A && vk <= VirtualKey::VK_Z) return { letters[vk - VirtualKey::VK_A], MOD_SHIFT };
    if (vk >= VirtualKey::VK_0 && vk <= VirtualKey::VK_9) return { digits[vk - VirtualKey::VK_0], MOD_NONE };
    if (vk >= VirtualKey::VK_KP_0 && vk <= VirtualKey::VK_KP_9) return { digits[vk - VirtualKey::VK_KP_0], MOD_NONE };
    if (vk >= VirtualKey::VK_F1 && vk <= VirtualKey::VK_F9) return { digits[1 + (vk - VirtualKey::VK_F1)], MOD_FCTN };

    switch (vk) {
        case VirtualKey::VK_SPACE:       return { TI_SPACE, MOD_NONE };
        case VirtualKey::VK_RETURN:
        case VirtualKey::VK_KP_ENTER:    return { TI_ENTER, MOD_NONE };
        case VirtualKey::VK_EQUALS:      return { TI_EQUALS, MOD_NONE };
        case VirtualKey::VK_PLUS:
        case VirtualKey::VK_KP_PLUS:     return { TI_EQUALS, MOD_SHIFT };
        case VirtualKey::VK_PERIOD:
        case VirtualKey::VK_KP_PERIOD:   return { TI_PERIOD, MOD_NONE };
        case VirtualKey::VK_GREATER:     return { TI_PERIOD, MOD_SHIFT };
        case VirtualKey::VK_COMMA:       return { TI_COMMA, MOD_NONE };
        case VirtualKey::VK_LESS:        return { TI_COMMA, MOD_SHIFT };
        case VirtualKey::VK_SEMICOLON:   return { TI_SEMI, MOD_NONE };
        case VirtualKey::VK_COLON:       return { TI_SEMI, MOD_SHIFT };
        case VirtualKey::VK_SLASH:
        case VirtualKey::VK_KP_DIVIDE:   return { TI_SLASH, MOD_NONE };
        case VirtualKey::VK_MINUS:
        case VirtualKey::VK_KP_MINUS:    return { TI_SLASH, MOD_SHIFT };
        case VirtualKey::VK_EXCLAIM:     return { TI_1, MOD_SHIFT };
        case VirtualKey::VK_AT:          return { TI_2, MOD_SHIFT };
        case VirtualKey::VK_HASH:        return { TI_3, MOD_SHIFT };
        case VirtualKey::VK_DOLLAR:      return { TI_4, MOD_SHIFT };
        case VirtualKey::VK_PERCENT:     return { TI_5, MOD_SHIFT };
        case VirtualKey::VK_CARET:       return { TI_6, MOD_SHIFT };
        case VirtualKey::VK_AMPERSAND:   return { TI_7, MOD_SHIFT };
        case VirtualKey::VK_ASTERISK:
        case VirtualKey::VK_KP_MULTIPLY: return { TI_8, MOD_SHIFT };
        case VirtualKey::VK_LEFTPAREN:   return { TI_9, MOD_SHIFT };
        case VirtualKey::VK_RIGHTPAREN:  return { TI_0, MOD_SHIFT };
        // FCTN symbols
        case VirtualKey::VK_QUOTEDBL:    return { TI_P, MOD_FCTN };
        case VirtualKey::VK_QUOTE:       return { TI_O, MOD_FCTN };
        case VirtualKey::VK_QUESTION:    return { TI_I, MOD_FCTN };
        case VirtualKey::VK_UNDERSCORE:  return { TI_U, MOD_FCTN };
        case VirtualKey::VK_VERTICALBAR: return { TI_A, MOD_FCTN };
        case VirtualKey::VK_GRAVEACCENT: return { TI_C, MOD_FCTN };
        case VirtualKey::VK_LEFTBRACE:   return { TI_F, MOD_FCTN };
        case VirtualKey::VK_RIGHTBRACE:  return { TI_G, MOD_FCTN };
        case VirtualKey::VK_LEFTBRACKET: return { TI_R, MOD_FCTN };
        case VirtualKey::VK_RIGHTBRACKET:return { TI_T, MOD_FCTN };
        case VirtualKey::VK_TILDE:       return { TI_W, MOD_FCTN };
        case VirtualKey::VK_BACKSLASH:   return { TI_Z, MOD_FCTN };
        // editing keys
        case VirtualKey::VK_BACKSPACE:   return { TI_S, MOD_FCTN };     // left
        case VirtualKey::VK_DELETE:      return { TI_1, MOD_FCTN };     // DEL
        case VirtualKey::VK_INSERT:      return { TI_2, MOD_FCTN };     // INS
        case VirtualKey::VK_ESCAPE:      return { TI_9, MOD_FCTN };     // BACK
        case VirtualKey::VK_F10:         return { TI_EQUALS, MOD_FCTN };// QUIT
        // arrows: FCTN+E/S/D/X, unless a program is reading joystick 1
        case VirtualKey::VK_UP:          return { TI_E, MOD_FCTN | MOD_ARROW };
        case VirtualKey::VK_LEFT:        return { TI_S, MOD_FCTN | MOD_ARROW };
        case VirtualKey::VK_RIGHT:       return { TI_D, MOD_FCTN | MOD_ARROW };
        case VirtualKey::VK_DOWN:        return { TI_X, MOD_FCTN | MOD_ARROW };
        default:                         return { TI_NONE, MOD_NONE };
    }
}

// keys currently held, tracked by physical key so a release matches its press even if
// shift changed in between (VK_a pressed, VK_A released)
struct Held { uint16_t id; TiKey ti; VirtualKey vk; };

// PS/2 set 2: press "xx" or "E0 xx", release "F0 xx" or "E0 F0 xx" -> one id per physical key
static uint16_t physicalKeyId(const uint8_t *sc) {
    int i = 0;
    uint16_t ext = 0;
    if (sc[i] == 0xE0) { ext = 0x100; i++; }
    if (sc[i] == 0xF0) i++;
    return ext | sc[i];
}
static Held held[8];
static int nHeld = 0;
static bool physShift = false, physAlt = false, physCtrl = false;
static bool altGr = false;     // right Alt belongs to the keyboard layout, not to FCTN

static void setKey(Byte *m, uint8_t k) {
    m[k >> 3] |= (1 << (k & 7));
}

static void rebuild() {
    Byte m[8] = {0}, mNoArrows[8] = {0};
    Byte j = 0;
    bool needShift = false, needFctn = false, anyKey = false, anyNonArrow = false;

    for (int i = 0; i < nHeld; i++) {
        const TiKey &t = held[i].ti;
        switch (held[i].vk) {
            case VirtualKey::VK_LEFT:  j |= 0x02; break;
            case VirtualKey::VK_RIGHT: j |= 0x04; break;
            case VirtualKey::VK_DOWN:  j |= 0x08; break;
            case VirtualKey::VK_UP:    j |= 0x10; break;
            case VirtualKey::VK_TAB:   j |= 0x01; break;
            default: break;
        }
        if (t.key == TI_NONE) continue;
        anyKey = true;
        setKey(m, t.key);
        if (t.mod & MOD_SHIFT) needShift = true;
        if (t.mod & MOD_FCTN) needFctn = true;
        if (!(t.mod & MOD_ARROW)) {
            anyNonArrow = true;
            setKey(mNoArrows, t.key);
            if (t.mod & MOD_FCTN) setKey(mNoArrows, TI_FCTN);
            if (t.mod & MOD_SHIFT) setKey(mNoArrows, TI_SHIFT);
        }
    }

    // a lone Shift reaches the TI; with a mapped key, the mapping decides
    if (needShift || (physShift && !anyKey)) setKey(m, TI_SHIFT);
    if (needFctn || physAlt) setKey(m, TI_FCTN);
    if (physCtrl) setKey(m, TI_CTRL);
    if (physShift && !anyNonArrow) setKey(mNoArrows, TI_SHIFT);
    if (physAlt) setKey(mNoArrows, TI_FCTN);
    if (physCtrl) setKey(mNoArrows, TI_CTRL);

    for (int c = 0; c < 8; c++) {
        kbMatrix[c] = m[c];
        kbMatrixNoArrows[c] = mNoArrows[c];
    }
    joy1 = j;
}

bool tiKeyEvent(const fabgl::VirtualKeyItem &item) {
    VirtualKey vk = item.vk;

    if (vk == VirtualKey::VK_F12) return false;

    switch (vk) {
        case VirtualKey::VK_LSHIFT: case VirtualKey::VK_RSHIFT:
            physShift = item.down; rebuild(); return true;
        case VirtualKey::VK_RALT:
            if (altGr) return true;
            // fall through
        case VirtualKey::VK_LALT:
            physAlt = item.down; rebuild(); return true;
        case VirtualKey::VK_LCTRL: case VirtualKey::VK_RCTRL:
            physCtrl = item.down; rebuild(); return true;
        case VirtualKey::VK_CAPSLOCK:
            if (item.down) alphaLockDown = !alphaLockDown;
            return true;
        default:
            break;
    }

    uint16_t id = physicalKeyId(item.scancode);
    int idx = -1;
    for (int i = 0; i < nHeld; i++) {
        if (held[i].id == id) { idx = i; break; }
    }

    if (item.down) {
        if (idx < 0 && nHeld < 8) {
            held[nHeld++] = { id, mapKey(vk), vk };
        }
    } else if (idx >= 0) {
        held[idx] = held[--nHeld];
    }
    rebuild();
    return true;
}

void tiKeySetAltGr(bool on) {
    altGr = on;
}

void tiKeyReleaseAll() {
    nHeld = 0;
    physShift = physAlt = physCtrl = false;
    rebuild();
}

/////////////////////////////////////////////////////////
// Typed input (serial "type" command): an overlay matrix OR'd into keyboard reads.
// Each character is held 4 frames and released 4 frames so KSCAN sees separate presses.
/////////////////////////////////////////////////////////
static char typeBuf[256];
static volatile int typeHead = 0, typeTail = 0;
static int typePhase = 0;

static TiKey asciiToTi(char c) {
    if (c >= 'a' && c <= 'z') return { letters[c - 'a'], MOD_NONE };
    if (c >= 'A' && c <= 'Z') return { letters[c - 'A'], MOD_NONE };    // alpha lock gives upper case
    if (c >= '0' && c <= '9') return { digits[c - '0'], MOD_NONE };
    switch (c) {
        case ' ': return { TI_SPACE, MOD_NONE };
        case '|': return { TI_ENTER, MOD_NONE };
        case '=': return { TI_EQUALS, MOD_NONE };
        case '+': return { TI_EQUALS, MOD_SHIFT };
        case '.': return { TI_PERIOD, MOD_NONE };
        case '>': return { TI_PERIOD, MOD_SHIFT };
        case ',': return { TI_COMMA, MOD_NONE };
        case '<': return { TI_COMMA, MOD_SHIFT };
        case ';': return { TI_SEMI, MOD_NONE };
        case ':': return { TI_SEMI, MOD_SHIFT };
        case '/': return { TI_SLASH, MOD_NONE };
        case '-': return { TI_SLASH, MOD_SHIFT };
        case '!': return { TI_1, MOD_SHIFT };
        case '@': return { TI_2, MOD_SHIFT };
        case '#': return { TI_3, MOD_SHIFT };
        case '$': return { TI_4, MOD_SHIFT };
        case '*': return { TI_8, MOD_SHIFT };
        case '(': return { TI_9, MOD_SHIFT };
        case ')': return { TI_0, MOD_SHIFT };
        case '"': return { TI_P, MOD_FCTN };
        case '\'': return { TI_O, MOD_FCTN };
        case '?': return { TI_I, MOD_FCTN };
        case '~': return { TI_9, MOD_FCTN };    // BACK
        default:  return { TI_NONE, MOD_NONE };
    }
}

void tiType(const char *text) {
    for (; *text; text++) {
        int next = (typeHead + 1) % sizeof(typeBuf);
        if (next == typeTail) break;
        typeBuf[typeHead] = *text;
        typeHead = next;
    }
}

void tiTypeFrame() {
    if (typePhase > 0) {
        if (--typePhase == 4) {
            for (int c = 0; c < 8; c++) typedMatrix[c] = 0;     // release after 4 frames
        }
        return;
    }
    if (typeTail == typeHead) return;
    TiKey t = asciiToTi(typeBuf[typeTail]);
    typeTail = (typeTail + 1) % sizeof(typeBuf);
    Byte m[8] = {0};
    if (t.key != TI_NONE) {
        setKey(m, t.key);
        if (t.mod & MOD_SHIFT) setKey(m, TI_SHIFT);
        if (t.mod & MOD_FCTN) setKey(m, TI_FCTN);
    }
    for (int c = 0; c < 8; c++) typedMatrix[c] = m[c];
    typePhase = 8;
}
