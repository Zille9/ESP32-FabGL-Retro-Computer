//############################################### externer I2C EEPROM/FRAM #################################################################
void writeEEPROM(int deviceaddress, word eeaddress, byte dat )
{
  myI2C.beginTransmission(deviceaddress);
  myI2C.write((int)(highByte(eeaddress)));   // MSB
  myI2C.write((int)(lowByte(eeaddress)));    // LSB
  myI2C.write(dat);
  myI2C.endTransmission();
  if (deviceaddress == EEprom_ADDR)                           //bei EEprom muss etwas gewartet werden
    delay(10);
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
  //myI2C.write(p_data,ln);

  for (byte i = 0; i < ln; i++)
  {
    myI2C.write(p_data[i]);
  }

  myI2C.endTransmission();

  // Write cycle time (tWR). See EEPROM memory datasheet for more details.
  if (deviceaddress == EEprom_ADDR)                                         //bei EEprom muss etwas gewartet werden
    delay(10);
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
  //spi_fram.begin(3);
  spi_fram.writeEnable(true);
  spi_fram.write8(addr, value);
  spi_fram.writeEnable(false);
}

void SPI_RAM_write(uint32_t addr, const uint8_t *values, int count) {
  spi_fram.writeEnable(true);
  spi_fram.write(addr, values, count);
  spi_fram.writeEnable(false);
}

void SPI_FRAM_init(void) {
  if (spi_fram.begin(3)) {
    Terminal.println("Found SPI FRAM");
  } else {
    Terminal.println("No SPI FRAM found\r\n");
  }
}
