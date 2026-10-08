/*
 * ============================================================
 *        CoCo 2&3 Emulator for ESP32-TTGO-VGA32-COCO
 *   (C) 2026 Reinaldo Torres / CoCo Byte Club
 *   https://github.com/reyco2000/TTGO-VGA32-COCO
 *   Based on XRoar , co-developed with Claude Code
 *   GPL-3.0-or-later License
 * ============================================================
 *  File   : sv_filebrowser.cpp
 *  Module : SD card file browser — FAT32 directory browser for .DSK/.VDK disk image selection
 * ============================================================
*/

/*
 * sv_filebrowser.cpp - SD card file browser for disk images
 *
 * Browses FAT32 SD card directories.
 * Supports .DSK (JVC) and .VDK files. DMK is listed dimmed, not mountable.
 *
 * Disk Manager screen: drive buttons on top, file list below, popups for
 * mount / replace / unmount. Drawn in the wide green frame.
 */

#include "sv_filebrowser.h"
#include "supervisor.h"
#include "sv_disk.h"
#include "sv_render.h"
#include "../utils/debug.h"
#include "../../config.h"
#include <SD.h>
#include <string.h>
#include <esp_heap_caps.h>

// External canvas instance from hal_video.cpp
extern OSDCanvas* hal_video_get_canvas(void);

// HID usage codes
#define HID_UP      0x52
#define HID_DOWN    0x51
#define HID_ENTER   0x28
#define HID_ESC     0x29
#define HID_BS      0x2A
#define HID_TAB     0x2B
#define HID_PGUP    0x4B
#define HID_PGDN    0x4E
#define HID_HOME    0x4A
#define HID_END     0x4D
#define HID_LEFT    0x50
#define HID_RIGHT   0x4F
#define HID_F       0x09
#define HID_F1      0x3A

// Layout inside the wide green frame: drive buttons on top, file list below.
#define FB_BTN_W      128
#define FB_BTN_H      42
#define FB_BTN_PITCH  134
#define FB_BTN_X0     (SVW_BOX_X + (SVW_BOX_W - ((SV_DISK_MAX_DRIVES - 1) * FB_BTN_PITCH + FB_BTN_W)) / 2)
#define FB_BTN_Y      (SVW_BOX_Y + 32)
#define FB_NAME_COLS  14                    // mounted name: two lines of 14
#define FB_LIST_X     (SVW_BOX_X + 8)
#define FB_LIST_Y     (SVW_BOX_Y + 78)
#define FB_LIST_W     (SVW_BOX_W - 24)
#define FB_ROW_H      10                    // dense rows: 8x8 font
#define FB_ROWS       SV_FB_VISIBLE_ITEMS
#define FB_SB_X       (SVW_BOX_X + SVW_BOX_W - 11)

#define POP_MSG_COLS  SVP_MSG_COLS

// Selected file path buffer
static char selected_path[256];
static bool has_selection = false;

bool sv_fb_is_disk_image(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".dsk") == 0 ||
            strcasecmp(ext, ".vdk") == 0 ||
            strcasecmp(ext, ".dmk") == 0);
}

static bool is_supported_format(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".dsk") == 0 ||
            strcasecmp(ext, ".vdk") == 0);
    // DMK is recognized but not fully supported
}

static int strcasecmp_wrapper(const char* a, const char* b) {
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (ca != cb) return ca - cb;
        a++; b++;
    }
    return *a - *b;
}

int sv_fb_scan_directory(const char* path, SV_FileEntry* entries, int max_entries) {
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return 0;
    }

    int count = 0;

    // Add ".." unless at root
    if (strcmp(path, "/") != 0) {
        strncpy(entries[count].name, "..", sizeof(entries[count].name));
        entries[count].size = 0;
        entries[count].is_dir = true;
        entries[count].is_supported = false;
        count++;
    }

    File entry = dir.openNextFile();
    while (entry && count < max_entries) {
        const char* fullpath = entry.name();
        // ESP32 SD returns full path — extract just the filename
        const char* name = strrchr(fullpath, '/');
        name = name ? name + 1 : fullpath;
        // Skip hidden files
        if (name[0] == '.') {
            entry = dir.openNextFile();
            continue;
        }

        strncpy(entries[count].name, name, sizeof(entries[count].name) - 1);
        entries[count].name[sizeof(entries[count].name) - 1] = '\0';
        entries[count].is_dir = entry.isDirectory();
        entries[count].size = entry.isDirectory() ? 0 : entry.size();
        entries[count].is_supported = entries[count].is_dir ? false : is_supported_format(name);

        count++;
        entry = dir.openNextFile();
    }
    dir.close();

    return count;
}

void sv_fb_sort_entries(SV_FileEntry* entries, int count) {
    // Simple insertion sort (max 128 entries)
    for (int i = 1; i < count; i++) {
        SV_FileEntry temp = entries[i];
        int j = i - 1;

        while (j >= 0) {
            bool swap = false;

            // ".." always first
            if (strcmp(temp.name, "..") == 0) {
                swap = true;
            } else if (strcmp(entries[j].name, "..") == 0) {
                swap = false;
            }
            // Directories before files
            else if (temp.is_dir && !entries[j].is_dir) {
                swap = true;
            } else if (!temp.is_dir && entries[j].is_dir) {
                swap = false;
            }
            // Within same type: alphabetical (case-insensitive)
            else if (temp.is_dir == entries[j].is_dir) {
                // Supported files before unsupported
                if (!temp.is_dir && temp.is_supported != entries[j].is_supported) {
                    swap = temp.is_supported && !entries[j].is_supported;
                } else {
                    swap = strcasecmp_wrapper(temp.name, entries[j].name) < 0;
                }
            }

            if (swap) {
                entries[j + 1] = entries[j];
                j--;
            } else {
                break;
            }
        }
        entries[j + 1] = temp;
    }
}

// ============================================================
// Screen state
// ============================================================

static bool fb_in_drives = false;       // focus: drive buttons or file list

// What is on screen, so a keypress repaints only what changed.
static struct {
    bool    valid;                      // false = repaint the whole screen
    bool    list_valid;                 // false = repaint the list (new folder)
    bool    in_drives;
    uint8_t drive;
    int16_t cursor;
    int16_t scroll;
} fb_drawn;

// Popup over the Disk Manager: drive picker or a Yes/No confirmation.
enum FB_Popup : uint8_t { POP_NONE, POP_MOUNT, POP_REPLACE, POP_AGAIN, POP_UNMOUNT, POP_FAILED };

static struct {
    FB_Popup    kind;
    const char* title;
    char        msg1[POP_MSG_COLS + 1];
    char        msg2[POP_MSG_COLS + 1];
    int8_t      count;                  // option rows
    int8_t      sel;
    int8_t      drawn_sel;              // -1 = draw the whole window
    uint8_t     drive;                  // drive the pending action applies to
} pop;

void sv_filebrowser_invalidate(void) {
    fb_drawn.valid = false;
    pop.drawn_sel = -1;
}

void sv_filebrowser_init(Supervisor_t* sv) {
    sv->file_cursor = 0;
    sv->file_scroll_offset = 0;
    sv->file_count = 0;
    has_selection = false;
    pop.kind = POP_NONE;
    sv_filebrowser_invalidate();
}

void sv_filebrowser_open(Supervisor_t* sv, const char* path, uint8_t target_drive) {
    sv->target_drive = target_drive;
    // path may be sv->current_path itself
    if (path != sv->current_path) {
        strncpy(sv->current_path, path, sizeof(sv->current_path) - 1);
        sv->current_path[sizeof(sv->current_path) - 1] = '\0';
    }

    sv->file_count = 0;
    sv->file_cursor = 0;
    sv->file_scroll_offset = 0;
    sv->needs_redraw = true;
    has_selection = false;
    pop.kind = POP_NONE;
    fb_drawn.list_valid = false;

    if (!sv->file_entries) {
        size_t bytes = SV_FB_MAX_ENTRIES * sizeof(SV_FileEntry);
        sv->file_entries = (SV_FileEntry*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
        if (!sv->file_entries) {
            DEBUG_PRINT("FileBrowser: failed to allocate file entries");
            fb_in_drives = true;
            return;
        }
    }

    // Scan directory (SD reads happen here, before any rendering)
    sv->file_count = sv_fb_scan_directory(sv->current_path, sv->file_entries, SV_FB_MAX_ENTRIES);
    sv_fb_sort_entries(sv->file_entries, sv->file_count);
    fb_in_drives = (sv->file_count == 0);

    DEBUG_PRINTF("FileBrowser: %s -> %d entries", sv->current_path, sv->file_count);
}

static void navigate_to_parent(Supervisor_t* sv) {
    char* last_slash = strrchr(sv->current_path, '/');
    if (last_slash && last_slash != sv->current_path) {
        *last_slash = '\0';
    } else {
        strcpy(sv->current_path, "/");
    }
    sv_filebrowser_open(sv, sv->current_path, sv->target_drive);
}

static void enter_directory(Supervisor_t* sv, const char* dirname) {
    if (strcmp(dirname, "..") == 0) {
        navigate_to_parent(sv);
        return;
    }

    char new_path[256];
    if (strcmp(sv->current_path, "/") == 0) {
        snprintf(new_path, sizeof(new_path), "/%s", dirname);
    } else {
        snprintf(new_path, sizeof(new_path), "%s/%s", sv->current_path, dirname);
    }
    sv_filebrowser_open(sv, new_path, sv->target_drive);
}

// ============================================================
// Mount / unmount dialogs
// ============================================================

static inline SV_DiskController* fb_fdc(Supervisor_t* sv) {
    return sv->machine ? &sv->machine->fdc : nullptr;
}

// Path of the image on a drive, or NULL when the drive is empty.
static const char* drive_path(Supervisor_t* sv, int drive) {
    SV_DiskController* fdc = fb_fdc(sv);
    if (!fdc || !sv_disk_is_mounted(fdc, drive)) return nullptr;
    const char* path = sv_disk_get_path(fdc, drive);
    return path ? path : "?";
}

// File name of a path without its folder and extension.
static void disk_title(const char* path, char* out, size_t size) {
    const char* name = strrchr(path, '/');
    snprintf(out, size, "%s", name ? name + 1 : path);
    char* ext = strrchr(out, '.');
    if (ext && ext != out) *ext = '\0';
}

static void popup_open(Supervisor_t* sv, FB_Popup kind, const char* title, int count, int sel) {
    // A follow-up popup may be smaller than the one it replaces: repaint
    // the screen underneath first.
    if (pop.kind != POP_NONE) fb_drawn.valid = false;
    pop.kind = kind;
    pop.title = title;
    pop.count = count;
    pop.sel = sel;
    pop.drawn_sel = -1;
    sv->needs_redraw = true;
}

// Closing a popup repaints the whole screen underneath.
static void popup_close(Supervisor_t* sv) {
    pop.kind = POP_NONE;
    sv_filebrowser_invalidate();
    sv->needs_redraw = true;
}

// Every confirmation is No / Yes and defaults to No.
static void confirm_open(Supervisor_t* sv, FB_Popup kind, const char* title) {
    popup_open(sv, kind, title, 2, 0);
}

static void popup_row(Supervisor_t* sv, int idx, char* label, size_t lsize, char* value, size_t vsize) {
    label[0] = value[0] = '\0';
    if (pop.kind == POP_MOUNT) {
        if (idx >= SV_DISK_MAX_DRIVES) { snprintf(label, lsize, "Cancel"); return; }
        snprintf(label, lsize, "Drive %d", idx);
        const char* path = drive_path(sv, idx);
        if (path) disk_title(path, value, vsize);
        else      snprintf(value, vsize, "(empty)");
    } else if (pop.kind == POP_FAILED) {
        snprintf(label, lsize, "OK");
    } else {
        snprintf(label, lsize, "%s", idx == 0 ? "No" : "Yes");
    }
}

static void do_mount(Supervisor_t* sv, int drive) {
    SV_DiskController* fdc = fb_fdc(sv);
    bool ok = fdc && sv_disk_mount(fdc, drive, selected_path);
    if (ok) {
        DEBUG_PRINTF("FileBrowser: Mounted %s to drive %d", selected_path, drive);
        supervisor_save_state();    // for quick-mount
        popup_close(sv);
    } else {
        DEBUG_PRINTF("FileBrowser: Failed to mount %s", selected_path);
        snprintf(pop.msg1, sizeof(pop.msg1), "Could not mount the image.");
        pop.msg2[0] = '\0';
        popup_open(sv, POP_FAILED, "Mount failed", 1, 0);
    }
}

// Ask which drive takes the selected image; the first empty drive is preselected.
static void mount_dialog(Supervisor_t* sv, const char* filename) {
    if (strcmp(sv->current_path, "/") == 0) {
        snprintf(selected_path, sizeof(selected_path), "/%s", filename);
    } else {
        snprintf(selected_path, sizeof(selected_path), "%s/%s", sv->current_path, filename);
    }
    has_selection = true;

    int first = 0;
    for (int d = SV_DISK_MAX_DRIVES - 1; d >= 0; d--) {
        if (!drive_path(sv, d)) first = d;
    }
    snprintf(pop.msg1, sizeof(pop.msg1), "%s", filename);
    pop.msg2[0] = '\0';
    popup_open(sv, POP_MOUNT, "Mount disk on", SV_DISK_MAX_DRIVES + 1, first);
}

static void unmount_dialog(Supervisor_t* sv, int drive) {
    const char* path = drive_path(sv, drive);
    if (!path) return;
    pop.drive = drive;
    snprintf(pop.msg1, sizeof(pop.msg1), "Unmount Drive %d?", drive);
    disk_title(path, pop.msg2, sizeof(pop.msg2));
    confirm_open(sv, POP_UNMOUNT, "Unmount disk");
}

static void popup_accept(Supervisor_t* sv) {
    switch (pop.kind) {
        case POP_MOUNT: {
            if (pop.sel >= SV_DISK_MAX_DRIVES) { popup_close(sv); break; }    // Cancel
            int drive = pop.sel;
            pop.drive = drive;
            const char* cur = drive_path(sv, drive);
            if (cur && strcmp(cur, selected_path) == 0) { popup_close(sv); break; }    // already there

            // Confirm before replacing a disk or mounting the image twice.
            if (cur) {
                snprintf(pop.msg1, sizeof(pop.msg1), "Drive %d already has a disk:", drive);
                disk_title(cur, pop.msg2, sizeof(pop.msg2));
                confirm_open(sv, POP_REPLACE, "Replace disk?");
                break;
            }
            for (int o = 0; o < SV_DISK_MAX_DRIVES; o++) {
                const char* other = drive_path(sv, o);
                if (!other || strcmp(other, selected_path) != 0) continue;
                snprintf(pop.msg1, sizeof(pop.msg1), "Already mounted on Drive %d.", o);
                snprintf(pop.msg2, sizeof(pop.msg2), "Mount on Drive %d as well?", drive);
                confirm_open(sv, POP_AGAIN, "Mount again?");
                return;
            }
            do_mount(sv, drive);
            break;
        }

        case POP_REPLACE:
        case POP_AGAIN:
            if (pop.sel == 1) do_mount(sv, pop.drive);
            else              popup_close(sv);
            break;

        case POP_UNMOUNT:
            if (pop.sel == 1 && fb_fdc(sv)) {
                sv_disk_eject(fb_fdc(sv), pop.drive);
                supervisor_save_state();
            }
            popup_close(sv);
            break;

        default:
            popup_close(sv);
            break;
    }
}

static void popup_on_key(Supervisor_t* sv, uint8_t hid_usage) {
    switch (hid_usage) {
        case HID_UP:
            if (pop.sel > 0) { pop.sel--; sv->needs_redraw = true; }
            break;
        case HID_DOWN:
            if (pop.sel < pop.count - 1) { pop.sel++; sv->needs_redraw = true; }
            break;
        case HID_ENTER:
            popup_accept(sv);
            break;
        case HID_ESC:
            popup_close(sv);
            break;
    }
}

// ============================================================
// Keys
// ============================================================

static void list_keep_cursor_visible(Supervisor_t* sv) {
    if (sv->file_cursor < 0) sv->file_cursor = 0;
    if (sv->file_cursor > sv->file_count - 1) sv->file_cursor = sv->file_count - 1;
    if (sv->file_cursor < sv->file_scroll_offset) {
        sv->file_scroll_offset = sv->file_cursor;
    }
    if (sv->file_cursor >= sv->file_scroll_offset + FB_ROWS) {
        sv->file_scroll_offset = sv->file_cursor - FB_ROWS + 1;
    }
}

void sv_filebrowser_on_key(Supervisor_t* sv, uint8_t hid_usage, bool pressed) {
    if (!pressed) return;

    // F1 closes the whole supervisor from any level.
    if (hid_usage == HID_F1) {
        pop.kind = POP_NONE;
        supervisor_toggle();
        return;
    }
    if (pop.kind != POP_NONE) {
        popup_on_key(sv, hid_usage);
        return;
    }

    switch (hid_usage) {
        case HID_ESC:
            sv->state = SV_MAIN_MENU;
            sv->menu_cursor = 0;  // land on the "Disks" tile
            sv->needs_redraw = true;
            return;

        case HID_BS:
            navigate_to_parent(sv);
            return;

        case HID_F: {
            // Write cached changes to the SD card: the focused drive, or all
            // drives when the file list has the focus.
            SV_DiskController* fdc = fb_fdc(sv);
            if (!fdc) return;
            if (!fb_in_drives) {
                sv_disk_flush_all(fdc);
                DEBUG_PRINT("disks: flush all drives");
            } else if (sv_disk_is_mounted(fdc, sv->target_drive)) {
                sv_disk_flush(fdc, sv->target_drive);
                DEBUG_PRINTF("disks: flush Drive %d", sv->target_drive);
            }
            return;
        }
    }

    if (fb_in_drives) {
        switch (hid_usage) {
            case HID_LEFT:
                if (sv->target_drive > 0) sv->target_drive--;
                break;
            case HID_RIGHT:
                if (sv->target_drive < SV_DISK_MAX_DRIVES - 1) sv->target_drive++;
                break;
            case HID_DOWN:
            case HID_TAB:
                if (sv->file_count > 0) fb_in_drives = false;
                break;
            case HID_ENTER:
                unmount_dialog(sv, sv->target_drive);
                break;
        }
    } else {
        switch (hid_usage) {
            case HID_UP:
                if (sv->file_cursor > 0) sv->file_cursor--;
                else fb_in_drives = true;
                break;
            case HID_DOWN: sv->file_cursor++; break;
            case HID_PGUP: sv->file_cursor -= FB_ROWS; break;
            case HID_PGDN: sv->file_cursor += FB_ROWS; break;
            case HID_HOME: sv->file_cursor = 0; break;
            case HID_END:  sv->file_cursor = sv->file_count - 1; break;
            case HID_TAB:  fb_in_drives = true; break;

            case HID_ENTER: {
                SV_FileEntry* e = &sv->file_entries[sv->file_cursor];
                if (e->is_dir) {
                    enter_directory(sv, e->name);
                } else if (e->is_supported) {
                    mount_dialog(sv, e->name);
                }
                return;
            }
        }
        list_keep_cursor_visible(sv);
    }
    sv->needs_redraw = true;
}

// ============================================================
// Drawing
// ============================================================

// 32x14 diskette: jacket, label, hub. Red when mounted, grey when empty.
static void draw_small_disk(OSDCanvas* tft, int x, int y, bool mounted) {
    tft->fillRect(x, y, 32, 14, mounted ? SVW_RED : SVW_GRAY);
    tft->fillRect(x + 6, y + 1, 20, 4, SVW_WHITE);
    tft->fillEllipse(x + 16, y + 9, 12, 5, mounted ? SVW_BLACK : SVW_DKGRAY);
}

static void draw_drive_button(OSDCanvas* tft, Supervisor_t* sv, int drive, bool focused) {
    int x = FB_BTN_X0 + drive * FB_BTN_PITCH, y = FB_BTN_Y;
    const char* path = drive_path(sv, drive);
    uint16_t bg = focused ? SVW_DKBLUE : SVW_WHITE;
    uint16_t fg = focused ? SVW_WHITE : SVW_BLACK;
    uint16_t edge = focused ? SVW_BLACK : SVW_DKBLUE;

    tft->fillRect(x, y, FB_BTN_W, FB_BTN_H, bg);
    tft->drawRect(x, y, FB_BTN_W, FB_BTN_H, edge);
    if (focused) tft->drawRect(x + 1, y + 1, FB_BTN_W - 2, FB_BTN_H - 2, edge);

    draw_small_disk(tft, x + 8, y + 5, path != nullptr);

    char text[16];
    tft->setTextFont(1);
    tft->setTextDatum(TL_DATUM);
    tft->setTextColor(fg, bg);
    snprintf(text, sizeof(text), "DRIVE %d", drive);
    tft->drawString(text, x + 50, y + 8);

    // image name on two lines, without its extension
    char name[64];
    if (path) disk_title(path, name, sizeof(name)); else strcpy(name, "empty");
    tft->setTextColor(path ? fg : SVW_GRAY, bg);
    snprintf(text, sizeof(text), "%.*s", FB_NAME_COLS, name);
    tft->drawString(text, x + 8, y + 22);
    if (strlen(name) > FB_NAME_COLS) {
        snprintf(text, sizeof(text), "%.*s", FB_NAME_COLS, name + FB_NAME_COLS);
        tft->drawString(text, x + 8, y + 32);
    }
}

// One list row: name on the left, <DIR> or the size right-aligned in the
// accent colour. The selected row is a full-width accent bar with white text.
static void draw_file_row(OSDCanvas* tft, Supervisor_t* sv, int row, bool highlighted) {
    int idx = sv->file_scroll_offset + row;
    int y = FB_LIST_Y + row * FB_ROW_H;
    uint16_t bg = highlighted ? SVW_DKBLUE : SVW_GREEN;

    tft->fillRect(FB_LIST_X, y, FB_LIST_W, FB_ROW_H, bg);
    if (idx >= sv->file_count) return;

    SV_FileEntry* e = &sv->file_entries[idx];
    bool usable = e->is_dir || e->is_supported;
    char value[12];
    if (e->is_dir) snprintf(value, sizeof(value), "<DIR>");
    else           snprintf(value, sizeof(value), "%luK", (unsigned long)((e->size + 1023) / 1024));

    tft->setTextFont(1);
    tft->setTextDatum(TL_DATUM);
    if (highlighted) tft->setTextColor(usable ? SVW_WHITE : SVW_GRAY, bg);
    else             tft->setTextColor(usable ? SVW_BLACK : SVW_DIM, bg);
    tft->drawString(e->name, FB_LIST_X + 8, y + 1);

    tft->setTextColor(highlighted ? SVW_WHITE : (usable ? SVW_DKBLUE : SVW_DIM), bg);
    tft->setTextDatum(TR_DATUM);
    tft->drawString(value, FB_LIST_X + FB_LIST_W - 8, y + 1);
    tft->setTextDatum(TL_DATUM);
}

// Thin scrollbar on the right, only when the list is longer than the page.
static void draw_list_scrollbar(OSDCanvas* tft, Supervisor_t* sv) {
    int h = FB_ROWS * FB_ROW_H;
    tft->fillRect(FB_SB_X, FB_LIST_Y, 4, h, SVW_GREEN);
    if (sv->file_count <= FB_ROWS) return;
    int len = h * FB_ROWS / sv->file_count;
    if (len < 8) len = 8;
    int pos = (h - len) * sv->file_scroll_offset / (sv->file_count - FB_ROWS);
    tft->fillRect(FB_SB_X, FB_LIST_Y + pos, 4, len, SVW_DKBLUE);
}

static void draw_list(OSDCanvas* tft, Supervisor_t* sv) {
    for (int r = 0; r < FB_ROWS; r++) {
        draw_file_row(tft, sv, r, !fb_in_drives && sv->file_scroll_offset + r == sv->file_cursor);
    }
    if (sv->file_count == 0) {
        tft->setTextFont(1);
        tft->setTextDatum(TL_DATUM);
        tft->setTextColor(SVW_DIM, SVW_GREEN);
        tft->drawString(sv->file_entries ? "(empty folder)" : "(out of memory)", FB_LIST_X + 8, FB_LIST_Y + 1);
    }
    draw_list_scrollbar(tft, sv);
}

static void draw_popup_row(Supervisor_t* sv, int y, int idx, bool highlighted) {
    char label[24], value[40];
    popup_row(sv, idx, label, sizeof(label), value, sizeof(value));
    value[30] = '\0';   // leave room for the label
    sv_render_popup_row(y, label, value, highlighted);
    if (highlighted) DEBUG_PRINTF("popup: %s > %s %s", pop.title, label, value);
}

// Whole window on the first pass, then only the two rows that changed.
static void draw_popup(Supervisor_t* sv) {
    bool all = (pop.drawn_sel < 0);
    int ry = sv_render_popup(pop.title, pop.msg1, pop.msg2, pop.count, all);
    if (all) {
        DEBUG_PRINTF("popup: %s: %s %s", pop.title, pop.msg1, pop.msg2);
        for (int i = 0; i < pop.count; i++) {
            draw_popup_row(sv, ry + i * SVP_ROW_H, i, i == pop.sel);
        }
    } else if (pop.drawn_sel != pop.sel) {
        draw_popup_row(sv, ry + pop.drawn_sel * SVP_ROW_H, pop.drawn_sel, false);
        draw_popup_row(sv, ry + pop.sel * SVP_ROW_H, pop.sel, true);
    }
    pop.drawn_sel = pop.sel;
}

void sv_filebrowser_render(Supervisor_t* sv) {
    OSDCanvas* tft = hal_video_get_canvas();
    if (!tft) return;

    if (!fb_drawn.valid) {
        // Whole screen: frame, every drive button, the list.
        sv_render_wide_frame("Disk Manager", "Arrows  TAB Drives/Files  ENTER  BS Up  F Flush  ESC Back");
        for (int d = 0; d < SV_DISK_MAX_DRIVES; d++) {
            draw_drive_button(tft, sv, d, fb_in_drives && d == sv->target_drive);
        }
        draw_list(tft, sv);
    } else if (pop.kind == POP_NONE) {
        // Only what the last keypress changed.
        bool focus_moved = (fb_drawn.in_drives != fb_in_drives);
        if (focus_moved || fb_drawn.drive != sv->target_drive) {
            draw_drive_button(tft, sv, fb_drawn.drive, fb_in_drives && fb_drawn.drive == sv->target_drive);
            draw_drive_button(tft, sv, sv->target_drive, fb_in_drives);
        }
        if (!fb_drawn.list_valid || fb_drawn.scroll != sv->file_scroll_offset) {
            draw_list(tft, sv);
        } else if (focus_moved || fb_drawn.cursor != sv->file_cursor) {
            draw_file_row(tft, sv, fb_drawn.cursor - sv->file_scroll_offset, false);
            draw_file_row(tft, sv, sv->file_cursor - sv->file_scroll_offset, !fb_in_drives);
        }
    }
    fb_drawn.valid = true;
    fb_drawn.list_valid = true;
    fb_drawn.in_drives = fb_in_drives;
    fb_drawn.drive = sv->target_drive;
    fb_drawn.cursor = sv->file_cursor;
    fb_drawn.scroll = sv->file_scroll_offset;

    if (pop.kind != POP_NONE) {
        draw_popup(sv);
    } else if (fb_in_drives) {
        const char* path = drive_path(sv, sv->target_drive);
        DEBUG_PRINTF("disks: [Drive %d] %s", sv->target_drive, path ? path : "(empty)");
    } else if (sv->file_count > 0) {
        DEBUG_PRINTF("disks: file %s", sv->file_entries[sv->file_cursor].name);
    }
}

const char* sv_filebrowser_get_selected_path(Supervisor_t* sv) {
    (void)sv;
    return has_selection ? selected_path : nullptr;
}
