// sms_ui.h
#ifndef SMS_UI_H
#define SMS_UI_H

#include <SD.h>
#include <vector>
#include <string>
#include "sms_vga_adapter.h"
#include "sms_keyboard.h"

// ==================== COLOR DEFINITIONS ====================
#define UI_COLOR_BLACK      0x0000
#define UI_COLOR_WHITE      0xFFFF
#define UI_COLOR_RED        0xF800
#define UI_COLOR_GREEN      0x07E0
#define UI_COLOR_BLUE       0x001F
#define UI_COLOR_YELLOW     0xFFE0
#define UI_COLOR_CYAN       0x07FF
#define UI_COLOR_PURPLE     0x801F
#define UI_COLOR_GRAY       0x8410

// UI Color Scheme
#define BG_COLOR                UI_COLOR_BLACK
#define BAR_COLOR               UI_COLOR_BLUE
#define BAR_TEXT_COLOR          UI_COLOR_WHITE
#define TEXT_COLOR              UI_COLOR_WHITE
#define SELECTED_TEXT_COLOR     UI_COLOR_BLACK
#define SELECTED_BG_COLOR       UI_COLOR_YELLOW
#define BOX_BORDER_COLOR        UI_COLOR_CYAN

// ==================== UI CLASS ====================
class SMS_UI
{
public:
    SMS_UI(SMSVGADisplay* screen);
    ~SMS_UI();
    
    std::string selectGame();
    void getGameFiles();
    void drawFileList();
    void drawBars();
    void showLoadingMessage(const char*);
    
private:
    void drawText(const char* text, const int x, const int y);
    
    SMSVGADisplay* screen = nullptr;
    int selected = 0;
    int scroll_offset = 0;
    int max_items = 0;
    static constexpr int ITEM_HEIGHT = 14;
    std::vector<std::string> files;
};

#endif