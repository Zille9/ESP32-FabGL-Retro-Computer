//************************************************integrated Helpsystem***************************************************
//*************************************************** Befehl Help ***************************************************************************
void show_help(void) {  //Unterprogramme des Helpsystems in help_sys
  int n, e, z, y;
  int x[] = {};
  x[0] = 0;
  x[1] = 9;
  x[2] = 18;
  x[3] = 27;
  Terminal.println("BASIC-COMMANDS:");
  n = z = 0;
  Terminal.println("---------------");

  for (int i = 0; i < KW_WORDS; i++)
  { e = 0;
    tc.setCursorPos(z * 8, tc.getCursorRow());
    while (!e) {
      if (keywords[n] > 0x80) {
        Terminal.write(keywords[n++] - 0x80);
        e = 1;
      }
      else  Terminal.write(keywords[n++]);
    }
    z++;
    if (z == 5) {
      Terminal.println();
      z = 0;
      y = tc.getCursorRow();
    }
  }

  if (wait_key(true) == 3 || break_marker) {
    break_marker = false; 
    return;
  }
  
  n = 0;
  z = 0;
  Terminal.println();
  Terminal.println();
  Terminal.println("BASIC-FUNCTIONS:");
  Terminal.println("----------------");
  y = tc.getCursorRow();
  for (int i = 0; i < FUNC_WORDS; i++)
  { e = 0;
    tc.setCursorPos(z * 8, tc.getCursorRow());

    while (!e) {
      if (func_tab[n] > 0x80) {
        Terminal.write(func_tab[n++] - 0x80);
        e = 1;
      }
      else Terminal.write(func_tab[n++]);
    }
    z++;
    if (z == 5 ) {
      Terminal.println();
      z = 0;
    }
  }
  Terminal.println();

}

void show_help_name(void) {                                             //Anzeige aller Befehle und Funktionen
  int kw, fu;
  tmptxtpos = txtpos;
  scantable(keywords);                                                  //Befehlstabelle lesen
  kw = table_index;
  txtpos = tmptxtpos;
  scantable(func_tab);                                                  //Funktionstabelle lesen
  fu = table_index;
  if (kw != KW_DEFAULT) show_Command_Help(kw);                          //Hilfe zum Befehl anzeigen
  if (fu != FUNC_UNKNOWN)show_Function_Help(fu);                        //Hilfe zur Funktion anzeigen
}

void show_Command_Help(int was) {                                       //Anzeige eines spezifischen Befehls oder Funktion
  switch (was) {
    case 0:
      Terminal.println("LIST <from Line, to Line>");
      Terminal.println("from and to Line is optional");
      break;
    case 1:
      Terminal.println("LOAD Filename");
      Terminal.println("Filename in qoute or as String");
      Terminal.println("Bsp.: LOAD'/TEST.BAS'");
      Terminal.println("LOAD without Name,load a Program from FRAM");
      break;
    case 2:
      Terminal.println("NEW");
      Terminal.println("Delete Program and clears the Variables");
      break;
    case 3:
      Terminal.println("RUN");
      Terminal.println("Start the Programm");
      break;
    case 4:
      Terminal.println("SAVE Filename");
      Terminal.println("Filename in qoute or as String");
      Terminal.println("SAVE without Name,save a Program in FRAM");
      break;
    case 5:
      Terminal.println("NEXT Variable");
      Terminal.println("End of FOR NEXT Loop");
      break;
    case 6:
      Terminal.println("REN Filename_old, Filename_new");
      Terminal.println("Filename in qoute or as String");
      break;
    case 7:
      Terminal.println("IF condition THEN result");
      Terminal.println("EXAMPLE: IF A>5 THEN PRINT A");
      break;
    case 8:
      Terminal.println("GOTO Linenumber");
      Terminal.println("EXAMPLE: GOTO 230");
      break;
    case 9:
    case 10:
      Terminal.println("GOSUB Linenumber");
      Terminal.println("EXAMPLE :500 GOSUB 700");
      Terminal.println("         600 END");
      Terminal.println("         700 PRINT A");
      Terminal.println("         710 RETURN");
      break;
    case 11:
      Terminal.println("REM comment");
      Terminal.println("EXAMPLE: REM here starts the Program");
      break;
    case 12:
      Terminal.println("FOR Var=Start to End-Condition");
      Terminal.println("EXAMPLE: FOR I=0 TO 25 STEP 2:PRINT I:NEXT I");
      break;
    case 13:
      Terminal.println("INPUT Inputtext;var,var$...");
      Terminal.println("EXAMPLE: INPUT'NAME:';A$");
      break;
    case 14:
      Terminal.println("PRINT Value, String...");
      Terminal.println("EXAMPLE: PRINT SIN(34)+SQR(18)");
      break;
    case 15:
      Terminal.println("POKE Memtype, Adress, Value");
      Terminal.println("Memtype 0=Ram, 1=FRam, 2=EEProm");
      Terminal.println("EXAMPLE: POKE 1,10,123");
      break;
    case 16:
      Terminal.println("DIR");
      Terminal.println("Shows the Directory of SD-Card");
      Terminal.println("DIR'BAS'");
      Terminal.println("Shows only Files with the BAS-Extension");
      break;
    case 17:
      Terminal.println("CLS");
      Terminal.println("Clear Screen");
      break;
    case 18:
      Terminal.println("POS x,y");
      Terminal.println("set the Cursor on Position x,y");
      Terminal.println("EXAMPLE: POS 2,5");
      break;
    case 19:
      Terminal.println("COL FG,BG");
      Terminal.println("Change the Foreground and Background Color");
      Terminal.println("EXAMPLE: COL 0,60");
      Terminal.println("set the Pencolor to black ,Background to yellow");
      Terminal.println("EXAMPLE: PRINT COL(0)");
      Terminal.println("Return 0=Pencolor, 1=Backgroundcolor");
      break;
    case 20:
      Terminal.println("PSET x,y,color");
      Terminal.println("Set a Pixel on x,y with color");
      Terminal.println("color is optional");
      break;
    case 21:
      Terminal.println("CIRC x,y,w,h,fill");
      Terminal.println("Draws a Circle on x,y,w (width),h (height),fill");
      Terminal.println("fill=1 draw a filled Circle");
      break;
    case 22:
      Terminal.println("LINE x,y,xx,yy");
      Terminal.println("Draws a Line on x,y,xx,yy with Pencolor");
      break;
    case 23:
      Terminal.println("RECT x,y,xx,yy,fill");
      Terminal.println("Draws a Rectangle on x,y to xx,yy,fill");
      Terminal.println("fill=1 draw a filled Rectangle");
      break;
    case 24:
      Terminal.println("FONT f");
      Terminal.println("Change the Fontset (0..26)");
      break;
    case 25:
      Terminal.println("PAUSE ms");
      Terminal.println("Pause in milliseconds");
      break;
    case 26:
      Terminal.println("END");
      Terminal.println("program is terminated");
      break;
    case 27:
      Terminal.println("CLEAR");
      Terminal.println("Clears all Variables and Arrays");
      break;
    case 28:
      Terminal.println("THEN");
      Terminal.println("show IF-Command");
      break;
    case 29:
      Terminal.println("ELSE");
      Terminal.println("alternative condition on IF THEN");
      break;
    case 30:
      Terminal.println("CUR 0..1");
      Terminal.println("CURSOR ON=1 or OFF=0");
      break;
    case 31:
      Terminal.println("PRZ(n)");
      Terminal.println("number of decimal places");
      break;
    case 32:
      Terminal.println("DMP Memtype,Adress");
      Terminal.println("Hexmonitor for Program 0 or FRAM 1");
      break;
    case 33:
      Terminal.println("STYLE 0..6");
      Terminal.println("Changed the Text-Style from 0-6");
      break;
    case 34:
      Terminal.println("SCROLL x,y");
      Terminal.println("Scroll the Screen x and y Pixel");
      Terminal.println("x>0 right x<0 left, y>0 down y<0 up");
      break;
    case 35:
      Terminal.println("START filename");
      Terminal.println("Load and Start the Program Filename");
      Terminal.println("Start without Parameter starts a Program in FRAM");
      break;
    case 36:
      Terminal.println("THEME 0..10");
      Terminal.println("changed the Start-Screen and Color Scheme");
      break;
    case 37://DATA
    case 38://READ
    case 39://RESTORE
      Terminal.println("DATA");
      Terminal.println("DATA Lines");
      Terminal.println("READ Var - reads Datalines");
      Terminal.println("RESTORE resets the Data-Pointer");
      Terminal.println("RESTORE lnr - set the Data-Pointer on Line->lnr");
      break;
    case 40:
      Terminal.println("DEL Filename");
      Terminal.println("Delete File on SD-Card");
      break;
    case 41:
      Terminal.println("AND");
      Terminal.println("Logical AND Condition");
      Terminal.println("EXAMPLE: IF A>5 AND B<6 THEN C=0");
      break;
    case 42:
      Terminal.println("OR");
      Terminal.println("Logical OR Condition");
      Terminal.println("EXAMPLE: IF A>5 OR B<6 THEN C=0");
      break;
    case 43:
      Terminal.println("RTC dd,mm,jjjj,hh,mm,ss");
      Terminal.println("Set the Realtime-Clock");
      Terminal.println("EXAMPLE: RTC 2,4,2023,13,34,00");
      break;
    case 44:
      Terminal.println("IIC(S,Deviceadress) Start Transmission");
      Terminal.println("IIC(W,Value) write value");
      Terminal.println("IIC(Q,Deviceadress,nBytes) IIC-RequestFrom");
      Terminal.println("IIC(E) End Transmission");
      Terminal.println("IIC(R) read a Byte");
      Terminal.println("IIC(A) Data Available?");
      break;
    case 45:
      Terminal.println("DOUT(PIN,0..1)");
      Terminal.println("Set Pin Low(0) or High(1)");
      Terminal.println("Outputpins can be 2,12,26,27");
      break;
    case 46:
      Terminal.println("PWM(PIN,Value)");
      Terminal.println("PWM-Output on PIN");
      Terminal.println("Outputpins can be 2,12,26,27");
      break;
    case 47:
      Terminal.println("DAC(Value in V)");
      Terminal.println("DAC-Output on PIN26 in 0..3.3V");
      break;
    case 48:
      Terminal.println("DRAW(X,Y,Draw)");
      Terminal.println("DRAW(x,y,0) move the Pen on x,y");
      Terminal.println("DRAW(x,y,1) draw from old Position to x,y");
      break;
    case 49:
      Terminal.println("SPRT(C,nr)-> create nr Sprites");                                //noch überarbeiten
      Terminal.println("SPRT(D,nr,xdim,ydim,color,A$) def Sprite ");
      Terminal.println("A$ holds the Sprite-Data");
      Terminal.println();
      Terminal.println("SPRT(S,nr,'H'/'V',x,y) Set Sprite on x,y");
      Terminal.println("H=Hide, V=Visible");
      break;
    case 50:
      Terminal.println("PULSE(PIN,nPulse,Pause1 ms,Pause 2 ms)");
      Terminal.println("Pulse on PIN,n count Pulse,Pause1(high),Pause2(low)");
      Terminal.println("Pins can be 2,12,26,27");
      break;
    case 51:
      Terminal.println("SND(waveform,frequency,duration,volume)");
      Terminal.println("Waveform: 1=SINE");
      Terminal.println("          2=SQUARE");
      Terminal.println("          3=TRIANGLE");
      Terminal.println("          4=SAWTOOTH");
      Terminal.println("          5=NOISE");
      Terminal.println("          6=VIC NOISE");
      Terminal.println("Frequency in Hertz");
      Terminal.println("Duration in mS");
      Terminal.println("Volume 0..127");
      break;
    case 52:
      Terminal.println("PEN(Color<,Width>)");
      Terminal.println("Change the Pen-Color and Pen-width");
      Terminal.println("Pen-width is optional");
      break;
    case 53:
      Terminal.println("ON Variable");
      Terminal.println("Expandet GOTO or GOSUB Command");
      Terminal.println("EXAMPLE: ON A GOTO 120,230,430");
      break;
    case 54:
      Terminal.println("LCD(I,0..3) init LCD");
      Terminal.println("LCD(C) clear LCD");
      Terminal.println("LCD(B,0..1) Backlight on/off");
      Terminal.println("LCD(G,1..4) goto Line");
      Terminal.println("LCD(L,1..4) clear Line");
      Terminal.println("LCD(M,0..1,n) move Display left/right n Pos.");
      Terminal.println("LCD(N,1..8) number of decimal places");
      Terminal.println("LCD(W,expression) Write expressions without nl");
      Terminal.println("LCD(P,expression) Print expressions with nl");
      Terminal.println("LCD(S,colums,rows,I2C-Adress) set the LCD-Parameter");
      break;
    case 55:
      Terminal.println("PORT(S/W,A/B/C)");
      Terminal.println("PORT(S,A/B/C,0..1) set Port A, B or AB");
      Terminal.println("PORT(W,A/B/C,Value) write Value on Port A,B or AB");
      break;
    case 56:
      Terminal.println("PIN(S/W,Pin,val)");
      Terminal.println("PIN(S,Pin,0..1) set Pin Input/Output");
      Terminal.println("PIN(W,Pin,0..1) write Pin High or Low");
      break;
    case 57:
      Terminal.println("CHD Pathname");
      Terminal.println("Change Path of SD-Card");
      Terminal.println("Pathname must be in quotes");
      break;
    case 58:
      Terminal.println("MKD Pathname");
      Terminal.println("Make Directory of SD-Card");
      Terminal.println("Pathname must be in quotes");
      break;
    case 59:
      Terminal.println("RMD Pathname");
      Terminal.println("Remove Directory from SD-Card");
      Terminal.println("Pathname must be in quotes");
      break;
    case 60:
      Terminal.println("DEFN Var=[Function]");
      Terminal.println("Create User-Function");
      Terminal.println("EXAMPLE: DEFN A(A,B)=[SIN(A)*COS(B)]");
      Terminal.println("         c=FN A(12,5)");
      break;
    case 61:
      Terminal.println("LED(S,Count,Pin,Typ) set Startparameter");
      Terminal.println("LED(B,0..255) Brightness");
      Terminal.println("LED(C) clear the Strip");
      Terminal.println("LED(M,1..2)->Rainbow");
      Terminal.println("LED(F,r,g,b,start,cnt)->Fill");
      Terminal.println("LED(P,r,g,b,nr)->Paint");
      Terminal.println("LED(W,r,g,b,delay)->Wipe");
      Terminal.println("LED(T,r,g,b,delay)->Theatre");
      break;
    case 62:
      Terminal.println("EDIT Linenumber");
      Terminal.println("Lineeditor");
      break;
    case 63:
      Terminal.println("DOKE Memtype,Adress,Word");
      Terminal.println("Save Word-Value in Memory");
      Terminal.println("Memtype 0=Ram, 1=FRam, 2=EEProm");
      break;
    case 64:
      Terminal.println("BEEP(Note,Duration)");
      Terminal.println("BEEP(20..80,duration ms)");
      Terminal.println("BEEP without Parameters generat a System-beep");
      break;
    case 65:
      Terminal.println("DIM Var(dim,dim,dim)");
      Terminal.println("dimensioned arrays with max 3 Dimensions");
      Terminal.println("strings cannot be dimensioned");
      break;
    case 66:
      Terminal.println("OPT Func=Val");
      Terminal.println("Option saved in EEPROM");
      Terminal.println("Available Options are:");
      Terminal.println("THEME=0..10");
      Terminal.println("KEY= ->(1=US,2=UK,3=GE,4=IT,5=ES,6=FR,7=BE,8=NO,9=JP");
      Terminal.println("COLOR=Foreground,Background");
      Terminal.println("FONT=0..25");
      Terminal.println("EEPROM=Adress");
      Terminal.println("FRAM=FRAM-CS-Pin");
      Terminal.println("SDCARD=SCK,MISO,MOSI,CS - Pins");
      Terminal.println("PATH='Workpath' (must be in quotes)");
      break;
    case 67:
      Terminal.println("FPOKE Memtype,Adress,Val");
      Terminal.println("Save Floatvalue in Memory");
      Terminal.println("Memtype 0=Ram, 1=FRam, 2=EEProm");
      break;
    case 68:
      Terminal.println("MNT");
      Terminal.println("Mount SD-Card");
      break;
    case 69:
      Terminal.println("COM");
      Terminal.println("Serial-Function");
      break;
    case 70:
      Terminal.println("PIC_I(x,y,BMP-Filename)-Import BMP");
      Terminal.println("PIC_E(x,y,xx,yy,BMP-Filename)-Export");
      Terminal.println();
      Terminal.println("PIC_S(place,Pic-Filename)");
      Terminal.println("Save Raw-Pic from FRAM (0..4)");
      Terminal.println();
      Terminal.println("PIC_L(place,Pic-Filename)");
      Terminal.println("Load Raw-Pic in FRAM (0..4)");
      Terminal.println();
      Terminal.println("PIC_P(place,x,y,xx,yy)");
      Terminal.println("Save Screen as Raw-Pic in FRAM (0..4)");
      Terminal.println();
      Terminal.println("PIC_D(place<mode,x,y>)");
      Terminal.println("Show Raw-Pic in FRAM (0..4)on x,y");
      Terminal.println("mode 0..1 changes the Background-color");
      break;
    case 71:
      Terminal.println("OPEN filename,mode");
      Terminal.println("open Filename,r or w");
      Terminal.println("Filename must be in qoutes");
      break;
    case 72:
      Terminal.println("CLOSE");
      Terminal.println("close the File");
      break;
    case 73:
      Terminal.println("FILE_WR val1,val2...");
      Terminal.println("writes values or strings in File");
      Terminal.println("EXAMPLE: 10 OPEN 'Neu.txt',W");
      Terminal.println("         20 A=12.34");
      Terminal.println("         30 FILE_WR A");
      Terminal.println("         40 CLOSE");
      Terminal.println("FILE_RD val1,val2...");
      Terminal.println("reads values or strings from File");
      break;
    case 74:
      Terminal.println("TYPE Filename");
      Terminal.println("shows the content of a file");
      Terminal.println("Filename must be in quotes");
      break;
    case 75:
      Terminal.println("BLOAD Filename.Bin");
      Terminal.println("start binary-Program");
      break;
    case 76:
      Terminal.println("GRID_typ(x,y,x_zell,y_zell,x_pix,y_pix,fcol,gcol,<opt>)");
      Terminal.println("draws a Grid in variable size at x,y position");
      Terminal.println("with framecolor and gridcolor");
      Terminal.println("typ can be - RS, KT, XY, UI");
      Terminal.println("opt -> pix_spacing, scale, arrow, frame");
      Terminal.println("EXAMPLE:GRID_XY(10,10,10,8,8,8,60,21,2,1,0,0)");
      break;
    case 77:
      Terminal.println("TEXT(x,y,font,String$)");
      Terminal.println("draws a String at x,y position");
      Terminal.println("EXAMPLE: TEXT(100,100,3,'Hello World')");
      break;
    case 78:
      Terminal.println("? val");
      Terminal.println("has the same function as Print");
      break;
    case 79:
      Terminal.println("WINDOW(nr,x,y,xx,yy<,color,Title>)");
      Terminal.println("creates a window nr=1..5");
      Terminal.println("WINDOW without Parameters switch to Mainwindow");
      Terminal.println("WINDOW(nr) switch to Window nr");
      Terminal.println("color determines the border-color");
      Terminal.println("Title is the window title"); 
      break;
    case 80:
      Terminal.println("HELP");
      Terminal.println("without Parameters shows all Commands");
      Terminal.println("HELP Keyword shows Keyword-explanations");
      break;
    case 81:
      Terminal.println("FRAME x,y,w,h,r");
      Terminal.println("Draws a rectangle with rounded corners");
      Terminal.println("FRAME x,y,w(width),h(height),r(radius)");
    default:

      break;
  }
}
void show_Function_Help(int was) {
  Terminal.println();
  switch (was) {
    case 0:
      Terminal.println("PEEK(Memtype,Adress)");
      Terminal.println("EXAMPLE: A=PEEK(1,1200)");
      break;
    case 1:
      Terminal.println("ABS(Value)");
      Terminal.println("EXAMPLE: A=ABS(-4)");
      break;
    case 2:
      Terminal.println("RND(Value)");
      Terminal.println("EXAMPLE: A=RND(25)");
      break;
    case 3:
      Terminal.println("SIN(Value)");
      Terminal.println("EXAMPLE: A=SIN(23)");
      Terminal.println("returns the Sinus in RAD");
      break;
    case 4:
      Terminal.println("COS(Value)");
      Terminal.println("EXAMPLE: A=COS(34)");
      Terminal.println("returns the Cosinus in RAD");
      break;
    case 5:
      Terminal.println("TAN(Value)");
      Terminal.println("EXAMPLE: A=TAN(45)");
      Terminal.println("returns the Tangens in RAD");
      break;
    case 6:
      Terminal.println("LOG(Value)");
      Terminal.println("EXAMPLE: A=LOG(35)");
      break;
    case 7:
      Terminal.println("SGN(Value)");
      Terminal.println("EXAMPLE: A=SGN(-3)");
      Terminal.println("Returns: SGN(x) x>0=1 x=0->0 x<0=-1");
      break;
    case 8:
      Terminal.println("SQR(Value)");
      Terminal.println("EXAMPLE: A=SQR(81)");
      break;
    case 9:
      Terminal.println("INT(Value)");
      Terminal.println("EXAMPLE: PRINT INT(81.23) ->81");
      break;
    case 10:
      Terminal.println("MIN(Val1,Val2)");
      Terminal.println("EXAMPLE: PRINT MIN(81,23) ->23");
      break;
    case 11:
      Terminal.println("MAX(Value1,Value2)");
      Terminal.println("EXAMPLE: PRINT MAX(81,23) ->81");
      break;
    case 12:
      Terminal.println("EXP(Val)");
      Terminal.println("EXAMPLE: PRINT EXP(1) ->2.718282");
      break;
    case 13:
      Terminal.println("GET(Val)");
      Terminal.println("EXAMPLE: GET(0)->get Cursor Col");
      Terminal.println("         GET(1)->get Cursor Row");
      break;
    case 14:
      Terminal.println("CHR$(Val)");
      Terminal.println("EXAMPLE: PRINT CHR$(65)->A");
      break;
    case 15:
      Terminal.println("INKEY");
      Terminal.println("EXAMPLE: A=INKEY");
      Terminal.println("A=last Keypress");
      break;
    case 16:
      Terminal.println("FONT");
      Terminal.println("EXAMPLE: PRINT FONT");
      Terminal.println("returns the current font number");
      break;
    case 17:
      Terminal.println("ASC(CHAR)");
      Terminal.println("EXAMPLE: PRINT ASC(char)");
      Terminal.println("returns the ASCII Code of char");
      Terminal.println("char must be in quotes");
      break;
    case 18:
      Terminal.println("TIMER");
      Terminal.println("EXAMPLE: A=TIMER");
      Terminal.println("returns the millis since ESP-start");
      break;
    case 19:
      Terminal.println("ATN(Val)");
      Terminal.println("EXAMPLE: A=ATN(VAL)");
      Terminal.println("returns the AcusTangens in RAD");
      break;
    case 20:
      Terminal.println("LEN(Val$)");
      Terminal.println("EXAMPLE: A=LEN(A$)");
      Terminal.println("returns the Length of A$");
      break;
    case 21:
      Terminal.println("INSTR(Search$,String$)");
      Terminal.println("EXAMPLE: A=INSTR(A$,B$)");
      Terminal.println("returns the pos if A$ exist in B$");
      break;
    case 22:
      Terminal.println("COMP$(Search$,String$)");
      Terminal.println("EXAMPLE: A=COMP$(A$,B$)");
      Terminal.println("returns 0 a$=B$, 1 A$>B$, -1 A$<B$");
      break;
    case 23:
      Terminal.println("BIN(Val)");
      Terminal.println("EXAMPLE: PRINT BIN(81)->%1010001");
      break;
    case 24:
      Terminal.println("HEX(Val)");
      Terminal.println("EXAMPLE: PRINT HEX(81)->#51");
      break;
    case 25:
      Terminal.println("LEFT$(String$,n)");
      Terminal.println("EXAMPLE: PRINT LEFT$(A$,4)");
      Terminal.println("Prints the left 4 chars of A$");
      break;
    case 26:
      Terminal.println("RIGHT$(String$,n)");
      Terminal.println("EXAMPLE: PRINT RIGHT$(A$,4)");
      Terminal.println("Prints the right 4 chars of A$");
      break;
    case 27:
      Terminal.println("MID$(String$,start,n)");
      Terminal.println("EXAMPLE: PRINT MID$(A$,4,3)");
      Terminal.println("Prints 3 chars of A$ starting at 4");
      break;
    case 28:
      Terminal.println("TAB(Val)");
      Terminal.println("EXAMPLE: PRINT TAB(5);12.3");
      Terminal.println("Print 12.3 on x-pos 5");
      break;
    case 29:
      Terminal.println("SPC(Val)");
      Terminal.println("EXAMPLE: PRINT SPC(5)");
      Terminal.println("Print 5 Spaces ");
      break;
    case 30:
      Terminal.println("STR$(Val,n)");
      Terminal.println("EXAMPLE: A$=STR$(-15.34,2)");
      Terminal.println("Converts Val to String with");
      Terminal.println("n numbers of decimal places");
      break;
    case 31:
      Terminal.println("DATE(Val)");
      Terminal.println("EXAMPLE: A=DATE(0)");
      Terminal.println("Return Day from Date-Array");
      Terminal.println("0=Day, 1=Month, 2=Year, 3=DayofWeek");
      break;
    case 32:
      Terminal.println("TIME(Val)");
      Terminal.println("EXAMPLE: A=TIME(0)");
      Terminal.println("Return Hour from Time-Array");
      Terminal.println("0=Hour, 1=Minute, 2=Second");
      break;
    case 33:                                                      //IIC
      break;
    case 34:
      Terminal.println("AKKU(Val)");
      Terminal.println("EXAMPLE: A=AKKU(0)");
      Terminal.println("Return the Akku-Value in V");
      Terminal.println("0=Volt, 1=Percent");
      break;
    case 35:
      Terminal.println("AIN(PIN<,mode>)");
      Terminal.println("EXAMPLE: A=AIN(2,1)");
      Terminal.println("Return the Analog-Value of Pin2 in V");
      Terminal.println("mode: 0=Digital, 1=Volt");
      Terminal.println("Pin's 2,12,26,27,34,35,36 allowed");
      break;
    case 36:
      Terminal.println("DIN(PIN)");
      Terminal.println("EXAMPLE: A=DIN(2)");
      Terminal.println("Return the Digital-Value of Pin2");
      Terminal.println("0=Low, 1=High");
      Terminal.println("Pin's 2,12,26,27,34,35,36 allowed");
      break;
    case 37:
      Terminal.println("UC$(A$)");
      Terminal.println("EXAMPLE: A$=UC$(B$)");
      Terminal.println("converts B$ to uppercase");
      break;
    case 38:
      Terminal.println("LC$(A$)");
      Terminal.println("EXAMPLE: A$=LC$(B$)");
      Terminal.println("converts B$ to lowercase");
      break;
    case 39:
      Terminal.println("VAL(A$)");
      Terminal.println("EXAMPLE: PRINT 5+VAL(A$)");
      Terminal.println("converts A$ to a numeric Value");
      Terminal.println("only Numbers in A$ allowed");
      break;
    case 40:
      Terminal.println("MEM");
      Terminal.println("EXAMPLE: PRINT MEM");
      Terminal.println("Returns the free Memory-Bytes");
      break;
    case 41:
      Terminal.println("!");
      Terminal.println("EXAMPLE: IF !A then PRINT B");
      Terminal.println("Logical Not Operator");
      break;
    case 42:
      Terminal.println("TEMP(PIN,CHAN)");
      Terminal.println("EXAMPLE: PRINT TEMP(2,1)");
      Terminal.println("Read Dallas DS18S20 Sensor");
      Terminal.println("only Pin 2 and 26 allowed!");
      break;
    case 43:
      Terminal.println("DHT(PIN,Typ, Temp/Humi)");
      Terminal.println("EXAMPLE: PRINT DHT(2,1,0)");
      Terminal.println("Read DHT Sensor");
      Terminal.println("Typ:1=DHT11, 2=DHT22, 3=dht21");
      Terminal.println("Temp=0, Humi=1");
      Terminal.println("only Pin 2 and 26 allowed!");
      break;
    case 44:                                                  //COL
      break;
    case 45:
      Terminal.println("BMP(mode)");
      Terminal.println("EXAMPLE: PRINT BMP(0)");
      Terminal.println("Read BMP085 Sensor");
      Terminal.println("0=Pressure in Pa, 1=Altitude in m, 2=Temp");
      break;
    case 46:                                                  //FN
      Terminal.println("DEFN Var=[Function]");
      Terminal.println("Create User-Function");
      Terminal.println("EXAMPLE: DEFN A(A,B)=[SIN(A)*COS(B)]");
      Terminal.println("         c=FN A(12,5)");
      break;
    case 47:
      Terminal.println("A=PORT(A) read Port A");
      Terminal.println("A=PORT(B) read Port B");
      Terminal.println("A=PORT() read Port A+B");
      break;
    case 48:
      Terminal.println("A=PIN(1..16) read Pin 1..16");
      break;
    case 49:
      Terminal.println("A=PI() = circular number pi");
      break;
    case 50:
      Terminal.println("A=LN(val) = natural Log.");
      Terminal.println("val must be greater as -1");
      break;
    case 51:
      Terminal.println("A=DEEK(Memtype,Adress)");
      Terminal.println("reads a Word value from Memory");
      Terminal.println("0=RAM, 1=FRAM, 2=EEPROM");
      break;
    case 52:
      Terminal.println("A=FPEEK(Memtype,Adress)");
      Terminal.println("reads a Float value from Memory");
      Terminal.println("0=RAM, 1=FRAM, 2=EEPROM");
      break;
    case 53:
      Terminal.println("A=GPX(x,y)");
      Terminal.println("reads the Color on pos x,y");
      break;
    case 54:
      Terminal.println("A=GPIC(0/1)");
      Terminal.println("reads the width(0) or height(1)");
      Terminal.println("from the last loaded BMP-Picture");
      break;
    case 55:
      Terminal.println("A=MAP(x,old_min,old_max,new_min,new_max)");
      Terminal.println("Re-maps a number from one range to another");
      Terminal.println("x is the Number to map");
      break;
    case 56:
      Terminal.println("A=CONS(x,min,max)");
      Terminal.println("Constrains a number to be within a range");
      Terminal.println("x is the Number to Constrain");
      break;
    case 57:
      Terminal.println("STRING$(n,String)");
      Terminal.println("repeats the string n times");
      Terminal.println("EXAMPLE: PRINT STRING$(23,'-')");
      Terminal.println("String must be in quotes");
      break;
    case 58:
      Terminal.println("val=FILE_PS");
      Terminal.println("return the Fileposition in a opened File");
      Terminal.println("val=FILE_SZ");
      Terminal.println("return the Filesize of an opened File");
      Terminal.println("Don't forget to close a file after use!!!");
      break;
    case 59:
      Terminal.println("val=GRID(0..11)");
      Terminal.println("0=x, 1=y, 2=xx, 3=yy, 4=x_zell, 5=y_zell");
      Terminal.println("6=x_pixel, 7=y_pixel, 8=frame_col, 9=Grid_col");
      Terminal.println("10=y-Pos x-Axis, 11=x-pos y-Axis");
      Terminal.println("return the last Gridparameter");
      Terminal.println("EXAMPLE: A=GRID(2) - xx_pos");
      break;

    default:
      Terminal.println("coming soon");
      break;
  }
}
