//------------------------------------------------- FILE_RD und FILE_WR --------------------------------------------------------------
int File_Operations(void) {
  char c;
  char *teilArray[16];
  int i;
  long ps;
  char *s;
  if (Test_char('_')) return 1;                       //Unterstrich für folgenden Befehlsbuchstaben
  c = spaces();                                       //Befehlsbuchstabe lesen
  txtpos++;
  switch (c) {
    case 'R':                                         //FILE_RD ->read from File
      if (Test_char('D')) return 1;
      File_read();                                    //Zeile aus Datei einlesen
      teilArray[0] = strtok(tempstring, ";");         //Zeile zerlegen
      if (File_line(teilArray[0])) return 1;          //ersten Wert an Variable/String übergeben
      if (*txtpos == ',') txtpos++;                   //weitere Werte?
      else return 0;                                  //nein, dann zurück
      i = 0;
      while (1) {
        s = strtok(NULL, ";");                        //tempstring weiter zerlegen (Trennzeichen ist ';')
        if (s != NULL) {
          teilArray[i++] = s;
          if (File_line(teilArray[i - 1])) return 1;  //weitere Werte in Variablen/Strings speichern
        }
        if (*txtpos == ',')
          txtpos++;
        else break;
      }
      break;

    case 'W':                                         //FILE_WR ->write in File
      if (Test_char('R')) return 1;
      File_write();
      break;
      
    case 'P':                                         //FILE_PS ->set Pos in File
      if (Test_char('S')) return 1;
      if(Test_char('(')) return 1;
      ps=long(get_value());
      if(Test_char(')')) return 1;
      if(ps<=File_size) File_pos=ps;
      break;

    default:
      break;
  }

return 0;
}

//--------------------------------------------- Befehl OPEN ---------------------------------------------------------------------------------------
void file_rw_open(void) {
  char c,File_function;
  int a = 1;
  get_value();                    //Dateiname in tempstring

  strcpy(filestring, tempstring); //Tempstring nach filestring kopieren

  if (Test_char(',')) {
    syntaxerror(sdfilemsg);
    return;
  }

  File_function = *txtpos;        //Datei-Operation (r,w)
  txtpos++;
  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

  if ( SD.exists( String(sd_pfad) + String(filestring)) && (File_function == 'W')) //Datei existiert, überschreiben?
  {
    Terminal.print("File exist, overwrite? (y/n)");
    while (a)
    {
      c = wait_key(false);
      if (c == 'y' || c == 'n')
        a = 0;
    }
    if (c == 'y') {
      SD.remove( String(sd_pfad) + String(filestring));
      Terminal.print(c);
    }
  }

  if (File_function == 'W') {                                         //Dateien für Write-Operation immer im Append-Modus öffnen
    fp = SD.open( String(sd_pfad) + String(filestring), FILE_APPEND);
    File_size = fp.size();                                            //Dateigrösse merken
    fp.close();
    Datei_open = true;                                                //Datei-geöffnet marker
  }

  else if (File_function == 'R') {                                  //Dateimodus Lesen
    if ( !SD.exists(String(sd_pfad) + String(tempstring)))          //Datei nicht vorhanden
    {
      syntaxerror(sdfilemsg);
      sd_ende();                                                    //SD-Card unmount
      return;
    }
    else {
      Datei_open = true;                                                //Datei-geöffnet marker
      fp = SD.open( String(sd_pfad) + String(filestring), FILE_READ); //Datei im Lesemodus öffnen
      File_size = fp.size();                                          //Dateigrösse merken
      fp.close();
    }
  }

  else
  { //falsche Funktion
    syntaxerror(sdfilemsg);
    sd_ende();                                                      //SD-Card unmount
    return;
  }
  sd_ende();
}

//--------------------------------------------- Befehl FWRITE --------------------------------------------------------------------------------
void File_write(void) {
  float b, t;
  char c[16];
  int i;
  int stellen;
  String stz, cbuf;
  int len;

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1
  if (!SD.exists( String(sd_pfad) + String(filestring)) || Datei_open == false) {
    syntaxerror(sdfilemsg);
    return;
  }
  fp = SD.open( String(sd_pfad) + String(filestring), FILE_APPEND);
  File_size = fp.size();
  //fp.seek(File_pos);                                            //Schreibposition setzen
weiter:                                                         //Schreibschleife

  b = get_value();

  if (string_marker) {
    i = 0;
    while (tempstring[i] > 0) {
      fp.write(tempstring[i++]);
    }
  }
  else {

    t = b - int(b);                               //Nachkommastelle vorhanden oder 0 ?
    if (t == 0) {
      cbuf = String(b, 0);
      cbuf.trim();
      cbuf.toCharArray(tempstring, cbuf.length() + 1);
      i = 0;
      while (tempstring[i] > 0) {
        fp.write(tempstring[i++]);
      }

    }

    else {
      dtostrf(b, 1, Prezision, c);               //Nullen abschneiden
      stz = c;
      len = stz.length();
      stellen = Prezision;
      for (int i = len - 1; i > 0; i--) {
        if ((c[i] > '0') || (c[i] == '.')) break; //keine Null oder komma?
        if (c[i] == '0') stellen -= 1;
      }

      dtostrf(b, 1, stellen, c);                //formatierte Ausgabe ohne Nullen
      for (int i = 0; i < stellen + 1; i++) {
        fp.write(c[i]);
      }
    }

  }

  string_marker = false;
  if (*txtpos == ',') {
    fp.write(';');
    txtpos++;
    goto weiter;                                          //solange wiederholen, bis kein komma mehr kommt
  }
  fp.write('\n');                                           //neue Zeile
  File_size = fp.size();                                    //Dateigrösse merken
  //File_pos = fp.position();                                 //Dateiposition merken
  fp.close();
  sd_ende();

}
//---------------------------------------------- eine Zeile aus Datei lesen und an tempstring übergeben ---------------------------------------------
void File_read(void) {
  float b, t;
  char c;
  int i;
  int stellen;
  String stz, cbuf;
  int len;

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1
  if (!SD.exists( String(sd_pfad) + String(filestring)) || Datei_open == false) {
    syntaxerror(sdfilemsg);
    return;
  }
  fp = SD.open( String(sd_pfad) + String(filestring), FILE_READ);
  File_size = fp.size();
  fp.seek(File_pos);
  i = 0;

  while (fp.available()) {
    c = fp.read();
    if (c == NL) {
      tempstring[i] = 0;
      File_pos = fp.position();
      break;
    }
    tempstring[i++] = c;
  }
  fp.close();
  sd_ende();
}

//------------------------------------------------------- Befehl TYPE -------------------------------------------------------------------------------------
void type_file(void) {
  char c, d;
  int b, ex = 0;
  get_value();                    //Dateiname in tempstring

  strcpy(filestring, tempstring); //Tempstring nach filestring kopieren
  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1
  if (!SD.exists( String(sd_pfad) + String(tempstring))) {  //Datei nicht vorhanden
    syntaxerror(sdfilemsg);
    return;
  }
  fp = SD.open( String(sd_pfad) + String(filestring), FILE_READ);

  while (fp.available()) {
    c = fp.read();
    if (c == NL) {
      Terminal.println();
      b++;
      continue;
    }
    if (b == 20)
    {
      if (wait_key(true) == 3) break;
      b = 0;
    }
    Terminal.print(c);
  }
  fp.close();
  sd_ende();
  string_marker = false;
}

//------------------------------ tempstring auseinander nehmen für FILE_RD Funktion -------------------------------------------
int File_line(char * vals) {        

  float value;
  float *var;
  char *st;
  int tmp, stmp, svar, i, o,var_pos, array_art;
  char c;
  String dbuf;
  word arr_adr;

  
  o = 0;

  if (*txtpos < 'A' || *txtpos > 'Z')                                     //erster Variablenbuchstabe
  {
    syntaxerror(syntaxmsg);
    return 1;
  }

  var_pos = *txtpos - 'A';
  var = (float *)variables_begin + *txtpos - 'A';
  stmp = (int) (*txtpos - 'A') * STR_LEN;                                 //Strings nur als einbuchstabige Variablen erlaubt, deshalb Variablenadresse sichern
  txtpos++;
  if (*txtpos >= 'A' && *txtpos <= 'Z') {                                 //zweiter Variablenbuchstabe
    tmp = (int) ((*txtpos - 'A' + 1) * 26);
    var = var + tmp;
    txtpos++;
  }

  //------------------------------------------------------------------------------------------------------------------------------------------------------------
  while (*txtpos >= 'A' && *txtpos <= 'Z') txtpos++;  //so sind auch lange Variablennamen möglich ->siehe auch expr4()
  //------------------------------------------------------------------------------------------------------------------------------------------------------------
  if (*txtpos == '(') {
    txtpos++;
    expression_error = 0;
    arr_adr = rw_array(var_pos, VAR_TBL);                                  //numerische Array? Adresse im Zahlen-Arrayfeld
    if (expression_error) return 1;                                        //Fehler? dann zurück
    array_art = 1;
  }

  if (spaces() == '$')
  { //String?
    txtpos++;
    if (*txtpos == '(') {                                                 //kommt eine Klammer vor, muss es sich um ein Array handeln
      txtpos++;
      expression_error = 0;
      arr_adr = rw_array(var_pos, STR_TBL);                               //String_array?, Adresse im String-Arrayfeld
      if (expression_error) return 1;                                     //Fehler? dann zurück
      array_art = 2;
    }
    i = 0;
    while (1)
    {
      c = vals[o++];
      if (c == '\0' || c == NL || c == ';' ) {
        if (array_art == 2) {
          SPI_RAM_write8(arr_adr + i, '\0');
        }
        else {
          Stringtable[stmp][i] = '\0';                                   //Nullterminator setzen
        }
        break;
      }
      else {
        if (i < STR_LEN) {
          if (array_art == 2) {
            SPI_RAM_write8(arr_adr + i++, c);
          }
          else {
            Stringtable[stmp][i++] = c;
          }
        }
        else {
          if (array_art == 2) {
            SPI_RAM_write8(arr_adr + i, '\0');
            break;
          }
          Stringtable[stmp][i] = '\0';
          break;
        }
      }
    }

    return 0;
  }//String?
  else
  {
    dbuf = String(vals);
    value =  dbuf.toFloat();
    string_marker = false;
  }
  
    if (array_art == 1) {
    byte* bytes = (byte*)&value;                            //float nach byte-array umwandeln
    SPI_RAM_write(arr_adr, bytes, 4);
    return 0;
  }
  *var = value;
  return 0;
}
