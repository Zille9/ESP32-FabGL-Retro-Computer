void show_Command_Help(int was){
  switch (was){
    case 0:
     Terminal.println("LIST <Linenumber>");
     Terminal.println("Linenumber is optional");
     break;
    case 1:
     Terminal.println("LOAD Filename");
     Terminal.println("Filename in qoute or as String");
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
     Terminal.println("POKE memtype, adress, value");
     Terminal.println("EXAMPLE: POKE 1,10,123");
     break;
    case 16:
     Terminal.println("DIR");
     Terminal.println("Shows the Directory of SD-Card");
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
     Terminal.println("RECT x,y,w,h,fill");
     Terminal.println("Draws a Rectangle on x,y,w (width),h (height),fill");
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
     Terminal.println("SPRT(X,Y,Draw)");                                //noch überarbeiten
     Terminal.println("SPRT(x,y,0) move the Pen on x,y");
     Terminal.println("SPRT(x,y,1) draw from old Position to x,y");
     break;
    case 50:
     Terminal.println("PULSE(PIN,nPulse,Pause1 ms,Pause 2 ms)");                                
     Terminal.println("Pulse on PIN,n count Pulse,Pause1(high),Pause2(low)");
     Terminal.println("Pins can be 2,12,26,27");
     break;
    case 51:
    case 52:
    case 53:
    case 54:
    case 55:
    case 56:
    case 57:
    case 58:
    case 59:
    case 60:
    case 61:
    case 62:
    case 63:
    case 64:
    case 65:
    case 66:
    case 67:
    case 68:
    case 69:
    case 70:
    case 71:
    case 72:
    case 73:
    case 74:
     
    default:
     Terminal.println("coming soon");
     break;      
  }
}
void show_Function_Help(int was){
  switch (was){
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
     break;
    case 4:
     Terminal.println("COS(Value)");
     Terminal.println("EXAMPLE: A=COS(34)");
     break;
    case 5:
     Terminal.println("TAN(Value)");
     Terminal.println("EXAMPLE: A=TAN(45)");
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
    case 10:
     break;
    case 11:
     break;
    case 12:
     break;
    case 13:
     break;
    case 14:
     break;
    case 15:
     break;
    case 16:
    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
    case 31:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
    case 48:
    case 49:
    case 50:
    case 51:
    case 52:
    case 53:
    case 54:
    case 55:
    case 56:
    case 57:
    
    default:
     Terminal.println("coming soon");
     break;      
  }
}
