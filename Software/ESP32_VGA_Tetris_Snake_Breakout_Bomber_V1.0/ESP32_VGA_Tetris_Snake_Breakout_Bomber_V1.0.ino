/********************************************************************
   ESP32 VGA Games - Version 1.0 - April the 12th, 2020
*                                                                   *
   ESP32 arcade game clones for VGA monitors: Tetris, Snake,
   Breakout, Bomber plus a Mandelbrot drawing.
   Written by Roberto Melzi, and based on the FabGL VGA Library by
   Fabrizio Di Vittorio. For more datails see:
*                                                                   *
   http://www.fabglib.org/
*                                                                   *
   See also my other projects on Instructables:
*                                                                   *
   http://www.instructables.com/member/Rob%20Cai/instructables/
*                                                                   *
********************************************************************/

#include "fabgl.h"
fabgl::VGAController    VGAController;
fabgl::Canvas           Canvas(&VGAController);
fabgl::PS2Controller    PS2Controller;
fabgl::Terminal         Terminal;

//------------------------------------------------------------------------------------
#include <SPI.h>
#include <SD.h>

SPIClass spiSD(HSPI);
#include <Update.h>

byte kSD_CS   = 13;
byte kSD_MISO = 16;
byte kSD_MOSI = 17;
byte kSD_CLK  = 14;

//-------------------- button pin definitions -----------------------
byte button1 = -1; //right
byte button2 = -1; //up (or rotate)
byte button3 = -1; //left
byte button4 = -1; //down fast

char key;
//-------------------------------------------------------------------
int backcolor[8][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0}, {1, 0, 1}, {0, 1, 1}, {1, 1, 1}};
//----------------------------------- Common ----------------------------------------------
boolean button;
boolean button_1;
boolean button_2;
boolean button_3;
boolean button_4;
boolean button_5;
boolean button_1_old;
boolean button_2_old;
boolean button_3_old;
boolean button_4_old;
boolean button_5_old;
int colA, colB, colC;
int choice;
int gameSelect = 1;

//---------------------------------- Tetris --------------------------------------
int block[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
int blockExt[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
int blockOld[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
int blockTmp[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
int blockTr[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
int yLine[4] = {0, 0, 0, 0};
int yLineTmp[4] = {0, 0, 0, 0};
int myRGB[3] = {0, 0, 0};
int yCounter = 0;
int x = 60;
int y = 6;
int z = 10;
int score = 1;
int scoreNew = 1;
int noLoop = -1;
int rotationDirection = 1;
int delta = 0;
int color = 1;
int colorOld;
int blockN, blockNext;
int busy;
int noDelete = 0;
int k = 0;
int a = 40;
int b = 10;
int counterMenu = 0;
unsigned long myTime = 0;
int fast = 14; //14;
int resX = 320;
int resY = 200;
int squareX = 7;
int squareY = 6;
int myColumns = 11;
int myRaws = 26;
int level = 1;
int grid[29][11];

int tetris_highscore = 1000;

//--------------------------------------------- Snake ------------------------------------
byte counterMenuSnake = 0;
byte counterMenu2 = 0;
byte state = 1;
byte scoreSnake = 0;
byte levelSnake = 1;
byte scoreMax = 12; //default = 12
int foodX;
int foodY;
int snakeMaxLength = 199;
int sx[200];     // > slength + scoreMax*delta + 1 = 40
int sy[200];
int slength = 9; // snake starting length
int slengthIni = 9; // snake starting length
int deltaSnake = 9;   // snake length increment
int i;
//int x;
//int y;
byte direct = 3;
int speedDelay = 32;
int VGAX_WIDTH = 320;
int VGAX_HEIGHT = 200;
//byte colA, colB, colC;
int x0Area = 100;
int y0Area = 20;
int x1Area = 300;
int y1Area = 180;
float cornerStep = 50.;

int snake_highscore = 0;
//---------------------------------------------------------------------------------------

//------------------------------------------- Breakout ----------------------------------
float ballSpeed;
float ballAngle;
float ballAngle0;
int ballX, ballY, ballXold, ballYold;
int padLength = 24;
float padX;
int levelBreakout;
int breakoutLives;
int sgx = 1;
int sgy = 1;
int sgx0, sgy0;
float vxr, vyr;
int vx, vy;
int hitA, hitB, hitC, hitD, hit, hit0;
int brickCounter, brickCounterOld;
int breakoutSpeed;
float padSpeedStep = 0.25;
float padSpeed;
byte padSpeedMax = 4;
int breakout_highscore = 0;
//---------------------------------------------------------------------------------------

//------------------------------- Bomber ------------------------------------------------
int bomberX = 120;
int bomberL = 22;
float bomberSpeed;
float speedStep = 0.25;
byte bomberSpeedMax = 8;
int bomberDelayOld = 10;
int bomberDelay = 10;
float planeLV = 1;
float planeRV = 0.8;
float planeLX, planeRX;
int planeLY, planeRY;
int planeLXold = 100;
int planeLYold = 40;
int planeRXold = 20;
int planeRYold = 30;
int expX, expY, expXmin, expXmax, expYmin, expYmax;
float bomb1X, bomb1Y, bomb1V, bomb2X, bomb2Y, bomb2V;
int bomb1Xold, bomb1Yold, bomb2Xold, bomb2Yold;
bool bomb1, bomb2;
double time1, time2;
double bomberScoreIni = 0;
double bomberScore = 0;
int bomberLives = 3;
int bomber_highscore = 0;
//---------------------------------------------------------------------------------------

//------------------------------ Mandelbrot ---------------------------------------------
float x1 = -2.4;
float yy1 = -1.4;
float x2 = 1.2;
float y2 = 1.4;
float am;
float a0;
float bm;
float cm;
float dx;
float dy;
int n;
int iteraction;
int iteraction0 = 20;
//int vgaWidth = 640;
//int vgaHeight = 350;
//int vgaHeight = 480;
//----------------------------------------------------------------------------------

//------------------------------------------------------ Setup -----------------------------------------------------------------
void setup()
{
  PS2Controller.begin(PS2Preset::KeyboardPort0);
  // 8 colors
  VGAController.begin();
  VGAController.setResolution(QVGA_320x240_60Hz);
  Canvas.selectFont(&fabgl::FONT_8x8);
  myColor(7);
  //Canvas.setPenColor(colA, colB, colC);

  Canvas.clear();

  Canvas.moveTo(1, 1);
  Terminal.begin(&VGAController);
  Terminal.connectLocally();                                                           // für Terminal Komandos

  randomSeed(analogRead(36));
  //choice = 6;

}

//------------------------------------- main loop --------------------------------------------
//--------------------------------------------------------------------------------------------
void loop() {
  switch (choice) {

    case 1:
      TetrisLoop();
      break;

    case 2:
      SnakeLoop();
      if (choice == 0) {
        Canvas.setPenColor(0, 0, 0);
        Canvas.clear();
      }
      break;

    case 3:
      BreakoutLoop();
      break;

    case 4:
      MandelbrotLoop();
      iteraction += 16;
      if (iteraction > iteraction0 + 16) {
        clearInputs();//button == 0;
        while (button == 0) {
          SnakeProcessInputs();
          delay(50);
        }
        Canvas.clear();
        iteraction = iteraction0;
        choice = 0;
      }
      break;

    case 5:
      BomberLoop();
      Canvas.clear();
      choice = 0;
      break;
      
    case 6:
      basic_loader();
      break;
      
    default:
      ChoiceMenu();
      break;
  }
}
//--------------------------------- end of main loop -----------------------------------------
//--------------------------------------------------------------------------------------------

//------------------------------------------------------ Menu -----------------------------------------------------------------
void ChoiceMenu() {
  clearInputs();
  smoothRect(60, 32, 180, 110 + 20, 10, 7);
  myPrint("Tetris     ", 90, 40, 4);
  myPrint("Snake      ", 90, 60, 5);
  myPrint("Breakout   ", 90, 80, 1);
  myPrint("Mandelbrot ", 90, 100, 6);
  myPrint("Bomber      ", 90, 120, 2);
  myPrint("Basic32+    ", 90, 140, 3);
  myRect(190, 40, 12, 12, 7);
  myRect(190, 60, 12, 12, 7);
  myRect(190, 80, 12, 12, 7);
  myRect(190, 100, 12, 12, 7);
  myRect(190, 120, 12, 12, 7);
  myRect(190, 140, 12, 12, 7);
  delay(100);
  SnakeProcessInputs();//TetrisProcessInputs();
  if (button_4 == 1) {
    myFullRect(191, 41 + (gameSelect - 1) * 20, 10, 10, 0);
    if (gameSelect < 6) {
      gameSelect += 1;
    }
    else {
      gameSelect = 1;
    }
    delay(100);
  }
  if (button_2 == 1) {
    myFullRect(191, 41 + (gameSelect - 1) * 20, 10, 10, 0);
    if (gameSelect > 1) {
      gameSelect -= 1;
    }
    else {
      gameSelect = 6;
    }
    delay(100);
  }
  myFullRect(191, 41 + (gameSelect - 1) * 20, 10, 10, 4);
  if (button_1 == 1 || button_3 == 1) {
    switch (gameSelect) {
      case 1: // Tetris
        Canvas.setPenColor(0, 0, 0);
        Canvas.clear();
        drawGameScreen();
        TetrisDrawScore(score);
        choice = 1;
        break;
      case 2: // Snake
        SnakeFoodIni();
        speedDelay = 32;
        levelSnake = 1;
        newMatch();
        drawSnakeIni();
        state = 2;
        choice = 2;
        break;
      case 3: // Breakout
        ballSpeed = 1;
        ballAngle = 45.;
        ballAngle0 = ballAngle;
        padX = 160;
        ballX = padX + padLength / 2;
        ballY = 142;
        levelBreakout = 1;
        breakoutLives = 3;
        breakoutSpeed = 18;
        brickCounter = 0;
        brickCounterOld = 0;
        Canvas.setPenColor(0, 0, 0);
        Canvas.clear();
        drawBall(padX + padLength / 2, ballY, 4);
        drawBreakoutIni();
        choice = 3;
        break;
      case 4: // Mandelbrot
        iteraction = iteraction0;
        Canvas.setPenColor(0, 0, 0);
        Canvas.clear();
        
        //VGAController.setResolution(VGA_640x350_70HzAlt1, 640, 350);
        //VGAController.setResolution(VGA_640x480_60Hz);
        //VGAController.setResolution(SVGA_800x600_60Hz, 800, 600);
        //VGAController.setResolution("\"640x480@60Hz\" 25.175 640 656 752 800 480 490 492 525 -HSync -VSync");
        choice = 4;
        break;
      case 5: // Bomber
        bomberScore = bomberScoreIni;
        bomberLives = 3;
        choice = 5;
        break;

      case 6:
        
        choice=6;
        break;
        
      default:
        choice = 0;
        break;
    }
  }
}
//------------------------------------------------ End of Menu -----------------------------------------------------------------------
/*
void TetrisProcessInputs() {
  button_4_old = button_4;

  if (button_1 == 0 ) {
    if (button_1_old == 0) {
      button_1 = digitalRead(button1);
      if (button_1 == 1) {
        button_1_old = 1;
      }
      //delay(40);
    }
    else
    {
      button_1_old = digitalRead(button1);
    }
  }
  else
  {
    button_1 = 0;
  }

  if (button_2 == 0 ) {
    if (button_2_old == 0) {
      button_2 = digitalRead(button2);
      if (button_2 == 1) {
        button_2_old = 1;
      }
      //delay(40);
    }
    else
    {
      button_2_old = digitalRead(button2);
    }
  }
  else
  {
    button_2 = 0;
  }

  if (button_3 == 0 ) {
    if (button_3_old == 0) {
      button_3 = digitalRead(button3);
      if (button_3 == 1) {
        button_3_old = 1;
      }
      //delay(40);
    }
    else
    {
      button_3_old = digitalRead(button3);
    }
  }
  else
  {
    button_3 = 0;
  }

  button_4 = digitalRead(button4);
  if (button_4 == 1 ) {
    delay(15);
  }
  button = button_2 || button_4;
}
*/
void SnakeProcessInputs() {
  

      PS2Controller.keyboard()-> onVirtualKey = [&](VirtualKey * vk, bool keyDown) {
      if (*vk == VirtualKey::VK_RIGHT) {
        if (keyDown) {
          button_1 = 1;
        }
        else button_1 = 0;
        *vk = VirtualKey::VK_NONE;
      }
      else if (*vk == VirtualKey::VK_UP) {                                               //LIST
        if (keyDown) {
          button_2 = 1;
        }
        else button_2 = 0;
        
        *vk = VirtualKey::VK_NONE;
      }
      else if (*vk == VirtualKey::VK_LEFT) {                                               //Grafiksymbole on/off
        if (keyDown) {
          button_3 = 1;
        }
        else button_3 = 0;
        *vk = VirtualKey::VK_NONE;
      }
      else if (*vk == VirtualKey::VK_DOWN) {                                               //TRON/TROFF
        if (keyDown) {
          button_4 = 1;
        }
        else button_4 = 0;
        *vk = VirtualKey::VK_NONE;
      }
      };


  //}
  //button_1 = digitalRead(button1);
  //button_2 = digitalRead(button2);
  //button_3 = digitalRead(button3);
  //button_4 = digitalRead(button4);
  button = button_1 | button_2 | button_3 | button_4;
  
}

void clearInputs() {
  button_1 = 0;
  button_2 = 0;
  button_3 = 0;
  button_4 = 0;
  button = 0;
  key=0;
}

void SnakeFoodIni() {
  do {
    foodX = random(x1Area - x0Area - 4) + x0Area + 2;
    foodY = random(y1Area - y0Area - 4) + y0Area + 2;
    // ------------ choose the following for food up to the border -----------------------------------------
    //foodX = random(x1Area - x0Area - 2) + x0Area + 1;
    //foodY = random(y1Area - y0Area - 2) + y0Area + 1;
  } while ( myGetPixel(foodX, foodY) > 1 );
}

void TetrisDrawMenu() {
  while (button_1 == 0 && button_2 == 0 && button_3 == 0 && button_4 == 0) {
    SnakeProcessInputs();
    smoothRect(60, 52, 230, 90, 10, 7);
    Canvas.setPenColor(1, 1, 0);
    //-------------------------------------------- Get the font info that best fits the specified number of columns and rows ---------------------
    //-------------------------------------------- usage: (80, 33) or (40, 14) -------------------------------------------------------------------
    Canvas.selectFont(Canvas.getFontInfo());
    Canvas.drawText(120, 60, "ESP32 Tetris");
    Canvas.selectFont(Canvas.getFontInfo());
    Canvas.setPenColor(1, 0, 1);
    Canvas.drawText(105, 80, "by Roberto Melzi");
    Canvas.setPenColor(0, 1, 1);
    Canvas.drawText(73, 100, "with ESP32Lib VGA library");
    Canvas.setPenColor(0, 1, 0);
    Canvas.drawText(80, 118, "by Fabrizio Di Vittorio");

    delay(100);
    counterMenu++;
  }
  Canvas.setPenColor(0, 0, 0);
  Canvas.clear();
  drawGameScreen();
  TetrisDrawScore(score);
}

void SnakeDrawBorder() {
  myColor(3);

  Canvas.drawRectangle(x0Area - 1, y0Area - 1, x1Area + 1, y1Area + 1);
}

void TetrisDrawScore(int i) {
  if (i > 60) {
    level = level + 1;
    score = 0;
    i = 0;
    if (fast > 5 ) {
      fast = fast - 2;
    }
    else {
      fast = fast - 1;
    }
    if (fast < 3) {
      fast = 2;
    }
  }
  smoothRect(208, 77, 62, 30, 8, 2);
  Canvas.drawText(222, 80, "Level");
  myDrawNumber(234, 93, level, 2);
  smoothRect(204, 124, 70, 30, 8, 2);
  Canvas.drawText(225, 126, "Score");
  myDrawNumber(245, 139 , scoreNew * 10, 2);
  smoothRect(10,10,100,40,8,2);
  Canvas.drawText(20, 20, "High-Score");
  myDrawNumber(60, 35 , tetris_highscore, 2);
}

void SnakeDrawScore() {

  myPrint("Score", 35, 20, 2);
  myPrint("Level", 35, 60, 5);
  myColor(0);
  myBackcol(0);
  Canvas.fillRectangle(20, 40, 70, 52);
  myBackcol(0);
  Canvas.fillRectangle(20, 80, 70, 92);
  vgaPrintNumber(scoreSnake % 10, 55, 40, 4);
  vgaPrintNumber(levelSnake % 10, 55, 80, 4);
  
  if (scoreSnake > 9) {
    vgaPrintNumber(1, 45, 40, 4);
  }
  if (levelSnake > 9) {
    vgaPrintNumber(1, 45, 80, 4);
  }
}

int checkHit(int x, int y) { //--------------------- check if snake hit himself -----------------------------------------------
  if (direct == 1) {
    if (myGetPixel(x + 1, y) == 2 || myGetPixel(x + 1, y - 1) == 2 || myGetPixel(x + 1, y + 1) == 2) {
      return 1;
    };
  }
  if (direct == 2) {
    if (myGetPixel(x + 1, y - 1) == 2 || myGetPixel(x , y - 1) == 2 || myGetPixel(x - 1, y - 1) == 2) {
      return 1;
    };
  }
  if (direct == 3) {
    if (myGetPixel(x - 1, y) == 2 || myGetPixel(x - 1, y - 1) == 2 || myGetPixel(x - 1, y + 1) == 2) {
      return 1;
    };
  }
  if (direct == 4) {
    if (myGetPixel(x + 1, y + 1) == 2 || myGetPixel(x , y + 1) == 2 || myGetPixel(x - 1, y + 1) == 2) {
      return 1;
    };
  }
  return 0;
}

//--------------- this is for the Snake game beginning window ---------------------------------------------------------------------------------------
void drawSnakeStartScreen() {
  Canvas.clear();
  SnakeDrawBorder();
  drawSnakeIni();
  SnakeDrawScore();
  button = 0;
  delay(200);
}

void drawSnakeIni() {
  for (byte i = 0; i < slength ; i++) {
    sx[i] = x0Area + 100 + i;
    sy[i] = y0Area + 70;
    putBigPixel(sx[i], sy[i], 2);
  }
  //direct = 1;
  for (byte i = slength; i < snakeMaxLength ; i++) {
    sx[i] = 1;
    sy[i] = 1;
  }
  //putpixel(foodX, foodY, 1);
  putBigPixel(foodX, foodY, 1);
}

//----------------- Snake re-inizialize new match -----------------------------------------------------------------------
void newMatch() {
  scoreSnake = 0;
  slength = slengthIni;
  i = slength - 1;
  for (int i = slength; i < snakeMaxLength; i++) {
    sx[i] = 0;
    sy[i] = 0;
  }
  Canvas.clear();
  SnakeDrawBorder();
  SnakeDrawScore();
  //putpixel(foodX, foodY, 1);
  putBigPixel(foodX, foodY, 1);
}

//---------------- Snake Game Over -----------------------------------------------------------------------------------
void SnakeGameOver() {
  smoothRect(16, 118, 78, 20, 6, 6);
  myPrint("Game Over", 20, 121, 6);
  delay(100);
  toneSafe(660, 200);
  toneSafe(330, 200);
  toneSafe(165, 200);
  toneSafe(82, 200);
  button == 0;
  clearInputs();
  while (button == 0) {
    SnakeProcessInputs();
  }
  speedDelay = 32;
  levelSnake = 1;
  //newMatch();
  //drawSnakeIni();
  state = 2;
  Canvas.setPenColor(0, 0, 0);
  Canvas.clear();
  choice = 0;
}

void drawBorder() { //-------------------- Tetris -------------------------------
  myRect(120, 4, squareX * (myColumns - 1) - 3, squareY * myRaws + 19, 4);
  myRect(119, 3, squareX * (myColumns - 1) - 1, squareY * myRaws + 21, 3);
  myRect(118, 2, squareX * (myColumns - 1) + 1, squareY * myRaws + 23, 1);
  myRect(117, 1, squareX * (myColumns - 1) + 3, squareY * myRaws + 25, 7);
  Canvas.drawText(220, 16, "next");
  smoothRect(210, 10, 50, 40, 10, 4);

}

// --------------------- this is for the beginning game window ------------------------
void drawStartScreen() {
  drawBorder();
  drawGameScreen();
  button = 0;
  delay(200);
}

// ---------------------- this is the main function to draw the game screen -----------
void drawGameScreen() {
  drawBorder();
}


// ----------------------- Tetriminos definition --------------------------------------
void blockDef(int i) {
  if (i == 1) {
    // O
    block[0][0] = 0;
    block[0][1] = 0;
    block[1][0] = 1;
    block[1][1] = 0;
    block[2][0] = 0;
    block[2][1] = 1;
    block[3][0] = 1;
    block[3][1] = 1;
    color = 1;
    //colorNew = 1;
  }
  if (i == 2) {
    // L
    block[0][0] = -1;
    block[0][1] = 0;
    block[1][0] = 0;
    block[1][1] = 0;
    block[2][0] = 1;
    block[2][1] = 0;
    block[3][0] = -1;
    block[3][1] = 1;
    color = 2;
    //colorNew = 2;
  }
  if (i == 3) {
    // J
    block[0][0] = -1;
    block[0][1] = 0;
    block[1][0] = 0;
    block[1][1] = 0;
    block[2][0] = 1;
    block[2][1] = 0;
    block[3][0] = 1;
    block[3][1] = 1;
    color = 3;
    //colorNew = 3;
  }
  if (i == 4) {
    // I
    block[0][0] = -1;
    block[0][1] = 0;
    block[1][0] = 0;
    block[1][1] = 0;
    block[2][0] = 1;
    block[2][1] = 0;
    block[3][0] = 2;
    block[3][1] = 0;
    color = 4;
    //colorNew = 4;
  }
  if (i == 5) {
    // S
    block[0][0] = -1;
    block[0][1] = 0;
    block[1][0] = 0;
    block[1][1] = 0;
    block[2][0] = 0;
    block[2][1] = 1;
    block[3][0] = 1;
    block[3][1] = 1;
    color = 5;
    //colorNew = 5;
  }
  if (i == 6) {
    // Z
    block[0][0] = -1;
    block[0][1] = 1;
    block[1][0] = 0;
    block[1][1] = 1;
    block[2][0] = 0;
    block[2][1] = 0;
    block[3][0] = 1;
    block[3][1] = 0;
    color = 6;
    //colorNew = 6;
  }
  if (i == 7) {
    // T
    block[0][0] = -1;
    block[0][1] = 0;
    block[1][0] = 0;
    block[1][1] = 0;
    block[2][0] = 0;
    block[2][1] = 1;
    block[3][0] = 1;
    block[3][1] = 0;
    color = 7;
    //colorNew = 7;
  }
}

// -------------------------- expansion for 4:3 monitors ------------------------------
void blockExtension() {
  for (int i = 0; i < 4; i++) {
    blockExt[0][0] = block[0][0] * 3;
    blockExt[0][1] = block[0][1] * 2;
    blockExt[1][0] = block[1][0] * 3;
    blockExt[1][1] = block[1][1] * 2;
    blockExt[2][0] = block[2][0] * 3;
    blockExt[2][1] = block[2][1] * 2;
    blockExt[3][0] = block[3][0] * 3;
    blockExt[3][1] = block[3][1] * 2;
  }
}

void blockRotation(int rotationDirection) {
  for (int i = 0; i < 4; i++) {
    blockOld[0][0] = block[0][0];
    blockOld[0][1] = block[0][1];
    blockOld[1][0] = block[1][0];
    blockOld[1][1] = block[1][1];
    blockOld[2][0] = block[2][0];
    blockOld[2][1] = block[2][1];
    blockOld[3][0] = block[3][0];
    blockOld[3][1] = block[3][1];
  }
  for (int i = 0; i < 4; i++) {
    block[0][0] = blockOld[0][1] * rotationDirection;
    block[0][1] = -blockOld[0][0] * rotationDirection;
    block[1][0] = blockOld[1][1] * rotationDirection;
    block[1][1] = -blockOld[1][0] * rotationDirection;
    block[2][0] = blockOld[2][1] * rotationDirection;
    block[2][1] = -blockOld[2][0] * rotationDirection;
    block[3][0] = blockOld[3][1] * rotationDirection;
    block[3][1] = -blockOld[3][0] * rotationDirection;
  }
}
void blockTranslation(int x, int y) {
  for (int i = 0; i < 4; i++) {
    blockTr[0][0] = blockExt[0][0] + x;
    blockTr[0][1] = blockExt[0][1] + y;
    blockTr[1][0] = blockExt[1][0] + x;
    blockTr[1][1] = blockExt[1][1] + y;
    blockTr[2][0] = blockExt[2][0] + x;
    blockTr[2][1] = blockExt[2][1] + y;
    blockTr[3][0] = blockExt[3][0] + x;
    blockTr[3][1] = blockExt[3][1] + y;
  }
}

void delBlock() {
  if (noDelete == 1) {
    noDelete = 0;
  }
  else {
    for (int i = 0; i < 4; i++) {
      mySquare(blockTr[i][0] * 2 + 31, blockTr[i][1] * 3 + 1, 0);
    }
  }
}

void drawBlock() {
  for (int i = 0; i < 4; i++) {
    mySquare(blockTr[i][0] * 2 + 31, blockTr[i][1] * 3 + 1, color);
  }
  for (int i = 0; i < 4; i++) {
    blockTmp[0][0] = blockTr[0][0];
    blockTmp[0][1] = blockTr[0][1];
    blockTmp[1][0] = blockTr[1][0];
    blockTmp[1][1] = blockTr[1][1];
    blockTmp[2][0] = blockTr[2][0];
    blockTmp[2][1] = blockTr[2][1];
    blockTmp[3][0] = blockTr[3][0];
    blockTmp[3][1] = blockTr[3][1];
  }
}

void drawBlockTmp() {
  for (int i = 0; i < 4; i++) {
    mySquare(blockTmp[i][0] * 2 + 31, blockTmp[i][1] * 3 + 1, color);
  }
}

void checkBlock() {
  busy = 0;
  for (int i = 0; i < 4; i++) {
    //busy = busy + vgaget(2*blockTr[i][0] + 31, 3*blockTr[i][1] + 2) + vgaget(2*(blockTr[i][0] + 2) + 32, 3*blockTr[i][1] + 2);
    busy = busy + vgaget(2 * blockTr[i][0] + 31, 3 * blockTr[i][1] + 2); // + vgaget(2*(blockTr[i][0] + 2) + 32, 3*blockTr[i][1] + 2);
    //busy = busy + vgaget(2*blockTr[i][0] + 31, 3*blockTr[i][1] + 1) + vgaget(2*(blockTr[i][0] + 2) + 32, 3*blockTr[i][1] + 1); //------------------------------- rivedere ---------------------------
  }
}

void replaceBlock() {
  blockExtension();
  blockTranslation(x, y);
  checkBlock();
  if (busy == 0) {
    drawBlock();
  }
  else // ---------- else is run if the block cannot get down  -----------------
  {
    drawBlockTmp();
    checkForFullLine(); // ---- check il the line is filled when the block cannot get down anymore ----------------------
    noLoop = 0;
    noDelete = 1;
    if (y < 8) {
      gameOver();
    }
  }
}

void gameOver() { // ------------------------------------------- Tetris Game Over ! --------------------------------------------------
  noLoop = -1;
  if ((scoreNew * 10) > tetris_highscore) tetris_highscore = scoreNew * 10;
  score = 1;
  level = 1;
  scoreNew = score;
  
  fast = 14;
  clearInputs();
  myTime = 0;
  Canvas.drawText(26, 96, "Game Over!");
  smoothRect(20, 91, 88, 25, 11, 1);
  
  delay(300);
  toneSafe(660, 200);
  toneSafe(330, 200);
  toneSafe(165, 200);
  toneSafe(82, 200);
  while (button_1 == 0 && button_2 == 0 && button_3 == 0 && button_4 == 0) {
    SnakeProcessInputs();
    delay(100);
  }
  clearInputs();
  choice = 0;
  Canvas.setPenColor(0, 0, 0);
  Canvas.clear();
}

void drawBlockNext() { // ----- draw next block on the right side --------------------------------
  blockExtension();
  blockTranslation(100, 10);
  //vga.fillRect(95*2 + 31, 8*3 + 1, 32, 7*3+1, vga.RGB(0, 0, 0));
  myColor(0);
  //Canvas.setPenColor(0, 0, 0);
  myBackcol(0);//Canvas.setBrushColor(fabgl::Black);
  Canvas.fillRectangle(221, 28, 252, 46);
  myBackcol(0);//Canvas.setBrushColor(fabgl::Black);
  drawBlock();
}

void checkBlockTranslation() {
  x = x + delta;
  blockExtension();
  blockTranslation(x, y);
  checkBlock();
  if (busy == 0) {
    drawBlock();
  }
  else
  {
    x = x - delta;
    blockExtension();
    blockTranslation(x, y);
    drawBlock();
  }
}

void checkBlockRotation() {
  //x = x + delta;
  blockExtension();
  blockTranslation(x, y);
  checkBlock();
  if (busy == 0) {
    drawBlock();
  }
  else
  {
    rotationDirection = -rotationDirection;
    blockRotation(rotationDirection);
    blockExtension();
    blockTranslation(x, y);
    drawBlock();
  }
}

void checkForFullLine() { // --------------------- check if the line is full and must be deleted --------------
  for (int i = 0; i < 4; i++) {
    for (int j = 45; j < 76; j += 3) {
      if (vgaget(2 * j + 32, 3 * blockTmp[i][1] + 3) > 0) {
        k++;
      }
    }
    if (k == 11) { // ------------------------------ line is full and must be deleted ----------------------------------------------------------
      myBackcol(0);//Canvas.setBrushColor(fabgl::Black);
      Canvas.fillRectangle(121, blockTmp[i][1] * 3 + 2, 120 + (squareX - 1) * 11, blockTmp[i][1] * 3 + 1 + squareY );
      yLineTmp[yCounter] = blockTmp[i][1];
      yLine[yCounter] = blockTmp[i][1];
      yCounter++;
      toneSafe(660, 30);
    }
    k = 0;
  }
  if (yLineTmp[yCounter - 1] < yLine[0]) { // ------------ qui va capito se va < o > (penso >) ----------------------
    for (int i = 0; i < yCounter; i++) { // ------------- inversion ---------------------------------------
      yLine[i] = yLineTmp[yCounter - i - 1];
    }
  }
  for (int i = 0; i < yCounter; i++) {  // --------------- block translation to lower position --------------
    for (int y = yLine[i] - 2; y > 0; y = y - 2) {
      for (int x = 45; x < 76; x += 3) {
        colorOld = vgaget(2 * x + 32, 3 * y + 2);
        if (colorOld > 0) {
          mySquare(x * 2 + 31, y * 3 + 1, 0);
          mySquare(x * 2 + 31, (y + 2) * 3 + 1, colorOld);

        }
      }
    }
  }
  if (yCounter != 0) {
    score += 2 * int(pow(2, yCounter)) * level;
    scoreNew += 2 * int(pow(2, yCounter)) * level;
    toneSafe(990, 30);
  }
  TetrisDrawScore(score);
  yCounter = 0;
}


//-----------------------------------------------------------------------------------------------
//--------------------- This is the main loop of Tetris ---------------------------------------
//-----------------------------------------------------------------------------------------------
void TetrisLoop()
{
  //clearInputs();
  SnakeProcessInputs();
  delay(20);
  if (noLoop < 1) { // --------------- to generate new Tetraminos --------------------------------
    blockN = blockNext;
    if (noLoop == -1 ) { // -------------- only at the game beginning  -------------------------
      blockNext = 2 + int(random(6)); // -------------- tetraminos "O" is excluded -----------------
      blockNext = blockN;
    }
    drawGameScreen();
    TetrisDrawScore(score);
    blockNext = 1 + int(random(7));
    blockDef(blockNext);
    drawBlockNext();
    blockDef(blockN);
    x = 57;
    y = 5;
    //button_1 = 1;
    noLoop = 1;
  }
  if (button_2 == 1) { // ------------------------ rotation -------------------------
    //if (button_5 == 1){rotationDirection = -1;}
    if (button_2 == 1) {
      rotationDirection = 1;
      button_2 = 0;  //*
    }
    delBlock();
    blockRotation(rotationDirection);
    checkBlockRotation();
  }
  if (button_1 == 1 || button_3 == 1) { // ------- translation ----------------------
    if (button_1 == 1) {
      delta = 3;
      button_1 = 0;  //*
    }
    if (button_3 == 1) {
      delta = -3;
      button_3 = 0; //*
    }
    delBlock();
    checkBlockTranslation();
  }
  myTime++;
  if (myTime % fast > fast - 2 || button_4 == 1) { // --- Tetraminos falling ----------
    if (fast < 3) {
      fast = 2;
    }
    y = y + 2;
    delBlock();
    replaceBlock();
  }
  else
  {
    delay(10 + 2 * fast);
  }
  
  //show the rendering
  //vga.show();
}
//-----------------------------------------------------------------------------------------------
//--------------------- This is the end of the main loop of Tetris ----------------------------------------
//-----------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------- This is the main loop of Snake --------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------
void SnakeLoop() {

  SnakeProcessInputs();

  if (state == 1) { //-------------------- start screen ---------------------------------------------
    delay(10);
    SnakeProcessInputs();
    if (button == 1) {
      button = 0;
      Canvas.clear();
      drawSnakeStartScreen();
      state = 2;
    }
  }

  if (state == 2) { //--------------------- snake waiting for start ------------------------------------------------
    if (scoreSnake == scoreMax || scoreSnake == 0) {
      SnakeProcessInputs();
    }
    if (button == 1) {
      scoreSnake = 0;
      SnakeDrawScore();
      button = 0;
      button_1 = 0;
      button_2 = 0;
      button_3 = 0;
      button_4 = 0;
      direct = 3;
      x = -1;
      y = 0;
      i = slength - 1;
      state = 3;
    }
  }

  if (state == 3) {
    SnakeProcessInputs();
    //-------------------- change direction --------------------------------------------
    if (direct == 1) {
      if (button_2 == 1) {
        x = 0;
        y = -1;
        direct = 2;
        button_4 = 0;
      }
      if (button_4 == 1) {
        x = 0;
        y = +1;
        direct = 4;
      }
    }
    else {
      if (direct == 2) {
        if (button_1 == 1) {
          x = +1;
          y = 0;
          direct = 1;
          button_3 = 0;
        }
        if (button_3 == 1) {
          x = -1;
          y = 0;
          direct = 3;
        }
      }
      else {
        if (direct == 3) {
          if (button_2 == 1) {
            x = 0;
            y = -1;
            direct = 2;
            button_4 = 0;
          }
          if (button_4 == 1) {
            x = 0;
            y = +1;
            direct = 4;
          }
        }
        else {
          if (direct == 4) {
            if (button_1 == 1) {
              x = +1;
              y = 0;
              direct = 1;
              button_3 = 0;
            }
            if (button_3 == 1) {
              x = -1;
              y = 0;
              direct = 3;
            }
          }
        }
      }
    }

    //----------------------- delete tail --------------------------------------
    putBigPixel(sx[i], sy[i], 0);
    if (i > 0) {
      putBigPixel(sx[i - 1], sy[i - 1], 2);
    }
    else {
      putBigPixel(sx[slength - 1], sy[slength - 1], 2);
    }
    if ( i == slength - 1) {
      sx[i] = sx[0] + x;
      sy[i] = sy[0] + y;
    }
    else {
      sx[i] = sx[i + 1] + x;
      sy[i] = sy[i + 1] + y;
    }

    /*
      //--------------------- out from border ------------------------------------
         if(sx[i] < x0Area + 1) {sx[i] = x1Area - 1;}
         if(sx[i] > x1Area - 1) {sx[i] = x0Area + 1;}
         if(sy[i] < y0Area + 1) {sy[i] = y1Area - 1;}
         if(sy[i] > y1Area - 1) {sy[i] = y0Area + 1;}
    */

    //--------------------- out from border ------------------------------------
    if (sx[i] < x0Area + 1) {
      SnakeGameOver();
    }
    if (sx[i] > x1Area - 1) {
      SnakeGameOver();
    }
    if (sy[i] < y0Area + 1) {
      SnakeGameOver();
    }
    if (sy[i] > y1Area - 1) {
      SnakeGameOver();
    }

    //--------------------- check eating food -------------------------------------------------------------------------------------------------------------------------
    if ( sx[i] > foodX - 3 && sx[i] < foodX + 3 && sy[i] > foodY - 3 && sy[i] < foodY + 3 ) {
      putBigPixel(foodX, foodY, 0);
      //putBigPixel(sx[i], sy[i], 2);
      toneSafe(660, 30);
      SnakeFoodIni();
      SnakeDrawBorder();
      putBigPixel(foodX, foodY, 1);
      if ( sx[i] == foodX || sy[i] == foodY ) {
        slength = slength + 2 * deltaSnake;
        scoreSnake += 2;
      }
      else {
        slength = slength + deltaSnake;
        scoreSnake++;
      }
      if (scoreSnake > scoreMax) {
        speedDelay = int(speedDelay * 0.8);
        levelSnake += 1;
        toneSafe(880, 30);
        newMatch();
        drawSnakeIni();
        state = 2;
      }
      SnakeDrawScore();
    }
    putBigPixel(foodX, foodY, 1);

    //----------------------- increase head and Game Over -------------------------------------
    //if (myGetPixel(sx[i], sy[i]) == 2) {
    if (checkHit(sx[i], sy[i]) == 0) {
      putBigPixel(sx[i], sy[i], 2);
    }
    else //-------- Sneke hit himself ----------------------------------------------------
    {
      SnakeGameOver();
      //putBigPixel(40, 40 + cancellami, 6);
      //cancellami += 4;
    }
    putBigPixel(1, 1, 0);

    i--;
    if ( i < 0) {
      i = slength - 1;
    }
    delay(speedDelay);
  }
}
//------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------- end of the main loop of Snake ------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------- main loop of Mandelbrot ------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------
void MandelbrotLoop() {
  dx = (x2 - x1) / Canvas.getWidth();
  dy = (y2 - yy1) / (Canvas.getHeight() + 1);
  for (float y0 = yy1; y0 <= y2; y0 = y0 + dy) {
    for (float x0 = x1; x0 <= x2; x0 = x0 + dx) {
      am = 0;
      bm = 0;
      n = 0;
      do {
        a0 = am * am - bm * bm + x0;
        bm = 2 * am * bm + y0;
        am = a0;
        n = n + 1;
      } while (n < iteraction + 1 && (am * am + bm * bm) < 4);
      if (n > iteraction) {
        myColor(0);
        Canvas.setPixel(mapFloat(x0, x1, x2, 0, Canvas.getWidth()), mapFloat(y0, yy1, y2, 0, Canvas.getHeight()));
      }
      else {
        myColorMandelbrot(n);
        Canvas.setPixel(mapFloat(x0, x1, x2, 0, Canvas.getWidth()), mapFloat(y0, yy1, y2, 0, Canvas.getHeight()));
      }
    }
  }
  
}
//------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------- end of the main loop of Mandelbrot -------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------

void drawBreakoutIni() {
  myRect(11, 5, 280, 156, 7);
  myRect(10, 4, 282, 158, 7);
  myPrint("Score", 10, 164, 2);
  myPrint("lives", 114, 164, 2);
  for (int i = 2; i < 13; i++) {
    for (int j = 1; j < 6; j++) {
      myFullRect(20 * i, 8 * j + 10, 18, 6, (2 * i + j) % 6 + 1);
    }
  }
  drawPad(padX, 1);
  drawLives(breakoutLives);
  delay(300);
  state = 1;
  BreakoutLoop();
}

void drawPad(int x, int c) {
  myFullRect(padX, 145, padLength, 4, 1*c);
  myFullRect(padX + 4, 145, padLength - 8, 2, 2*c);
  //putpixel(padX + padLength/2, 147, 4);
}

void drawBall(int x, int y, int color) {
  //myColor(color);
  //Canvas.drawEllipse(x, y, 4, 4);
  myLine(x - 1, y - 2, x + 1, y - 2, color);
  myLine(x - 1, y + 2, x + 1, y + 2, color);
  myLine(x - 2, y - 1, x - 2, y + 1, color);
  myLine(x + 2, y - 1, x + 2, y + 1, color);
  myFullRect(x - 1, y - 1, 2, 2, color);
  //putpixel(x, y, 3*color/4);
}

void drawLives(int lives) {
  for (int i = 1; i < 15; i++) {
    if (i <= lives) {
      drawBall(150 + i * 10, 172, 3);
    } else {
      drawBall(150 + i * 10, 172, 0);
    }
  }
}

int checkBrick(int x, int y, int c) {
  hit0 = 0;
  if (y < 130 && x > 22 + 4 && x < 280 - 4 && c != 7 ) { //-------------- potrebbe aver colpito un mattone -----------------
    drawBall(ballXold, ballYold, 0);
    if (vgaget(20 * int(x / 20) + 10 , 8 * int((y - 10) / 8) + 10 + 4) > 0) { //-------------- ha effettivamente colpito un mattone -----------------
      drawBall(ballXold, ballYold, 4);
      myFullRect(20 * int(x / 20), 8 * int((y - 10) / 8) + 10, 18, 6, 0);
      hit0 = 1;
    }
  }
  else {
    //if(y > 130 && y < 154){ //--------------------- ha colpito il pad --------------------
    if (c == 1) {
      drawPad(padX, 1);
      //myFullRect(158, 164, 24, 16, 1);
      //myDrawNumber(170, 164, myPos(ballX - padX - padLength/2), 6);
      if (myPos(ballX - padX - padLength / 2) > 8) {
        sgx = -sgx;
      }
      //delay(2000);
    }
    else {
      if (y > 154) { //--------------------- ha colpito il fondo -------------------------------
        //--------------------------------- per ora non succede nulla -------------------------
      }
    }
  }
}

//------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------- main loop of Breakout --------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------
void BreakoutLoop() {
  SnakeProcessInputs();

  if (state == 1) { //--------------------- Breakout waiting for start ------------------------------------------------
    breakoutPad(1);
    if (button_4 == 1) {
      delay(400);
      SnakeProcessInputs();
      if (button_4 == 1) {
        Canvas.setPenColor(0, 0, 0);
        Canvas.clear();
        clearInputs();
        choice = 0;
        delay(50);
        ChoiceMenu();
      }
    }
    if (button_2 == 1) {
      //ballAngle = 45.,
      //vxr = ballSpeed*cos(ballAngle*3.1416/180.);
      //vyr = ballSpeed*sin(ballAngle*3.1416/180.);
      vx = 1;
      vy = 1;
      sgy = -1;
      //brickCounter = 0;
      //level += 2;
      if (breakoutSpeed > 10) {
        breakoutSpeed += -2;
      } else {
        breakoutSpeed += -1;
      }
      if (breakoutSpeed < 6) {
        breakoutSpeed = 6;
      }
      //drawLives(breakoutLives);
      state = 2;
    }
  }

  if (state == 2) { //--------------------- Breakout running ------------------------------------------------
    ballXold = ballX;
    ballYold = ballY;
    ballX = ballX + sgx * vx;
    ballY = ballY + sgy * vy;
    sgx0 = sgx;
    sgy0 = sgy;

    hitA = myGetPixel(ballX + 2 * sgx, ballY + 2 * sgy);
    hitB = myGetPixel(ballX - 2 * sgx, ballY + 2 * sgy);
    hitC = myGetPixel(ballX + 2 * sgx, ballY - 2 * sgy);
    if (hitA > 0) {
      if (hitB > 0) {
        checkBrick(ballX - 2 * sgx, ballY + 2 * sgy, hitB);
        hit += hit0;
        checkBrick(ballX + 2 * sgx, ballY + 2 * sgy, hitA);
        hit += hit0;
        {
          sgy = -sgy;
          ballY = ballY + sgy * vy;
        }
      }
      if (hitC > 0) {
        checkBrick(ballX + 2 * sgx, ballY - 2 * sgy, hitC);
        hit += hit0;
        checkBrick(ballX + 2 * sgx, ballY + 2 * sgy, hitA);
        hit += hit0;
        {
          sgx = -sgx;
          ballX = ballX + sgx * vx;
        }
      }
    }
    else {
      if (hitB > 0) {
        checkBrick(ballX - 2 * sgx, ballY + 2 * sgy, hitB);
        hit += hit0;
        hit0 = 0;
        {
          sgy = -sgy;
          ballY = ballY + sgy * vy;
        }
      }
      if (hitC > 0) {
        checkBrick(ballX + 2 * sgx, ballY - 2 * sgy, hitC);
        hit += hit0;
        hit0 = 0;
        {
          sgx = -sgx;
          ballX = ballX + sgx * vx;
        }
      }
    }
    hit0 = 0;

    if (hit > 0) {
      brickCounter += hit;
      brickCounterOld += hit;
      myDrawNumber(70, 164, brickCounter, 4);
      //myFullRect(138, 164, 32, 12, 0);
      //myDrawNumber(150, 164, 55 - brickCounterOld, 5);
      hit = 0;

      if (brickCounterOld > 54) { //--------------- ha distrutto tutti i mattoni ----------------------
        drawBall(ballXold, ballYold, 0);
        ballX = padX + padLength / 2;
        ballY = 142;
        ballXold = ballX;
        ballYold = ballY;
        breakoutLives += 1;
        drawBreakoutIni();
        brickCounterOld = 0;
        state = 1;
      }
    }

    if (ballX > 288 || ballX < 16 ) {
      sgx = -sgx;
      ballX = ballX + 2 * sgx * vx;
    }
    //if(ballY > 158 || ballY < 8 ){sgy = -sgy; ballY = ballY + 2*sgy*vy; }
    if (ballY < 8 ) {
      sgy = -sgy;
      ballY = ballY + 2 * sgy * vy;
    }

    if (ballY > 156) { //--------------------------- the ball hit the bottom and looss a life ------------------------------------
      //level += -2;
      drawBall(ballXold, ballYold, 0);
      ballX = padX + padLength / 2;
      ballY = 142;
      ballXold = ballX;
      ballYold = ballY;
      breakoutLives += -1;
      drawLives(breakoutLives);
      if (breakoutSpeed >= 10) {
        breakoutSpeed += 2;
      } else {
        breakoutSpeed += 1;
      }
      state = 1;
    }

    drawBall(ballXold, ballYold, 0);
    drawBall(ballX, ballY, 4);
    delay(breakoutSpeed);

    breakoutPad(0);

    if (breakoutLives < 1) { //----------------- Breakout game Over! ----------------------------
      brickCounterOld = 0;
      if (breakoutLives < 1) {
        button_4 = 0;
        myPrint("Game Over!", 110, 90, 1);
        delay(500);
        while (button == 0) {
          SnakeProcessInputs();
          delay(50);
        }
      }
      Canvas.setPenColor(0, 0, 0);
      Canvas.clear();
      //if(button_4 == 1){choice = 0;}else{choice = 3;}
      choice = 0;
      clearInputs();
      delay(50);
      ChoiceMenu();
    }
  }
}
//------------------------------------------------------------------------------------------------------------------------------------------
//----------------------------- end of the main loop of Breakout ---------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------------------------------

void breakoutPad(bool ball) { //----------------------- breakout pad motion control -----------------------------------
  //if (button_3 == 1 && padX > (2 + padLength/2)) {
  if (button_3 == 1) {
    drawPad(int(padX), 0);
    if (ball == 1) {
      ballX = int(padX) + padLength / 2;
      drawBall(ballX, ballY, 0);
    }
    if (button_3_old == 1) {
      padSpeed += -padSpeedStep;
      if (padSpeed < -padSpeedMax) {
        padSpeed = -padSpeedMax;
      }
    }
    else {
      padSpeed = -padSpeedStep;
      button_3_old = 1;
    }
    padX += padSpeed;
    if (padX <= 2 + padLength / 2) {
      padX = padLength / 2;
    }
    drawPad(int(padX), 1);
    if (ball == 1) {
      ballX = int(padX) + padLength / 2;
      drawBall(ballX, ballY, 4);
      delay(20);
    }
  }
  else {
    button_3_old = 0;
  }
  //if (button_1 == 1 && padX < (275 - padLength/2)){
  if (button_1 == 1) {
    drawPad(int(padX), 0);
    if (ball == 1) {
      ballX = int(padX) + padLength / 2;
      drawBall(ballX, ballY, 0);
    }
    if (button_1_old == 1) {
      padSpeed += padSpeedStep;
      if (padSpeed > padSpeedMax) {
        padSpeed = padSpeedMax;
      }
    }
    else {
      padSpeed = padSpeedStep;
      button_1_old = 1;
    }
    padX += padSpeed;
    if (padX >= 278 - padLength / 2) {
      padX = 278 - padLength / 2;
    }
    drawPad(int(padX), 1);
    if (ball == 1) {
      ballX = int(padX) + padLength / 2;
      drawBall(ballX, ballY, 4);
      delay(20);
    }
  }
  else {
    button_1_old = 0;
  }
}

void toneSafe(int freq, int duration) {
  //vga.tone(freq);
  delay(duration);
  //vga.noTone();
}

void vgaPrintNumber(int number, int x, int y, byte color) {
  char scoreChar[2];
  sprintf(scoreChar, "%d", number);
  myPrint(scoreChar, x, y, color);
}

void myDrawNumber(int x, int y, int number, byte color) {
  if (number > 0) {
    int digits = int(log10(number)) + 1;
    char scoreChar[digits];
    sprintf(scoreChar, "%d", number);
    myFullRect(x + 8, y, -10 - 8 * (digits - 1), 12, 0);
    myPrint(scoreChar, x - 8 * (digits - 1), y, color);
  }
  else {
    myPrint("00", x, y, color);
  }
}

void draw_line(int x0, int y0, int x1, int y1, byte color) {
  myColor(color);
  Canvas.drawLine(x0, y0, x1, y1);
}

void myLine(int x0, int y0, int x1, int y1, int color) {
  myColor(color);
  //Canvas.setPenColor(colA, colB, colC);
  Canvas.drawLine(x0, y0, x1, y1);
}

void putpixel(int x0, int y0, byte color) {
  myColor(color);
  Canvas.setPixel(x0, y0);
}

void putBigPixel(int x0, int y0, byte color) {
  myColor(color);
  Canvas.setPixel(x0, y0);
  myBackcol(color);//Canvas.setBrushColor(backcolor[8][0],backcolor[8][1],backcolor[8][2]);
  Canvas.fillRectangle(x0 - 1, y0 - 1, x0 + 1, y0 + 1);
  myBackcol(0);//Canvas.setBrushColor(0, 0, 0);
}

int myGetPixel(int x, int y) { //--------------------- for Snake ----------------------------
  //Canvas.waitCompletion();
  int red = Canvas.getPixel(x, y).R;
  int green = Canvas.getPixel(x, y).G;
  int blue = Canvas.getPixel(x, y).B;
  if (red == 0 && green == 0 && blue == 0) {return 0;}
  if (red > 0 && green == 0 && blue == 0) {return 1;}
  if (red == 0 && green > 0 && blue == 0) {return 2;}
  if (red == 0 && green == 0 && blue > 0) {return 3;}
  if (red > 0 && green > 0 && blue == 0) {return 4;}
  if (red > 0 && green == 0 && blue > 0) {return 5;}
  if (red == 0 && green > 0 && blue > 0) {return 6;}
  if (red > 0 && green > 0 && blue > 0) {return 7;}
}

int vgaget(int x, int y) { //----------------------- for Tetris ------------------------------
  Canvas.waitCompletion();
  int red = Canvas.getPixel(x + 2, y + 1).R;
  int green = Canvas.getPixel(x + 2, y + 1).G;
  int blue = Canvas.getPixel(x + 2, y + 1).B;
  if (red == 0 && green == 0 && blue == 0) {return 0;}
  if (red > 0 && green == 0 && blue == 0) {return 1;}
  if (red == 0 && green > 0 && blue == 0) {return 2;}
  if (red == 0 && green == 0 && blue > 0) {return 3;}
  if (red > 0 && green > 0 && blue == 0) {return 4;}
  if (red > 0 && green == 0 && blue > 0) {return 5;}
  if (red == 0 && green > 0 && blue > 0) {return 6;}
  if (red > 0 && green > 0 && blue > 0) {return 7;}
  /*
  if (red == 0 && green == 0 && blue == 0) {return 0;}
  if (red == 1 && green == 0 && blue == 0) {return 1;}
  if (red == 0 && green == 1 && blue == 0) {return 2;}
  if (red == 0 && green == 0 && blue == 1) {return 3;}
  if (red == 1 && green == 1 && blue == 0) {return 4;}
  if (red == 1 && green == 0 && blue == 1) {return 5;}
  if (red == 0 && green == 1 && blue == 1) {return 6;}
  if (red == 1 && green == 1 && blue == 1) {return 7;}
  */
  
}

void myRect(int x0, int y0, int w, int h, int color) {
  myColor(color);
  //Canvas.setPenColor(colA, colB, colC);
  Canvas.drawRectangle(x0, y0, x0 + w, y0 + h);
}

void myFullRect(int x0, int y0, int w, int h, int color) {
  myColor(color);
  myBackcol(color);
  //Canvas.setBrushColor(backcolor[color][0],backcolor[color][1],backcolor[color][2]);
  Canvas.fillRectangle(x0, y0, x0 + w, y0 + h);
  myBackcol(0);//Canvas.setBrushColor(0, 0, 0);
}

void mySquare(int x0, int y0, int color) { //--------------------------------------------- double color inner version ----------------------------------
  myColor(color);

  myBackcol(color);//Canvas.setBrushColor(backcolor[color][0],backcolor[color][1],backcolor[color][2]); //--------------------------------------------------- colore riempimento e sfondo --------------------------
  Canvas.fillRectangle(x0, y0 + 1, x0 + squareX - 2, y0 + squareY);
  if (color != 0) {
    myColor(color + 1);
  }
  if (color == 7) {
    myColor(2);
  }
  //Canvas.setPenColor(colA, colB, colC);
  myColor(color);
  Canvas.setPixel(x0 + squareX / 2, y0 + squareY / 2);
  Canvas.setPixel(x0 + squareX / 2 - 1, y0 + squareY / 2);
  Canvas.setPixel(x0 + squareX / 2, y0 + squareY / 2 + 1);
  Canvas.setPixel(x0 + squareX / 2 - 1, y0 + squareY / 2 + 1);
  myBackcol(0);//Canvas.setBrushColor(0, 0, 0);
}

void mySquare4(int x0, int y0, int color) { //--------------------------------------------- double color border + inner version ------------------------
  myColor(color);
  myBackcol(color);//Canvas.setBrushColor(backcolor[color][0],backcolor[color][1],backcolor[color][2]); //--------------------------------------------------- colore riempimento e sfondo --------------------------
  Canvas.fillRectangle(x0 + 1, y0 + 2, x0 + squareX - 3, y0 + 1 + squareY - 2);
  if (color != 0) {
    myColor(color + 1);
  }
  if (color == 7) {
    myColor(2);
  }
  myColor(color);
  //Canvas.setPenColor(colA, colB, colC);
  Canvas.drawRectangle(x0, y0 + 1, x0 + squareX - 2, y0 + squareY);
  Canvas.setPixel(x0 + squareX / 2, y0 + squareY / 2);
  Canvas.setPixel(x0 + squareX / 2 - 1, y0 + squareY / 2);
  Canvas.setPixel(x0 + squareX / 2, y0 + squareY / 2 + 1);
  Canvas.setPixel(x0 + squareX / 2 - 1, y0 + squareY / 2 + 1);
  myBackcol(0);//Canvas.setBrushColor(0, 0, 0);
}

void mySquare3(int x0, int y0, int color) { //--------------------------------------------- double color border version --------------------------------
  myColor(color);
  myBackcol(color);//Canvas.setBrushColor(colA, colB, colC); //--------------------------------------------------- colore riempimento e sfondo --------------------------
  Canvas.fillRectangle(x0 + 1, y0 + 2, x0 + squareX - 3, y0 + 1 + squareY - 2);
  if (color != 0) {
    myColor(color + 1);
  }
  if (color == 7) {
    myColor(2);
  }
  myColor(color);
  //Canvas.setPenColor(colA, colB, colC);
  Canvas.drawRectangle(x0, y0 + 1, x0 + squareX - 2, y0 + squareY);
  myBackcol(0);//Canvas.setBrushColor(0, 0, 0);
}

void mySquare2(int x0, int y0, int color) { //------------------------------------------------------- simple version -----------------------------------
  myColor(color);
  myBackcol(color);//Canvas.setBrushColor(colA, colB, colC);
  Canvas.fillRectangle(x0, y0 + 1, x0 + squareX - 2, y0 + squareY);
  myBackcol(0);//Canvas.setBrushColor(0, 0, 0);
}

void myColor(int color) {
  color = color % 8;

  if (color == 0) {
    Canvas.setPenColor(Color::Black);
  }
  if (color == 1) {
    Canvas.setPenColor(Color::Red);
  }
  if (color == 2) {
    Canvas.setPenColor(Color::Green);
  }
  if (color == 3) {
    Canvas.setPenColor(Color::Yellow);
  }
  if (color == 4) {
    Canvas.setPenColor(Color::Blue);
  }
  if (color == 5) {
    Canvas.setPenColor(Color::Magenta);
  }
  if (color == 6) {
    Canvas.setPenColor(Color::Cyan);
  }
  if (color == 7) {
    Canvas.setPenColor(Color::White);
  }
}

void myBackcol(int color) {
  color = color % 8;

  if (color == 0) {
    Canvas.setBrushColor(Color::Black);
  }
  if (color == 1) {
    Canvas.setBrushColor(Color::Red);
  }
  if (color == 2) {
    Canvas.setBrushColor(Color::Green);
  }
  if (color == 3) {
    Canvas.setBrushColor(Color::Yellow);
  }
  if (color == 4) {
    Canvas.setBrushColor(Color::Blue);
  }
  if (color == 5) {
    Canvas.setBrushColor(Color::Magenta);
  }
  if (color == 6) {
    Canvas.setBrushColor(Color::Cyan);
  }
  if (color == 7) {
    Canvas.setBrushColor(Color::White);
  }
}

void myColorMandelbrot(int color) {
  if (color != 0) {
    color = color % 7 + 1; //------------- version for Mandelbrot ---------------------
  }
  if (color == 0) {
    Canvas.setPenColor(Color::Black);
  }
  if (color == 1) {
    Canvas.setPenColor(Color::Red);
  }
  if (color == 2) {
    Canvas.setPenColor(Color::Green);
  }
  if (color == 3) {
    Canvas.setPenColor(Color::Yellow);
  }
  if (color == 4) {
    Canvas.setPenColor(Color::Blue);
  }
  if (color == 5) {
    Canvas.setPenColor(Color::Magenta);
  }
  if (color == 6) {
    Canvas.setPenColor(Color::Cyan);
  }
  if (color == 7) {
    Canvas.setPenColor(Color::White);
  }
  //Canvas.setPenColor(colA, colB, colC);
}

void setPrint(int x, int y, int color) {
  myColor(color);
  //Canvas.setPenColor(colA, colB, colC);
  Canvas.moveTo(x, y);
}

void smoothRect(int x0, int y0, int w, int h, int r, int color) { //----- 1.6 comes from the resolution ratio - 320/200 --------------
  myLine(x0 + 1.6 * r, y0 - 1, x0 + w - 1.6 * r, y0 - 1, color);
  myLine(x0 + 1.6 * r, y0 + h, x0 + w - 1.6 * r, y0 + h, color);
  myLine(x0 - 1, y0 + r, x0 - 1, y0 + h - r, color);
  myLine(x0 + w, y0 + r, x0 + w, y0 + h - r, color);
  myColor(color);
  for (int i = 0; i <= 25; i++) {
    myColor(color);
    Canvas.setPixel(x0 + w - r * 1.6 * (1 - cos(i / 25.*3.1415 / 2.)), y0 + r * (1 - sin(i / 25.*3.1415 / 2.)));
    Canvas.setPixel(x0 + r * 1.6 * (1 - cos(i / 25.*3.1415 / 2.)), y0 + r * (1 - sin(i / 25.*3.1415 / 2.)));
    Canvas.setPixel(x0 + w - r * 1.6 * (1 - cos(i / 25.*3.1415 / 2.)), y0 + h - r * (1 - sin(i / 25.*3.1415 / 2.)));
    Canvas.setPixel(x0 + r * 1.6 * (1 - cos(i / 25.*3.1415 / 2.)), y0 + h - r * (1 - sin(i / 25.*3.1415 / 2.)));
  }
}

void myPrint(char* str, byte x, byte y, byte color) {
  Canvas.selectFont(Canvas.getFontInfo());
  myColor(color);
  Canvas.drawText(x, y, str);
}

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

int myPos(int x) {
  if (x < 0) {
    return -x;
  }
  else {
    return x;
  }
}

//-----------------------------------------------------------------------------------------------
//--------------------- This is the main loop of Bomber -----------------------------------------
//-----------------------------------------------------------------------------------------------
void BomberLoop() {
  Canvas.clear();
  drawBomberIni();
  BomberLives(bomberLives);
  BomberScore(bomberScore);
  planeRX = 340; //------------- this is needed for correct inizialization of planeR -------------
  planeRXold = 340;
  bomberX = 120;
  bomb1 = 0;
  bomb2 = 0;
  while (state == 1) { //----------------------- Bomber starts here --------------------------
    bomberDelay = bomberDelayOld;
    planeLX += -planeLV;
    if (planeLX < 12) {
      planeLX = 267;
      //bomberScore += -4;
      BomberScore(bomberScore);
      planeLY = random(10, 105);
      planeLV = random(80., 190.) / 100.;
    }
    planeRX += planeRV;
    if (planeRX > 291) {
      planeRX = 28;
      //bomberScore += -4;
      BomberScore(bomberScore);
      planeRY = random(10, 105);
      planeRV = random(80., 190.) / 100.;
    }
    SnakeProcessInputs();
    //bomber(bomberX, 1);
    planeL(planeLXold, planeLYold, 0);
    planeL(int(planeLX), planeLY, 3);
    planeR(planeRXold, planeRYold, 0);
    planeR(int(planeRX), planeRY, 6);
    myBomb1();
    myBomb2();
    shot(bomberX, 1);
    bomber(bomberX, 1);
    if (button_4 == 1 && button_2 == 1) {
      state = 0; //----------- stop the game is UP and DOWN are pressed together ------------------
    }
    planeLXold = int(planeLX);
    planeLYold = planeLY;
    planeRXold = int(planeRX);
    planeRYold = planeRY;
    bomb1Xold = int(bomb1X);
    bomb1Yold = int(bomb1Y);
    bomb2Xold = int(bomb2X);
    bomb2Yold = int(bomb2Y);
    delay(bomberDelay);
  }
  clearInputs();
  delay(200);
}
//-----------------------------------------------------------------------------------------------
//--------------------- This is the end loop of Bomber ------------------------------------------
//-----------------------------------------------------------------------------------------------

void drawBomberIni() { //-------------- Bomber screen inizialization -----------------------------
  myRect(11, 5, 280, 156, 7);
  myRect(10, 4, 282, 158, 7);
  myPrint("Score", 10, 164, 2);
  myPrint("lives", 114, 164, 2);
  drawLives(breakoutLives);
  myFullRect(12, 155, 278, 5, 2);
  state = 1;
}

void shot(int x, int c) {
  if (button_2 == 1) {
    myLine(bomberX + bomberL / 2, 141, bomberX + bomberL / 2, 6, 1);
    for (int i = 1; i <= 9; i++) {
      myLine(bomberX + bomberL / 2, 156 - i * 15, bomberX + bomberL / 2, 156 - (i + 1) * 15, 4);
      delay(bomberDelay / 10);
      //myLine(bomberX + bomberL/2, 156 - i*15, bomberX + bomberL/2, 156 - (i + 1)*15, 0);
    }
    //delay(bomberDelay);
    myLine(bomberX + bomberL / 2, 141, bomberX + bomberL / 2, 6, 0);
    bomberDelay = 0;
    //bomberScore += 1;
    BomberScore(bomberScore);
    checkShot(bomberX + bomberL / 2);
  }
}

void myBomber(int x, int c) {
  myFullRect(x + bomberL / 2 - 1, 142, 2, 3, c);
  myFullRect(x + bomberL / 2 - 4, 145, 8, 4, c);
  myFullRect(x, 150, bomberL, 4, c);
}

void planeL(int x, int y, int c) {
  myLine(x, y , x + 12, y, c);
  myLine(x + 1, y - 1, x + 13, y - 1, c);
  myLine(x + 3, y - 2 , x + 9, y - 2, c);
  myLine(x + 13, y - 2, x + 14, y - 2, c);
  myLine(x + 14, y - 3, x + 15, y - 3, c);
  myLine(x + 15, y - 4, x + 16, y - 4, c);
  myLine(x + 4, y + 1, x + 7, y + 1, c);
  myLine(x + 6, y + 2, x + 9, y + 2, c);
}

void planeR(int x, int y, int c) {
  myLine(x, y , x - 12, y, c);
  myLine(x - 1, y - 1, x - 13, y - 1, c);
  myLine(x - 3, y - 2 , x - 9, y - 2, c);
  myLine(x - 13, y - 2, x - 14, y - 2, c);
  myLine(x - 14, y - 3, x - 15, y - 3, c);
  myLine(x - 15, y - 4, x - 16, y - 4, c);
  myLine(x - 4, y + 1, x - 7, y + 1, c);
  myLine(x - 6, y + 2, x - 9, y + 2, c);
}

void bomb(int x, int y, int c) {
  myLine(x, y , x + 8, y, c);
  myLine(x + 9, y - 1, x + 9, y + 1, c);
  myLine(x + 1, y + 1, x + 6, y + 1, c);
  myLine(x + 1, y - 1, x + 6, y - 1, c);
  myLine(x + 2, y + 2 , x + 5, y + 2, c);
  myLine(x + 2, y - 2 , x + 5, y - 2, c);
}

void bombR(int x, int y, int c) {
  myLine(x, y , x - 8, y, c);
  myLine(x - 9, y - 1, x - 9, y + 1, c);
  myLine(x - 1, y + 1, x - 6, y + 1, c);
  myLine(x - 1, y - 1, x - 6, y - 1, c);
  myLine(x - 2, y + 2 , x - 5, y + 2, c);
  myLine(x - 2, y - 2 , x - 5, y - 2, c);
}

void bomber(float x, int c) { //----------------------- bomber motion control -----------------------------------
  if (button_3 == 1) {
    myBomber(bomberX, 0);
    if (button_3_old == 1) {
      bomberSpeed += -speedStep;
      if (bomberSpeed < -bomberSpeedMax) {
        bomberSpeed = -bomberSpeedMax;
      }
    }
    else {
      bomberSpeed = -speedStep;
      button_3_old = 1;
    }
    bomberX += bomberSpeed;
    if (bomberX < 12) {
      bomberX = 12;
    }
    myBomber(bomberX, c);
    delay(bomberDelay);
    bomberDelay = 0;
  }
  else {
    button_3_old = 0;
  }
  if (button_1 == 1) {
    myBomber(bomberX, 0);
    if (button_1_old == 1) {
      bomberSpeed += speedStep;
      if (bomberSpeed > bomberSpeedMax) {
        bomberSpeed = bomberSpeedMax;
      }
    }
    else {
      bomberSpeed = speedStep;
      button_1_old = 1;
    }
    bomberX += bomberSpeed;
    if (bomberX > 290 - bomberL) {
      bomberX = 290 - bomberL;
    }
    myBomber(bomberX, c);
    delay(bomberDelay);
    bomberDelay = 0;
  }
  else {
    button_1_old = 0;
  }
}

void checkShot(int x) { //--------------------- This checks if Planes or Bombs have been shot ----------------------------
  if (x >= planeLX && x <= planeLX + 16) { //----------------- plane Left has been shot -----------------------
    planeL(planeLX, planeLY, 1);
    expXmin = planeLX + 8;
    expXmax = expXmin;
    expYmin = planeLY - 1;
    expYmax = expYmin;
    delay(100);
    planeL(planeLX, planeLY, 0);
    for (int i = 1; i < 4; i++) {
      for (int j = 1; j < 10; j++) {
        expX = random(-6 * i, 6 * i) + planeLX + 8;
        expY = random(-6 * i, 6 * i) + planeLY - 1;
        if (expX > 12 && expX < 290 && expY > 10 && expY < 135) {
          putpixel(expX, expY, 3);
          if (expX < expXmin) {
            expXmin = expX;
          }
          if (expX > expXmax) {
            expXmax = expX;
          }
          if (expY < expYmin) {
            expYmin = expY;
          }
          if (expY > expYmax) {
            expYmax = expY;
          }
        }
      }
      delay(50);
    }
    myFullRect(expXmin, expYmin, expXmax - expXmin, expYmax - expYmin, 0);
    planeLX = 267 - random(24);
    planeLY = random(10, 105);
    bomberScore += 1;//bomberScoreIni / 10 + 1;
    BomberScore(bomberScore);
  }
  if (x >= planeRX - 16 && x <= planeRX) { //----------------- plane Right has been shot -----------------------
    planeR(planeRX, planeRY, 1);
    expXmin = planeRX - 8;
    expXmax = expXmin;
    expYmin = planeRY - 1;
    expYmax = expYmin;
    delay(100);
    planeR(planeRX, planeRY, 0);
    for (int i = 1; i < 4; i++) {
      for (int j = 1; j < 10; j++) {
        expX = random(-6 * i, 6 * i) + planeRX - 8;
        expY = random(-6 * i, 6 * i) + planeRY - 1;
        if (expX > 12 && expX < 290 && expY > 10 && expY < 135) {
          putpixel(expX, expY, 3);
          if (expX < expXmin) {
            expXmin = expX;
          }
          if (expX > expXmax) {
            expXmax = expX;
          }
          if (expY < expYmin) {
            expYmin = expY;
          }
          if (expY > expYmax) {
            expYmax = expY;
          }
        }
      }
      delay(50);
    }
    myFullRect(expXmin, expYmin, expXmax - expXmin, expYmax - expYmin, 0);
    planeRX = 28 + random(24);
    planeRY = random(10, 105);
    bomberScore += 1;//bomberScoreIni / 10 + 1;
    BomberScore(bomberScore);
  }
  if (x >= int(bomb1X) && x <= int(bomb1X) + 8) { //----------------- bomb one has been shot -----------------------
    bomb(int(bomb1X), int(bomb1Y), 6);
    expXmin = int(bomb1X) + 4;
    expXmax = expXmin;
    expYmin = int(bomb1Y);
    expYmax = expYmin;
    delay(100);
    bomb(int(bomb1X), int(bomb1Y), 0);
    for (int i = 1; i < 4; i++) {
      for (int j = 1; j < 10; j++) {
        expX = random(-4 * i, 4 * i) + int(bomb1X) + 4;
        expY = random(-4 * i, 4 * i) + int(bomb1Y);
        if (expX > 12 && expX < 290 && expY > 10 && expY < 135) {
          putpixel(expX, expY, 3);
          if (expX < expXmin) {
            expXmin = expX;
          }
          if (expX > expXmax) {
            expXmax = expX;
          }
          if (expY < expYmin) {
            expYmin = expY;
          }
          if (expY > expYmax) {
            expYmax = expY;
          }
        }
      }
      delay(50);
      myFullRect(expXmin, expYmin, expXmax - expXmin, expYmax - expYmin, 0);
      bomb1X = 0;
      bomb1 = 0;
    }
    bomberScore += 1;//bomberScoreIni / 20 + 1;
    BomberScore(bomberScore);
  }
  if (x >= int(bomb2X) - 8 && x <= int(bomb2X)) { //----------------- bomb two has been shot -----------------------
    bombR(int(bomb2X), int(bomb2Y), 6);
    expXmin = int(bomb2X) + 4;
    expXmax = expXmin;
    expYmin = int(bomb2Y);
    expYmax = expYmin;
    delay(100);
    bombR(int(bomb2X), int(bomb2Y), 0);
    for (int i = 1; i < 4; i++) {
      for (int j = 1; j < 10; j++) {
        expX = random(-4 * i, 4 * i) + int(bomb2X) - 4;
        expY = random(-4 * i, 4 * i) + int(bomb2Y);
        if (expX > 12 && expX < 290 && expY > 10 && expY < 135) {
          putpixel(expX, expY, 3);
          if (expX < expXmin) {
            expXmin = expX;
          }
          if (expX > expXmax) {
            expXmax = expX;
          }
          if (expY < expYmin) {
            expYmin = expY;
          }
          if (expY > expYmax) {
            expYmax = expY;
          }
        }
      }
      delay(50);
      myFullRect(expXmin, expYmin, expXmax - expXmin, expYmax - expYmin, 0);
      bomb2X = 0;
      bomb2 = 0;
    }
    bomberScore += 1;//bomberScoreIni / 20 + 1;
    BomberScore(bomberScore);
  }
}

void checkBomberShot(int whichBomb) { //------------------------- checks if bomb has hit the Bomber -----------------------------------------------
  if (int(bomb1X) + 9 >= bomberX && int(bomb1X) <= bomberX + bomberL && whichBomb == 1) { //----------------- bomber has been shot by bomb Left -----------------------
    bomber(bomberX, 4);
    expXmin = bomberX + bomberL / 2;
    expXmax = expXmin;
    expYmin = 146;
    expYmax = expYmin;
    delay(100);
    //bomber(bomberX, 0);
    for (int i = 1; i < 8; i++) {
      for (int j = 1; j < 15; j++) {
        expX = random(-5 * i, 5 * i) + bomberX + bomberL / 2;
        expY = random(-6 * i, 3 * i) + 146;
        if (expX > 12 && expX < 290 && expY > 10 && expY < 162) {
          putpixel(expX, expY, 3);
          if (expX < expXmin) {
            expXmin = expX;
          }
          if (expX > expXmax) {
            expXmax = expX;
          }
          if (expY < expYmin) {
            expYmin = expY;
          }
          if (expY > expYmax) {
            expYmax = expY;
          }
        }
      }
      delay(50);
    }
    delay(1000);
    myFullRect(expXmin, expYmin, expXmax - expXmin, expYmax - expYmin, 0);
    drawBomberIni();
    myBomber(bomberX, 1);
    //bomberScore = bomberScoreIni;
    bomberLives += -1;
    BomberLives(bomberLives);
  }
  if (int(bomb2X) >= bomberX && int(bomb2X) - 9 <= bomberX + bomberL && whichBomb == 2) { //----------------- bomber has been shot by bomb Right -----------------------
    bomber(bomberX, 4);
    expXmin = bomberX + bomberL / 2;
    expXmax = expXmin;
    expYmin = 146;
    expYmax = expYmin;
    delay(100);
    //bomber(bomberX, 0);
    for (int i = 1; i < 8; i++) {
      for (int j = 1; j < 15; j++) {
        expX = random(-5 * i, 5 * i) + bomberX + bomberL / 2;
        expY = random(-6 * i, 3 * i) + 146;
        if (expX > 12 && expX < 290 && expY > 10 && expY < 162) {
          putpixel(expX, expY, 3);
          if (expX < expXmin) {
            expXmin = expX;
          }
          if (expX > expXmax) {
            expXmax = expX;
          }
          if (expY < expYmin) {
            expYmin = expY;
          }
          if (expY > expYmax) {
            expYmax = expY;
          }
        }
      }
      delay(50);
    }
    delay(1000);
    myFullRect(expXmin, expYmin, expXmax - expXmin, expYmax - expYmin, 0);
    drawBomberIni();
    myBomber(bomberX, 1);
    //bomberScore = bomberScoreIni;
    bomberLives += -1;
    BomberLives(bomberLives);
  }
}

void myBomb1() { //------------------------- bomb 1 motion ---------------------------
  if (bomb1 == 1) {
    bomb1X -= bomb1V;
    bomb1Y += pow(((millis() - time1) / 1000.), 2);
    bomb(bomb1Xold, bomb1Yold, 0);
    if (bomb1Y > 150) {
      checkBomberShot(1); //------------------- this checks if bomb hit the Bomber ----------------
      myBomber(bomberX, 1);
    }
    if (bomb1X < 12 || bomb1X > 281 || bomb1Y > 150) {
      bomb(bomb1Xold, bomb1Yold, 0);
      bomb1 = 0;
    }
    else {
      bomb(int(bomb1X), int(bomb1Y), 5);
    }
  }
  else {
    if (random(50) == 5) { //---------------- heter it draws a new bomb ---------------------
      bomb1X = planeLX;
      bomb1Y = planeLY + 4;
      bomb1V = random(-100., 100.) / 100.;
      time1 = millis();
      bomb1 = 1;
    }
  }
}

void myBomb2() { //------------------------- bomb 2 motion ---------------------------
  if (bomb2 == 1) {
    bomb2X -= bomb2V;
    bomb2Y += pow(((millis() - time2) / 1000.), 2);
    bombR(bomb2Xold, bomb2Yold, 0);
    if (bomb2Y > 150) {
      checkBomberShot(2); //------------------- this checks if bomb hit the Bomber ----------------
      myBomber(bomberX, 1);
    }
    if (bomb2X < 12 + 9 || bomb2X > 281 + 9 || bomb2Y > 150) {
      bombR(bomb2Xold, bomb2Yold, 0);
      bomb2 = 0;
    }
    else {
      bombR(int(bomb2X), int(bomb2Y), 7);
    }
  }
  else {
    if (random(50) == 5) { //---------------- here it draws a new bomb  ---------------------
      bomb2X = planeRX;
      bomb2Y = planeRY + 4;
      bomb2V = random(-100., 100.) / 100.;
      time2 = millis();
      bomb2 = 1;
    }
  }
}

void BomberScore(int i) { //----------------------- bomber score drawing ----------------------
  if (i > 2000 && bomberLives < 12) {
    bomberLives += 1;
    bomberScore = bomberScoreIni;
    BomberLives(bomberLives);
  }
  if (i < 0) {
    bomberLives += -1;
    bomberScore = bomberScoreIni;
    delay(200);
    BomberLives(bomberLives);
    delay(200);
  }
  myFullRect(52, 165, 50, 10, 0);
  myDrawNumber(85, 164, bomberScore, 7);
}

void BomberLives(int bomberLivesTemp) { //----------------------- bomber lives drawing ----------------------
  for (int j = 1; j < 12; j++) {
    if (j <= bomberLivesTemp) {
      drawBall(150 + j * 10, 172, 3);
    } else {
      drawBall(150 + j * 10, 172, 0);
    }
  }
  if (bomberLivesTemp == 0) { //------------------------- bomber Game Over ----------------------------------------
    smoothRect(116, 68, 78, 20, 6, 6);
    myPrint("Game Over", 120, 71, 6);
    delay(100);
    toneSafe(660, 200);
    toneSafe(330, 200);
    toneSafe(165, 200);
    toneSafe(82, 200);
    button == 0;
    clearInputs();
    while (button == 0) {
      SnakeProcessInputs();
    }
    if (button_4 == 1) {
      state = 0;
    }
    else { //-------------------------------- restart new Bomber game --------------------------------
      Canvas.clear();
      drawBomberIni();
      bomberScore = bomberScoreIni;
      bomberLives = 3;
      for (int j = 1; j < 4; j++) {
        drawBall(150 + j * 10, 172, 3);
      }
      myFullRect(52, 165, 50, 10, 0);
      myDrawNumber(85, 164, bomberScore, 4);
      planeRX = 340; //------------- this is needed for correct inizialization of planeR -------------
      planeRXold = 340;
      planeLX = 28; //------------- this is needed for correct inizialization of planeR -------------
      planeLXold = 340;
      bomberX = 120;
      myBomber(bomberX, 1);
      bomb1 = 0;
      bomb2 = 0;
      delay(100);
    }
    //state = 0;
  }
}

// perform the actual update from a given stream
void performUpdate(Stream &updateSource, size_t updateSize) {

  if (Update.begin(updateSize, U_FLASH, 2, 1, "Basic")) {
    size_t written = Update.writeStream(updateSource);

    if (Update.end()) {
      //vga.setTextColor(WHITE,BLACK);
      //vga.setCursor(10, 80);
      myPrint("Basic32+ loaded , now Reboot", 20, 170, 7);
      //vga.print("Basic32+ loaded successfully, now Reboot");//, (30), (60), WHITE, BLACK); //Serial.println("OTA done!");
      if (Update.isFinished()) {
        delay(1000);
        ESP.restart();
      }
    }
  }
}
//*********************************
void basic_loader(void) {

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

  if ( !SD.begin( kSD_CS, spiSD )) {                        //SD-Card starten
     myPrint("SD-Card Mount-Error!", 20, 170, 7);
    
    return;
  }
  if ( !SD.exists("/basic.bin") ) {
    
    myPrint("Basic not found!", 20, 170, 7);
  }

  File updateBin = SD.open("/basic.bin");

  if (updateBin) {
    size_t updateSize = updateBin.size();

    if (updateSize > 0) {
      
      myPrint("load Basic32+", 20, 170, 7);
      //Serial.println("load Basic32+");
      performUpdate(updateBin, updateSize);
    }

    updateBin.close();
  }

}
