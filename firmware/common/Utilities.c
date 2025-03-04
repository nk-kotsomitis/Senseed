#include "Utilities.h"


void Timestamp2string(time_t ts, char *str)
{
	// Convert to tm structure - UTC
	struct tm* tm_time = gmtime(&ts);
	// Convert to readable string
	strftime(str, 26, "%Y-%m-%d %H:%M:%S", tm_time);
}

time_t BatteryEcoToSeconds(uint8_t battery_eco)
{
	// TODO
	
	return 0;
}

uint8_t DoubleToHighByte(double value)
{
	return (uint8_t)value;
}

uint8_t DoubleToLowByte(double value)
{
	return (uint8_t)(round((value * 100) - ((uint8_t)value * 100)));
}

double BytesToDouble(uint8_t high_byte, uint8_t low_byte)
{
	return ((double)low_byte / 100) + high_byte;
}

uint16_t BytesToUint16(uint8_t high_byte, uint8_t low_byte)
{
	return (uint16_t)((high_byte << 8) | low_byte);
}


uint32_t BytesToUint32(uint8_t byte_32, uint8_t byte_24, uint8_t byte_16, uint8_t byte_8)
{
	return (uint32_t)(((uint32_t)byte_32 << 24) | ((uint32_t)byte_32 << 16) | ((uint32_t)byte_32 << 8) | (uint32_t)byte_32);
}

uint8_t GetRandomNumber8bit(uint8_t min, uint8_t max)
{
	return (uint8_t)(rand() % (max - min) + min);
}

uint16_t RoundInteger16(uint16_t n)
{
	// e.g. n = 2344
	// a = 2340
	// b = 2350
	// return a because 4 > 6
	
	// Smaller multiple
	uint16_t a = (n / 10) * 10;
	
	// Larger multiple
	uint16_t b = a + 10;
	
	// Return of closest of two
	return (n - a > b - n)? b : a;
}
