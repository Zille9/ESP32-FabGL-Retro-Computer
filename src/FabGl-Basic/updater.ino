//------------------------------------- Loader für Bin-Dateien -----------------------------------------------------------------------------
int load_binary(void) {

  spiSD.begin(kSD_CLK, kSD_MISO, kSD_MOSI, kSD_CS);         //SCK,MISO,MOSI,SS 13 //HSPI1

  if ( !SD.exists(String(sd_pfad) + String(tempstring)))
  {
    Terminal.print("nicht gefunden");
    syntaxerror(sdfilemsg);
    sd_ende();
    return 1;
  }

  File updateBin = SD.open(String(sd_pfad) + String(tempstring));
  if (updateBin) {
    size_t updateSize = updateBin.size();

    if (updateSize > 0) {
      Terminal.println("load " + String(tempstring));
      performUpdate(updateBin, updateSize);
    }
    else {
      Terminal.println("Error, file is empty");
    }
    updateBin.close();
  }
  else {
    Terminal.println("Could not load Binary from sd");
  }
}

// perform the actual update from a given stream
void performUpdate(Stream &updateSource, size_t updateSize) {
  if (Update.begin(updateSize)) {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize) {
      Terminal.println("Written : " + String(written) + " successfully");
    }
    else {
      Terminal.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
    }
    if (Update.end()) {
      Terminal.println("OTA done!");
      if (Update.isFinished()) {
        Terminal.println("successfully completed. Now Rebooting.");
        delay(1000);
        ESP.restart();
      }
      else {
        Terminal.println("not finished? Something went wrong!");
      }
    }
    else {
      Terminal.println("Error Occurred. Error #: " + String(Update.getError()));
    }

  }
  else
  {
    Terminal.println("Not enough space to begin OTA");
  }
}
