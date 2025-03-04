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
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include "CONSOLE_log.h"
#include "CONTROLLER_HP.h"

// Static variables
volatile static uint8_t WD_INT = 0;

// Main function
int main(void)
{
	WDT_INIT;
	HW_INIT;
	TIMERS_INIT;
		
	CONTROLLER_HP_init();
						
	while(1) 
    {
		CONTROLLER_HP_cycle_start(); 
		CONTROLLER_HP_measurements();
		CONTROLLER_HP_tasks();
		CONTROLLER_HP_connection();
		CONTROLLER_HP_cycle_end();
		wdt_reset();	
		// Cycle++
    }
}

// Watchdog Interrupt - NOT USED
ISR(WDT_vect)
{

}

// NRF24L01 IRQ pin
ISR(INT0_vect) 
{
	NRF_IRQ = 1;
}

// Interrupt 1 - NOT USED
ISR(INT1_vect) 
{

}

// Timer 0 - CTC mode 50ms - Main LED
ISR(TIMER0_COMPA_vect)	
{
	// Led state current
	static Led_state_t led_state = LED_STATE_BLINK_1S;	
	// Led state next
	Led_state_t led_state_next = g_led_state;
	
	if (g_led_flash != LED_FLASH_NONE && led_state != LED_STATE_FLASH)
	{	
		led_state = LED_STATE_FLASH;
		g_led_ticks = 0;
	}
	
	switch (led_state)
	{
		case LED_STATE_ON:
			if (led_state != led_state_next)
			{
				LED_OFF;
				led_state = led_state_next;
				g_led_ticks = 0;
				break;
			}	
			LED_ON;	
			break;
			
		case LED_STATE_OFF:
			LED_OFF;	
			led_state = led_state_next;
			break;
			
		case LED_STATE_BLINK_1S: // First OFF Then On
			g_led_ticks++;
			if (g_led_ticks >= 19)
			{
				g_led_ticks = 0;
				
				if (led_state != led_state_next)
				{
					led_state = led_state_next;
					break;
				}
				LED_TOGGLE;	
			}
			break;
			
		case LED_STATE_BLINK_500MS:
			g_led_ticks++;
			
			if (g_led_ticks >= 9)
			{
				g_led_ticks = 0;
				
				// Next state
				if (led_state != led_state_next)
				{
					led_state = led_state_next;
					break;
				}				
				LED_TOGGLE;
			}			
			break;
		
		case LED_STATE_FLASH:
			g_led_ticks++;
			LED_TOGGLE;
			
			if (g_led_ticks >= (uint16_t)g_led_flash)
			{
				g_led_ticks = 0;
				led_state = led_state_next;
				g_led_flash = LED_FLASH_NONE;
			}
			break;
		
		default:
			break;
	}
}

// Timer 2 - CTC mode 10ms - Buttons
ISR(TIMER2_COMPA_vect)
{
	uint8_t current_state = BUTTON_2_STATE;	
		
	switch (g_button_2)
	{
		case BUTTON_UNPRESSED:
			if (current_state)
			{
				g_button_2_ticks = 0;
				g_button_2 = BUTTON_IN_PROGRESS;
			}
			break;
			
		case BUTTON_IN_PROGRESS:
			g_button_2_ticks++;
				
			if (g_button_2_ticks >= BUTTON_LONG_PRESS_TICKS)
			{
				// LED blink to indicate that long press is achieved
				g_led_flash = LED_FLASH_1_SEC;
			}
				
			if (!current_state)
			{
				if (g_button_2_ticks >= BUTTON_LONG_PRESS_TICKS)
				{
						
					g_button_2 = BUTTON_LONG_PRESS;
				}
				else
				{
					g_button_2 = BUTTON_SHORT_PRESS;
				}
			}

			break;
			
		case BUTTON_SHORT_PRESS:
		case BUTTON_LONG_PRESS:
			g_button_2_ticks = 0;
			break;
			
		default:
			break;
	}		
}

/*** end of file ***/




