#pragma once
#include <Arduino.h>

// Arduino/Wire implementation for MLX90641 (chunked reads to fit Wire RX buffer)

int  MLX90641_I2CRead(uint8_t slaveAddr, uint16_t startAddress,
                      uint16_t nWords, uint16_t* data);
int  MLX90641_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data);
void MLX90641_I2CFreqSet(int freq);
int  MLX90641_I2CGeneralReset(void);
void MLX90641_I2CInit(void);