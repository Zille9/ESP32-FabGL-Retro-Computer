// Classic99 for ESP32 - F12 "Supervisor" on-screen menu
// Drawn with FabGL's Canvas over the paused emulator picture.

#include <Arduino.h>
#include <dirent.h>
#include <esp_heap_caps.h>
#include "menu.h"
#include "ti_types.h"
#include "emu.h"
#include "cart.h"
#include "vdp9918.h"
#include "ticc.h"
#include "sound9919.h"
#include "keyboard_ti.h"
#include "kbd_layouts.h"
#include "speech.h"
#include "version.h"

using fabgl::VirtualKey;

#define CONFIG_FILE TI_ROOT "/config.txt"
#define MAX_DISK_FILES 100

// box centred on the 320x240 screen; every screen uses the TI title-screen colours
#define BOX_X 24
#define BOX_Y 16
#define BOX_W 272
#define BOX_H 208
#define HEAD_Y (BOX_Y + 36)                 // first line under the title
#define HINT_Y (BOX_Y + BOX_H - 27)         // key hints, above the bottom colour bar
#define TEXT_COLS 30
#define MENU_ROW_H 16                       // menus: 8x14 font
#define LIST_ROW_H 10                       // long file lists: 8x8 font

#define TI_CYAN     RGB888(85, 255, 255)
#define TI_DKBLUE   RGB888(0, 0, 170)
#define TI_BLACK    RGB888(0, 0, 0)
#define TI_WHITE    RGB888(255, 255, 255)
#define TI_GRAY     RGB888(170, 170, 170)
#define TI_DKGRAY   RGB888(85, 85, 85)

static fabgl::VGAController *disp;
static fabgl::Keyboard *kbd;
static char (*diskFiles)[64] = nullptr;
static int nDiskFiles = 0;

/////////////////////////////////////////////////////////
// Configuration file: "cart=<name>", "dsk1=<file>" ...
/////////////////////////////////////////////////////////
// the Speech setting as saved; it only takes effect when the speech ROM is on the card
static bool speechWanted = false;

void configSave() {
    FILE *fp = fopen(CONFIG_FILE, "w");
    if (!fp) return;
    fprintf(fp, "debug=%d\n", debugLog ? 1 : 0);
    fprintf(fp, "kbd=%s\n", kbdLayoutId(kbdLayoutGet()));
    fprintf(fp, "speech=%d\n", speechWanted ? 1 : 0);
    fprintf(fp, "cart=%s\n", cartCurrentName());
    for (int d = 1; d <= TICC_DRIVES; d++) fprintf(fp, "dsk%d=%s\n", d, diskMounted(d));
    fclose(fp);
}

bool configDebugEnabled() {
    FILE *fp = fopen(CONFIG_FILE, "r");
    if (!fp) return false;
    char ln[128];
    bool on = false;
    while (fgets(ln, sizeof(ln), fp)) {
        if (strncmp(ln, "debug=", 6) == 0) on = (ln[6] == '1');
    }
    fclose(fp);
    return on;
}

void configLoad() {
    FILE *fp = fopen(CONFIG_FILE, "r");
    if (!fp) {
        diskMount(1, "DSK1.dsk");       // first run: default image, if present
        return;
    }
    char ln[128];
    while (fgets(ln, sizeof(ln), fp)) {
        ln[strcspn(ln, "\r\n")] = 0;
        char *val = strchr(ln, '=');
        if (!val) continue;
        *val++ = 0;
        if (!*val) continue;
        if (strcmp(ln, "kbd") == 0) {
            int k = kbdLayoutFind(val);
            if (k >= 0) kbdLayoutSet(k);
        } else if (strcmp(ln, "speech") == 0) {
            speechWanted = (val[0] == '1');
            speechSetEnabled(speechWanted);
        } else if (strcmp(ln, "cart") == 0) {
            int n = cartScan();
            for (int i = 0; i < n; i++) {
                if (strcmp(cartGet(i)->name, val) == 0) { cartLoad(i); break; }
            }
        } else if (strncmp(ln, "dsk", 3) == 0 && ln[3] >= '1' && ln[3] <= '3') {
            diskMount(ln[3] - '0', val);
        }
    }
    fclose(fp);
}

/////////////////////////////////////////////////////////
// Drawing
/////////////////////////////////////////////////////////
struct Row { char label[40]; char value[32]; };
typedef void (*RowFn)(int idx, Row &row);

static bool closeAll;       // F12 pressed: leave every menu level

static VirtualKey waitKey() {
    fabgl::VirtualKeyItem item;
    for (;;) {
        if (kbd->getNextVirtualKey(&item, 100) && item.down) return item.vk;
    }
}

static inline void fillBox(fabgl::Canvas &cv, fabgl::RGB888 c, int x1, int y1, int x2, int y2) {
    cv.setBrushColor(c);
    cv.fillRectangle(x1, y1, x2, y2);
}

// the colour bars of the TI title screen, in the 9918A palette (nearest VGA colours)
static void drawColourBar(fabgl::Canvas &cv, int y) {
    static const uint8_t bars[12][3] = {
        { 170, 85, 85 }, { 255, 85, 85 }, { 255, 170, 170 }, { 170, 170, 85 }, { 255, 255, 85 }, { 85, 255, 85 },
        { 0, 170, 85 }, { 0, 170, 0 }, { 85, 170, 255 }, { 85, 85, 255 }, { 170, 85, 170 }, { 255, 255, 255 },
    };
    for (int i = 0; i < 12; i++) {
        int x = BOX_X + 4 + i * 22;
        fillBox(cv, RGB888(bars[i][0], bars[i][1], bars[i][2]), x, y, x + 21, y + 9);
    }
}

// cyan box, colour bars top and bottom, black title, key hints
static void drawFrame(fabgl::Canvas &cv, const char *title, const char *hint) {
    cv.setGlyphOptions(fabgl::GlyphOptions().FillBackground(true));
    fillBox(cv, TI_CYAN, BOX_X, BOX_Y, BOX_X + BOX_W - 1, BOX_Y + BOX_H - 1);
    cv.setPenColor(TI_DKBLUE);
    cv.drawRectangle(BOX_X, BOX_Y, BOX_X + BOX_W - 1, BOX_Y + BOX_H - 1);
    drawColourBar(cv, BOX_Y + 4);
    drawColourBar(cv, BOX_Y + BOX_H - 14);

    cv.setBrushColor(TI_CYAN);
    cv.setPenColor(TI_BLACK);
    cv.selectFont(&fabgl::FONT_8x14);
    cv.drawText(BOX_X + (BOX_W - 8 * (int)strlen(title)) / 2, BOX_Y + 17, title);
    cv.selectFont(&fabgl::FONT_6x8);
    cv.drawText(BOX_X + (BOX_W - 6 * (int)strlen(hint)) / 2, HINT_Y, hint);
}

// one line: label on the left starting at x+textOff, value on the right; the
// highlighted line is a dark blue bar with white text
static void drawRowAt(fabgl::Canvas &cv, int x, int y, int w, int textOff, bool compact, const Row &r, bool hl,
                      fabgl::RGB888 bg) {
    int h = compact ? LIST_ROW_H : MENU_ROW_H;
    int fontH = compact ? 8 : 14;
    int cols = (w - textOff - 4) / 8;
    if (cols > TEXT_COLS) cols = TEXT_COLS;
    if (hl) bg = TI_DKBLUE;
    fillBox(cv, bg, x, y, x + w - 1, y + h - 1);
    int ty = y + (h - fontH) / 2;

    int vlen = strlen(r.value);
    int room = cols - (vlen ? 1 : 0);
    int llen = strlen(r.label);
    if (llen > room) llen = room;
    if (vlen > room - llen) vlen = room - llen;

    char text[TEXT_COLS + 1];
    cv.selectFont(compact ? &fabgl::FONT_8x8 : &fabgl::FONT_8x14);
    cv.setPenColor(hl ? TI_WHITE : TI_BLACK);
    snprintf(text, sizeof(text), "%.*s", llen, r.label);
    cv.drawText(x + textOff, ty, text);
    if (vlen > 0) {
        snprintf(text, sizeof(text), "%.*s", vlen, r.value);
        cv.setPenColor(hl ? TI_WHITE : TI_DKBLUE);
        cv.drawText(x + textOff + (cols - vlen) * 8, ty, text);
    }
}

#define LIST_X (BOX_X + 4)
#define LIST_W (BOX_W - 14)

static void drawScrollbar(fabgl::Canvas &cv, int y, int h, int top, int visible, int count) {
    int x = BOX_X + BOX_W - 8;
    fillBox(cv, TI_CYAN, x, y, x + 2, y + h - 1);
    if (count <= visible) return;
    int len = h * visible / count;
    if (len < 8) len = 8;
    int pos = (h - len) * top / (count - visible);
    fillBox(cv, TI_DKBLUE, x, y + pos, x + 2, y + pos + len - 1);
}

// Small white window over the current screen with up to two message lines and a list
// of options. Returns the chosen option, or -1 on Esc/F12. The caller redraws afterwards.
#define POPUP_W 232
#define POPUP_TITLE_H 18
static int popup(const char *title, const char *msg1, const char *msg2, int count, RowFn rowFn, int selected) {
    fabgl::Canvas cv(disp);
    const char *msgs[2] = { msg1 ? msg1 : "", msg2 ? msg2 : "" };
    int nmsg = (msgs[0][0] ? 1 : 0) + (msgs[1][0] ? 1 : 0);
    int h = POPUP_TITLE_H + 8 + nmsg * 10 + (nmsg ? 4 : 0) + count * MENU_ROW_H + 6;
    int x = (VDP_SCREEN_W - POPUP_W) / 2, y = (VDP_SCREEN_H - h) / 2;

    cv.setGlyphOptions(fabgl::GlyphOptions().FillBackground(true));
    fillBox(cv, TI_WHITE, x, y, x + POPUP_W - 1, y + h - 1);
    cv.setPenColor(TI_DKBLUE);
    cv.drawRectangle(x, y, x + POPUP_W - 1, y + h - 1);
    cv.drawRectangle(x + 1, y + 1, x + POPUP_W - 2, y + h - 2);
    fillBox(cv, TI_DKBLUE, x + 2, y + 2, x + POPUP_W - 3, y + POPUP_TITLE_H + 1);
    cv.selectFont(&fabgl::FONT_8x14);
    cv.setPenColor(TI_WHITE);
    cv.drawText(x + (POPUP_W - 8 * (int)strlen(title)) / 2, y + 4, title);

    int ry = y + POPUP_TITLE_H + 8;
    cv.selectFont(&fabgl::FONT_8x8);
    cv.setBrushColor(TI_WHITE);
    cv.setPenColor(TI_BLACK);
    for (int i = 0; i < 2; i++) {
        if (!msgs[i][0]) continue;
        char text[28];
        snprintf(text, sizeof(text), "%.27s", msgs[i]);
        cv.drawText(x + (POPUP_W - 8 * (int)strlen(text)) / 2, ry, text);
        ry += 10;
        DBG("popup: %s: %s\n", title, text);
    }
    if (nmsg) ry += 4;

    if (selected < 0 || selected >= count) selected = 0;
    for (;;) {
        Row row;
        for (int i = 0; i < count; i++) {
            row.label[0] = row.value[0] = 0;
            rowFn(i, row);
            drawRowAt(cv, x + 4, ry + i * MENU_ROW_H, POPUP_W - 8, 8, false, row, i == selected, TI_WHITE);
            if (i == selected) DBG("popup: %s > %s %s\n", title, row.label, row.value);
        }
        cv.waitCompletion();

        switch (waitKey()) {
            case VirtualKey::VK_UP:       if (selected > 0) selected--; break;
            case VirtualKey::VK_DOWN:     if (selected < count - 1) selected++; break;
            case VirtualKey::VK_RETURN:
            case VirtualKey::VK_KP_ENTER: return selected;
            case VirtualKey::VK_ESCAPE:   return -1;
            case VirtualKey::VK_F12:      closeAll = true; return -1;
            default: break;
        }
    }
}

static void confirmRow(int idx, Row &row) {
    strcpy(row.label, idx == 0 ? "No" : "Yes");
}

static bool confirm(const char *title, const char *msg1, const char *msg2) {
    return popup(title, msg1, msg2, 2, confirmRow, 0) == 1;
}

// 16x12 row icons
enum { ICON_NONE, ICON_CART, ICON_SLOT, ICON_KEYBOARD, ICON_SPIDER, ICON_SPEECH };
typedef int (*IconFn)(int idx);

static void drawSmallIcon(fabgl::Canvas &cv, int icon, int x, int y, fabgl::RGB888 fg) {
    switch (icon) {
        case ICON_CART:         // cartridge: body, label, edge connector
            fillBox(cv, TI_DKGRAY, x, y, x + 15, y + 8);
            fillBox(cv, RGB888(255, 255, 85), x + 3, y + 2, x + 12, y + 5);
            fillBox(cv, RGB888(170, 170, 85), x + 4, y + 9, x + 11, y + 11);
            break;
        case ICON_SLOT:         // empty cartridge slot
            cv.setPenColor(fg);
            cv.drawRectangle(x, y, x + 15, y + 8);
            cv.drawLine(x + 3, y + 4, x + 12, y + 4);
            break;
        case ICON_KEYBOARD:     // keyboard: case, two rows of keys, space bar
            fillBox(cv, TI_DKGRAY, x, y + 1, x + 15, y + 10);
            for (int r = 0; r < 2; r++) {
                for (int k = 0; k < 5; k++) {
                    fillBox(cv, TI_WHITE, x + 2 + k * 3 - r, y + 3 + r * 2, x + 3 + k * 3 - r, y + 3 + r * 2);
                }
            }
            fillBox(cv, TI_WHITE, x + 4, y + 8, x + 11, y + 8);
            break;
        case ICON_SPIDER: {     // spider: body, head, four legs each side
            static const int8_t legs[4][4] = { { 6, 5, 1, 2 }, { 5, 6, 0, 6 }, { 5, 8, 0, 9 }, { 6, 9, 2, 11 } };
            cv.setPenColor(fg);
            for (int l = 0; l < 4; l++) {
                cv.drawLine(x + legs[l][0], y + legs[l][1], x + legs[l][2], y + legs[l][3]);
                cv.drawLine(x + 15 - legs[l][0], y + legs[l][1], x + 15 - legs[l][2], y + legs[l][3]);
            }
            fillBox(cv, fg, x + 5, y + 4, x + 10, y + 10);
            fillBox(cv, fg, x + 6, y + 1, x + 9, y + 3);
            fillBox(cv, RGB888(255, 85, 85), x + 6, y + 2, x + 6, y + 2);
            fillBox(cv, RGB888(255, 85, 85), x + 9, y + 2, x + 9, y + 2);
            break;
        }
        case ICON_SPEECH:       // speech bubble with three dots
            fillBox(cv, TI_WHITE, x + 1, y, x + 14, y + 7);
            fillBox(cv, TI_WHITE, x + 3, y + 8, x + 6, y + 9);
            fillBox(cv, TI_WHITE, x + 3, y + 10, x + 4, y + 11);
            for (int d = 0; d < 3; d++) {
                fillBox(cv, TI_DKGRAY, x + 4 + d * 3, y + 3, x + 5 + d * 3, y + 4);
            }
            break;
        default:
            break;
    }
}

// One menu level with an icon on every row. Returns the chosen index, or -1 on Esc.
// F12 also returns -1 and sets closeAll. `header`, if given, draws above a list that
// starts at listY; without it a short list is centred vertically.
typedef void (*HeaderFn)(fabgl::Canvas &cv);

static int runList(const char *title, int count, RowFn rowFn, IconFn iconFn, int selected,
                   HeaderFn header = nullptr, int listY = HEAD_Y + 4) {
    fabgl::Canvas cv(disp);
    int visible = (HINT_Y - 4 - listY) / MENU_ROW_H;
    int y0 = listY;
    if (!header && count < visible) y0 += (visible - count) / 2 * MENU_ROW_H;
    if (selected < 0 || selected >= count) selected = 0;
    int top = 0;

    drawFrame(cv, title, "Up/Dn   ENTER Select   ESC Back   F12 Exit");
    if (header) header(cv);
    for (;;) {
        if (selected < top) top = selected;
        if (selected >= top + visible) top = selected - visible + 1;

        Row row;
        for (int r = 0; r < visible && top + r < count; r++) {
            bool hl = (top + r == selected);
            int y = y0 + r * MENU_ROW_H;
            row.label[0] = row.value[0] = 0;
            rowFn(top + r, row);
            drawRowAt(cv, LIST_X, y, LIST_W, 28, false, row, hl, TI_CYAN);
            drawSmallIcon(cv, iconFn(top + r), LIST_X + 6, y + 2, hl ? TI_WHITE : TI_BLACK);
            if (hl) DBG("menu: %s > %s %s\n", title, row.label, row.value);
        }
        drawScrollbar(cv, listY, visible * MENU_ROW_H, top, visible, count);
        cv.waitCompletion();

        switch (waitKey()) {
            case VirtualKey::VK_UP:       if (selected > 0) selected--; break;
            case VirtualKey::VK_DOWN:     if (selected < count - 1) selected++; break;
            case VirtualKey::VK_PAGEUP:   selected = (selected > visible) ? selected - visible : 0; break;
            case VirtualKey::VK_PAGEDOWN: selected = (selected + visible < count) ? selected + visible : count - 1; break;
            case VirtualKey::VK_HOME:     selected = 0; break;
            case VirtualKey::VK_END:      selected = count - 1; break;
            case VirtualKey::VK_RETURN:
            case VirtualKey::VK_KP_ENTER: return selected;
            case VirtualKey::VK_ESCAPE:   return -1;
            case VirtualKey::VK_F12:      closeAll = true; return -1;
            default: break;
        }
    }
}

/////////////////////////////////////////////////////////
// Cartridges: the inserted cartridge on top, the list below
/////////////////////////////////////////////////////////
#define CART_PANEL_H 36

// 32x32 TI cartridge: body, label, grip ridges, edge connector
static void drawBigCart(fabgl::Canvas &cv, int x, int y, bool gray) {
    fillBox(cv, gray ? TI_GRAY : TI_DKGRAY, x + 2, y + 5, x + 29, y + 26);
    fillBox(cv, gray ? TI_WHITE : RGB888(255, 255, 85), x + 6, y + 8, x + 25, y + 16);
    for (int r = 0; r < 3; r++) {
        fillBox(cv, gray ? TI_WHITE : TI_GRAY, x + 6, y + 19 + r * 3, x + 25, y + 19 + r * 3);
    }
    fillBox(cv, gray ? TI_GRAY : RGB888(170, 170, 85), x + 9, y + 27, x + 22, y + 30);
}

static void cartHeader(fabgl::Canvas &cv) {
    int x = BOX_X + 8, y = HEAD_Y, w = BOX_W - 16;
    bool inserted = cartCurrentName()[0] != 0;
    fillBox(cv, TI_WHITE, x, y, x + w - 1, y + CART_PANEL_H - 1);
    cv.setPenColor(TI_DKBLUE);
    cv.drawRectangle(x, y, x + w - 1, y + CART_PANEL_H - 1);
    drawBigCart(cv, x + 8, y + 2, !inserted);

    cv.setBrushColor(TI_WHITE);
    if (inserted) {
        char name[26];
        snprintf(name, sizeof(name), "%.25s", cartCurrentName());
        cv.selectFont(&fabgl::FONT_6x8);
        cv.setPenColor(TI_DKBLUE);
        cv.drawText(x + 50, y + 5, "Inserted cartridge");
        cv.selectFont(&fabgl::FONT_8x14);
        cv.setPenColor(TI_BLACK);
        cv.drawText(x + 50, y + 16, name);
    } else {
        cv.selectFont(&fabgl::FONT_8x14);
        cv.setPenColor(TI_GRAY);
        cv.drawText(x + 50, y + 11, "No cartridge");
    }
}

static int cartCount;

static void cartRow(int idx, Row &row) {
    if (idx >= cartCount) strcpy(row.label, "Remove cartridge");
    else strlcpy(row.label, cartGet(idx)->name, sizeof(row.label));
}

static int cartIcon(int idx) {
    return idx >= cartCount ? ICON_SLOT : ICON_CART;
}

// returns true if the cartridge changed (the console must be reset)
static bool cartMenu() {
    cartCount = cartScan();
    bool inserted = cartCurrentName()[0] != 0;
    // "Remove cartridge" is the last row, and only when there is one to remove
    int c = runList("Cartridges", cartCount + (inserted ? 1 : 0), cartRow, cartIcon, 0, cartHeader,
                    HEAD_Y + CART_PANEL_H + 4);
    if (c < 0) return false;
    if (c >= cartCount) { cartEject(); return true; }
    if (strcmp(cartGet(c)->name, cartCurrentName()) == 0) return false;     // already inserted
    cartLoad(c);
    return true;
}

/////////////////////////////////////////////////////////
// Disk Manager: drive buttons on top, disk images below
/////////////////////////////////////////////////////////
#define DRIVE_BTN_W 84
#define DRIVE_BTN_H 46
#define DRIVE_BTN_PITCH 88
#define DISK_LIST_Y (HEAD_Y + DRIVE_BTN_H + 5)
#define DISK_LIST_ROWS ((HINT_Y - 4 - DISK_LIST_Y) / LIST_ROW_H)

static int scanDisks() {
    if (!diskFiles) diskFiles = (char (*)[64])heap_caps_calloc(MAX_DISK_FILES, 64, MALLOC_CAP_SPIRAM);
    nDiskFiles = 0;
    if (!diskFiles) return 0;
    DIR *dir = opendir(TI_DISK_DIR);
    if (!dir) return 0;
    struct dirent *de;
    while ((de = readdir(dir)) != nullptr && nDiskFiles < MAX_DISK_FILES) {
        if (de->d_name[0] == '.' || strlen(de->d_name) >= 64) continue;
        strcpy(diskFiles[nDiskFiles++], de->d_name);
    }
    closedir(dir);
    qsort(diskFiles, nDiskFiles, 64, (int (*)(const void *, const void *))strcasecmp);
    return nDiskFiles;
}

// file name without a trailing ".dsk"
static void diskTitle(const char *file, char *out, size_t size) {
    strlcpy(out, file, size);
    size_t n = strlen(out);
    if (n > 4 && strcasecmp(out + n - 4, ".dsk") == 0) out[n - 4] = 0;
}

static void drawDriveButton(fabgl::Canvas &cv, int drive, bool focused) {
    int x = BOX_X + 6 + (drive - 1) * DRIVE_BTN_PITCH, y = HEAD_Y;
    bool mounted = diskMounted(drive)[0] != 0;
    fabgl::RGB888 bg = focused ? TI_DKBLUE : TI_WHITE;
    fabgl::RGB888 fg = focused ? TI_WHITE : TI_BLACK;

    fillBox(cv, bg, x, y, x + DRIVE_BTN_W - 1, y + DRIVE_BTN_H - 1);
    cv.setPenColor(focused ? TI_BLACK : TI_DKBLUE);
    cv.drawRectangle(x, y, x + DRIVE_BTN_W - 1, y + DRIVE_BTN_H - 1);
    if (focused) cv.drawRectangle(x + 1, y + 1, x + DRIVE_BTN_W - 2, y + DRIVE_BTN_H - 2);

    // diskette icon: body, metal shutter on top, label below; grey when the drive is empty
    int ix = x + 5, iy = y + 5;
    fillBox(cv, mounted ? RGB888(255, 85, 85) : TI_GRAY, ix, iy, ix + 15, iy + 15);
    fillBox(cv, mounted ? TI_GRAY : TI_DKGRAY, ix + 4, iy, ix + 11, iy + 5);
    fillBox(cv, TI_WHITE, ix + 3, iy + 9, ix + 12, iy + 13);

    char text[32];
    cv.setBrushColor(bg);
    cv.selectFont(&fabgl::FONT_8x8);
    cv.setPenColor(fg);
    snprintf(text, sizeof(text), "DSK%d", drive);
    cv.drawText(x + 28, y + 9, text);

    // image name on two lines of 12 characters
    char name[64];
    if (mounted) diskTitle(diskMounted(drive), name, sizeof(name)); else strcpy(name, "empty");
    cv.selectFont(&fabgl::FONT_6x8);
    cv.setPenColor(mounted ? fg : TI_GRAY);
    snprintf(text, sizeof(text), "%.12s", name);
    cv.drawText(x + 5, y + 25, text);
    if (strlen(name) > 12) {
        snprintf(text, sizeof(text), "%.12s", name + 12);
        cv.drawText(x + 5, y + 35, text);
    }
}

static void mountRow(int idx, Row &row) {
    if (idx >= TICC_DRIVES) { strcpy(row.label, "Cancel"); return; }
    snprintf(row.label, sizeof(row.label), "DSK%d", idx + 1);
    if (diskMounted(idx + 1)[0]) diskTitle(diskMounted(idx + 1), row.value, sizeof(row.value));
    else strcpy(row.value, "(empty)");
}

// ask which drive takes the image; confirm before replacing a disk or mounting it twice
static void mountDialog(const char *file) {
    int first = 0;
    for (int d = TICC_DRIVES; d >= 1; d--) {
        if (!diskMounted(d)[0]) first = d - 1;
    }
    int d = popup("Mount disk on", file, nullptr, TICC_DRIVES + 1, mountRow, first);
    if (d < 0 || d >= TICC_DRIVES) return;
    int drive = d + 1;
    const char *cur = diskMounted(drive);
    if (strcmp(cur, file) == 0) return;         // already there

    char m1[40], m2[40];
    if (cur[0]) {
        snprintf(m1, sizeof(m1), "DSK%d already has a disk:", drive);
        diskTitle(cur, m2, sizeof(m2));
        if (!confirm("Replace disk?", m1, m2)) return;
    } else {
        for (int o = 1; o <= TICC_DRIVES; o++) {
            if (strcmp(diskMounted(o), file) != 0) continue;
            snprintf(m1, sizeof(m1), "Already mounted on DSK%d.", o);
            snprintf(m2, sizeof(m2), "Mount on DSK%d as well?", drive);
            if (!confirm("Mount again?", m1, m2)) return;
            break;
        }
    }
    diskMount(drive, file);
}

static void unmountDialog(int drive) {
    if (!diskMounted(drive)[0]) return;
    char m1[40], m2[40];
    snprintf(m1, sizeof(m1), "Unmount DSK%d?", drive);
    diskTitle(diskMounted(drive), m2, sizeof(m2));
    if (confirm("Unmount disk", m1, m2)) diskUnmount(drive);
}

static void diskMenu() {
    fabgl::Canvas cv(disp);
    int n = scanDisks();
    int drive = 1, sel = 0, top = 0;
    bool inDrives = (n == 0);       // focus: drive buttons or file list
    bool frame = true;

    while (!closeAll) {
        if (frame) {
            drawFrame(cv, "Disk Manager", "Arrows   TAB Drives/Files   ENTER   ESC Back");
            frame = false;
        }
        for (int d = 1; d <= TICC_DRIVES; d++) drawDriveButton(cv, d, inDrives && d == drive);

        if (sel < top) top = sel;
        if (sel >= top + DISK_LIST_ROWS) top = sel - DISK_LIST_ROWS + 1;
        Row row;
        row.value[0] = 0;
        for (int r = 0; r < DISK_LIST_ROWS && top + r < n; r++) {
            strlcpy(row.label, diskFiles[top + r], sizeof(row.label));
            drawRowAt(cv, LIST_X, DISK_LIST_Y + r * LIST_ROW_H, LIST_W, 6, true, row, !inDrives && top + r == sel, TI_CYAN);
        }
        if (n == 0) {
            strcpy(row.label, "(no disk images in /ti99/disks)");
            drawRowAt(cv, LIST_X, DISK_LIST_Y, LIST_W, 6, true, row, false, TI_CYAN);
        }
        drawScrollbar(cv, DISK_LIST_Y, DISK_LIST_ROWS * LIST_ROW_H, top, DISK_LIST_ROWS, n);
        cv.waitCompletion();
        if (inDrives) DBG("disks: [DSK%d] %s\n", drive, diskMounted(drive)[0] ? diskMounted(drive) : "(empty)");
        else DBG("disks: file %s\n", diskFiles[sel]);

        VirtualKey vk = waitKey();
        if (vk == VirtualKey::VK_ESCAPE) return;
        if (vk == VirtualKey::VK_F12) { closeAll = true; return; }
        bool enter = (vk == VirtualKey::VK_RETURN || vk == VirtualKey::VK_KP_ENTER);

        if (inDrives) {
            switch (vk) {
                case VirtualKey::VK_LEFT:  if (drive > 1) drive--; break;
                case VirtualKey::VK_RIGHT: if (drive < TICC_DRIVES) drive++; break;
                case VirtualKey::VK_DOWN:
                case VirtualKey::VK_TAB:   if (n > 0) inDrives = false; break;
                default:
                    if (enter) { unmountDialog(drive); frame = true; }
                    break;
            }
        } else {
            switch (vk) {
                case VirtualKey::VK_UP:       if (sel > 0) sel--; else inDrives = true; break;
                case VirtualKey::VK_DOWN:     if (sel < n - 1) sel++; break;
                case VirtualKey::VK_PAGEUP:   sel = (sel > DISK_LIST_ROWS) ? sel - DISK_LIST_ROWS : 0; break;
                case VirtualKey::VK_PAGEDOWN: sel = (sel + DISK_LIST_ROWS < n) ? sel + DISK_LIST_ROWS : n - 1; break;
                case VirtualKey::VK_HOME:     sel = 0; break;
                case VirtualKey::VK_END:      sel = n - 1; break;
                case VirtualKey::VK_TAB:      inDrives = true; break;
                default:
                    if (enter) { mountDialog(diskFiles[sel]); frame = true; }
                    break;
            }
        }
    }
}

/////////////////////////////////////////////////////////
// Setup
/////////////////////////////////////////////////////////
static void layoutRow(int idx, Row &row) {
    strlcpy(row.label, kbdLayoutName(idx), sizeof(row.label));
    if (idx == kbdLayoutGet()) strcpy(row.value, "active");
}

static void setupRow(int idx, Row &row) {
    if (idx == 0) {
        strcpy(row.label, "Keyboard");
        strlcpy(row.value, kbdLayoutName(kbdLayoutGet()), sizeof(row.value));
    } else if (idx == 1) {
        strcpy(row.label, "Speech");
        strcpy(row.value, !speechAvailable() ? "no ROM" : speechEnabled() ? "ON" : "OFF");
    } else {
        strcpy(row.label, "Debug log");
        strcpy(row.value, debugLog ? "ON" : "OFF");
    }
}

static int keyboardIcon(int idx) {
    return ICON_KEYBOARD;
}

static int setupIcon(int idx) {
    return idx == 0 ? ICON_KEYBOARD : idx == 1 ? ICON_SPEECH : ICON_SPIDER;
}

static void setupMenu() {
    int sel = 0;
    while (!closeAll) {
        sel = runList("Setup", 3, setupRow, setupIcon, sel);
        if (sel < 0) return;
        if (sel == 0) {
            int k = runList("Keyboard layout", kbdLayoutCount(), layoutRow, keyboardIcon, kbdLayoutGet());
            if (k >= 0) kbdLayoutSet(k);
        } else if (sel == 1) {
            if (speechAvailable()) {    // Enter toggles; a running program notices after a reset
                speechWanted = !speechEnabled();
                speechSetEnabled(speechWanted);
            }
        } else {
            debugLog = !debugLog;       // Enter toggles; saved when the menu closes
        }
    }
}

/////////////////////////////////////////////////////////
// About
/////////////////////////////////////////////////////////
static void aboutScreen() {
    fabgl::Canvas cv(disp);
    drawFrame(cv, "About", "ESC Back   F12 Exit");

    static const char *const lines[] = {
        "Classic99 ESP32",
        "TI-99/4A for TTGO VGA32",
        "",
        "Derived from Classic99",
        "(C) Mike Brent (Tursi)",
    };
    cv.selectFont(&fabgl::FONT_8x8);
    cv.setBrushColor(TI_CYAN);
    int y = HEAD_Y + 2;
    for (unsigned i = 0; i < sizeof(lines) / sizeof(lines[0]); i++, y += 10) {
        cv.setPenColor(i < 2 ? TI_BLACK : TI_DKBLUE);
        cv.drawText(BOX_X + (BOX_W - 8 * (int)strlen(lines[i])) / 2, y, lines[i]);
    }
    y += 4;
    cv.setPenColor(TI_DKBLUE);
    cv.drawLine(BOX_X + 24, y, BOX_X + BOX_W - 25, y);
    y += 6;

    Row info[4];
    strcpy(info[0].label, "Version");
    strcpy(info[0].value, CLASSIC99_ESP32_VERSION);
    strcpy(info[1].label, "Build date");
    strcpy(info[1].value, CLASSIC99_ESP32_BUILD_DATE);
    strcpy(info[2].label, "PSRAM free");
    snprintf(info[2].value, sizeof(info[2].value), "%u KB", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    strcpy(info[3].label, "IRAM available");       // free internal RAM
    snprintf(info[3].value, sizeof(info[3].value), "%u KB", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
    for (int i = 0; i < 4; i++, y += LIST_ROW_H + 2) {
        drawRowAt(cv, LIST_X, y, LIST_W, 12, true, info[i], false, TI_CYAN);
        DBG("about: %s %s\n", info[i].label, info[i].value);
    }
    cv.waitCompletion();

    for (;;) {
        VirtualKey vk = waitKey();
        if (vk == VirtualKey::VK_F12) closeAll = true;
        if (vk == VirtualKey::VK_F12 || vk == VirtualKey::VK_ESCAPE || vk == VirtualKey::VK_RETURN) return;
    }
}

/////////////////////////////////////////////////////////
// Boot error: full-screen warning shown instead of starting the emulator
/////////////////////////////////////////////////////////
#define ERR_RED     RGB888(255, 0, 0)
#define ERR_LTRED   RGB888(255, 85, 85)
#define ERR_YELLOW  RGB888(255, 255, 85)
#define ERR_GREEN   RGB888(85, 255, 85)
#define ERR_TEXT_X  28

// yellow warning triangle with a black "!", 28 wide and 24 high
static void drawWarningSign(fabgl::Canvas &cv, int x, int y) {
    fabgl::Point tri[3] = { fabgl::Point(x + 14, y), fabgl::Point(x + 27, y + 23), fabgl::Point(x, y + 23) };
    cv.setBrushColor(ERR_YELLOW);
    cv.fillPath(tri, 3);
    fillBox(cv, TI_BLACK, x + 13, y + 8, x + 15, y + 16);
    fillBox(cv, TI_BLACK, x + 13, y + 19, x + 15, y + 21);
}

// DIP memory chip, 120x48: pins top and bottom, pin-1 notch, "ROM" label and a red "?"
static void drawRomChip(fabgl::Canvas &cv, int x, int y) {
    for (int i = 0; i < 10; i++) {
        int px = x + 8 + i * 11;
        fillBox(cv, TI_GRAY, px, y, px + 5, y + 47);
    }
    fillBox(cv, TI_DKGRAY, x, y + 6, x + 119, y + 41);
    cv.setBrushColor(TI_BLACK);
    cv.fillEllipse(x, y + 24, 12, 12);
    fillBox(cv, TI_WHITE, x + 16, y + 14, x + 75, y + 33);
    cv.setPenColor(TI_BLACK);
    cv.setBrushColor(TI_WHITE);
    cv.selectFont(&fabgl::FONT_8x14);
    cv.drawText(x + 34, y + 17, "ROM");

    cv.setGlyphOptions(fabgl::GlyphOptions().FillBackground(true).DoubleWidth(1));
    cv.selectFont(&fabgl::FONT_10x20);
    cv.setBrushColor(TI_DKGRAY);
    cv.setPenColor(ERR_LTRED);
    cv.drawText(x + 88, y + 14, "?");
    cv.setGlyphOptions(fabgl::GlyphOptions().FillBackground(true));
}

void bootErrorScreen(fabgl::VGAController *display, fabgl::Keyboard *keyboard, bool sdMounted,
                     const BootRomStatus *roms, int count) {
    disp = display;
    kbd = keyboard;
    fabgl::Canvas cv(disp);
    const char *headline = sdMounted ? "MISSING ROMS ON SD CARD" : "SD CARD NOT FOUND";

    cv.setGlyphOptions(fabgl::GlyphOptions().FillBackground(true));
    cv.setBrushColor(TI_BLACK);
    cv.clear();
    cv.setPenColor(ERR_RED);
    cv.drawRectangle(0, 0, VDP_SCREEN_W - 1, VDP_SCREEN_H - 1);
    cv.drawRectangle(1, 1, VDP_SCREEN_W - 2, VDP_SCREEN_H - 2);
    drawColourBar(cv, 5);
    drawColourBar(cv, VDP_SCREEN_H - 15);

    // red band with the warning in double-width letters
    fillBox(cv, ERR_RED, 2, 19, VDP_SCREEN_W - 3, 50);
    drawWarningSign(cv, 44, 23);
    drawWarningSign(cv, VDP_SCREEN_W - 44 - 28, 23);
    cv.setGlyphOptions(fabgl::GlyphOptions().FillBackground(true).DoubleWidth(1));
    cv.selectFont(&fabgl::FONT_10x20);
    cv.setBrushColor(ERR_RED);
    cv.setPenColor(TI_WHITE);
    cv.drawText((VDP_SCREEN_W - 20 * 7) / 2, 25, "WARNING");
    cv.setGlyphOptions(fabgl::GlyphOptions().FillBackground(true));

    drawRomChip(cv, (VDP_SCREEN_W - 120) / 2, 58);

    cv.selectFont(&fabgl::FONT_10x20);
    cv.setBrushColor(TI_BLACK);
    cv.setPenColor(ERR_LTRED);
    cv.drawText((VDP_SCREEN_W - 10 * (int)strlen(headline)) / 2, 112, headline);

    cv.selectFont(&fabgl::FONT_8x8);
    cv.setPenColor(TI_WHITE);
    cv.drawText(ERR_TEXT_X, 140, sdMounted ? "Copy these files to the SD card:" : "Insert a FAT card with these files:");

    bool optionalMissing = false;
    int y = 154;
    for (int i = 0; i < count; i++, y += 12) {
        cv.setPenColor(TI_WHITE);
        cv.drawText(ERR_TEXT_X, y, roms[i].path + 3);       // without the "/SD" mount point
        if (!sdMounted) continue;
        const char *state = "OK";
        fabgl::RGB888 colour = ERR_GREEN;
        if (!roms[i].present) {
            state = roms[i].optional ? "MISSING*" : "MISSING";
            colour = roms[i].optional ? ERR_YELLOW : ERR_LTRED;
            optionalMissing |= roms[i].optional;
        }
        cv.setPenColor(colour);
        cv.drawText(ERR_TEXT_X + 24 * 8, y, state);
    }
    cv.selectFont(&fabgl::FONT_6x8);
    if (optionalMissing) {
        cv.setPenColor(TI_GRAY);
        cv.drawText(ERR_TEXT_X, y + 2, "* optional, only needed for the disk drives");
    }
    static const char hint[] = "Press any key to restart";
    cv.setPenColor(TI_CYAN);
    cv.drawText((VDP_SCREEN_W - 6 * (int)strlen(hint)) / 2, 210, hint);
    cv.waitCompletion();

    waitKey();
    ESP.restart();
    for (;;) vTaskDelay(1000);
}

/////////////////////////////////////////////////////////
// Main menu: icon tiles
/////////////////////////////////////////////////////////
#define TILE_W 80
#define TILE_H 58
#define TILE_X0 (BOX_X + 8)
#define TILE_Y0 (BOX_Y + 40)
#define TILE_PITCH_X 88
#define TILE_PITCH_Y 66
#define TILE_COLS 3
#define TILE_COUNT 6

enum { TILE_CART, TILE_DISKS, TILE_SETUP, TILE_RESET, TILE_ABOUT, TILE_RESUME };
static const char *const tileLabels[TILE_COUNT] = { "Cartridge", "Disks", "Setup", "Reset", "About", "Resume" };

// 32x32 icons, drawn with rectangles and ellipses; bg/fg are the tile's own colours
static void drawIcon(fabgl::Canvas &cv, int icon, int x, int y, fabgl::RGB888 bg, fabgl::RGB888 fg) {
    switch (icon) {
        case TILE_CART:
            drawBigCart(cv, x, y, false);
            break;
        case TILE_DISKS:    // diskette: body, shutter with slot, label
            fillBox(cv, RGB888(255, 85, 85), x + 3, y + 2, x + 28, y + 29);
            fillBox(cv, TI_GRAY, x + 9, y + 2, x + 22, y + 11);
            fillBox(cv, TI_DKGRAY, x + 17, y + 4, x + 20, y + 9);
            fillBox(cv, TI_WHITE, x + 7, y + 16, x + 24, y + 29);
            break;
        case TILE_SETUP: {  // three sliders
            static const uint8_t knob[3] = { 7, 19, 12 };
            for (int r = 0; r < 3; r++) {
                int yy = y + 6 + r * 9;
                fillBox(cv, fg, x + 3, yy, x + 28, yy + 1);
                fillBox(cv, RGB888(0, 170, 0), x + knob[r], yy - 3, x + knob[r] + 5, yy + 4);
            }
            break;
        }
        case TILE_RESET:    // power symbol: ring open at the top, bar through the gap
            cv.setBrushColor(RGB888(255, 170, 0));
            cv.fillEllipse(x + 16, y + 17, 26, 26);
            cv.setBrushColor(bg);
            cv.fillEllipse(x + 16, y + 17, 18, 18);
            fillBox(cv, bg, x + 11, y + 2, x + 20, y + 14);
            fillBox(cv, RGB888(255, 170, 0), x + 14, y + 2, x + 17, y + 16);
            break;
        case TILE_ABOUT:    // "i" in a disc
            cv.setBrushColor(RGB888(0, 170, 85));
            cv.fillEllipse(x + 16, y + 16, 28, 28);
            fillBox(cv, TI_WHITE, x + 15, y + 7, x + 17, y + 9);
            fillBox(cv, TI_WHITE, x + 15, y + 12, x + 17, y + 24);
            break;
        case TILE_RESUME:   // play triangle
            for (int r = -12; r <= 12; r++) {
                int len = (12 - abs(r)) * 20 / 12;
                fillBox(cv, RGB888(170, 85, 170), x + 7, y + 16 + r, x + 7 + len, y + 16 + r);
            }
            break;
    }
}

static void drawTile(fabgl::Canvas &cv, int idx, bool selected) {
    int x = TILE_X0 + (idx % TILE_COLS) * TILE_PITCH_X;
    int y = TILE_Y0 + (idx / TILE_COLS) * TILE_PITCH_Y;
    fabgl::RGB888 bg = selected ? TI_DKBLUE : TI_WHITE;
    fabgl::RGB888 fg = selected ? TI_WHITE : TI_BLACK;

    fillBox(cv, bg, x, y, x + TILE_W - 1, y + TILE_H - 1);
    cv.setPenColor(selected ? TI_BLACK : TI_DKBLUE);
    cv.drawRectangle(x, y, x + TILE_W - 1, y + TILE_H - 1);
    if (selected) cv.drawRectangle(x + 1, y + 1, x + TILE_W - 2, y + TILE_H - 2);

    drawIcon(cv, idx, x + (TILE_W - 32) / 2, y + 6, bg, fg);

    cv.selectFont(&fabgl::FONT_8x8);
    cv.setBrushColor(bg);
    cv.setPenColor(fg);
    cv.drawText(x + (TILE_W - 8 * (int)strlen(tileLabels[idx])) / 2, y + 44, tileLabels[idx]);
}

// returns the chosen tile, or -1 on Esc/F12
static int mainMenu(int selected) {
    fabgl::Canvas cv(disp);
    drawFrame(cv, "TI-99/4A SUPERVISOR", "Arrows   ENTER Select   ESC/F12 Exit");

    if (selected < 0 || selected >= TILE_COUNT) selected = 0;
    int drawn = -1;
    for (;;) {
        // first pass draws every tile; afterwards only the two that changed
        for (int i = 0; i < TILE_COUNT; i++) {
            if (drawn < 0 || i == drawn || i == selected) drawTile(cv, i, i == selected);
        }
        drawn = selected;
        cv.waitCompletion();
        DBG("menu: SUPERVISOR > %s\n", tileLabels[selected]);

        switch (waitKey()) {
            case VirtualKey::VK_LEFT:     if (selected % TILE_COLS > 0) selected--; break;
            case VirtualKey::VK_RIGHT:    if (selected % TILE_COLS < TILE_COLS - 1 && selected < TILE_COUNT - 1) selected++; break;
            case VirtualKey::VK_UP:       if (selected >= TILE_COLS) selected -= TILE_COLS; break;
            case VirtualKey::VK_DOWN:     if (selected + TILE_COLS < TILE_COUNT) selected += TILE_COLS; break;
            case VirtualKey::VK_RETURN:
            case VirtualKey::VK_KP_ENTER: return selected;
            case VirtualKey::VK_ESCAPE:   return -1;
            case VirtualKey::VK_F12:      closeAll = true; return -1;
            default: break;
        }
    }
}

void menuRun(fabgl::VGAController *display, fabgl::Keyboard *keyboard) {
    disp = display;
    kbd = keyboard;
    emuPause(true);
    soundMute(true);
    tiKeyReleaseAll();

    bool needReset = false;
    closeAll = false;
    int sel = 0;
    while (!closeAll) {
        sel = mainMenu(sel);
        if (sel < 0 || sel == TILE_RESUME) break;
        switch (sel) {
            case TILE_CART:
                if (cartMenu()) { needReset = true; closeAll = true; }
                break;
            case TILE_DISKS: diskMenu(); break;
            case TILE_SETUP: setupMenu(); break;
            case TILE_RESET:
                if (confirm("Reset console", "Reset the TI-99/4A?", nullptr)) { needReset = true; closeAll = true; }
                break;
            case TILE_ABOUT: aboutScreen(); break;
        }
    }

    configSave();
    if (needReset) emuReset();
    soundMute(false);
    emuPause(false);
}
