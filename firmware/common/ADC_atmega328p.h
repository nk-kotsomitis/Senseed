/*
* @file ADC_atmega328p.h
*
* @brief ADC library for the Atmega328P.
*
*/

#ifndef ADC_H
#define ADC_H

#include "PinsAndMacros.h"
#include <avr/io.h>
#include <util/delay.h>

typedef enum
{
	ADC_VREF_1V1,
	ADC_VREF_AVCC,
	ADC_VREF_AREF,
	
} Adc_volt_ref_t;

#define ATMEGA_328P_ADC_10_BIT_RANGE 1024

void ADC_init(Adc_volt_ref_t vref);
void ADC_enable(void);
void ADC_disable(void);
uint16_t ADC_get_measurement(uint8_t adctouse);

#endif

/*** end of file ***/