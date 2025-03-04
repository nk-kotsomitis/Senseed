#ifndef EVENT_LOGGER_H
#define EVENT_LOGGER_H

#include <stdio.h>
#include "CONSOLE_log.h"

#define EVT_BUFFER_SIZE 64
#define EVT_BUFFER_MASK EVT_BUFFER_SIZE-1
#define EVT_ID 1
#define EVT_ERROR_CODE_LOST_EVTS 1

typedef int16_t Evt_error_code_t;
typedef uint8_t Evt_id_t;

typedef enum 
{
	EVT_DEBUG = 1,
	EVT_INFO, 
	EVT_WARN, 
	EVT_ERROR
	
}Evt_type_t;

typedef enum
{
	MODULE_EVT = 1,
	MODULE_MAIN,
	MODULE_THINGS,
	MODULE_NRF, 
	MODULE_MQTT
}Evt_module_t;
	
typedef struct
{
	Evt_id_t id;
	time_t timestamp;
	Evt_module_t module;
	Evt_type_t type;
	Evt_error_code_t error_code;
	bool lost_events;
}Event_logger_event_t;

typedef struct
{
	uint16_t free_elements;
	uint16_t head;
	uint16_t tail;
	uint16_t temp_tail;
	Event_logger_event_t buffer[EVT_BUFFER_SIZE];
	
}Event_logger_t;

// Library initialization
void EVENT_LOGGER_init();
// Add event
void EVENT_LOGGER(time_t timestamp, Evt_type_t type, Evt_module_t module, Evt_id_t number, Evt_error_code_t error_code);
// Get event
bool EVENT_LOGGER_get(time_t *timestamp, Evt_type_t *type, Evt_module_t *module, Evt_id_t *id, Evt_error_code_t *error_code);
// Events transmitted successfully
void EVENT_LOGGER_send_ok();
// Events transmission failed
void EVENT_LOGGER_send_fail();
// Returns true if buffer is empty, false otherwise
bool EVENT_LOGGER_is_empty();

#endif