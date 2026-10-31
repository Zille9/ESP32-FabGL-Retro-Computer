// Classic99 for ESP32 - PS/2 keyboard layouts selectable from the Setup menu
//
// FabGL translates scancodes to virtual keys through the active layout, and
// keyboard_ti.cpp maps those virtual keys to the TI matrix, so switching the
// layout here is all that is needed.
//
// FabGL reads the dead-key tables from the active layout only (they are not
// inherited), so every layout below leaves them empty: accents such as ^ ` ~
// are typed directly instead of waiting for a following vowel the TI cannot
// show anyway.

#include <Arduino.h>
#include "fabgl.h"
#include "kbd_layouts.h"
#include "ti_types.h"
#include "keyboard_ti.h"

using namespace fabgl;

// Spain: FabGL's layout without its dead keys
static const KeyboardLayout SpainLayout = {
    "ES", "Spanish (Spain)", &SpanishLayout,
    {}, {}, {}, {}, {},
};

// Latin America
static const KeyboardLayout LatamLayout = {
    "LA", "Spanish (Latam)", &USLayout,
    {
        { 0x0E, VK_VERTICALBAR },
        { 0x4E, VK_QUOTE },
        { 0x55, VK_QUESTION_INV },
        { 0x54, VK_ACUTEACCENT },
        { 0x5B, VK_PLUS },
        { 0x4C, VK_TILDE_n },
        { 0x52, VK_LEFTBRACE },
        { 0x5D, VK_RIGHTBRACE },
        { 0x61, VK_LESS },
        { 0x4A, VK_MINUS },
    },
    {},
    //  in_key, { CTRL, LALT, RALT, SHIFT }, out_key
    {
        { VK_VERTICALBAR,  { 0, 0, 0, 1 }, VK_DEGREE },
        { VK_VERTICALBAR,  { 0, 0, 1, 0 }, VK_NEGATION },
        { VK_2,            { 0, 0, 0, 1 }, VK_QUOTEDBL },
        { VK_6,            { 0, 0, 0, 1 }, VK_AMPERSAND },
        { VK_7,            { 0, 0, 0, 1 }, VK_SLASH },
        { VK_8,            { 0, 0, 0, 1 }, VK_LEFTPAREN },
        { VK_9,            { 0, 0, 0, 1 }, VK_RIGHTPAREN },
        { VK_0,            { 0, 0, 0, 1 }, VK_EQUALS },
        { VK_QUOTE,        { 0, 0, 0, 1 }, VK_QUESTION },
        { VK_QUOTE,        { 0, 0, 1, 0 }, VK_BACKSLASH },
        { VK_QUESTION_INV, { 0, 0, 0, 1 }, VK_EXCLAIM_INV },
        { VK_q,            { 0, 0, 1, 0 }, VK_AT },
        { VK_ACUTEACCENT,  { 0, 0, 0, 1 }, VK_DIAERESIS },
        { VK_PLUS,         { 0, 0, 0, 1 }, VK_ASTERISK },
        { VK_PLUS,         { 0, 0, 1, 0 }, VK_TILDE },
        { VK_TILDE_n,      { 0, 0, 0, 1 }, VK_TILDE_N },
        { VK_LEFTBRACE,    { 0, 0, 0, 1 }, VK_LEFTBRACKET },
        { VK_LEFTBRACE,    { 0, 0, 1, 0 }, VK_CARET },
        { VK_RIGHTBRACE,   { 0, 0, 0, 1 }, VK_RIGHTBRACKET },
        { VK_RIGHTBRACE,   { 0, 0, 1, 0 }, VK_GRAVEACCENT },
        { VK_LESS,         { 0, 0, 0, 1 }, VK_GREATER },
        { VK_COMMA,        { 0, 0, 0, 1 }, VK_SEMICOLON },
        { VK_PERIOD,       { 0, 0, 0, 1 }, VK_COLON },
        { VK_MINUS,        { 0, 0, 0, 1 }, VK_UNDERSCORE },
    },
    {}, {},
};

// French AZERTY: FabGL's layout without dead keys, plus the keys it leaves on
// their US meaning (2 7 0 row, the "= +" key, the "u-grave %" key)
static const KeyboardLayout AzertyLayout = {
    "FR", "French", &FrenchLayout,
    {
        { 0x55, VK_EQUALS },
        { 0x79, VK_KP_PLUS },
        { 0x52, VK_GRAVE_u },
    },
    {},
    {
        { VK_2,       { 0, 0, 0, 1 }, VK_2 },
        { VK_2,       { 0, 0, 1, 0 }, VK_TILDE },
        { VK_7,       { 0, 0, 0, 1 }, VK_7 },
        { VK_7,       { 0, 0, 1, 0 }, VK_GRAVEACCENT },
        { VK_0,       { 0, 0, 0, 1 }, VK_0 },
        { VK_0,       { 0, 0, 1, 0 }, VK_AT },
        { VK_GRAVE_u, { 0, 0, 0, 1 }, VK_PERCENT },
    },
    {}, {},
};

// Italian: FabGL's layout plus the accented-letter keys, which carry @ # [ ] { } ^
static const KeyboardLayout ItalyLayout = {
    "IT", "Italian", &ItalianLayout,
    {
        { 0x54, VK_GRAVE_e },
        { 0x4C, VK_GRAVE_o },
        { 0x52, VK_GRAVE_a },
        { 0x5D, VK_GRAVE_u },
        { 0x55, VK_GRAVE_i },
    },
    {},
    {
        { VK_GRAVE_e, { 0, 0, 0, 1 }, VK_ACUTE_e },
        { VK_GRAVE_e, { 0, 0, 1, 0 }, VK_LEFTBRACKET },
        { VK_GRAVE_e, { 0, 0, 1, 1 }, VK_LEFTBRACE },
        { VK_GRAVE_o, { 0, 0, 0, 1 }, VK_CEDILLA_c },
        { VK_GRAVE_o, { 0, 0, 1, 0 }, VK_AT },
        { VK_GRAVE_a, { 0, 0, 0, 1 }, VK_DEGREE },
        { VK_GRAVE_a, { 0, 0, 1, 0 }, VK_HASH },
        { VK_GRAVE_u, { 0, 0, 0, 1 }, VK_SECTION },
        { VK_GRAVE_i, { 0, 0, 0, 1 }, VK_CARET },
    },
    {}, {},
};

// Portuguese, Brazil ABNT2. The extra "/ ?" key beside right Shift is scancode 0x51;
// AltGr+Q and AltGr+W give the same characters on keyboards without it.
static const KeyboardLayout BrazilLayout = {
    "BR", "Portuguese (Brazil)", &USLayout,
    {
        { 0x0E, VK_QUOTE },
        { 0x54, VK_ACUTEACCENT },
        { 0x5B, VK_LEFTBRACKET },
        { 0x4C, VK_CEDILLA_c },
        { 0x52, VK_TILDE },
        { 0x5D, VK_RIGHTBRACKET },
        { 0x61, VK_BACKSLASH },
        { 0x4A, VK_SEMICOLON },
        { 0x51, VK_SLASH },
    },
    {},
    {
        { VK_6,           { 0, 0, 0, 1 }, VK_DIAERESIS },
        { VK_ACUTEACCENT, { 0, 0, 0, 1 }, VK_GRAVEACCENT },
        { VK_CEDILLA_c,   { 0, 0, 0, 1 }, VK_CEDILLA_C },
        { VK_TILDE,       { 0, 0, 0, 1 }, VK_CARET },
        { VK_q,           { 0, 0, 1, 0 }, VK_SLASH },
        { VK_w,           { 0, 0, 1, 0 }, VK_QUESTION },
    },
    {}, {},
};

static const KeyboardLayout *const layouts[] = {
    &USLayout, &SpainLayout, &LatamLayout, &AzertyLayout, &ItalyLayout, &BrazilLayout,
};
static const int LAYOUT_COUNT = sizeof(layouts) / sizeof(layouts[0]);
static int current = 0;

int kbdLayoutCount() { return LAYOUT_COUNT; }
int kbdLayoutGet() { return current; }

const char *kbdLayoutName(int idx) {
    return (idx >= 0 && idx < LAYOUT_COUNT) ? layouts[idx]->desc : "";
}

const char *kbdLayoutId(int idx) {
    return (idx >= 0 && idx < LAYOUT_COUNT) ? layouts[idx]->name : "";
}

int kbdLayoutFind(const char *id) {
    for (int i = 0; i < LAYOUT_COUNT; i++) {
        if (strcasecmp(layouts[i]->name, id) == 0) return i;
    }
    return -1;
}

void kbdLayoutSet(int idx) {
    if (idx < 0 || idx >= LAYOUT_COUNT) idx = 0;
    current = idx;
    fabgl::Keyboard *kbd = fabgl::PS2Controller::keyboard();
    if (kbd) kbd->setLayout(layouts[idx]);
    // on the US layout both Alt keys are FCTN; elsewhere right Alt is AltGr
    tiKeySetAltGr(idx != 0);
    tiKeyReleaseAll();
    DBG("Keyboard: %s\n", layouts[idx]->desc);
}
