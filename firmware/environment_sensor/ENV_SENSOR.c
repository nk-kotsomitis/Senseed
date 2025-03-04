/*
* @file ENV_SENSOR.c
*
* @brief Environment Sensor main high-level library
*
*/
#include <stdio.h>
#include <stdbool.h>
#include <avr/wdt.h>

#include "DataTypes.h"
#include "ADC_atmega328p.h"
#include "NRF_lib.h"
#include "THINGS_lib.h"
#include "ENV_SENSOR.h"

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

// Static functions
static void handle_channel_state(Soil_channel_t *channel);
static void update_eeprom();
static void button_pressed();
static void button_long_press_actions();
static void get_battery_voltage();

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
void ENV_SENSOR_init()
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
	Rf_address_t temp_add_gw = NRF_ADDRESS_GATEWAY_2_ENV_SENSORS; // TODO: NRF_ADDRESS_GATEWAY_1_SOIL_SENSORS
	memcpy(g_soil_sensor.address_gw, temp_add_gw, RF_ADDRESS_BYTES);
		
	// g_soil_sensor.clock = g_time_epoch_y2k;
	g_soil_sensor.packet_id_tx = INVALID_PACKET_ID;
	
	// TODO: EEPROM
	//g_soil_sensor = SOIL_SENSOR_DEFAULT;
	//EEPROM_init_soil_sensor(&g_soil_sensor, EE_SOIL_SENSOR_DEFAULT);	
	
	// Initialize NRF lib
	NRF_init(TYPE_SOIL_SENSOR, g_soil_sensor.address_gw, g_soil_sensor.address);
	// SHT40 init
	SHT40_init();
}

/*!
* @brief Sensor's power-up
*/
void ENV_SENSOR_power_up()
{
	// NOTE: Button and soil clock 1 share the same pin. Use as soil clock from now on
	//INT_1_DISABLE;
	
	// Turn on ADC
	ADC_enable();
	// Turn on RF module
	NRF_power_on();
	
	// Button
	button_pressed();

	// Initialize soil clock 1 to be used for measurements
	//SOIL_CLK_1_INIT;
	
	// Increase cycles
	g_cycles++;
}

/*!
* @brief Sensor's power-down
*/
void ENV_SENSOR_power_down()
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
	//SOIL_CLK_1_INIT; // TEST
	//INT_1_ENABLE;
	
	// Turn off ADC
	ADC_disable();
	// Turn off RF module
	NRF_power_off();
	
	_delay_ms(1); // TBC if this is required
}

/*!
* @brief Sensor's measurements
*/
void ENV_SENSOR_measurements()
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
				
		// Get temperature and humidity
		SHT40_get_measurements();
		
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
void ENV_SENSOR_tasks()
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
void ENV_SENSOR_connection()
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
	print("\r\n");
}
#endif

/*** end of file ***/