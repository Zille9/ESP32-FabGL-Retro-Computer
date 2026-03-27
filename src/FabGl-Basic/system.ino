//################################################## Systemparameter anzeigen ###########################################################
    void show_systemparameters(void) {
      Terminal.println();
      Terminal.write("BuiltTime : ");
      Terminal.write(BuiltTime);
      Terminal.println();
      Terminal.write("Release   : ");
      Terminal.write(BasicVersion);
      Terminal.println();
      Terminal.write("Keyboard  : ");
      Terminal.print(Keyboard_lang, DEC);
      Terminal.write("=");
      Terminal.write(Keylayout[Keyboard_lang]);
      Terminal.println();
      Terminal.write("Eeprom-Adr: #");
      Terminal.print(EEPROM.read(11), HEX);
      Terminal.println();
      Terminal.write("SPI-RAM   : ");
      Terminal.print(SPI_memSize/0x400);
      Terminal.write(" kB");
      Terminal.println();
      Terminal.write("Workpath  : ");
      Terminal.print(sd_pfad);
      Terminal.println();
      Terminal.write("Theme     : ");
      Terminal.print(Theme_state);
      Terminal.print("=");
      Terminal.print(Themes[Theme_state]);
      Terminal.println();
      Terminal.write("Font      : ");
      Terminal.print(fontsatz);
      Terminal.println();
      Terminal.write("VMode     : ");
      Terminal.print(v_mode);
      Terminal.println();
      Terminal.write("Video     : ");
      Terminal.print(VGAController.getViewPortWidth());
      Terminal.print("x");
      Terminal.print(VGAController.getViewPortHeight());
      Terminal.println();
      Terminal.write("Terminal  : ");
      Terminal.print(VGAController.getViewPortWidth() / x_char[fontsatz]);
      Terminal.write("x");
      Terminal.print(VGAController.getViewPortHeight() / y_char[fontsatz]);
      Terminal.println();
      Terminal.write("ESP-Memory: ");
      Terminal.print(ESP.getFreeHeap());
      line_terminator();
      line_terminator();
      printmsg("READY.", 1);
    }

void show_function_key(void){
  
      Terminal.println();
      Terminal.println("F1  - show this Help");
      Terminal.println("F2  - LIST");
      Terminal.println("F3  - RUN");
      Terminal.println("F4  - DIR");
      Terminal.println("F5  - TRON/TROFF");
      Terminal.println("F6  - Char-Table 32..127");
      Terminal.println("F7  - Char-Table 128..255");
      Terminal.println("F8  - Colour-Table");
      Terminal.println("F9  - Graphic-Symbol's on/off");
      Terminal.println("F10 - Systeminfo");
      Terminal.println("F11 - Erase SPI-RAM");
      Terminal.println("F12 - Reboot");
      Terminal.println();
      printmsg("READY.", 1);
  
}
