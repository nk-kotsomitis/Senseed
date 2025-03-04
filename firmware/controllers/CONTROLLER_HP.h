/*
* @file CONTROLLER_HP.h
*
* @brief Controller HP main library
*
*/

#ifndef CONTROLLER_HP_H
#define CONTROLLER_HP_H

// Debugging Flags
#define DEBUG_MEASUREMENTS 1 // TBC

#define DEBUG_OUTPUTS_OPERATION 0
#define DEBUG_MCP79410 0
#define DEBUG_LEDS 1
#define DEBUG_NRF 0

// Connect cycles: e.g. 10 [sec]
#define CONTROLLER_HP_CONNECT_SEC 10
// Measurement cycles: e.g. 60 [sec]
#define CONTROLLER_HP_MEASUREMENT_SEC 30

// Number of ADC samples for Volt peak-to-peak. Each sample is one cycle.
#define ACS712_NUM_OF_SAMPLES 2000 // TODO: This number is random. To be checked!
// Sensitivity for 30A ACS712185 in [mV]
#define ACS712_SENSITIVITY_MV 185.0
// Controller's volt for using it in ADC conversion
#define CONTROLLER_HP_ADC_VOLT 5.0

// Button de-bounce [ms]
#define CONTROLLER_HP_BUTTON_DEBOUNCE_MS 100
// Button long press duration [sec]
#define CONTROLLER_HP_BUTTON_LONG_PRESS_SEC 5

// Extern variables used by main.c for buttons and LEDs
volatile Button_state_t g_button_1;
volatile Button_state_t g_button_2;
volatile Led_state_t g_led_state;
volatile Led_flash_t g_led_flash;
volatile uint16_t g_button_1_ticks;
volatile uint16_t g_button_2_ticks;
volatile uint16_t g_led_ticks;

// Library initialization
void CONTROLLER_HP_init();
// Controller start cycle
void CONTROLLER_HP_cycle_start();
// Controller end cycle
void CONTROLLER_HP_cycle_end();
// Controller measurements
void CONTROLLER_HP_measurements();
// Controller tasks
void CONTROLLER_HP_tasks();
// Controller connection within network
void CONTROLLER_HP_connection();

#endif

/*** end of file ***/