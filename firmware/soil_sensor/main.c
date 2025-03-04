/*
* @file main.c
*
* @brief Main file
*
*/

#include "PinsAndMacros.h"

// Standard libraries
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <util/delay.h>

// AVR-specific libraries
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include "CONSOLE_log.h"
#include "SOIL_SENSOR.h"

// Static variables
time_t g_time_epoch_y2k = 0;
volatile uint8_t g_wake_up = 0;

// Static functions
static void sleep(uint8_t sleep_cycles);

// Main function
int main(void)
{
	HW_INIT;  
	WDT_INIT;
	TIMERS_INIT;
	SOIL_SENSOR_init();
	
	while(1) 
    {	
		// NOTE: For time measurements, TIMER_2 is unused
		// TIMER_2 = 0;
		// uint16_t t1 = TIMER_2;
		// print_uint16("EXEC_TIME", t1);
		
		SOIL_SENSOR_power_up();
		SOIL_SENSOR_measurements();
		SOIL_SENSOR_tasks();
		SOIL_SENSOR_connection();	
		SOIL_SENSOR_power_down();
		
		// Connect cycles: e.g. 2 [sec] (WDT) * 5 = 10 [sec]
		sleep(SOIL_SENSOR_CONNECT_CYCLES);
    }
}

/*!
* @brief MCU sleep mode for the given number of cycles
*/
static void sleep(uint8_t sleep_cycles)
{
	g_wake_up = 0;
	
	// Set watchdog to Interrupt mode and 2 seconds
	WDT_INIT_SLEEP;
	// Set sleep power down mode
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	cli();
	sleep_enable();
	
	for(uint8_t i = 0; i < sleep_cycles; i++)
	{
		// Enable PRR
		PRR = (PRTWI << 1) | (PRTIM2 << 1) | (PRTIM0 << 1) | (PRTIM1 << 1) | (PRSPI << 1) | (PRUSART0 << 1) | (PRADC << 1);
		// Disable BOD before sleep
		sleep_bod_disable();
		// Go to sleep
		sei();
		sleep_cpu();
		// Exit loop if button pushed
		if (g_wake_up)
		{	
			break;
		}
	}
		
	// Wake up!
	sleep_disable();
	// Disable PRR
	PRR = 0x00;
	// Set watchdog to Interrupt & System Reset mode and 2 seconds
	WDT_INIT;
}

ISR(INT0_vect)
{
	// NRF24L01 IRQ pin
	NRF_IRQ = 1;
}

ISR(INT1_vect)
{
	//LED_TOGGLE;
	
	// Used to wake up!
	// NOTE: Button and soil clock 1 share the same pin.
	g_wake_up = 1;
}

ISR(WDT_vect)
{
	// Wake up!
}

ISR(TIMER0_COMPA_vect)
{
	// Unused
}

ISR(TIMER1_COMPA_vect)
{
	// Unused
}

ISR(TIMER2_COMPA_vect)
{
	// Unused
}

/*** end of file ***/