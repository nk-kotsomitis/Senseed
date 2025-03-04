/*
* @file ENV_SENSOR.h
*
* @brief Environment Sensor main high-level library
*
*/

#ifndef SOIL_SENSOR_H
#define SOIL_SENSOR_H

// Debugging Flags
#define DEBUG_MEASUREMENTS 1
#define DEBUG_MEASUREMENTS_DELAY_MS 1000

// Connect cycles: e.g. 2 [sec] (WDT) * 5 = 10 [sec]
#define SOIL_SENSOR_CONNECT_CYCLES 5
// Measurement cycles: e.g. 10 [sec] (WDT * Connect) * 6 = 60 [sec]
#define SOIL_SENSOR_MEASUREMENT_CYCLES 6

// WDT seconds based on existing configuration. DO NOT CHANGE !!!
#define SOIL_SENSOR_SLEEP_WDT_SEC 2
// Connect interval in [sec]
#define SOIL_SENSOR_CONNECT_CYCLES_SECS (SOIL_SENSOR_SLEEP_WDT_SEC*SOIL_SENSOR_CONNECT_CYCLES)
// Measurement interval in [sec]
#define SOIL_SENSOR_MEASUREMENT_CYCLES_SECS (SOIL_SENSOR_CONNECT_CYCLES_SECS*SOIL_SENSOR_MEASUREMENT_CYCLES)

// ********* Sensor's Delays *********
// NRF24L01 power-up		: 1.5 [ms]
// ADC init					: 0.1 [ms]
// Sensor battery voltage	: 200 [ms]
// Sensor excitation signal : 300 [ms]
// Sensor power-down		: 1   [ms] TBC if required
// ***********************************

// Measurement Delays
#define SOIL_SENSOR_DELAY_BATTERY_VOLTAGE_US 200000
#define SOIL_SENSOR_DELAY_EXCITATION_SIGNAL_MS 300

// Button de-bounce [ms]
#define SOIL_SENSOR_BUTTON_DEBOUNCE_MS 100
// Button long press duration [sec]
#define SOIL_SENSOR_BUTTON_LONG_PRESS_SEC 5

// Library initialization
void ENV_SENSOR_init();
// Sensors power-up
void ENV_SENSOR_power_up();
// Sensors power-down
void ENV_SENSOR_power_down();
// Sensor measurements
void ENV_SENSOR_measurements();
// Sensor tasks
void ENV_SENSOR_tasks();
// Sensor connection within network
void ENV_SENSOR_connection();

#endif

/*** end of file ***/