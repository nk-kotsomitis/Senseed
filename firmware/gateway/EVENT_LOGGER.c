#include "EVENT_LOGGER.h"

// Static
static Event_logger_t g_event_logger = {0};
static const Event_logger_event_t EVENT_ZERO = {0};
	
static void set_ovf_event(uint16_t ovf_idx);
static void remove_element(uint16_t idx);
static void set_ovf_event(uint16_t ovf_idx);
static char *type_to_str(Evt_type_t type);
static char *module_to_str(Evt_module_t module);

void EVENT_LOGGER_init()
{
	g_event_logger.head = g_event_logger.tail = g_event_logger.temp_tail = g_event_logger.free_elements = EVT_BUFFER_SIZE-1;
		
	for (uint16_t row = 0; row < EVT_BUFFER_SIZE; row++)
	{
		g_event_logger.buffer[row] = EVENT_ZERO;
	}
}

void EVENT_LOGGER(time_t timestamp, Evt_type_t type, Evt_module_t module, Evt_id_t number, Evt_error_code_t error_code)
{
	// Add to queue
	g_event_logger.buffer[g_event_logger.head].timestamp = timestamp;
	g_event_logger.buffer[g_event_logger.head].type	= type;
	g_event_logger.buffer[g_event_logger.head].module = module;
	g_event_logger.buffer[g_event_logger.head].id = number;
	g_event_logger.buffer[g_event_logger.head].error_code = error_code;
			
	// Update number of free elements
	if (g_event_logger.free_elements > 0)
		g_event_logger.free_elements -= 1;
		
	// Update ring buffer
	g_event_logger.head++;
	g_event_logger.head &= EVT_BUFFER_MASK;
		
		
	// Check for Overflow
	if (g_event_logger.head == g_event_logger.tail)
	{		
		// Delete the last element
		remove_element(g_event_logger.tail);
		
		// Add event logger error (Lost events)
		uint16_t idx = g_event_logger.tail;
		idx++;
		idx &= EVT_BUFFER_MASK;
		set_ovf_event(idx);
	}
		
		
//#ifdef DEBUG_PRINT
	char s[20];
	print("EVT ");
	struct tm* tm_info;
	tm_info = localtime(&timestamp);
	strftime(s, 26, "%Y-%m-%d %H:%M:%S", tm_info);
	print(s);
	print(module_to_str(module));
	print(type_to_str(type));
	//print(utoa(number, s, 10));
    sprintf(s, "%d", number);
    print(s);
	print(" ");
	// print(itoa(error_code, s, 10));
    sprintf(s, "%d", error_code);
    print(s);
	print("\r\n");
//#endif

}

bool EVENT_LOGGER_get(time_t *timestamp, Evt_type_t *type, Evt_module_t *module, Evt_id_t *id, Evt_error_code_t *error_code)
{
	if (g_event_logger.buffer[g_event_logger.temp_tail].id)
	{
		*id = g_event_logger.buffer[g_event_logger.temp_tail].id;
		*timestamp = g_event_logger.buffer[g_event_logger.temp_tail].timestamp;
		*type = g_event_logger.buffer[g_event_logger.temp_tail].type;
		*module = g_event_logger.buffer[g_event_logger.temp_tail].module;
		*error_code = g_event_logger.buffer[g_event_logger.temp_tail].error_code;
		
		g_event_logger.temp_tail++;
		g_event_logger.temp_tail &= EVT_BUFFER_MASK;
		
		return true;
	}
	
	return false;
}

void EVENT_LOGGER_send_ok()
{
	for (uint16_t idx = g_event_logger.tail; idx != g_event_logger.temp_tail; idx++, idx &= EVT_BUFFER_MASK)
	{
		// Remove element
		remove_element(idx);
	}
	g_event_logger.tail = g_event_logger.temp_tail;
}

void EVENT_LOGGER_send_fail()
{
	g_event_logger.temp_tail = g_event_logger.tail;
}

bool EVENT_LOGGER_is_empty()
{
	
	if (g_event_logger.free_elements >= EVT_BUFFER_SIZE-1)
	{
		return true;
	}
	
	return false;
}

static void set_ovf_event(uint16_t ovf_idx)
{
	//g_event_logger.buffer[g_event_logger.head].timestamp;
	g_event_logger.buffer[ovf_idx].id = EVT_ID;
	g_event_logger.buffer[ovf_idx].type	= EVT_ERROR;
	g_event_logger.buffer[ovf_idx].module = MODULE_EVT;
	g_event_logger.buffer[ovf_idx].error_code = EVT_ERROR_CODE_LOST_EVTS;
	g_event_logger.buffer[ovf_idx].lost_events = true;
}

static void remove_element(uint16_t idx)
{
	// Delete element
	g_event_logger.buffer[idx] = EVENT_ZERO;
	
	// Increase number of empty elements
	if (g_event_logger.free_elements < EVT_BUFFER_SIZE)
		g_event_logger.free_elements += 1;
	
	// Increase tail
	g_event_logger.tail++;
	g_event_logger.tail &= EVT_BUFFER_MASK;
}

static char *type_to_str(Evt_type_t type)
{
	char *type_str = NULL;
	
	switch (type)
	{
		case EVT_DEBUG:
			type_str = " [ DEBUG ] ";
			break;
		
		case EVT_INFO:
			type_str = " [ INFO ] ";
			break;
		
		case EVT_WARN:
			type_str = " [ WARN ] ";
			break;
		
		case EVT_ERROR:
			type_str = " [ ERROR ] ";
			break;
		
		default:
			break;
	}
	
	return type_str;
}

static char *module_to_str(Evt_module_t module)
{
	char *module_str = NULL;
	
	switch (module)
	{
		case MODULE_MAIN:
			module_str = " MAIN ";
			break;
			
		case MODULE_THINGS:
			module_str = " THINGS ";
			break;
			
		case MODULE_NRF:
			module_str = " NRF ";
			break;
			
		case MODULE_MQTT:
			module_str = " MQTT ";
			break;
		default:
			break;
	}
	
	return module_str;
}