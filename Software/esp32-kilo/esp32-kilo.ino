/*
    KILO text editor
*/

#include "fabgl.h"
//#include "SPIFFS.h"

fabgl::VGA16Controller   DisplayController;
fabgl::Terminal          Terminal;
fabgl::PS2Controller     PS2Controller;

#include <SD.h>
#include <SPI.h>
#include <Update.h>
//-------------------------------------- Verwendung der SD-Karte ----------------------------------------------------------------------------------
//SPI CLASS FOR REDEFINED SPI PINS !
SPIClass spiSD(HSPI);
//Konfiguration der SD-Karte unter FabGl - kann mit OPT geändert werden
#define kSD_CS   13
#define kSD_MISO 16
#define kSD_MOSI 17
#define kSD_CLK  14
#define SD_LED   2

short int colour = 0;
#include <EEPROM.h>
#define EEPROM_SIZE 512
/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me−no−dev/arduino−esp32fs−plugin */
//#define FORMAT_SPIFFS_IF_FAILED false

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 2

enum editorKey {
  BACKSPACE = 127,
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN,
  ENTER_KEY,
  ESCAPE_KEY,
  F1_KEY,
  F2_KEY,
  F3_KEY,
  F4_KEY,
  F5_KEY,
  F6_KEY,
  F7_KEY,
  F8_KEY,
  F9_KEY
};

typedef struct erow {
  int size;
  int rsize;
  char *chars;
  char *render;
} erow;

struct editorConfig {
  int cx, cy;
  int rx;
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  int dirty;
  int numrows;
  erow *row;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
}; struct editorConfig E;

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t')
      rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
    rx++;
  }
  return rx;
}

void editorUpdateRow(erow *row) {
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t') tabs++;
  free(row->render);
  row->render = (char *)malloc(row->size + tabs * (KILO_TAB_STOP - 1) + 1);
  int idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % KILO_TAB_STOP != 0) row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;
}

void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > E.numrows) return;
  E.row = (erow *)realloc(E.row, sizeof(erow) * (E.numrows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
  E.row[at].size = len;
  E.row[at].chars = (char *)malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';
  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  editorUpdateRow(&E.row[at]);
  E.numrows++;
  E.dirty++;
}

void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
}
void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows) return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
  E.numrows--;
  E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = (char *)realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = (char *)realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  E.dirty++;
}

void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    editorInsertRow(E.numrows, "", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  E.cx++;
}

void editorInsertNewline() {
  if (E.cx == 0) {
    editorInsertRow(E.cy, "", 0);
  } else {
    erow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
    row = &E.row[E.cy];
    row->size = E.cx;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  E.cy++;
  E.cx = 0;
}

void editorDelChar() {
  if (E.cy == E.numrows) return;
  if (E.cx == 0 && E.cy == 0) return;
  erow *row = &E.row[E.cy];
  if (E.cx > 0) {
    editorRowDelChar(row, E.cx - 1);
    E.cx--;
  } else {
    E.cx = E.row[E.cy - 1].size;
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
  }
}

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size) return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editorUpdateRow(row);
  E.dirty++;
}

size_t getline(char *buf, size_t *size, File *stream)
{
  char c;
  size_t count = 0;

  while (c != '\r') {
    if (stream->available()) {
      c = stream->read();
      if (c == '\n') continue;
      buf[count] = c;
      count++;
    } else {
      if (count == 0) return -1;
      else break;
    }
  }

  buf[count] = '\0';
  return count;
}

char *editorRowsToString(int *buflen) {
  int totlen = 0;
  int j;
  for (j = 0; j < E.numrows; j++)
    totlen += E.row[j].size + 1;
  *buflen = totlen;
  char *buf = (char *)malloc(totlen);
  char *p = buf;
  for (j = 0; j < E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\r';
    p++;
  } *--p = '\0';
  return buf;
}

void editorSave(fs::FS &fs) {
  if (E.filename == NULL) return;
  File fp = fs.open(E.filename, FILE_WRITE);
  int len;
  char *buf = editorRowsToString(&len);

  if (!fp) {
    editorSetStatusMessage("Failed to open file");
    return;
  }

  if (fp.print(buf)) editorSetStatusMessage("File written");
  else editorSetStatusMessage("Failed to write file!");
  E.dirty = 0;
  fp.close();
  free(buf);
}

void editorOpen(fs::FS &fs, const char *filename) {
  free(E.filename);
  E.filename = strdup(filename);

  File fp = fs.open(filename);
  if (!fp || fp.isDirectory()) {
    xprintf("− failed to open file for reading\n\r");
    return;
  }

  if (!fp) {
    xprintf("No file found\n\r");
    return;
  }
  char line[200];  // 200 chars per line allowed
  size_t linecap = 0;
  ssize_t linelen;

  while ((linelen = getline(line, &linecap, &fp)) != -1) {
    while (linelen > 0 && (line[linelen - 1] == '\r' )) linelen--;// || line[linelen - 1] == '\r')) linelen--;
    editorInsertRow(E.numrows, line, linelen);
  }
  delay(50);
  fp.close();
  E.dirty = 0;
}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *newb = (char *)realloc(ab->b, ab->len + len);
  if (newb == NULL) return;
  memcpy(&newb[ab->len], s, len);
  ab->b = newb;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}

void editorScroll() {
  E.rx = 0;
  if (E.cy < E.numrows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);
  }
  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screencols + 1;
  }
}

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
                                  "Kilo editor -- version %s", KILO_VERSION);
        if (welcomelen > E.screencols) welcomelen = E.screencols;
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = E.row[filerow].rsize - E.coloff;
      if (len < 0) len = 0;
      if (len >= E.screencols) len = E.screencols - 1;
      abAppend(ab, &E.row[filerow].render[E.coloff], len);
    }
    abAppend(ab, "\x1b[K", 3);     // clear line
    abAppend(ab, "\n\r", 2);
  }
}

void editorDrawStatusBar(struct abuf *ab) {
  abAppend(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
                     E.filename ? E.filename : "[No Name]", E.numrows,
                     E.dirty ? "(modified)" : "");
  int rlen = snprintf(rstatus, sizeof(rstatus), "row %d, col %d",
                      E.cy + 1, E.cx + 1);
  if (len > E.screencols) len = E.screencols;
  abAppend(ab, status, len);
  while (len < E.screencols) {
    if (E.screencols - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols) msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() {
  editorScroll();
  struct abuf ab = ABUF_INIT;
  abAppend(&ab, "\x1b[?25l", 6);  // hide cursor
  abAppend(&ab, "\x1b[H", 3);     // cursor home
  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);
  abAppend(&ab, "\x1b[?25h", 6);  // show cursor
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
           (E.rx - E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));
  Terminal.write(ab.b, ab.len);
  abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  //if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';
  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
  return 0;
}

void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
    case ARROW_LEFT:
      if (E.cx != 0) {
        E.cx--;
      } else if (E.cy > 0) {
        E.cy--;
        E.cx = E.row[E.cy].size;
      }
      break;
    case ARROW_RIGHT:
      if (row && E.cx < row->size) {
        E.cx++;
      } else if (row && E.cx == row->size) {
        E.cy++;
        E.cx = 0;
      }
      break;
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case ARROW_DOWN:
      if (E.cy < E.numrows) {
        E.cy++;
      }
      break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

int editorReadKey() {
  auto keyboard = PS2Controller.keyboard();
  while (!keyboard->virtualKeyAvailable()) {
    /* wait for a key press */
  }
  VirtualKeyItem item;
  if (keyboard->getNextVirtualKey(&item)) {
    // debug
    /*
      if (item.down) {
      xprintf("Scan codes: ");
      xprintf("ctrl %d  0x%02X 0x%02X 0x%02X\n\r", item.scancode[0], item.scancode[1], item.scancode[2]);
      delay(1000);
      }
    */
    if (item.down) {
        Serial.println(item.scancode[0]);
        Serial.println(item.scancode[1]);
        Serial.println();
      if (item.scancode[0] == 0xE0) {
        switch (item.scancode[1]) {
          case 0x6B: return ARROW_LEFT;
          case 0x74: return ARROW_RIGHT;
          case 0x75: return ARROW_UP;
          case 0x72: return ARROW_DOWN;
          case 0x71: return DEL_KEY;
          case 0x6C: return HOME_KEY;
          case 0x69: return END_KEY;
          case 0x7D: return PAGE_UP;
          case 0x7A: return PAGE_DOWN;
          
        }
      } else {
        switch (item.scancode[0]) {
          case 0x76: return ESCAPE_KEY;
          case 0x5A: return ENTER_KEY;
          case 0x66: return BACKSPACE;
          case 0x05: return F1_KEY;
          case 0x06: return F2_KEY;
          case 0x04: return F3_KEY;
          case 0x0C: return F4_KEY;
          case 0x03: return F5_KEY;
          case 0x0B: return F6_KEY;
          case 0x83: return F7_KEY;
          case 0x0A: return F8_KEY;
          case 0x01: return F9_KEY;
          default: return item.ASCII;
        }
      }
    } return 0x00;
  }

}

void editorProcessKeypress() {
  int c = editorReadKey();
  if (!c) return;
  switch (c) {
    case F1_KEY:
      setColour();
      break;
    case F2_KEY:
      
      break;
    case F3_KEY:
      
      break;
    case F4_KEY:
      
      break;
    case F5_KEY:
      
      break;
    case F6_KEY:
      
      break;
    case F7_KEY:
      
      break;
    case F8_KEY:
      
      break;
    case F9_KEY:
      
      break;
    
    case ENTER_KEY:
      editorInsertNewline();
      //editorSave(SPIFFS);
      break;
    case HOME_KEY:
      E.cx = 0;
      break;
    case END_KEY:
      if (E.cy < E.numrows)
        E.cx = E.row[E.cy].size;
      break;

    case BACKSPACE:
      if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
      editorDelChar();
      break;
    case ESCAPE_KEY:
      EEPROM.write(200,colour);
      EEPROM.commit();
      editorSave(SD);
      basic_load();
      break;

    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (c == PAGE_UP) {
          E.cy = E.rowoff;
        } else if (c == PAGE_DOWN) {
          E.cy = E.rowoff + E.screenrows - 1;
          if (E.cy > E.numrows) E.cy = E.numrows;
        }
        int times = E.screenrows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;

    default:
      editorInsertChar(c);
      editorSetStatusMessage("");
      break;
  }
}

void setColour(){
  colour+=1;
  if(colour>8) colour=0;
  switch (colour){
    case 0:
      Terminal.setBackgroundColor(Color::Blue);
      Terminal.setForegroundColor(Color::BrightCyan);
      break;
    case 1:
      Terminal.setBackgroundColor(Color::Red);
      Terminal.setForegroundColor(Color::BrightWhite);
      break;
    case 2:
      Terminal.setBackgroundColor(Color::Black);
      Terminal.setForegroundColor(Color::BrightGreen);
      break;
    case 3:
      Terminal.setBackgroundColor(Color::Magenta);
      Terminal.setForegroundColor(Color::Black);
      break;
    case 4:
      Terminal.setBackgroundColor(Color::White);
      Terminal.setForegroundColor(Color::Black);
      break;
    case 5:
      Terminal.setBackgroundColor(Color::Cyan);
      Terminal.setForegroundColor(Color::Blue);
      break;
    case 6:
      Terminal.setBackgroundColor(Color::BrightGreen);
      Terminal.setForegroundColor(Color::Black);
      break;
    case 7:
      Terminal.setBackgroundColor(Color::Blue);
      Terminal.setForegroundColor(Color::BrightYellow);
      break;
    case 8:
      Terminal.setBackgroundColor(Color::BrightYellow);
      Terminal.setForegroundColor(Color::Black);
      break;
}

}

void readFile(fs::FS &fs, const char * path) {
  File file = fs.open(path);
  while (file.available()) xprintf("%c", file.read());
}

void writeFile(fs::FS &fs, const char * path, const char * message) {
  File file = fs.open(path, FILE_WRITE);
  if (!file) {
    xprintf("− failed to open file for writing\n\r");
    return;
  }
  if (file.print(message)) xprintf("− file written\n\r");
  else xprintf("− frite failed\n\r");
}

void deleteFile(fs::FS &fs, const char * path) {
  xprintf("Deleting file: %s\r\n", path);
  if (fs.remove(path)) xprintf("− file deleted\n\r");
  else {
    xprintf("− delete failed\n\r");
  }
}

void listDir(fs::FS &fs) {
  int lines;
  File root = fs.open("/");
  File file = root.openNextFile();
  while (file) {
    lines++;
    xprintf("  FILE: ");
    xprintf("%s", file.name());
    xprintf("\tSIZE: ");
    xprintf("%d\n\r", file.size());
    file = root.openNextFile();
    if (lines == 20) {
      wait_key(0);
      lines = 0;
    }
  }
}

static unsigned short wait_key(bool modes) {
  char c;

  while (1) {
    if (Terminal.available())
    {
      c = Terminal.read();
      break;
    }

  }
  return c;
}

void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rowoff = 0;
  E.coloff = 0;
  E.screenrows = 25;
  E.screencols = 80;
  E.numrows = 0;
  E.row = NULL;
  E.dirty = 0;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
  E.screenrows -= 2;
}

void xprintf(const char * format, ...) {
  va_list ap;
  va_start(ap, format);
  int size = vsnprintf(nullptr, 0, format, ap) + 1;
  if (size > 0) {
    va_end(ap);
    va_start(ap, format);
    char buf[size + 1];
    vsnprintf(buf, size, format, ap);
    Terminal.write(buf);
  } va_end(ap);
}

void setup() {
  // init serial port
  Serial.begin(115200);
  delay(500);  // avoid garbage into the UART
  EEPROM. begin ( EEPROM_SIZE ) ;
  delay(200);

  // ESP32 peripherals setup
  PS2Controller.begin(PS2Preset::KeyboardPort0);
  PS2Controller.keyboard() -> setLayout(&fabgl::GermanLayout);                //deutsche Tastatur
  DisplayController.begin();
  DisplayController.setResolution(VGA_640x480_60Hz);
  Terminal.begin(&DisplayController);
  colour=EEPROM.read(200) - 1;
  setColour();
  
  //Terminal.connectLocally();
  // init SPIFFS
//  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
//    xprintf("SPIFFS Mount Failed");
//    return;
//  }

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1
  SPI.setFrequency(40000000);

  if ( !SD.begin( kSD_CS, spiSD )) {                        //SD-Card starten
    // mount-fehler
    spiSD.end();                                            //unmount
    editorSetStatusMessage("****** Mount fail! *******");
  }
  else
    editorSetStatusMessage("SD-Card mounted");
  // init text editor
  initEditor();

  //listDir(SD);
  editorSetStatusMessage("Loading .....");
  editorOpen(SD, "/tmp.bas"); // you need to write it first via writeFile()
  editorSetStatusMessage("                           Press ESCAPE to save file                            ");

}

void loop() {
  editorRefreshScreen();
  editorProcessKeypress();
}

// perform the actual update from a given stream
void performUpdate(Stream &updateSource, size_t updateSize) {

  if (Update.begin(updateSize, U_FLASH, 2, 1, "Basic")) {
    size_t written = Update.writeStream(updateSource);

    if (Update.end()) {
      editorSetStatusMessage("   Basic32+ loaded successfully, now Reboot   "); 
      if (Update.isFinished()) {
        delay(1000);
        ESP.restart();
      }
    }
  }
}

void basic_load(void) {

  if ( !SD.exists("/basic.bin") ) editorSetStatusMessage("     Basic not found!     ");

  File updateBin = SD.open("/basic.bin");

  if (updateBin) {
    size_t updateSize = updateBin.size();

    if (updateSize > 0) {
      editorSetStatusMessage("             load Basic32+           ");
      //Serial.println("load Basic32+");
      performUpdate(updateBin, updateSize);
    }
    updateBin.close();
  }

}
