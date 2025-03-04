/*
* @file ADC_atmega328p.c
*
* @brief ADC library for the Atmega328P.
*
*/

#include "ADC_atmega328p.h"

// Delay after changing to 1V1 vref. Defined by trial and error
// At the datasheet the maximum bandgap reference startup time is 70us (at 2.7V)
#define ADC_REF_1V1_DELAY_US 100 // 5000 

/*!
* @brief Initialize ADC
*/
void ADC_init(Adc_volt_ref_t vref)
{
	/*
	REFS1	REFS0	
	0		0		AREF, Internal Vref turned off
	0		1		AVCC with external capacitor at AREF pin
	1		0		Reserved
	1		1		Internal 1.1V Voltage Reference with external capacitor at AREF pin
	*/
	
	// Select voltage reference
	switch (vref)
	{
		case ADC_VREF_AVCC:
			// AVcc
			ADMUX = (0 << REFS1) | (1 << REFS0);
		break;
		
		case ADC_VREF_1V1:
			// 1V1
			ADMUX = (1 << REFS1) | (1 << REFS0); 
		break;
		
		case ADC_VREF_AREF:
			// AREF
			ADMUX = (0 << REFS1) | (0 << REFS0); 
		break;
		
		default:
			break;
	}
	 
	// Select right adjusted resolution (it is the default)
 	ADMUX &= ~(1 << ADLAR);
	
	/*
	ADPS2	ADPS1	ADPS0	Division Factor
	0		0		0		2
	0		0		1		2
	0		1		0		4
	0		1		1		8
	1		0		0		16
	1		0		1		32
	1		1		0		64
	1		1		1		128
	
	4MHz / 16 = 250kHz
	4MHz / 32 = 125kHz
	4MHz / 64 = 62.5kHz
	
	*/
	
	// Select the pre-scaler
	// For 4MHz
	// ADCSRA = (1 << ADPS2) | (0 << ADPS1) | (0 << ADPS0); // 250 Khz
 	//ADCSRA = (1 << ADPS2) | (0 << ADPS1) | (1 << ADPS0); // 125 Khz
	uint8_t adcsra_reg = (1 << ADPS2) | (0 << ADPS1) | (1 << ADPS0); // 125 Khz
	ADCSRA = (ADCSRA & 0xF8) | adcsra_reg;
	// ADCSRA = (1 << ADPS2) | (1 << ADPS1) | (0 << ADPS0); // 62.5 Khz
	
	// FOR 16MHZ FCPU - ARDUINO
	// ADCSRA = (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // 125 Khz
	
	// Enable the ADC
	//ADCSRA |= (1 << ADEN);
	
	// Wait for Vref 1V1 to stabilize
	if (vref == ADC_VREF_1V1)
	{
		_delay_us(ADC_REF_1V1_DELAY_US);
	}
}

/*!
* @brief Enables the ADC
*/
void ADC_enable()
{	
	// Enable the ADC
	ADCSRA |= (1 << ADEN);
}

/*!
* @brief Disables the ADC
*/
void ADC_disable()
{
	// Disable the ADC
	ADCSRA &= ~(1 << ADEN);
}

/*!
* @brief Performs a mesurement
*/
uint16_t ADC_get_measurement(uint8_t channel)
{
	// 0 - 7:	Input channels (ADC0 - ADC7)
	// 8:		ADC8 for temperature sensor
	// 14:		Vbg 1V1
	// 15:		GND
		
	// Select channel
	ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
	
	// Start the ADC conversion
	ADCSRA |= (1 << ADSC);										

	// Waits for the conversion to finish
	while(ADCSRA & (1 << ADSC));								
	
	// Read the conversion result
	uint16_t result = ADCL;
	result = (ADCH << 8) + result;
		
	return result;
}

/*** end of file ***/