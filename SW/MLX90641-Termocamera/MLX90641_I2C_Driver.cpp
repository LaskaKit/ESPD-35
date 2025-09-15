#include <Arduino.h>
#include <Wire.h>
#include "MLX90641_I2C_Driver.h"

void MLX90641_I2CInit(void) {
  // Wire.begin() is called in the sketch (setup()).
}

int MLX90641_I2CGeneralReset(void) {
  return 0;
}

void MLX90641_I2CFreqSet(int freq) {
  // Melexis API may call this; pass it to Wire.
  Wire.setClock(freq);
}

int MLX90641_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data) {
  Wire.beginTransmission(slaveAddr);
  Wire.write((writeAddress >> 8) & 0xFF);
  Wire.write(writeAddress & 0xFF);
  Wire.write((data >> 8) & 0xFF);
  Wire.write(data & 0xFF);
  if (Wire.endTransmission() != 0) return -1;
  return 0;
}

int MLX90641_I2CRead(uint8_t slaveAddr, uint16_t startAddress,
                     uint16_t nWords, uint16_t* out) {
  // ESP32 Wire RX buffer is ~128 bytes. Read in chunks to avoid timeouts.
  const uint16_t MAX_WORDS_PER_CHUNK = 32; // 32 words = 64 bytes
  uint16_t wordsRead = 0;

  while (wordsRead < nWords) {
    uint16_t chunk = nWords - wordsRead;
    if (chunk > MAX_WORDS_PER_CHUNK) chunk = MAX_WORDS_PER_CHUNK;

    uint16_t addr = startAddress + wordsRead;

    // Set register address (repeated START)
    Wire.beginTransmission(slaveAddr);
    Wire.write((addr >> 8) & 0xFF);
    Wire.write(addr & 0xFF);
    if (Wire.endTransmission(false) != 0) return -1;

    // Request 2*chunk bytes
    uint16_t bytesToRead = chunk * 2;
    int got = Wire.requestFrom((int)slaveAddr, (int)bytesToRead, (int)true);
    if (got != bytesToRead) return -2;

    for (uint16_t i = 0; i < chunk; i++) {
      uint8_t msb = Wire.read();
      uint8_t lsb = Wire.read();
      out[wordsRead + i] = ((uint16_t)msb << 8) | lsb;
    }

    wordsRead += chunk;
    delayMicroseconds(100); // Small breather improves stability on ESP32-S3
  }
  return 0;
}