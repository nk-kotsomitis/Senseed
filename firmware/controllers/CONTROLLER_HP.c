/*
* @file CONTROLLER_HP.h
*
* @brief Controller HP main library.
*
*/

#include <stdio.h>
#include <stdbool.h>

#include "PinsAndMacros.h"
#include "DataTypes.h"
#include "ADC_atmega328p.h"
#include "MCP79410.h"
#include "NRF_lib.h"
#include "THINGS_lib.h"
#include "CONTROLLER_HP.h"

// Main
volatile uint16_t g_cycles = 0;

// Measurements
volatile uint16_t g_measurements_ticks = 0;
Consumption_measurements_t g_consumption[NUM_OF_SUB_CONTROLLERS] = {{0, 0, 0, 0, CURRENT_SENSOR_1}, {0, 0, 0, 0, CURRENT_SENSOR_2}};

// Leds
volatile uint16_t g_led_ticks = 0;
volatile Led_state_t g_led_state = LED_STATE_BLINK_1S;
volatile Led_flash_t g_led_flash = LED_FLASH_NONE;

// Buttons
volatile uint16_t g_button_1_ticks = 0;
volatile uint16_t g_button_2_ticks = 0;
volatile Button_state_t g_button_1 = BUTTON_UNPRESSED;
volatile Button_state_t g_button_2 = BUTTON_UNPRESSED;
	
// Static functions
static void get_state(uint8_t sub_ctrl);
static void get_consumption(uint8_t sub_controller);
static void update_state(uint8_t sub_controller);

#if DEBUG_OUTPUTS_OPERATION
	static void debug_outputs_operation();
#endif

#if DEBUG_LEDS
	static void debug_leds();
#endif


#if DEBUG_MCP79410

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

static void rtc_testing();
#endif

/*!
* @brief Controller's initializations
*/
void CONTROLLER_HP_init()
{
#ifdef DEBUG
	CONSOLE_init();
	CONSOLE_log("MCU init...", 1);
#endif

#if DEBUG_OUTPUTS_OPERATION
	// OUTPUTS operation testing and debug - This is a while(1) function
	debug_outputs_operation();
#endif

#if DEBUG_LEDS
	// LED testing and debug - This is a while(1) function
	debug_leds();
#endif

#if DEBUG_NRF
	// NRF Testing - This is a while(1) function
	NRF_TEST_transmitter();
#endif

#if DEBUG_MCP79410
	// RTC Testing - This is a while(1) function
	rtc_testing();
#endif

	// Initialize ADC
	ADC_init(ADC_VREF_AVCC);
		
	// Initialize RTC ad get time
	MCP79410_init(false);
			
	// Controller init
	g_controller_hp = CONTROLLER_HP_ZERO;
	g_controller_hp.id = atoi((const char *)STRINGIZE(UUID)); // = TESTING_UUID_CONTROLLER_HP_1
	g_controller_hp.clock = MCP79410_get_time();
	
	// RF Address from symbol
	// uint8_t temp_add[5] = RF_ADDRESS;
	Rf_address_t temp_add = TESTING_NRF_ADDR_THING_CONTR_HP_1;
	memcpy(g_controller_hp.address, temp_add, RF_ADDRESS_BYTES);
		
	// Global gateway RF address for Soil Sensors
	Rf_address_t temp_add_gw = NRF_ADDRESS_GATEWAY_3_CONTROLLERS;
	memcpy(g_controller_hp.address_gw, temp_add_gw, RF_ADDRESS_BYTES);
			
	g_controller_hp.packet_id_tx = INVALID_PACKET_ID;
			
	// Led init
	g_led_state = LED_STATE_BLINK_1S;
	
	// Consumption initialization
	g_controller_hp.consumption[0].timestamp = g_controller_hp.consumption[1].timestamp = g_controller_hp.clock;
	g_consumption[0].state = CONSUMPTION_STATE_DISABLED;
	g_consumption[1].state = CONSUMPTION_STATE_DISABLED;
		
#if DEBUG_MEASUREMENTS
	
	// Turn relays on to enable measurements
	
	// TODO: MAJOR BUG !!! Although pin is turning on and off, relay is not responding !!!
	//RELAY_ON(0);
	//RELAY_ON(1);
	
	//RELAY_OFF(0);
	//RELAY_OFF(1);
	
#endif
	
	// Initialize NRF lib
	NRF_init(TYPE_CONTROLLER_HP, g_controller_hp.address_gw, g_controller_hp.address);
}

/*!
* @brief Controller's actions at the start of the cycle
*/
void CONTROLLER_HP_cycle_start()
{
	g_cycles++;
}

/*!
* @brief Controller's actions at the end of the cycle
*/
void CONTROLLER_HP_cycle_end()
{
	// Clear ALL measurements
	g_controller_hp.consumption[0].measurement = g_controller_hp.consumption[1].measurement = 0;
	g_controller_hp.consumption[0].is_ready = g_controller_hp.consumption[1].is_ready = false;
	
	// NOTE: Measurement's timestamp should NOT be deleted - It is used to determine when the measurement should start.
}

/*!
* @brief Controller's measurements
*/
void CONTROLLER_HP_measurements()
{	
	// Get RTC time
	g_controller_hp.clock = MCP79410_get_time();
		
	// Get states
	get_state(0);
	get_state(1);
	
	// Get measurements
	//get_consumption(0);
	//get_consumption(1);
}

/*!
* @brief Controller's tasks
*/
void CONTROLLER_HP_tasks()
{
	switch (g_button_2)
	{
		case BUTTON_UNPRESSED:
			break;
		
		case BUTTON_IN_PROGRESS:
			break;
		
		case BUTTON_SHORT_PRESS:
			RELAY_2_TOGGLE;
			g_button_2 = BUTTON_UNPRESSED;
			break;
		case BUTTON_LONG_PRESS:
			print_uint8("BUTTON 2 LONG PRESS", 1);
			g_button_2 = BUTTON_UNPRESSED;
			break;
		
		default:
			break;
	}
	
	// Update states
	update_state(0);
	update_state(1);
}

/*!
* @brief Controller connection within network
*/
void CONTROLLER_HP_connection()
{	
	NRF_LIB_Connect_res_t res_rx = NRF_LIB_CONNECT_ERROR;
	NRF_LIB_Connect_res_t res_tx = NRF_LIB_CONNECT_ERROR;
	static time_t last_connection_ts = 0;
	
	// Connect with gateway in reception
	res_rx = NRF_connect_rx(g_controller_hp.id, TYPE_CONTROLLER_HP);
	
	// Rx Actions
	switch (res_rx)
	{
		case NRF_LIB_CONNECT_SUCCESS:
			// New Rx packet
			g_led_flash = LED_FLASH_1_SEC;
			break;
			
		case NRF_LIB_CONNECT_RX_NO_PACKET:
		default:
			// Do nothing.
			break;
	}
	
	// Connect with gateway in transmission only in these 2 cases:
	// 1. Elapsed time since last connection is the expected (i.e. Connect cycle)
	// 2. A measurement is ready to be transmitted (from either sub-controller)
	
	// Get the elapsed time since the last connection
	int32_t con_delta = difftime(g_controller_hp.clock, last_connection_ts);
		
	if (con_delta >= CONTROLLER_HP_CONNECT_SEC || g_controller_hp.consumption[0].is_ready || g_controller_hp.consumption[1].is_ready)
	{
		// print_int32("Time since last connection", con_delta);
		// CONSOLE_print_time(last_connection_ts, false);
		
		last_connection_ts = g_controller_hp.clock;
	
		// Connect with gateway in transmission
		res_tx = NRF_connect_tx(g_controller_hp.id, TYPE_CONTROLLER_HP);
	
		// Tx Actions
		switch (res_tx)
		{
			case NRF_LIB_CONNECT_SUCCESS:			
				// Success
				g_led_state = LED_STATE_ON;
				// g_controller_hp.packet_id_tx = g_controller_hp.packet_id_rx;
				break;
						
			case NRF_LIB_CONNECT_ERROR:
				// Error
				g_led_state = LED_STATE_BLINK_1S;
				break;
		
			case NRF_LIB_CONNECT_SUCCESS_TX_ERROR_ACK:
			default:
				break;
		}
	}
}

/************************************************************************************
******************************** Static Functions ***********************************
************************************************************************************/

// Get relays' states (i.e. ON or OFF)
void get_state(uint8_t sub_ctrl)
{	
	// Get relay's state
	g_controller_hp.state[sub_ctrl] = RELAY_STATE(sub_ctrl);
		
	// Debug
	//CONSOLE_print_controller_hp(g_controller_hp, sub_ctrl);
}

// Get consumption
void get_consumption(uint8_t sub_ctrl)
{	
	// Get relay's state (i.e. ON or OFF)
	uint8_t relay_state = g_controller_hp.state[sub_ctrl];
		
	// Consumption state
	switch (g_consumption[sub_ctrl].state)
	{
		case CONSUMPTION_STATE_DISABLED:
			{
				// NOTE: It is assumed that RTC is always greater than consumption's timestamp (since it is initialized when device booted up).
				//time_t measurement_delta = g_controller_hp.clock - g_controller_hp.consumption[sub_controller].timestamp;
		
				// Next state - In progress - When relay is turned on 
				if (relay_state)
				{
					// Clear all and change state					
					g_consumption[sub_ctrl].n_samples = g_consumption[sub_ctrl].vpp_raw_max = 0;
					g_consumption[sub_ctrl].vpp_raw_min = ATMEGA_328P_ADC_10_BIT_RANGE;
					g_consumption[sub_ctrl].state = CONSUMPTION_STATE_IN_PROGRESS;
				}
			}
			break;
			
		case CONSUMPTION_STATE_IDLE:
			{				
				// Previous State - Disabled - When relay is turned off
				if (!relay_state)
				{
					g_consumption[sub_ctrl].state = CONSUMPTION_STATE_DISABLED;
				}
			
				// Get the elapsed time from the last measurement
				int32_t measurement_delta = difftime(g_controller_hp.clock, g_controller_hp.consumption[sub_ctrl].timestamp);
								
				// Next state - In progress - When relay is turned on OR it is time for a measurement
				if (measurement_delta >= CONTROLLER_HP_MEASUREMENT_SEC)
				{
					// Clear all and change state
					g_consumption[sub_ctrl].n_samples = g_consumption[sub_ctrl].vpp_raw_max = 0;
					g_consumption[sub_ctrl].vpp_raw_min = ATMEGA_328P_ADC_10_BIT_RANGE;
					g_consumption[sub_ctrl].state = CONSUMPTION_STATE_IN_PROGRESS;
				}
			}
			break;
		
		case CONSUMPTION_STATE_IN_PROGRESS:
			{
				// Get sample
				uint16_t sample_adc = ADC_get_measurement(g_consumption[sub_ctrl].adc_pin);
				g_consumption[sub_ctrl].n_samples++;
			
				// Update minimum and maximum values
				if (sample_adc > g_consumption[sub_ctrl].vpp_raw_max)
				{
					g_consumption[sub_ctrl].vpp_raw_max = sample_adc;
				}
				else if (sample_adc < g_consumption[sub_ctrl].vpp_raw_min)
				{
					g_consumption[sub_ctrl].vpp_raw_min = sample_adc;
				}
			
				// Previous state - Idle state - In case relay is turned OFF
				if (!relay_state)
				{
					// Change state
					g_consumption[sub_ctrl].state = CONSUMPTION_STATE_DISABLED;
				}
			
				// Next state - Completed - When the expected number of samples is reached
				if (g_consumption[sub_ctrl].n_samples >= ACS712_NUM_OF_SAMPLES)
				{
					g_consumption[sub_ctrl].state = CONSUMPTION_STATE_COMPLETED;
				}
			}
			break;
		
		case CONSUMPTION_STATE_COMPLETED:
			{
				// Calculate Volt peak-to-peak
				double Vpp_v = (((g_consumption[sub_ctrl].vpp_raw_max - g_consumption[sub_ctrl].vpp_raw_min) * CONTROLLER_HP_ADC_VOLT) / (double)ATMEGA_328P_ADC_10_BIT_RANGE);
				// Convert Vpp to Vrms			
				double Vrms_v = (Vpp_v / 2.0) * 0.707;
				// Convert Vrms to A
				double Ampere = (Vrms_v * 1000) / ACS712_SENSITIVITY_MV;
				// Convert A to mA
				// TODO: Convert A (double) to mA (uint16_t)
				g_controller_hp.consumption[sub_ctrl].measurement = (uint16_t)Ampere;
				
				int32_t last_measurement = difftime(g_controller_hp.clock, g_controller_hp.consumption[sub_ctrl].timestamp);
				// print_int32("Time since last measurement", difftime(g_controller_hp.clock, g_controller_hp.consumption[sub_controller].timestamp));
								
				g_controller_hp.consumption[sub_ctrl].timestamp = g_controller_hp.clock;
				g_controller_hp.consumption[sub_ctrl].is_ready = true;
				
				// Measurement completed!
								
				// Next state - Idle - Wait for the next measurement cycle
				g_consumption[sub_ctrl].state = CONSUMPTION_STATE_IDLE;
				
#if DEBUG_MEASUREMENTS
				CONSOLE_print_consumption(sub_ctrl + 1, g_controller_hp.consumption[sub_ctrl].timestamp, last_measurement, Vpp_v, Vrms_v, Ampere, g_controller_hp.consumption[sub_ctrl].measurement);
#endif
			}
			break;
			
		default:
			break;
	}
}


static void update_state(uint8_t sub_controller)
{
	// TODO: Update scheduler
	
	switch (g_controller_hp.scheduler[sub_controller])
	{
		case SCHEDULER_MANUAL:
		case SCHEDULER_TIMER:
		case SCHEDULER_AUTO:
		
		default:
			
			// Update relay's state (based on auto_cmd)
			RELAY_UPDATE(sub_controller, g_controller_hp.auto_cmd[sub_controller]);
			g_controller_hp.state[sub_controller] = g_controller_hp.auto_cmd[sub_controller];
				
			break;
	}
}

#if DEBUG_OUTPUTS_OPERATION

// NOTE: This test is executed only at Controller LP (28/1/2023).

#include <avr/wdt.h>

static void debug_delay(uint16_t ms)
{
	for (uint16_t i = 0; i < ms; i++)
	{
		_delay_ms(1);
		wdt_reset();
	}
}

static void debug_outputs_operation()
{
	#define TEST_DELAY_MS 5000
	
	while(1)
	{
		// Output 1|2: OFF |  OFF
		print_uint8("TEST OUTPUTS: OFF | OFF", 0);
		RELAY_OFF(0);
		RELAY_OFF(1);
		debug_delay(TEST_DELAY_MS);
		
		// Output 1|2: ON  |  OFF
		print_uint8("TEST OUTPUTS: ON | OFF", 0);
		RELAY_ON(0);
		RELAY_OFF(1);
		debug_delay(TEST_DELAY_MS);
		
		// Output 1|2: ON  |  ON
		print_uint8("TEST OUTPUTS: ON | ON", 0);
		RELAY_ON(0);
		RELAY_ON(1);
		debug_delay(TEST_DELAY_MS);
		
		// Output 1|2: OFF  |  ON
		print_uint8("TEST OUTPUTS: OFF | ON", 0);
		RELAY_OFF(0);
		RELAY_ON(1);
		debug_delay(TEST_DELAY_MS);
		
	}
}

#endif /* DEBUG_OUTPUTS_OPERATION */

#if DEBUG_LEDS

#include <avr/wdt.h>

#define TEST_SHORT_DELAY_MS 1000
#define TEST_LONG_DELAY_MS 5000

static void debug_delay(uint16_t ms)
{
	for (uint16_t i = 0; i < ms; i++)
	{
		_delay_ms(1);
		wdt_reset();
	}
}

static void debug_main_led()
{
	// Test Cases
	// 1. LED_STATE_OFF
	// 2. LED_STATE_ON
	// 3. LED_STATE_BLINK_1S
	// 4. LED_STATE_BLINK_500MS
	// 5. LED_STATE_FLASH - NOT USED (see below)
		
	// 6. LED_FLASH_NONE - NOT USED
	// 7. LED_FLASH_10
	// 8. LED_FLASH_20
		
	// 1. LED_STATE_OFF
	g_led_state = LED_STATE_OFF;
	print_uint8("TEST LED OFF", g_led_state);
	debug_delay(TEST_LONG_DELAY_MS);
	
	// 2. LED_STATE_ON
	g_led_state = LED_STATE_ON;
	print_uint8("TEST LED ON", g_led_state);
	debug_delay(TEST_LONG_DELAY_MS);
	
	// 3. LED_STATE_BLINK_1S
	g_led_state = LED_STATE_BLINK_1S;
	print_uint8("TEST LED BLINK 1s", g_led_state);
	debug_delay(TEST_LONG_DELAY_MS);
	
	// 4. LED_STATE_BLINK_500MS
	g_led_state = LED_STATE_BLINK_500MS;
	print_uint8("TEST LED BLINK 500 [ms]", g_led_state);
	debug_delay(TEST_LONG_DELAY_MS);
	
	// 5. SKIP
	
	// 6. SKIP
	
	// 7. LED_FLASH_500_MS
	g_led_flash = LED_FLASH_500_MS;
	print_uint8("TEST LED FLASH 500 [ms]", g_led_state);
	debug_delay(TEST_LONG_DELAY_MS);
	
	// 2. LED_STATE_ON
	g_led_state = LED_STATE_ON;
	print_uint8("TEST LED ON", g_led_state);
	debug_delay(TEST_LONG_DELAY_MS);
	
	// 7. LED_FLASH_1_SEC
	g_led_flash = LED_FLASH_1_SEC;
	print_uint8("TEST LED FLASH 1 [sec]", g_led_state);
	debug_delay(TEST_LONG_DELAY_MS);
	
	// 3. LED_STATE_BLINK_1S
	g_led_state = LED_STATE_BLINK_1S;
	print_uint8("TEST LED BLINK 1s", g_led_state);
	debug_delay(TEST_LONG_DELAY_MS);
	
	// 1. LED_STATE_OFF
	g_led_state = LED_STATE_OFF;
	print_uint8("TEST LED OFF", g_led_state);
	debug_delay(TEST_LONG_DELAY_MS);
	
	// 7. LED_FLASH_1_SEC
	g_led_flash = LED_FLASH_1_SEC;
	print_uint8("TEST LED FLASH 1 [sec]", g_led_state);
	debug_delay(TEST_LONG_DELAY_MS);
	
	// 7. LED_FLASH_500_MS
	g_led_flash = LED_FLASH_500_MS;
	print_uint8("TEST LED FLASH 500 [ms]", g_led_state);
	debug_delay(TEST_LONG_DELAY_MS);
	
	// 2. LED_STATE_ON
	g_led_state = LED_STATE_ON;
	print_uint8("TEST LED ON", g_led_state);
	debug_delay(TEST_LONG_DELAY_MS);
}

static void debug_button_led_1()
{
	BUTTON_1_INIT;
		
	while(1)
	{
		uint16_t button_counter = 0;
		
		while(!button_counter)
		{
			// Wait for button press...
				
			while (BUTTON_1_STATE)
			{			
				debug_delay(CONTROLLER_HP_BUTTON_DEBOUNCE_MS);
				button_counter++;
			}
			
			wdt_reset();
		}
		
		
		// Button long-press
		if ((button_counter * CONTROLLER_HP_BUTTON_DEBOUNCE_MS) >= (CONTROLLER_HP_BUTTON_LONG_PRESS_SEC * 1000))
		{
			LED_BUTTON_1_OFF;
			print_uint16("Long press", button_counter);
		}
		// Button short-press
		else
		{
			LED_BUTTON_1_TOGGLE;
			print_uint16("Short press", button_counter);
		}
		
		wdt_reset();
	}
}

static void debug_button_led_2()
{
	BUTTON_2_INIT;
	
	while(1)
	{
		uint16_t button_counter = 0;
		
		while(!button_counter)
		{
			// Wait for button press...
			
			while (BUTTON_2_STATE)
			{
				debug_delay(CONTROLLER_HP_BUTTON_DEBOUNCE_MS);
				button_counter++;
			}
			
			wdt_reset();
		}
		
		
		// Button long-press
		if ((button_counter * CONTROLLER_HP_BUTTON_DEBOUNCE_MS) >= (CONTROLLER_HP_BUTTON_LONG_PRESS_SEC * 1000))
		{
			LED_BUTTON_2_OFF;
			print_uint16("Long press", button_counter);
		}
		// Button short-press
		else
		{
			LED_BUTTON_2_TOGGLE;
			print_uint16("Short press", button_counter);
		}
		
		wdt_reset();
	}
}

static void debug_leds()
{
	print("TESTING LEDs...\r\n");
	
	while(1)
	{	
		// debug_main_led();
		// debug_button_led_1();
		debug_button_led_2();
	}	
}
#endif /* DEBUG_LEDS */

#if DEBUG_MCP79410

// RTC Testing
void rtc_testing()
{
	WDT_INIT_TESTING;
	MCP79410_init(false);
	
	time_t time_get = MCP79410_get_time();
	print_uint32("Time get init:", time_get);
	
	// Test timestamp t0
	time_t time_t0 = 1674671316 - UNIX_OFFSET; // 727,986,416
	
	// Set time from y2k epoch (i.e. from 2000)
	time_t time_set = TESTING_TIME_SET;
	MCP79410_set_time(time_set);
	
	print_uint32("Time set:", time_set);
	
	while(1)
	{
		// Get RTC time
		time_t time_get = MCP79410_get_time();
		
		print_uint32("Time get:", time_get);
		
		CONSOLE_print_time(time_get, false);
		CONSOLE_print_time(time_get, true);
			
		// int32_t diff = difftime(time_set, time_t0);
		// print_int32("DIFF:", diff);
		
		_delay_ms(1000);
		wdt_reset();
	}
	
	return;
}
#endif /* DEBUG_MCP79410 */

/*** end of file ***/