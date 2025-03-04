/*
* @file PinsAndMacros.h
*
* @brief Pin and macros.
*
*/

#ifndef PINS_AND_MACROS_H
#define PINS_AND_MACROS_H

#define GETBIT(x) (1 << (x)) /* Get bit y in byte x*/
#define SETBIT(x,y) (x |= (y)) /* Set bit y in byte x*/
#define CLEARBIT(x,y) (x &= (~y)) /* Clear bit y in byte x*/
#define CHECKBIT(x,y) (x & (y)) /* Check bit y in byte x*/
#define _STRINGIZE(x) #x
#define STRINGIZE(x) _STRINGIZE(x)
	
/************************************************************************************
******************************** Gateway ********************************************
************************************************************************************/

#ifdef GATEWAY

// CPU Frequency
#define F_CPU 16000000UL

// SPI Pins for SPI library
#define SPI_PORT PORTB
#define SPI_DDR  DDRB
#define SPI_SCK  DDB7
#define SPI_MISO DDB6
#define SPI_MOSI DDB5
#define SPI_SS   DDB4

// CS and CE for NRF24L01
#define RF_CE		DDD4 // PD4
#define RF_CSN		DDB4 // PB4
#define RF_CE_INIT	DDRD |=  (1 << RF_CE);
#define RF_CS_INIT  DDRB |=  (1 << RF_CSN);
#define RF_CE_HIGH  PORTD |=  (1 << RF_CE);
#define RF_CE_LOW   PORTD &= ~(1 << RF_CE);
#define RF_CE_STATE (PORTB&GETBIT(RF_CE))
#define RF_CS_HIGH  PORTB |=  (1 << RF_CSN);
#define RF_CS_LOW   PORTB &= ~(1 << RF_CSN);

// Timers
#define TIMER_0 TCNT0
#define TIMER_1 TCNT1
#define TIMER_2 TCNT2
#define TIMER_0_TICK_MS		0.064
#define TIMER_1_TICK_MS		0.064
#define TIMER_2_TICK_MS		0.064

// Modules/Leds/Buttons
#define CONSUMPTION	0x04	// PA2
#define RELAY1		0x08	// PA3
#define NRF_CONTROL	0x20	// PD5
#define ESP_CONTROL	0x10	// PA4
#define BUTTON_1	0x04	// PB2
#define BUTTON_3	0x02	// PA1
#define LED1 0x02			// PB1
#define LED2 0x01			// PB0
#define LED3 0x01			// PA0
#define LED4 0x40 			// PD6
#define LED5 0x08			// PB3

#define consumption_init	DDRA&=~CONSUMPTION
#define relay_1_init	DDRA|=RELAY1
#define relay_1_on		PORTA|=RELAY1
#define relay_1_off		PORTA&=~RELAY1
#define relay_1_toggle	PORTA^=RELAY1
#define relay_1_read	((PORTA&RELAY1)>>3)

#define button_1_init	DDRB&=~BUTTON_1
#define button_1_pushed !(PINB&BUTTON_1)
#define button_1_pushed_seconds 3

#define led_1_init		DDRB|=LED1		// Internet
#define led_1_on		PORTB|=LED1
#define led_1_off		PORTB&=~LED1
#define led_1_toggle	PORTB^=LED1

#define led_2_init		DDRB|=LED2		// WPS
#define led_2_on		PORTB|=LED2
#define led_2_off		PORTB&=~LED2
#define led_2_toggle	PORTB^=LED2

#define led_3_init		DDRA|=LED3		// Warning
#define led_3_on		PORTA|=LED3
#define led_3_off		PORTA&=~LED3
#define led_3_toggle	PORTA^=LED3

#define led_4_init		DDRD|=LED4		// NRF packets
#define led_4_on		PORTD|=LED4
#define led_4_off		PORTD&=~LED4
#define led_4_toggle	PORTD^=LED4

#define led_5_init		DDRB|=LED5		// Relay status
#define led_5_on		PORTB|=LED5
#define led_5_off		PORTB&=~LED5
#define led_5_toggle	PORTB^=LED5


#define INTERNET_LED_ON			{TIMSK0&=(0 << OCIE0A); led_1_on;}
#define INTERNET_LED_OFF		{TIMSK0&=(0 << OCIE0A); led_1_off;}
#define INTERNET_LED_BLINK_MODE	{TIMSK0|=(1 << OCIE0A); TCNT0 = 0; INTERNET_LED_FAST }
#define INTERNET_LED_SLOW		OCR0A=157;
#define INTERNET_LED_NORMAL		OCR0A=100;
#define INTERNET_LED_FAST		OCR0A=38;
#define INTERNET_LED_SUPER_FAST	OCR0A=13;

// Tested - OK
//#define INTERNET_LED_ON			{TIMSK0&=(0 << OCIE0A); led_1_on;}
//#define INTERNET_LED_OFF		{TIMSK0&=(0 << OCIE0A); led_1_off;}
//#define INTERNET_LED_BLINK_MODE	{TIMSK0|=(1 << OCIE0A);}
//#define INTERNET_LED_SLOW		{OCR0A=157; TCNT0 = 0;}
//#define INTERNET_LED_NORMAL		{OCR0A=100; TCNT0 = 0;} // untested
//#define INTERNET_LED_FAST		{OCR0A=38; TCNT0 = 0;}
//#define INTERNET_LED_SUPER_FAST	{OCR0A=13; TCNT0 = 0;}


#define interrupt_0_init	{ EICRA |= _BV(ISC01); EIMSK |= _BV(INT0); }	// IRQ
#define interrupt_1_init	{ EICRA |= _BV(ISC11); EIMSK |= _BV(INT1); }	// Button2 - TX_debug
#define interrupt_2_init	{ EICRA |= _BV(ISC21); EIMSK |= _BV(INT2); }	// Button1

//interrupt_1_init; // Disabled because we need TX debug TODO

#define HW_INIT_GW { consumption_init; relay_1_init; button_1_init; led_1_init; led_2_init; led_3_init; led_4_init; led_5_init; interrupt_0_init; interrupt_2_init; sei(); }
	
#define WDT_INIT_GW { cli(); MCUSR &= ~(1<<WDRF); WDTCSR = (1<<WDCE)|(1<<WDE); WDTCSR = (1<<WDIE)|(0<<WDE)|(1<<WDP3)|(0<<WDP2)|(0<<WDP1|(0<<WDP0)); sei(); wdt_enable(WDTO_4S); }

/*
	Interrupts
	0 0 The low level of INTn generates an interrupt request
	0 1 Any edge of INTn generates asynchronously an interrupt request
	1 0 The falling edge of INTn generates asynchronously an interrupt request
	1 1 The rising edge of INTn generates asynchronously an interrupt request


	Watchdog 
	3|2|1|0
 	0|1|0|0		250ms
 	0|1|0|1		500ms
 	0|1|1|0		1s
 	1|0|0|0		4s
*/

#endif

/************************************************************************************
******************************** Soil Sensor ****************************************
************************************************************************************/

#ifdef SOIL_SENSOR

// CPU Frequency
#define F_CPU 4000000UL
//#define F_CPU 16000000UL

// SPI Pins for SPI library
#define SPI_PORT PORTB
#define SPI_DDR  DDRB
#define SPI_SCK  DDB5
#define SPI_MISO DDB4
#define SPI_MOSI DDB3
#define SPI_SS   DDB2

// CS and CE for NRF24L01
#define RF_CE		DDB0
#define RF_CS		DDB2
#define RF_CE_INIT	DDRB |= (1 << RF_CE);
#define RF_CS_INIT	DDRB |= (1 << RF_CS);
#define RF_CE_HIGH	PORTB |= (1 << RF_CE);
#define RF_CE_LOW	PORTB &= ~(1 << RF_CE);
#define RF_CE_STATE (PORTB&GETBIT(RF_CE))
#define RF_CS_HIGH	PORTB |= (1 << RF_CS);
#define RF_CS_LOW	PORTB &= ~(1 << RF_CS);

// Led Main
#define LED GETBIT(PORTD7)
#define LED_INIT DDRD|=LED
#define LED_ON PORTD|=LED
#define LED_OFF PORTD&=~LED
#define LED_TOGGLE PORTD^=LED
#define LED_STATE (PORTD&LED)
// Button 1
#define BUTTON_1 GETBIT(PORTD3)	// NOTE: This is the same pin with Soil Clock 1 !!!
#define BUTTON_1_INIT {DDRD &= ~BUTTON_1; PORTD &= ~BUTTON_1;}
#define BUTTON_1_STATE (PIND&BUTTON_1)
// Battery Control
#define BATT_CTRL GETBIT(PORTD4)
#define BATT_CTRL_INIT {DDRD|=BATT_CTRL; BATT_CTRL_OFF;} // Initialize and set to low BATT_CTRL_OFF
#define BATT_CTRL_ON PORTD|=BATT_CTRL
#define BATT_CTRL_OFF PORTD&=~BATT_CTRL
// Battery
#define BATTERY GETBIT(PORTC4)
#define BATTERY_ADC_CH PORTC4
#define BATTERY_INIT {DDRC&=~BATTERY; PORTC&=~BATTERY;}
// Soil Clock 1
#define SOIL_CLK_1 GETBIT(PORTD3)	// NOTE: This is the same pin with button !!!
#define SOIL_CLK_1_INIT DDRD|=SOIL_CLK_1
// Soil Clock 2
#define SOIL_CLK_2 GETBIT(PORTD5)
#define SOIL_CLK_2_INIT DDRD|=SOIL_CLK_2
// Soil Clock 3
#define SOIL_CLK_3 GETBIT(PORTD6)
#define SOIL_CLK_3_INIT DDRD|=SOIL_CLK_3
// Soil Clock 4
#define SOIL_CLK_4 GETBIT(PORTB1)
#define SOIL_CLK_4_INIT DDRB|=SOIL_CLK_4
// Soil Capacitance 1
#define SOIL_CAP_1 GETBIT(PORTC0)
#define SOIL_CAP_1_ADC_CH PORTC0
#define SOIL_CAP_1_INIT {DDRC&=~SOIL_CAP_1; PORTC&=~SOIL_CAP_1;}
// Soil Capacitance 2
#define SOIL_CAP_2 GETBIT(PORTC1)
#define SOIL_CAP_2_ADC_CH PORTC1
#define SOIL_CAP_2_INIT {DDRC&=~SOIL_CAP_2; PORTC&=~SOIL_CAP_2;}
// Soil Capacitance 3
#define SOIL_CAP_3 GETBIT(PORTC2)
#define SOIL_CAP_3_ADC_CH PORTC2
#define SOIL_CAP_3_INIT {DDRC&=~SOIL_CAP_3; PORTC&=~SOIL_CAP_3;}
// Soil Capacitance 4
#define SOIL_CAP_4 GETBIT(PORTC3)
#define SOIL_CAP_4_ADC_CH PORTC3
#define SOIL_CAP_4_INIT {DDRC&=~SOIL_CAP_4; PORTC&=~SOIL_CAP_4;}
// Unused Pins: PC5, ADC6, ADC7, PD0, PD1 (Tx Debug). Set to inputs with pull-up
// TODO: PORTC7 and PD1 (Tx Debug)
#define UNUSED_PINS_C (GETBIT(PORTC5) | GETBIT(PORTC6)) // | GETBIT(PORTC7));
#define UNUSED_PINS_C_INIT { {DDRC &= ~(UNUSED_PINS_C); PORTC |= UNUSED_PINS_C;} }
#define UNUSED_PINS_D (GETBIT(PORTD1))
#define UNUSED_PINS_D_INIT { {DDRD &= ~(UNUSED_PINS_D); PORTD |= UNUSED_PINS_D;} }
#define UNUSED_PINS_INIT {UNUSED_PINS_C_INIT; UNUSED_PINS_D_INIT;}

// Interrupt 0 - IRQ - Falling Edge
#define INT_0_INIT	{ EICRA |= (1 << ISC01) | (0 << ISC00); EIMSK |= (1 << INT0); }
// Interrupt 1 - Button - Rising Edge
#define INT_1_ENABLE	{ EIMSK |= (1 << INT1); }
#define INT_1_DISABLE	{ EIMSK &= ~(1 << INT1); }
#define INT_1_INIT		{ EICRA |= (1 << ISC11) | (1 << ISC10); INT_1_ENABLE; }
	
// All initializations
#define HW_INIT			{ UNUSED_PINS_INIT; LED_INIT; BUTTON_1_INIT; BATT_CTRL_INIT; BATTERY_INIT; SOIL_CLK_1_INIT; SOIL_CLK_2_INIT; SOIL_CLK_3_INIT; SOIL_CLK_4_INIT; SOIL_CAP_1_INIT; SOIL_CAP_2_INIT; SOIL_CAP_3_INIT; SOIL_CAP_4_INIT; INT_0_INIT; INT_1_INIT; sei(); }
// Watchdog main - Interrupt and System Reset Mode at 2s
#define WDT_INIT		{ cli(); wdt_reset(); MCUSR &= ~(1<<WDRF); WDTCSR = (1 << WDCE) | (1 << WDE); WDTCSR = (1 << WDIE) | (1 << WDE) | (0 << WDP3) | (1 << WDP2) | (1 << WDP1 | (1 << WDP0)); sei(); }
// Watchdog sleep - Interrupt Mode at 2s
#define WDT_INIT_SLEEP	{ cli(); wdt_reset(); MCUSR &= ~(1<<WDRF); WDTCSR = (1 << WDCE) | (1 << WDE); WDTCSR = (1 << WDIE) | (0 << WDE) | (0 << WDP3) | (1 << WDP2) | (1 << WDP1 | (1 << WDP0)); sei(); }

// Timer 0 Init - 1024 prescaler, 256us tick
#define TIMER_0 TCNT0
#define TIMER_0_TICK_MS	0.256
#define TIMER_0_INIT {TCCR0B = (1 << CS02) | (0 << CS01) | (1 << CS00); TCNT0 = 0;}
// Timer 1 Init - 64 prescaler, 16us tick
#define TIMER_1 TCNT1
#define TIMER_1_TICK_MS	0.016
#define TIMER_1_INIT {TCCR1B = (0 << CS12) | (1 << CS11) | (1 << CS10); TCNT1 = 0;}
// Timer 2 Init - Unused
#define TIMER_2 TCNT2
#define TIMER_2_TICK_MS	0.256
#define TIMER_2_INIT { TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20); TCNT2 = 0; }
// Timers common
#define TIMERS_INIT {TIMER_0_INIT; TIMER_1_INIT; TIMER_2_INIT;}
#define TIMERS_STOP_ALL {TCCR0A = TCCR0B = TCCR1A = TCCR1B = TCCR2A = TCCR2B = 0;}

#endif

/************************************************************************************
******************************** Environment Sensor *********************************
************************************************************************************/

#ifdef ENV_SENSOR

// CPU Frequency
#define F_CPU 4000000UL

// SPI Pins for SPI library
#define SPI_PORT PORTB
#define SPI_DDR  DDRB
#define SPI_SCK  DDB5
#define SPI_MISO DDB4
#define SPI_MOSI DDB3
#define SPI_SS   DDB2

// CS and CE for NRF24L01
#define RF_CE		DDB1
#define RF_CS		DDB2
#define RF_CE_INIT	DDRB |= (1 << RF_CE);
#define RF_CS_INIT	DDRB |= (1 << RF_CS);
#define RF_CE_HIGH	PORTB |= (1 << RF_CE);
#define RF_CE_LOW	PORTB &= ~(1 << RF_CE);
#define RF_CE_STATE (PORTB&GETBIT(RF_CE))
#define RF_CS_HIGH	PORTB |= (1 << RF_CS);
#define RF_CS_LOW	PORTB &= ~(1 << RF_CS);

// Led Main
#define LED GETBIT(PORTD5)
#define LED_INIT DDRD|=LED
#define LED_ON PORTD|=LED
#define LED_OFF PORTD&=~LED
#define LED_TOGGLE PORTD^=LED
#define LED_STATE (PORTD&LED)
// Button 1
#define BUTTON_1 GETBIT(PORTD2)	// NOTE: This is INT0
#define BUTTON_1_INIT {DDRD &= ~BUTTON_1; PORTD &= ~BUTTON_1;}
#define BUTTON_1_STATE (PIND&BUTTON_1)
// VCC Enable
#define VCC_EN GETBIT(PORTD6)
#define VCC_EN_INIT {DDRD|=VCC_EN; VCC_EN_ON;} // Initialize and set to low - Reverse Logic (P-Mosfet) !!!
#define VCC_EN_ON PORTD|=VCC_EN
#define VCC_EN_OFF PORTD&=~VCC_EN
// Boost Enable
#define BOOST_EN GETBIT(PORTB0)
#define BOOST_EN_INIT {DDRB|=BOOST_EN; BOOST_EN_OFF;} // Initialize and set to low BOOST_EN_OFF
#define BOOST_EN_ON PORTB|=BOOST_EN
#define BOOST_EN_OFF PORTB&=~BOOST_EN
// Battery Control
#define BATT_CTRL GETBIT(PORTD4)
#define BATT_CTRL_INIT {DDRD|=BATT_CTRL; BATT_CTRL_OFF;} // Initialize and set to low BATT_CTRL_OFF
#define BATT_CTRL_ON PORTD|=BATT_CTRL
#define BATT_CTRL_OFF PORTD&=~BATT_CTRL
// Battery
#define BATTERY GETBIT(PORTC3)
#define BATTERY_ADC_CH PORTC3
#define BATTERY_INIT {DDRC&=~BATTERY; PORTC&=~BATTERY;}
// CO2 - Digital Output
#define CO2_CS GETBIT(PORTC0)
#define CO2_CS_INIT DDRC|=CO2_CS
#define CO2_CS_ON PORTC|=CO2_CS
#define CO2_CS_OFF PORTC&=~CO2_CS
	
// Unused Pins: PC1, PC2, ADC6, ADC7, PD0, PD1 (Tx Debug), PD7. Set to inputs with pull-up
// TODO: PORTC7 and PD1 (Tx Debug)
#define UNUSED_PINS_C (GETBIT(PORTC1) | GETBIT(PORTC2) | GETBIT(PORTC6)) // | GETBIT(PORTC7));
#define UNUSED_PINS_C_INIT { {DDRC &= ~(UNUSED_PINS_C); PORTC |= UNUSED_PINS_C;} }
#define UNUSED_PINS_D (GETBIT(PORTD0) | GETBIT(PORTD7)) // | GETBIT(PORTD7)
#define UNUSED_PINS_D_INIT { {DDRD &= ~(UNUSED_PINS_D); PORTD |= UNUSED_PINS_D;} }
#define UNUSED_PINS_INIT {UNUSED_PINS_C_INIT; UNUSED_PINS_D_INIT;}

// Interrupt 0 - Button - Rising Edge
#define INT_0_ENABLE	{ EIMSK |= (1 << INT0); }
#define INT_0_DISABLE	{ EIMSK &= ~(1 << INT0); }
#define INT_0_INIT		{ EICRA |= (1 << ISC01) | (1 << ISC00); INT_0_ENABLE; }

// Interrupt 1 - IRQ - Falling Edge
#define INT_1_INIT	{ EICRA |= (1 << ISC11) | (0 << ISC10); EIMSK |= (1 << INT1); }

// All initializations
#define HW_INIT			{ UNUSED_PINS_INIT; LED_INIT; BUTTON_1_INIT; VCC_EN_INIT; BOOST_EN_INIT; CO2_CS_INIT; BATT_CTRL_INIT; BATTERY_INIT;  INT_0_INIT; INT_1_INIT; sei(); }
// Watchdog main - Interrupt and System Reset Mode at 2s
#define WDT_INIT		{ cli(); wdt_reset(); MCUSR &= ~(1<<WDRF); WDTCSR = (1 << WDCE) | (1 << WDE); WDTCSR = (1 << WDIE) | (1 << WDE) | (0 << WDP3) | (1 << WDP2) | (1 << WDP1 | (1 << WDP0)); sei(); }
// Watchdog sleep - Interrupt Mode at 2s
#define WDT_INIT_SLEEP	{ cli(); wdt_reset(); MCUSR &= ~(1<<WDRF); WDTCSR = (1 << WDCE) | (1 << WDE); WDTCSR = (1 << WDIE) | (0 << WDE) | (0 << WDP3) | (1 << WDP2) | (1 << WDP1 | (1 << WDP0)); sei(); }

// Timer 0 Init - 1024 prescaler, 256us tick
#define TIMER_0 TCNT0
#define TIMER_0_TICK_MS	0.256
#define TIMER_0_INIT {TCCR0B = (1 << CS02) | (0 << CS01) | (1 << CS00); TCNT0 = 0;}
// Timer 1 Init - 64 prescaler, 16us tick
#define TIMER_1 TCNT1
#define TIMER_1_TICK_MS	0.016
#define TIMER_1_INIT {TCCR1B = (0 << CS12) | (1 << CS11) | (1 << CS10); TCNT1 = 0;}
// Timer 2 Init - Unused
#define TIMER_2 TCNT2
#define TIMER_2_TICK_MS	0.256
#define TIMER_2_INIT { TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20); TCNT2 = 0; }
// Timers common
#define TIMERS_INIT {TIMER_0_INIT; TIMER_1_INIT; TIMER_2_INIT;}
#define TIMERS_STOP_ALL {TCCR0A = TCCR0B = TCCR1A = TCCR1B = TCCR2A = TCCR2B = 0;}
	
#endif

/************************************************************************************
******************************** Controller HP **************************************
************************************************************************************/

#if defined(CONTROLLER_HP) || defined(CONTROLLER_LP)

// CPU Frequency
#define F_CPU 4000000UL

// SPI Pins for SPI library
#define SPI_PORT PORTB
#define SPI_DDR  DDRB
#define SPI_SCK  DDB5
#define SPI_MISO DDB4
#define SPI_MOSI DDB3
#define SPI_SS   DDB2

// CS and CE for NRF24L01
#define RF_CE		DDB1
#define RF_CSN		DDB2 
#define RF_CE_INIT	DDRB |=  (1 << RF_CE);
#define RF_CS_INIT  DDRB |=  (1 << RF_CSN);
#define RF_CE_HIGH  PORTB |=  (1 << RF_CE);
#define RF_CE_LOW   PORTB &= ~(1 << RF_CE);
#define RF_CS_HIGH  PORTB |=  (1 << RF_CSN);
#define RF_CS_LOW   PORTB &= ~(1 << RF_CSN);

// Led Main
#define LED GETBIT(PORTC2)
#define LED_INIT DDRC|=LED
#define LED_ON PORTC|=LED
#define LED_OFF PORTC&=~LED
#define LED_TOGGLE PORTC^=LED
#define LED_STATE (PORTC&LED)
// Led Button 1
#define LED_BUTTON_1 GETBIT(PORTD5)
#define LED_BUTTON_1_INIT DDRD|=LED_BUTTON_1
#define LED_BUTTON_1_ON PORTD|=LED_BUTTON_1
#define LED_BUTTON_1_OFF PORTD&=~LED_BUTTON_1
#define LED_BUTTON_1_TOGGLE PORTD^=LED_BUTTON_1
// Led Button 2
#define LED_BUTTON_2 GETBIT(PORTD6)
#define LED_BUTTON_2_INIT DDRD|=LED_BUTTON_2
#define LED_BUTTON_2_ON PORTD|=LED_BUTTON_2
#define LED_BUTTON_2_OFF PORTD&=~LED_BUTTON_2
#define LED_BUTTON_2_TOGGLE PORTD^=LED_BUTTON_2
// Button 1
#define BUTTON_1 GETBIT(PORTD3)
#define BUTTON_1_INIT {DDRD&=~BUTTON_1; PORTD&=~BUTTON_1;}
#define BUTTON_1_STATE (PIND&BUTTON_1)
// Button 2
#define BUTTON_2 GETBIT(PORTD4)
#define BUTTON_2_INIT {DDRD&=~BUTTON_2; PORTD&=~BUTTON_2;}
#define BUTTON_2_STATE (PIND&BUTTON_2)
// Relay 1
#define RELAY_1 GETBIT(PORTD7)
#define RELAY_1_BIT PORTD7
#define RELAY_1_INIT (DDRD|=RELAY_1)
#define RELAY_1_ON (PORTD|=RELAY_1)
#define RELAY_1_OFF (PORTD&=~RELAY_1)
#define RELAY_1_TOGGLE (PORTD^=RELAY_1)
#define RELAY_1_STATE ((PORTD&RELAY_1) >> RELAY_1_BIT)
// Relay 2
#define RELAY_2 GETBIT(PORTB0)
#define RELAY_2_BIT PORTB0
#define RELAY_2_INIT (DDRB|=RELAY_2)
#define RELAY_2_ON (PORTB|=RELAY_2)
#define RELAY_2_OFF (PORTB&=~RELAY_2)
#define RELAY_2_TOGGLE (PORTB^=RELAY_2)
#define RELAY_2_STATE ((PORTB&RELAY_2) >> RELAY_2_BIT)
// Common Relay
#define RELAY_STATE(sub_ctrl) ((sub_ctrl) ? RELAY_2_STATE : RELAY_1_STATE)
#define RELAY_ON(sub_ctrl) ((sub_ctrl) ? RELAY_2_ON : RELAY_1_ON)
#define RELAY_OFF(sub_ctrl) ((sub_ctrl) ? RELAY_2_OFF : RELAY_1_OFF)
#define RELAY_UPDATE(sub_ctrl, new_state) ((new_state) ? RELAY_ON(sub_ctrl) : RELAY_OFF(sub_ctrl))

// Current Sensor 1
#define CURRENT_SENSOR_1 GETBIT(PORTC0)
#define CURRENT_SENSOR_1_INIT DDRC|=CURRENT_SENSOR_1
// Current Sensor 2
#define CURRENT_SENSOR_2 GETBIT(PORTC1)
#define CURRENT_SENSOR_2_INIT DDRC|=CURRENT_SENSOR_2

// Interrupt 0 - IRQ - Falling Edge
#define INT_0_INIT	{ EICRA |= (1 << ISC01) | (0 << ISC00); EIMSK |= (1 << INT0); }
// Interrupt - Unused
#define INT_1_INIT
// All initializations
#define HW_INIT { LED_INIT; LED_BUTTON_1_INIT; LED_BUTTON_2_INIT; BUTTON_1_INIT; BUTTON_2_INIT; RELAY_1_INIT; RELAY_2_INIT; CURRENT_SENSOR_1_INIT; CURRENT_SENSOR_2_INIT; INT_0_INIT; INT_1_INIT; sei(); }
// Watchdog init - Interrupt and System Reset Mode at 500 [ms]
#define WDT_INIT { cli(); wdt_reset(); MCUSR &= ~(1 << WDRF); WDTCSR = (1 << WDCE) | (1 << WDE); WDTCSR = (1 << WDIE) | (1 << WDE) | (0 << WDP3) | (1 << WDP2) | (0 << WDP1 | (1 << WDP0)); sei(); }
// Watchdog init for Testing - Interrupt and System Reset Mode at 2 [sec]
#define WDT_INIT_TESTING { cli(); wdt_reset(); MCUSR &= ~(1 << WDRF); WDTCSR = (1 << WDCE) | (1 << WDE); WDTCSR = (1 << WDIE) | (1 << WDE) | (0 << WDP3) | (1 << WDP2) | (1 << WDP1 | (1 << WDP0)); sei(); }

// Timer 0 Init - 1024 prescaler, 256us tick, CTC mode at 195 ticks (i.e. 50ms)
#define TIMER_0 TCNT0
#define TIMER_0_TICK_MS	0.256
#define TIMER_0_INIT {TCCR0A = (1 << WGM01); TCCR0B = (1 << CS02) | (0 << CS01) | (1 << CS00); TCNT0 = 0; OCR0A = 195; TCNT0 = 0; TIMSK0 |= (1 << OCIE0A);}
// Timer 1 Init - 64 prescaler, 16us tick
#define TIMER_1 TCNT1
#define TIMER_1_TICK_MS	0.016
#define TIMER_1_INIT {TCCR1B = (0 << CS12) | (1 << CS11) | (1 << CS10); TCNT1 = 0;}
// Timer 2 Init - 1024 prescaler, 256us tick, CTC mode at 39 ticks (i.e. 10ms)
#define TIMER_2 TCNT2
#define TIMER_2_TICK_MS	0.256
#define TIMER_2_INIT { TCCR2A |= (1<<WGM21); TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20); OCR2A = 39; TCNT2 = 0; TIMSK2 |= (1<<OCIE2A);}
	
// Timers common
#define TIMERS_INIT {TIMER_0_INIT; TIMER_1_INIT; TIMER_2_INIT;}
#define TIMERS_STOP_ALL {TCCR0A = TCCR0B = TCCR1A = TCCR1B = TCCR2A = TCCR2B = 0;}
	
#endif

/************************************************************************************
******************************** Controller LP **************************************
************************************************************************************/

#ifdef CONTROLLER_LP

	
#endif

/************************************************************************************
******************************** Hydro **********************************************
************************************************************************************/

#ifdef HYDRO
// CPU Frequency
#define F_CPU 16000000UL

// SPI Pins for SPI library
#define SPI_PORT PORTB
#define SPI_DDR  DDRB
#define SPI_SCK  DDB5
#define SPI_MISO DDB4
#define SPI_MOSI DDB3
#define SPI_SS   DDB2

// CS and CE for NRF24L01
#define RF_CE		DDB1
#define RF_CSN		DDB2
#define RF_CE_INIT	DDRB |=  (1 << RF_CE);
#define RF_CS_INIT  DDRB |=  (1 << RF_CSN);
#define RF_CE_HIGH  PORTB |=  (1 << RF_CE);
#define RF_CE_LOW   PORTB &= ~(1 << RF_CE);
#define RF_CS_HIGH  PORTB |=  (1 << RF_CSN);
#define RF_CS_LOW   PORTB &= ~(1 << RF_CSN);

// Timers
#define TIMER_0 TCNT0
#define TIMER_1 TCNT1
#define TIMER_2 TCNT2
#define TIMER_0_TICK_MS	0.256
#define TIMER_1_TICK_MS	0.016
#define TIMER_2_TICK_MS	0.256
// Led Main
#define LED GETBIT(PORTC2)
#define LED_INIT DDRC|=LED
#define LED_ON PORTC|=LED
#define LED_OFF PORTC&=~LED
#define LED_TOGGLE PORTC^=LED
#define LED_STATE (PORTC&LED)
// Led Button 1
#define LED_BUTTON_1 GETBIT(PORTD5)
#define LED_BUTTON_1_INIT DDRD|=LED_BUTTON_1
#define LED_BUTTON_1_ON PORTD|=LED_BUTTON_1
#define LED_BUTTON_1_OFF PORTD&=~LED_BUTTON_1
#define LED_BUTTON_1_TOGGLE PORTD^=LED_BUTTON_1

// Button 1
#define BUTTON_1 GETBIT(PORTD3)
#define BUTTON_1_INIT {DDRD&=~BUTTON_1; PORTD&=~BUTTON_1;}
#define BUTTON_1_STATE (PIND&BUTTON_1)
// Button 2
#define BUTTON_2 GETBIT(PORTD4)
#define BUTTON_2_INIT {DDRD&=~BUTTON_2; PORTD&=~BUTTON_2;}
#define BUTTON_2_STATE (PIND&BUTTON_2)

// Pump 1
#define PUMP_1 GETBIT(PORTD6)
#define PUMP_1_INIT DDRD|=PUMP_1
#define PUMP_1_ON PORTD|=PUMP_1
#define PUMP_1_OFF PORTD&=~PUMP_1
#define PUMP_1_TOGGLE PORTD^=PUMP_1
#define PUMP_1_STATE (PORTD&PUMP_1)

// Interrupt IRQ
#define INT_0_INIT	{ EICRA |= _BV(ISC01); EIMSK |= _BV(INT0); }
// Interrupt - Unused
#define INT_1_INIT
// All initializations
#define HW_INIT { LED_INIT; LED_BUTTON_1_INIT; BUTTON_1_INIT; BUTTON_2_INIT; PUMP_1_INIT; INT_0_INIT; INT_1_INIT; sei(); }
// Watchdog init // Interrupt and System Reset Mode at 500ms
#define WDT_INIT { cli(); MCUSR &= ~(1<<WDRF); WDTCSR = (1<<WDCE)|(1<<WDE); WDTCSR = (1<<WDIE)|(1<<WDE)|(0<<WDP3)|(1<<WDP2)|(0<<WDP1|(1<<WDP0)); sei(); }
#endif



#endif

/*** end of file ***/