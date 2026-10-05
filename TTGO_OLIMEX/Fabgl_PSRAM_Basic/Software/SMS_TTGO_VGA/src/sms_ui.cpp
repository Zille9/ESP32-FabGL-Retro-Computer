// sms_ui.cpp
#include "sms_ui.h"
extern bool backto;  //Marker fuer OTA-Funktion (F12)
SMS_UI::SMS_UI(SMSVGADisplay* screen)
{    
    this->screen = screen;
    selected = 0;
    scroll_offset = 0;
}

SMS_UI::~SMS_UI()
{
}

void SMS_UI::drawBars() 
{
    int screenWidth  = screen->getWidth();
    int screenHeight = screen->getHeight();
    
    // Top bar
    screen->fillRect(0, 0, screenWidth, 24, BAR_COLOR);
    screen->fillRect(0, 22, screenWidth, 2, UI_COLOR_CYAN);
    
    screen->setTextColor(BAR_TEXT_COLOR, BAR_COLOR);
    screen->setCursor(55, 8);
    screen->print("SMS/GG EMULATOR FOR TTGO VGA");
    
    // Bottom bar reduced to 30 pixels
    screen->fillRect(0, screenHeight - 30, screenWidth, 30, BAR_COLOR);
    screen->fillRect(0, screenHeight - 32, screenWidth, 2, UI_COLOR_CYAN);
    
    // First line - Controls (higher up)
    int y1 = screenHeight - 22;
    screen->setTextColor(UI_COLOR_YELLOW, BAR_COLOR);
    screen->setCursor(60, y1);
    screen->print("B1=Y   B2=X");
    
    screen->setTextColor(UI_COLOR_CYAN, BAR_COLOR);
    screen->setCursor(180, y1);
    screen->print("\x18  \x19  \x1A  \x1B");
    
    // Second line - System buttons (directly below)
    int y2 = screenHeight - 12;
    screen->setTextColor(UI_COLOR_GREEN, BAR_COLOR);
    screen->setCursor(30, y2);
    screen->print("START=ENTER   PAUSE=SPACE   EXIT=ESC");
}

void SMS_UI::showLoadingMessage(const char* message)
{
    int screenWidth  = screen->getWidth();
    int screenHeight = screen->getHeight();
    
    // Draw a box similar to the ROM loading box
    screen->fillRect(0, screenHeight/2 - 15, screenWidth, 30, UI_COLOR_BLACK);
    screen->drawRect(screenWidth/2 - 100, screenHeight/2 - 12, 200, 24, UI_COLOR_CYAN);
    screen->setTextColor(UI_COLOR_YELLOW, UI_COLOR_BLACK);
    screen->setCursor((screenWidth - screen->textWidth(message)) / 2, screenHeight/2 - 4);
    screen->print(message);
    screen->updateDisplay();
}

std::string SMS_UI::selectGame()
{
    unsigned int last_input_time = 0;
    constexpr unsigned int delay = 120;
    
    int screenWidth  = screen->getWidth();
    int screenHeight = screen->getHeight();
    
    // Expanded selection box (bottom bar 30px)
    int list_x = 8;
    int list_y = 30;
    int list_w = screenWidth - 16;
    int list_h = screenHeight - 70;
    
    max_items = (list_h - 8) / ITEM_HEIGHT;

    drawBars();
    
    // Show loading message while reading SD card
    showLoadingMessage("Reading files in SD...");
    
    getGameFiles();
    
    // Clear loading message
    screen->fillRect(0, screenHeight/2 - 15, screenWidth, 30, BG_COLOR);
    
    screen->drawRect(list_x, list_y, list_w, list_h, BOX_BORDER_COLOR);
    drawFileList();

    const int size = files.size();
    while (true)
    {
        unsigned int now = millis();

        if (now - last_input_time > delay)
        {
            uint8_t key = SMSKeyboard::read();
            
            if (key & SMSKeyboard::SMS_UP) 
            {
                selected--;
                if (selected < 0) {
                    selected = size - 1;
                    scroll_offset = selected - max_items + 1;
                } else if (selected < scroll_offset) {
                    scroll_offset = selected;
                }
                if (scroll_offset < 0) scroll_offset = 0;
                drawFileList();
                last_input_time = now;
            }

            if (key & SMSKeyboard::SMS_DOWN) 
            {
                selected++;
                if (selected > size - 1) {
                    selected = 0;
                    scroll_offset = 0;
                } else if (selected >= scroll_offset + max_items) {
                    scroll_offset = selected - max_items + 1;
                }
                if (scroll_offset < 0) scroll_offset = 0;
                drawFileList();
                last_input_time = now;
            }
            
            if ((key & SMSKeyboard::SMS_BTN1) || (key & SMSKeyboard::SMS_START))
            {
                if (selected >= 0 && selected < size)
                {
                    std::string fullPath = "/" + files[selected];
                    std::vector<std::string>().swap(files);
                    
                    // Show ROM loading message
                    screen->fillRect(0, screenHeight/2 - 12, screenWidth, 24, UI_COLOR_BLACK);
                    screen->drawRect(screenWidth/2 - 60, screenHeight/2 - 10, 120, 20, UI_COLOR_CYAN);
                    screen->setTextColor(UI_COLOR_YELLOW, UI_COLOR_BLACK);
                    screen->setCursor((screenWidth - screen->textWidth("Loading...")) / 2, screenHeight/2 - 4);
                    screen->print("Loading...");
                    screen->updateDisplay();
                    
                    return fullPath;
                }
            }
            if (backto){  //OTA FUNKTION F12
                screen->fillRect(0, screenHeight/2 - 12, screenWidth, 24, UI_COLOR_BLACK);
                screen->drawRect(screenWidth/2 - 60, screenHeight/2 - 10, 120, 20, UI_COLOR_CYAN);
                screen->setTextColor(UI_COLOR_YELLOW, UI_COLOR_BLACK);
                screen->setCursor((screenWidth - screen->textWidth("Loading Basic...")) / 2, screenHeight/2 - 4);
                screen->print("Loading Basic...");
                screen->updateDisplay();
                return "BASIC";
            } 
        }
    }
}

void SMS_UI::getGameFiles()
{
    files.clear();
    File root = SD.open("/");
    if (!root) return;
    
    while (true)
    {
        File file = root.openNextFile();
        if (!file) break;
        if (!file.isDirectory())
        {
            std::string filename = file.name();
            if (filename.rfind("._", 0) == 0) 
            {
                file.close(); // Wichtig: Datei schließen, bevor wir zum nächsten Schleifendurchlauf springen!
                continue;
            }
            
            std::string ext = filename.substr(filename.find_last_of(".") + 1);
            for (auto& c : ext) c = tolower(c);
            
            if (ext == "sms" || ext == "gg")
                files.push_back(filename);
        }
        file.close();
    }
    root.close();
    
    std::sort(files.begin(), files.end());
}

void SMS_UI::drawFileList()
{
    int screenWidth  = screen->getWidth();
    int screenHeight = screen->getHeight();
    
    // Clear inner area of the box (expanded)
    screen->fillRect(10, 32, screenWidth - 20, screenHeight - 80, BG_COLOR);

    const int size = files.size();
    for (int i = 0; i < max_items; i++)
    {
        int item = i + scroll_offset;
        if (item >= size) break;

        std::string file = files[item];
        int maxWidth = screenWidth - 48;
        
        while (screen->textWidth(file.c_str()) > maxWidth)
            file.pop_back();
        if (file.size() < files[item].size())
            file.replace(file.size() - 3, 3, "...");

        const char* filename = file.c_str();
        int y = 36 + i * ITEM_HEIGHT;
        
        if (item == selected)
        {
            screen->fillRect(12, y - 2, screenWidth - 24, ITEM_HEIGHT, SELECTED_BG_COLOR);
            screen->setTextColor(SELECTED_TEXT_COLOR, SELECTED_BG_COLOR);
            screen->drawString(filename, 20, y, 1);
        }
        else
        {
            screen->setTextColor(TEXT_COLOR, BG_COLOR);
            screen->drawString(filename, 20, y, 1);
        }
    }
}