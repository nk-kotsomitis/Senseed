#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

// Convert timestamp from time_t to readable string
void Timestamp2string(time_t ts, char *str);

// Convert battery eco to seconds
time_t BatteryEcoToSeconds(uint8_t battery_eco);

// Get the high byte from a double (integer part)
uint8_t DoubleToHighByte(double value);

// Get the low byte from a double (decimal part)
uint8_t DoubleToLowByte(double value);

// Get double from high (integer) and low byte (decimal)
double BytesToDouble(uint8_t high_byte, uint8_t low_byte);

// Get uint16 from high and low byte
uint16_t BytesToUint16(uint8_t high_byte, uint8_t low_byte);

// Get uint32 from bytes
uint32_t BytesToUint32(uint8_t byte_32, uint8_t byte_24, uint8_t byte_16, uint8_t byte_8);

// Get a random 8-bit number
uint8_t GetRandomNumber8bit(uint8_t min, uint8_t max);

// Round an integer TBC
uint16_t RoundInteger16(uint16_t n);

#endif