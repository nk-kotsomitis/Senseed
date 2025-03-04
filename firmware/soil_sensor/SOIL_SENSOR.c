/*
* @file SOIL_SENSOR.c
*
* @brief Soil Sensor main high-level library
*
*/

#include <stdio.h>
#include <stdbool.h>
#include <avr/wdt.h>

#include "DataTypes.h"
#include "ADC_atmega328p.h"
#include "NRF_lib.h"
#include "THINGS_lib.h"
#include "SOIL_SENSOR.h"

// Set to apply a an offset at battery measurement, clear otherwise
#define DEBUG_APPLY_VCC_OFFSET 1
// Set to round the ADC raw values, clear otherwise
#define DEBUG_SOIL_ADC_ROUND 1

// Data types
typedef enum
{
	SOIL_CHANNEL_1 = 1,
	SOIL_CHANNEL_2 = 2,
	SOIL_CHANNEL_3 = 3,
	SOIL_CHANNEL_4 = 4,
	
} Channel_t;

// Static variables
static uint8_t g_cycles = 0;
static uint16_t debug_batt_adc_raw = 0;
static double debug_batt_adc_volt = 0;
static uint16_t debug_soil_raw_1 = 0;
static uint16_t debug_soil_raw_2 = 0;
static uint16_t debug_soil_raw_3 = 0;
static uint16_t debug_soil_raw_4 = 0;

// Static functions
static void handle_channel_state(Soil_channel_t *channel);
static void update_eeprom();
static void button_pressed();
static void button_long_press_actions();
static void get_battery_voltage();
static void get_soil_moisture();
static void start_excitation_signal();
static void stop_excitation_signal();

#if 0
static uint16_t get_raw_maximum_value(double batt_voltage);
static uint16_t get_raw_minimum_value(double batt_voltage);
#endif

#if DEBUG_MEASUREMENTS
static void debug_measurements(void);
#endif

/*!
* @brief Sensor's initializations
*/
void SOIL_SENSOR_init()
{

#ifdef DEBUG
	CONSOLE_init();
	CONSOLE_log("SYSTEM STARTUP ", 1);
#endif

#if 0
	// NRF Testing - This is a while(1) function
	NRF_TEST_transmitter();
#endif

	// Sensor init
	g_cycles = 0;
	g_soil_sensor = SOIL_SENSOR_ZERO;
	g_soil_sensor.id = atoi((const char *)STRINGIZE(UUID));
	
	// RF Address from symbol
	// uint8_t temp_add[5] = RF_ADDRESS;
	Rf_address_t temp_add = TESTING_NRF_ADDR_THING_SENSOR_1;
	memcpy(g_soil_sensor.address, temp_add, RF_ADDRESS_BYTES);
	
	// Global gateway RF address for Soil Sensors
	Rf_address_t temp_add_gw = NRF_ADDRESS_GATEWAY_2_ENV_SENSORS;
	memcpy(g_soil_sensor.address_gw, temp_add_gw, RF_ADDRESS_BYTES);
		
	// g_soil_sensor.clock = g_time_epoch_y2k;
	g_soil_sensor.packet_id_tx = INVALID_PACKET_ID;
	
	// TODO: EEPROM
	//g_soil_sensor = SOIL_SENSOR_DEFAULT;
	//EEPROM_init_soil_sensor(&g_soil_sensor, EE_SOIL_SENSOR_DEFAULT);	
	
	// Initialize NRF lib
	NRF_init(TYPE_SOIL_SENSOR, g_soil_sensor.address_gw, g_soil_sensor.address);
}

/*!
* @brief Sensor's power-up
*/
void SOIL_SENSOR_power_up()
{
	// NOTE: Button and soil clock 1 share the same pin. Use as soil clock from now on
	INT_1_DISABLE;
	
	// Turn on ADC
	ADC_enable();
	// Turn on RF module
	NRF_power_on();
	
	// Button
	button_pressed();

	// Initialize soil clock 1 to be used for measurements
	SOIL_CLK_1_INIT;
	
	// Increase cycles
	g_cycles++;
}

/*!
* @brief Sensor's power-down
*/
void SOIL_SENSOR_power_down()
{
	if (g_cycles >= SOIL_SENSOR_MEASUREMENT_CYCLES || g_soil_sensor.is_active)
	{
		// Clear cycles
		g_cycles = 0;
	}
	
	// IMPORTANT NOTE: In case a retransmission may take place, consider re-sending the "previous" measurement with a flag in the packet.
	
	// Clear ALL measurements
	g_soil_sensor.battery_mv = 0;
	g_soil_sensor.channel_1.measurement = g_soil_sensor.channel_2.measurement = g_soil_sensor.channel_3.measurement = g_soil_sensor.channel_4.measurement = 0;
	
	// NOTE: Button and soil clock 1 share the same pin. Use as a button from now on
	SOIL_CLK_1_INIT; // TEST
	INT_1_ENABLE;
	
	// Turn off ADC
	ADC_disable();
	// Turn off RF module
	NRF_power_off();
	
	_delay_ms(1); // TBC if this is required
}

/*!
* @brief Sensor's measurements
*/
void SOIL_SENSOR_measurements()
{
	
#if (DEBUG_MEASUREMENTS)
	while (1) 
	{
#else		
		if (g_cycles < SOIL_SENSOR_MEASUREMENT_CYCLES && !g_soil_sensor.is_active)
		{
			return;
		}

#endif
		// Get battery voltage
		ADC_init(ADC_VREF_1V1);
		get_battery_voltage();

		// Get soil moisture
		start_excitation_signal();
		_delay_ms(SOIL_SENSOR_DELAY_EXCITATION_SIGNAL_MS);
		ADC_init(ADC_VREF_AVCC);
		get_soil_moisture();
		stop_excitation_signal();

#if (DEBUG_MEASUREMENTS)
		// Debug measurements
		debug_measurements();
		
		_delay_ms(DEBUG_MEASUREMENTS_DELAY_MS);
		wdt_reset();
	}
#endif

}

/*!
* @brief Sensor's tasks
*/
void SOIL_SENSOR_tasks()
{
	// NOTE: To handle the state of a channel, a measurement SHOULD take place just before.
	
	// Handle states
	handle_channel_state(&g_soil_sensor.channel_1);
	handle_channel_state(&g_soil_sensor.channel_2);
	handle_channel_state(&g_soil_sensor.channel_3);
	handle_channel_state(&g_soil_sensor.channel_4);
	
	// Update Sensor's active state. 
	g_soil_sensor.is_active = g_soil_sensor.channel_1.active | g_soil_sensor.channel_2.active | g_soil_sensor.channel_3.active | g_soil_sensor.channel_4.active;
}

/*!
* @brief Sensor's connection within network
*/
void SOIL_SENSOR_connection()
{
	NRF_LIB_Connect_res_t res = NRF_LIB_CONNECT_ERROR;
	
	// Connect Interval: 10 [sec]
	// Measurement Interval: 60 [sec]
	
	// Connect with gateway	
	res = NRF_connect_tx(g_soil_sensor.id, TYPE_SOIL_SENSOR);
	
	switch (res)
	{
		case NRF_LIB_CONNECT_SUCCESS:
			// Success
			g_soil_sensor.packet_id_tx = g_soil_sensor.packet_id_rx;
			
			// Update EEPROM
			update_eeprom();
			
			break;
			
		case NRF_LIB_CONNECT_SUCCESS_TX_ERROR_ACK:
			// Success, ACK Error
			g_soil_sensor.packet_id_tx = g_soil_sensor.packet_id_rx; // TBC
			break;
		
		case NRF_LIB_CONNECT_ERROR:
			break;
			
		default:
			break;
	}	
}

/************************************************************************************
******************************** Static functions ***********************************
************************************************************************************/

// Handle channel state
void handle_channel_state(Soil_channel_t *channel)
{
	// NOTE: The measurement here is supposed to be NEW (i.e. from this cycle)
	
	// TODO: Needs update to cover all cases, e.g. automation, timer, manual. Enable/disable, active state, force active state,... See word document.
	
	// Check if channel is enable for automation
	if (!channel->enabled)
	{
		channel->active = 0;
		return;
	}
	
	// Active state
	if (channel->active)
	{
		// Check stop condition
		if (channel->measurement - channel->measurement_prev <= 0 || channel->measurement >= channel->active)
		{
			channel->active = 0;
		}
		// Save last measurement
		channel->measurement_prev = channel->measurement;
	}
	
	// Inactive state - Check start condition
	if (channel->measurement <= channel->threshold_low)
	{
		// Save last measurement
		channel->measurement_prev = channel->measurement;
		// Set active
		channel->active = 1;
	}
}

// Updates EEPROM values
static void update_eeprom()
{
	
	if (!g_soil_sensor.sw_change)
	{
		return;
	}
	
	// TODO: EEPROM
	
	// EEPROM updated successfully
	g_soil_sensor.sw_change = 0;
	
}

// Checks for long press button 
static void button_pressed()
{
	uint16_t button_counter = 0;
	
	// Button de-bounce. Otherwise  the measurements will be affected !!!
	BUTTON_1_INIT;
	while (BUTTON_1_STATE)
	{
		//print_uint8("B ", BUTTON_1_STATE);
		wdt_reset();
		_delay_ms(SOIL_SENSOR_BUTTON_DEBOUNCE_MS);
		button_counter++;
	}
	
	// Button long-press
	if ((button_counter * SOIL_SENSOR_BUTTON_DEBOUNCE_MS) >= (SOIL_SENSOR_BUTTON_LONG_PRESS_SEC * 1000))
	{
		button_long_press_actions();
	}
}

// Actions for long press button
static void button_long_press_actions()
{
	// TODO: Hard reset
	
	print_uint8("LONG PRESS", 1);
}

// Get the battery voltage
static void get_battery_voltage()
{
	// Generate a pulse to open the P-MOSFET
	// NOTE: The MOSFET will remain open for a limited time (based on RC values on the circuit)
	BATT_CTRL_ON;
	_delay_us(SOIL_SENSOR_DELAY_BATTERY_VOLTAGE_US); // Defined by trial and error
	BATT_CTRL_OFF;	 
		 
	//_delay_us(130); // Defined by trial and error
	
	// Measure the voltage
	ADC_get_measurement(BATTERY_ADC_CH);
	uint16_t adc_raw = ADC_get_measurement(BATTERY_ADC_CH);
		
	
	//double mv = ((double)adc_raw * BATTERY_MV_MAX / 1024);
	
	// Convert ADC raw to mV
	// Vcc = ADC * (Vref / 1024) * (1 + R1/R2)
	double mv = (double)adc_raw * (BATTERY_VD_VREF / 1024.0) * (1.0 + (BATTERY_VD_R1/BATTERY_VD_R2));
	
#if (DEBUG_APPLY_VCC_OFFSET)
	// Apply offset
	// y = 1.07142857142857x + 42.8571428571429
	mv = 1.07142857142857 * mv + 42.8571428571429;
#endif

	// Convert mV from double to uint16
	// NOTE: Accuracy is +- 10 mV
	g_soil_sensor.battery_mv = RoundInteger16((uint16_t)mv);
	
	// Debug values. To be deleted
	debug_batt_adc_raw = adc_raw;
	debug_batt_adc_volt = mv;
}

// Get the soil moisture of all channels
void get_soil_moisture()
{	
	// Get measurements
	uint16_t adc_raw_value_1 = ADC_get_measurement(SOIL_CAP_1_ADC_CH);
	uint16_t adc_raw_value_2 = ADC_get_measurement(SOIL_CAP_2_ADC_CH);
	uint16_t adc_raw_value_3 = ADC_get_measurement(SOIL_CAP_3_ADC_CH);
	uint16_t adc_raw_value_4 = ADC_get_measurement(SOIL_CAP_4_ADC_CH);
		
#if (DEBUG_SOIL_ADC_ROUND)
	
	// NOTE: Accuracy is +- 10
	adc_raw_value_1 = RoundInteger16(adc_raw_value_1);
	adc_raw_value_2 = RoundInteger16(adc_raw_value_2);
	adc_raw_value_3 = RoundInteger16(adc_raw_value_3);
	adc_raw_value_4 = RoundInteger16(adc_raw_value_4);
#endif
	
	// Update measurements
	g_soil_sensor.channel_1.measurement = adc_raw_value_1;
	g_soil_sensor.channel_2.measurement = adc_raw_value_2;
	g_soil_sensor.channel_3.measurement = adc_raw_value_3;
	g_soil_sensor.channel_4.measurement = adc_raw_value_4;

	// Debug
	debug_soil_raw_1 = adc_raw_value_1;
	debug_soil_raw_2 = adc_raw_value_2;
	debug_soil_raw_3 = adc_raw_value_3;
	debug_soil_raw_4 = adc_raw_value_4;
}

// Starts the excitation signal
static void start_excitation_signal()
{
	/*	
	Timer 0
	OC0A - OCR0A - PD6 - SOIL_CLK_3
	OC0B - OCR0B - PD5 - SOIL_CLK_2
	
	Timer 1
	OC1A - OCR1A - PB1 - SOIL_CLK_4
	OC1B - OCR1B - PB2 - Reserved by SPI
	
	Timer 2
	OC2A - OCR2A - PB3 - Reserved by SPI
	OC2B - OCR2B - PD3 - SOIL_CLK_1
	
	- Compare Output Mode (Toggle Pin)
	- Waveform Generation Mode (CTC)
	- Prescaler (No prescaler)
	- Compare value (0 for 4Mhz and no prescaler)
	*/

#if 0
	switch (1)
	{
		case SOIL_CHANNEL_1:
			break;
		case SOIL_CHANNEL_2:
			break;
		case SOIL_CHANNEL_3:
			break;
		case SOIL_CHANNEL_4:
			break;
	}
#endif

#if 1
	// **************************** Timer 0 ****************************
	// Toggle OC0A and OC0B on Compare Match
	TCCR0A = (0 << COM0A1) | (1 << COM0A0) | (0 << COM0B1) | (1 << COM0B0);
	// CTC mode
	TCCR0A |= (1 << WGM01) | (0 << WGM00);
	// Select prescaler
	TCCR0B = (0 << WGM02) | (0 << CS02) | (0 << CS01) | (1 << CS00);	// 4MHz/1 => 250ns tick
	// Select compare value
	OCR0A = 0;
	OCR0B = 0;
	// Clear counter
	TCNT0 = 0;
		
	// **************************** Timer 1 ****************************
	// Toggle OC1A on Compare Match
	TCCR1A = (0 << COM1A1) | (1 << COM1A0) | (0 << COM1B1) | (0 << COM1B0);
	// CTC mode
	TCCR1A |= (0 << WGM11) | (0 << WGM10);
	// Select prescaler
	TCCR1B = (0 << WGM13) | (1 << WGM12) | (0 << CS12) | (0 << CS11) | (1 << CS10);	// 4MHz/1 => 250ns tick
	// Select compare value
	OCR1A = 0;
	//OCR1B = 0;
	// Clear counter
	TCNT1 = 0;
	
#endif

	// **************************** Timer 2 ****************************

	// Toggle OC2B on Compare Match
	TCCR2A = (0 << COM2A1) | (0 << COM2A0) | (0 << COM2B1) | (1 << COM2B0);
	// CTC mode
	TCCR2A |= (1 << WGM21) | (0 << WGM20);
	// Select prescaler
	TCCR2B = (0 << WGM22) | (0 << CS22) | (0 << CS21) | (1 << CS20);	// 4MHz/1 => 250ns tick
	// Select compare value
	//OCR2A = 0;
	OCR2B = 0;
	// Clear counter
	TCNT2 = 0;
}

// Stops the excitation signal
static void stop_excitation_signal()
{
	// Stop all timers
	TIMERS_STOP_ALL;
	// Timer 0 is used for debugging - 8bit, 1024 pre-scaler, 256us tick
	TIMER_0_INIT;
	// Timer 1 is used by NRF library - 16bit, 16 pre-scaler, 16us tick
	TIMER_1_INIT;
}

#if DEBUG_MEASUREMENTS

// Debugging measurements
static void debug_measurements(void)
{
	static uint32_t counter = 0;
	counter++;
	
	char s[30];
	print("DEBUG");
	print(MEAS_SEP);
	print(utoa(counter, s, 10));
	print(MEAS_SEP);
	print(utoa(debug_batt_adc_raw, s, 10));
	print(MEAS_SEP);
	print(dtostrf(debug_batt_adc_volt, 3, 3, s));
	print(MEAS_SEP);
	print(utoa(g_soil_sensor.battery_mv, s, 10));
	print(MEAS_SEP);
	print(utoa(debug_soil_raw_1, s, 10));
	print(MEAS_SEP);
	print(utoa(debug_soil_raw_2, s, 10));
	print(MEAS_SEP);
	print(utoa(debug_soil_raw_3, s, 10));
	print(MEAS_SEP);
	print(utoa(debug_soil_raw_4, s, 10));
	
	//print(utoa(g_soil_sensor.soil_1, s, 10));
	//print(MEAS_SEP);
	//print(utoa(g_soil_sensor.soil_1, s, 10));
	//print(MEAS_SEP);
	//print(utoa(g_soil_sensor.soil_2, s, 10));
	//print(MEAS_SEP);
	//print(utoa(g_soil_sensor.soil_3, s, 10));
	//print(MEAS_SEP);
	//print(utoa(g_soil_sensor.soil_4, s, 10));
	print("\r\n");
}
#endif

/*** end of file ***/