/*
* @file DataTypes.h
*
* @brief Data Types
*
*/

#ifndef DATA_TYPES_H
#define DATA_TYPES_H

#include <avr/io.h>
#include <time.h>
#include <stdbool.h>
#include "Constants.h"

//typedef struct NRF_ADDR_t { uint8_t address[5]; } NRF_ADDR_t;
#define ZERO					0
#define NRF_INVALID_8			0xFF
#define NRF_INVALID_16			0xFFFF
#define NRF_INVALID_DOUBLE		0xFFFF
#define INVALID_PACKET_ID		-1
#define NUM_OF_SUB_CONTROLLERS	2

#define GW_QUEUE_SIZE 32
#define GW_QUEUE_MASK GW_QUEUE_SIZE-1

// Gateway's ID
#define ID_GATEWAY 1

typedef uint16_t ID_t;
typedef uint8_t Thing_param_t;
typedef uint8_t Thing_room_t;
typedef int8_t Packet_id_t;
typedef uint8_t Room_id_t;
typedef uint8_t Rf_address_t[5];
typedef uint8_t Rf_address_lsb_t[1];
//typedef uint8_t* Rf_address_p;

#define RF_ADDRESS_BYTES 5

typedef enum
{
	TYPE_NONE = 0,
	TYPE_GATEWAY,
	TYPE_SOIL_SENSOR,
	TYPE_ENV_SENSOR,
	TYPE_CONTROLLER_HP,
	TYPE_CONTROLLER_LP,
	TYPE_HYDRO
} TYPE_t;

typedef enum
{
	MQTT_TOPIC_UNDEFINED = 0,
	MQTT_TOPIC_GATEWAY,
	MQTT_TOPIC_SOIL_SENSOR,
	MQTT_TOPIC_ENV_SENSOR,
	MQTT_TOPIC_CONTROLLER_HP,
	MQTT_TOPIC_CONTROLLER_LP,
	MQTT_TOPIC_HYDRO
} TOPIC_t;

typedef enum
{
	THING_STATUS_INIT = -1,	// Status is at initialization (unknown)
	THING_STATUS_WARN,		// Status is NOT OK. Warning!
	THING_STATUS_OK,		// Status is OK
	THING_STATUS_OK_DB		// Status is OK. The current values should be saved at DB.
	
} Thing_Status_t;

typedef enum
{
	RES_NONE = 0,
	RES_STRUCT_LITE_FOUND,
	RES_STRUCT_LITE_NOT_FOUND_NEW,
	RES_STRUCT_LITE_NOT_FOUND_FULL,
	RES_STRUCT_FOUND,
	RES_STRUCT_NOT_FOUND_NEW,
	RES_STRUCT_NOT_FOUND_FULL
} Things_struct_res_t;

typedef enum
{
	REQUEST_NONE = 0,
	REQUEST_INIT_THINGS,		// Request all things' ID, Type, Param, Room
	REQUEST_INIT_THINGS_VALUES, // All things will be added to MQTT queue
	REQUEST_INIT_ROOM_SETTINGS, // Request all room settings
	REQUEST_SYNC_TIME,
	REQUEST_THING_ADD,
	REQUEST_THING_ADD_AND_UPDATE_PARAM,
	REQUEST_THING_ADD_AND_UPDATE_ROOM,
	REQUEST_THING_ADD_AND_UPDATE_ALL,
	REQUEST_THING_UPDATE_PARAM,
	REQUEST_THING_UPDATE_ROOM,
	REQUEST_THING_UPDATE_ALL,
	REQUEST_THING_DELETE,
	REQUEST_ROOM_SETTINGS_UPDATE,
	REQUEST_ROOM_SETTINGS_CLEAR,
	REQUEST_SERVER_SETTINGS_SERVER_IP_UPDATE,
	REQUEST_FOR_TESTING
} Things_Gw_Request_t;

typedef enum
{
	RESPONSE_NONE = 0,
	RESPONSE_OK,
	RESPONSE_ERROR
} Things_Gw_Response_t;

typedef struct  
{
	Things_Gw_Request_t request[GW_QUEUE_SIZE];
	Things_Gw_Response_t response[GW_QUEUE_SIZE];
	Packet_id_t packet_id[GW_QUEUE_SIZE];	
	uint8_t head;
	uint8_t tail;
	uint8_t num_of_requests;
} Things_Gw_queue_t;

typedef struct
{	
	// Flags
	bool server_change_flag;
	
	// Counter
	uint8_t new_server_connection_retries;
	
	// Server Settings
	char server_ip[MAX_NUM_OF_CHARS_IPV4];
	char server_ip_new[MAX_NUM_OF_CHARS_IPV4];
	uint16_t server_port;
	char client_id[MAX_NUM_OF_CHARS_CLIENT_ID];
	char virtual_host[MAX_NUM_OF_CHARS_VHOST];
	char username[MAX_NUM_OF_CHARS_USER];
	char password[MAX_NUM_OF_CHARS_PASS];
	
} Server_t;

typedef struct  
{
	time_t time_now;
	Rf_address_t rx_pipe_1;
	Rf_address_lsb_t rx_pipe_2;
	Rf_address_lsb_t rx_pipe_3;
	Rf_address_lsb_t rx_pipe_4;
	Rf_address_lsb_t rx_pipe_5;
	Things_Gw_queue_t queue;
	Server_t server_settings;
	ID_t deleted_thing;
	
	// Flags
	bool tx_flag;
	bool things_rf_warning;
	
} Gateway_t;

typedef struct  
{
	ID_t id;
	TYPE_t type;
	Thing_param_t parameter; // NOTE: For controllers: device Type. For sensors: eco
	Thing_room_t room;
	
	// Thing_Status_t status;
	// uint8_t address[5];
	
} Thing_t;

typedef enum
{
	SCHEDULER_MANUAL,
	SCHEDULER_TIMER,
	SCHEDULER_AUTO
} Scheduler_t;

typedef struct  
{
	uint8_t measurement;
	
	uint8_t enabled; // Enable/Disable Automation
	uint8_t threshold_low;
	uint8_t threshold_high;
	
	uint8_t active; // Active means watering is in progress
	uint8_t measurement_prev;

} Soil_channel_t;

typedef struct
{	
	ID_t id;
	Rf_address_t address;
	Rf_address_t address_gw;
	time_t timestamp;		// Timestamp of measurement (i.e. RF received packet)
	Thing_Status_t status;
	uint8_t room;
	
	bool tx_flag; // TBC
	Packet_id_t packet_id_rx;
	Packet_id_t packet_id_tx;
	
	uint8_t hw_change;
	uint8_t sw_change;
	uint8_t blink;
	
	uint16_t battery_mv;
	uint8_t battery_eco;
	uint8_t battery_eco_to_update;
	
	uint8_t is_active;	
	Soil_channel_t channel_1;
	Soil_channel_t channel_2;
	Soil_channel_t channel_3;
	Soil_channel_t channel_4;
		
} Soil_Sensor_t;

typedef struct
{
	ID_t id;
	Rf_address_t address;
	Rf_address_t address_gw;
	time_t timestamp;
	Thing_Status_t status;
	uint8_t room;
	
	bool tx_flag; // TBC
	Packet_id_t packet_id_rx;
	Packet_id_t packet_id_tx;
	
	uint8_t hw_change;
	uint8_t sw_change;
	uint8_t blink;
	
	uint16_t battery_mv;
	uint8_t battery_eco;
	uint8_t battery_eco_to_update;
	
	double temperature;
	uint8_t humidity;
	uint16_t co2;
	uint8_t spectrum; // TBD
	
} Env_Sensor_t;

typedef struct
{
	uint16_t measurement;
	time_t timestamp;
	bool is_ready;
} Consumption_t;

typedef struct
{
	ID_t id;
	Rf_address_t address;
	Rf_address_t address_gw;
	time_t clock;
	time_t clock_gw;
	time_t timestamp;
	time_t timestamp_DB;
	Thing_Status_t status;
	uint8_t room;
	
	bool tx_flag; // TBC
	Packet_id_t packet_id_rx;
	Packet_id_t packet_id_tx;
	
	uint8_t hw_change;
	uint8_t sw_change;
	uint8_t blink;
	
	uint8_t type[NUM_OF_SUB_CONTROLLERS];
	Scheduler_t scheduler[NUM_OF_SUB_CONTROLLERS];
	uint8_t state[NUM_OF_SUB_CONTROLLERS];
	uint8_t auto_cmd[NUM_OF_SUB_CONTROLLERS];
	uint8_t days[NUM_OF_SUB_CONTROLLERS];
	double timer_on[NUM_OF_SUB_CONTROLLERS];
	double timer_off[NUM_OF_SUB_CONTROLLERS];
	Consumption_t consumption[NUM_OF_SUB_CONTROLLERS];
	
} Controller_HP_t;

typedef struct
{
	ID_t id;
	Rf_address_t address;
	Rf_address_t address_gw;
	time_t clock;
	time_t timestamp;
	time_t timestamp_DB;
	Thing_Status_t status;
	uint8_t room;
	
	bool tx_flag; // TBC
	Packet_id_t packet_id_rx;
	Packet_id_t packet_id_tx;
	
	uint8_t hw_change;
	uint8_t sw_change;
	uint8_t blink;
	
	uint8_t type[NUM_OF_SUB_CONTROLLERS];
	Scheduler_t scheduler[NUM_OF_SUB_CONTROLLERS];
	uint8_t state[NUM_OF_SUB_CONTROLLERS];
	uint8_t auto_cmd[NUM_OF_SUB_CONTROLLERS];
	uint8_t days[NUM_OF_SUB_CONTROLLERS];
	double timer_on[NUM_OF_SUB_CONTROLLERS];
	double timer_off[NUM_OF_SUB_CONTROLLERS];
	Consumption_t consumption[NUM_OF_SUB_CONTROLLERS];
		
} Controller_LP_t;

typedef struct
{
	ID_t id;
	Rf_address_t address;
	Rf_address_t address_gw;
	time_t clock;
	time_t timestamp;
	time_t timestamp_DB;
	Thing_Status_t status;
	uint8_t room;
	
	bool tx_flag; // TBC
	Packet_id_t packet_id_rx;
	Packet_id_t packet_id_tx;

	uint8_t hw_change;
	uint8_t sw_change;
	uint8_t blink;
	
	// TBD	
} Hydro_t;

typedef struct
{
	Room_id_t id;
	uint8_t value1;
	uint8_t value2;
	uint8_t value3;
	uint8_t value4;
	
} Room_t;

typedef enum
{
	BUTTON_UNPRESSED = 0,
	BUTTON_IN_PROGRESS,
	BUTTON_SHORT_PRESS,
	BUTTON_LONG_PRESS
} Button_state_t;

typedef enum
{
	LED_STATE_OFF = 0,
	LED_STATE_ON,
	LED_STATE_BLINK_1S,
	LED_STATE_BLINK_500MS,
	LED_STATE_FLASH
} Led_state_t;

typedef enum
{
	LED_FLASH_NONE = 0,
	LED_FLASH_500_MS = 10,
	LED_FLASH_1_SEC = 20,
} Led_flash_t;


typedef enum
{
	CONSUMPTION_STATE_DISABLED = 0,
	CONSUMPTION_STATE_IDLE,
	CONSUMPTION_STATE_IN_PROGRESS,
	CONSUMPTION_STATE_COMPLETED,
} Consumption_state_t;

typedef struct
{
	Consumption_state_t state;	
	uint16_t vpp_raw_max;
	uint16_t vpp_raw_min;	
	uint16_t n_samples;
	const uint8_t adc_pin;
		
} Consumption_measurements_t;

#endif

/*** end of file ***/