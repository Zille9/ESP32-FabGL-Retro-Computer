//############################################### externer I2C EEPROM/FRAM #################################################################
void writeEEPROM(int deviceaddress, word eeaddress, byte dat )
{
  myI2C.beginTransmission(deviceaddress);
  myI2C.write((int)(highByte(eeaddress)));   // MSB
  myI2C.write((int)(lowByte(eeaddress)));    // LSB
  myI2C.write(dat);
  myI2C.endTransmission();
  if (deviceaddress == EEprom_ADDR)                           //bei EEprom muss etwas gewartet werden
  // Write cycle time (tWR). See EEPROM memory datasheet for more details.
    delay(5);
}

byte readEEPROM(int deviceaddress, word eeaddress )
{
  myI2C.beginTransmission(deviceaddress);
  myI2C.write((int)(highByte(eeaddress)));   // MSB
  myI2C.write((int)(lowByte(eeaddress)));    // LSB
  myI2C.endTransmission();
  if (deviceaddress == EEprom_ADDR)
    delay(2);
  myI2C.requestFrom(deviceaddress, 1);
  while (myI2C.available() == 0);
  return myI2C.read();
  //Serial.println(myI2C.read(), HEX);
}

void WriteBuffer(int deviceaddress, word address, byte ln, byte* p_data)
{
  myI2C.beginTransmission(deviceaddress);
  myI2C.write(highByte(address));
  myI2C.write(lowByte(address));

  for (byte i = 0; i < ln; i++)
  {
    myI2C.write(p_data[i]);
  }

  myI2C.endTransmission();

  // Write cycle time (tWR). See EEPROM memory datasheet for more details.
  if (deviceaddress == EEprom_ADDR)                                         //bei EEprom muss etwas gewartet werden
    delay(5);
}

void readBuffer(int deviceaddress, word address, byte ln, byte* p_data)
{
  myI2C.beginTransmission(deviceaddress);
  myI2C.write(highByte(address));
  myI2C.write(lowByte(address));
  myI2C.endTransmission();
  myI2C.requestFrom(deviceaddress, ln);
  for (byte i = 0; i < ln; i++)
  {
    if (myI2C.available())
    {
      p_data[i] = myI2C.read();
    }
  }
}

//############################################### externer SPI-FRAM #################################################################
void SPI_RAM_write8(uint32_t addr, uint8_t value) {
  spi_fram.writeEnable(true);
  spi_fram.write8(addr, value);
  spi_fram.writeEnable(false);
}

void SPI_RAM_write(uint32_t addr, const uint8_t *values, int count) {
  
  spi_fram.writeEnable(true);
  spi_fram.write(addr, values, count);
  spi_fram.writeEnable(false);
  
}

void SPI_RAM_fill(uint32_t addr, uint32_t addr2,const uint8_t value)
{ 
  spi_fram.writeEnable(true);
  for(uint32_t i=addr; i < addr2; i++){
    spi_fram.write8(i, value);
  }
  spi_fram.writeEnable(false);
}

void SPI_RAM_clear(uint32_t start_adr, uint32_t laenge) {
  
  const int bufSize = 128; // Puffergröße wählen
  byte zeroBuf[bufSize];
  memset(zeroBuf, 0, bufSize); // Puffer einmalig nullen

  uint32_t verbleibend = laenge;
  uint32_t aktuelle_adr = start_adr;

  while (verbleibend > 0) {
    int chunk = (verbleibend > bufSize) ? bufSize : verbleibend;
    SPI_RAM_write(aktuelle_adr, zeroBuf, chunk);
    aktuelle_adr += chunk;
    verbleibend -= chunk;
  }
  
}

uint8_t addrSizeInBytes = 2; // Default to address size of two bytes


int32_t readBack(uint32_t addr, int32_t data) {
  
  int32_t check = !data;
  int32_t wrapCheck, backup;
  
  spi_fram.read(addr, (uint8_t *)&backup, sizeof(int32_t));
  spi_fram.writeEnable(true);
  spi_fram.write(addr, (uint8_t *)&data, sizeof(int32_t));
  spi_fram.writeEnable(false);
  spi_fram.read(addr, (uint8_t *)&check, sizeof(int32_t));
  spi_fram.read(0, (uint8_t *)&wrapCheck, sizeof(int32_t));
  spi_fram.writeEnable(true);
  spi_fram.write(addr, (uint8_t *)&backup, sizeof(int32_t));
  spi_fram.writeEnable(false);
  // Check for warparound, address 0 will work anyway
  if (wrapCheck == check)
    check = 0;
  return check;
}

bool testAddrSize(uint8_t addrSize) {
  spi_fram.setAddressSize(addrSize);
  if (readBack(4, 0xbeefbead) == 0xbeefbead)
    return true;
  return false;
}

void SPI_FRAM_info(){
  
if (spi_fram.begin(addrSizeInBytes)) {
    Terminal.println();//"Test of SPI RAM");
  } else {
    Terminal.println("No SPI RAM found ... check your connections\r\n");
    return;
  }

  if (testAddrSize(2))
    addrSizeInBytes = 2;
  else if (testAddrSize(3))
    addrSizeInBytes = 3;
  else if (testAddrSize(4))
    addrSizeInBytes = 4;
  else {
    Terminal.println(
        "SPI RAM can not be read/written with any address size\r\n");
    return;
  }

  SPI_memSize = 0;
  while (readBack(SPI_memSize, SPI_memSize) == SPI_memSize) {
    SPI_memSize += 256;
    // Serial.print("Block: #"); Serial.println(memSize/256);
  }
/*
  Terminal.println("SPI FRAM address size is ");
  Terminal.println(addrSizeInBytes);
  Terminal.println(" bytes.");
  Terminal.println("SPI FRAM capacity appears to be..");
  Terminal.println(SPI_memSize);
  Terminal.println(" bytes");
  Terminal.println(SPI_memSize / 0x400);
  Terminal.println(" kilobytes");
  Terminal.println((SPI_memSize * 8) / 0x400);
  Terminal.println(" kilobits");
  if (SPI_memSize >= (0x100000 / 8)) {
    Terminal.println((SPI_memSize * 8) / 0x100000);
    Terminal.println(" megabits");
  }
  */
  FRAM_PIC_OFFSET = (VGAController.getViewPortWidth() * VGAController.getViewPortHeight()) + 4;     //Bildoffset im Speicher X*Y Dimension + 4 Byte für Dimensionsdaten
  //digitalWrite(FRAM_CS,HIGH);
  //SPI_memSize = SPI_memSize ;/// 0x400;
  //line_terminator();
  //printmsg("READY.", 1);
}
