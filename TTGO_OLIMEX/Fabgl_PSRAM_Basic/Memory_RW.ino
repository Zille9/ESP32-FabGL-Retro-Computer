//############################################### PSRAM - User#################################################################
void USER_RAM_write8(uint32_t addr, uint8_t value) {
  if (user_psram == nullptr) return;
  if (addr >= user_groesse) return; 
  uint8_t* psram_bytes = (uint8_t*)user_psram;
  psram_bytes[addr] = value;
}

void USER_RAM_write(uint32_t address, const uint8_t* buffer, size_t length) {
  if (user_psram == nullptr || buffer == nullptr || length == 0) return;
  if (address >= user_groesse) return;
  if (address + length > user_groesse) {
    length = user_groesse - address; // Schreibe nur so viel, wie noch reinpasst
  }

  uint8_t* psram_bytes = (uint8_t*)user_psram;
  memcpy(&psram_bytes[address], buffer, length);
}

uint8_t user_fram_read8(uint32_t address) {
  if (user_psram == nullptr) return 0;
  if (address >= user_groesse) return 0;
  uint8_t* psram_bytes = (uint8_t*)user_psram;
  return psram_bytes[address];
}

void user_fram_read(uint32_t address, uint8_t* buffer, size_t length) {
  if (user_psram == nullptr || buffer == nullptr || length == 0) return;
    if (address >= user_groesse) return;
  if (address + length > user_groesse) {
    length = user_groesse - address; // Lies nur so viel, wie tatsächlich existiert
  }

  uint8_t* psram_bytes = (uint8_t*)user_psram;
  memcpy(buffer, &psram_bytes[address], length);
}


void USER_RAM_fill(uint32_t addr, uint32_t addr2,const uint8_t value)
{ 
  if (user_psram == nullptr || addr2 <= addr) return;
  uint8_t* psram_bytes = (uint8_t*)user_psram;
  size_t length = addr2 - addr;
  memset(&psram_bytes[addr], value, length);
}

//############################################### PSRAM - Variablen#################################################################
void SPI_RAM_write8(uint32_t addr, uint8_t value) {
  if (var_table_psram == nullptr) return;
  uint8_t* psram_bytes = (uint8_t*)var_table_psram;
  psram_bytes[addr] = value;
}

void SPI_RAM_write(uint32_t address, const uint8_t* buffer, size_t length) {
  if (var_table_psram == nullptr || buffer == nullptr || length == 0) return;
  uint8_t* psram_bytes = (uint8_t*)var_table_psram;
  memcpy(&psram_bytes[address], buffer, length);
}

// 4. Ersatz für spi_fram.read8(adresse) bzw. spi_fram_read8
uint8_t spi_fram_read8(uint32_t address) {
  if (var_table_psram == nullptr) return 0;
  uint8_t* psram_bytes = (uint8_t*)var_table_psram;
  return psram_bytes[address];
}

void spi_fram_read(uint32_t address, uint8_t* buffer, size_t length) {
  if (var_table_psram == nullptr || buffer == nullptr || length == 0) return;
  uint8_t* psram_bytes = (uint8_t*)var_table_psram;
  
  // Holt das Byte-Paket im direkten RAM-Takt aus dem PSRAM
  memcpy(buffer, &psram_bytes[address], length);
}


void SPI_RAM_fill(uint32_t addr, uint32_t addr2,const uint8_t value)
{ 
  if (var_table_psram == nullptr || addr2 <= addr) return;
  uint8_t* psram_bytes = (uint8_t*)var_table_psram;
  size_t length = addr2 - addr;
  memset(&psram_bytes[addr], value, length);
}

void SPI_FRAM_info() {
  Terminal.println();
  if (program != nullptr) {
    Terminal.println("PSRAM detected.");
  } else {
    Terminal.println("No PSRAM allocated ... check your setup()/IDE settings\r\n");
    return;
  }

  //SPI_memSize = ESP.getPsramSize();
  FRAM_PIC_OFFSET = (VGAController.getViewPortWidth() * VGAController.getViewPortHeight()) + 4;
  // 4. Ausgabe der Größe im Terminal
  Terminal.print("PSRAM Active: ");
  Terminal.print((SPI_memSize + user_groesse) / 1024);
  Terminal.println(" kB");
  Terminal.println();
}

void readFRAMIdent() {
  Terminal.print("Storage Type: ESP32 Internal PSRAM (Memory Mapped)\r\n");
  Terminal.print("Free PSRAM: ");
  Terminal.println(ESP.getFreePsram() / 1024);  
  Terminal.println(" kB");
}
