/*
* @file SHT40.h
*
* @brief SHT40 main library.
*
*/

#include "PinsAndMacros.h"

#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <util/delay.h>
#include <avr/pgmspace.h>

#include "TWI_atmega328p.h"
#include "SHT40.h"

/*!
* @brief Library init
*/
void SHT40_init()
{
	// I2C init	
	sensirion_i2c_init();
	
	int16_t res = sht4x_probe();
	
	if(res) 
	{
        print_int16("SHT sensor probing failed: ", res);
	}
	else
	{
		print("SHT sensor probing successful\r\n");	
	}
}

/*!
* @brief Get temperature and humidity
*/
uint16_t SHT40_get_measurements()
{
	int32_t temperature, humidity = 0;
	
	int8_t ret = sht4x_measure_blocking_read(&temperature, &humidity);
	
	if (ret == STATUS_OK)
	{
		print_int32("Temperature: ", temperature);
		print_uint32("Humidity: ", humidity);
		//printf("measured temperature: %0.2f degreeCelsius, measured humidity: %0.2f percentRH\n", temperature / 1000.0f, humidity / 1000.0f);
	}
	else
	{
		print_int8("Error reading measurement", ret);
	}
	
	return 0;
}

/*** end of file ***/