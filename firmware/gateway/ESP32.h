#ifndef ESP32_H
#define ESP32_H

/* ----------------------------------------- 
	Library Configuration
	----------------------------------------
	
	// Dependencies:
	UART - ESP_sendbuf(), ESP_sendbuf_P(), ESP_sendc()
	Timers - Timeout byte (100ms) and Smart Config timeout (4s)
	WDT reset - It is used for long blocking functions (i.e. waiting for AT COMMAND response)
	
	// Exports
	ESP_tcp_status()
	ESP_wifi_status()
	ESP_wifi_signal_status()
	ESP_smart_config_status()
	
	// Notes
	The buffer passed at the receiving functions (ESP_tcp_receive_data and ESP_tcp_receive_data_poll) MUST be equal to ESP_BUFFER_IPD_SIZE !!!
	
	// TODOs
	- ESP_wifi_deserialize_tcp_status() Update this function and the Tcp_status_t g_tcp_status IMPORTANT!
	
*/	
#define DEBUG_PRINT

// Timers
#define ESP_TIMER //TIMER_1
#define ESP_TIMER_TICK_MS //TIMER_1_TICK_MS

// Timeouts
#define ESP_TIMEOUT_BYTE_MS 100
#define ESP_TIMEOUT_SMART_CONFIG_MS 4000 // Smart config timeout should be lower than timer's max value

// WDT reset (for long blocking functions)
#define ESP_WDT_RESET // wdt_reset() TBC

// Constants
#define ESP_RAM_LIMIT 25000

// Buffers (one for AT COMMANDS and one for IPD data) - Note: IPD Buffer should be >= UART Rx Buffer
#define ESP_BUFFER_SIZE	1024
#define ESP_BUFFER_IPD_SIZE	2048

// Maximum number of digits for IPD length that are accepted (i.e. 1 - 2048). This is used in case of an error
#define ESP_BUFFER_IPD_NUMBER_MAX_LEN 4

/* ----------------------------------------- */
#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "UART_SAMD51.h"
//#include "CONSOLE_log.h"

#define ESP_TIMEOUT_BYTE_TICKS (ESP_TIMEOUT_BYTE_MS/ESP_TIMER_TICK_MS - 1)
#define ESP_TIMEOUT_SMART_CONFIG_TICKS (ESP_TIMEOUT_SMART_CONFIG_MS/ESP_TIMER_TICK_MS - 1)

typedef struct
{
	char general[ESP_BUFFER_SIZE];
	uint16_t general_len;
	bool general_buffer_full;
	
	uint8_t ipd[ESP_BUFFER_IPD_SIZE];
	uint16_t ipd_len;
	bool ipd_buffer_full;
		
} Buffers_t;

typedef enum
{
	SMART_CONFIG_OK = 0,
	SMART_CONFIG_ERROR,
	SMART_CONFIG_START,
	SMART_CONFIG_TIMEOUT,
	SMART_CONFIG_GOT_RESPONSE,
} Smart_config_t;

typedef enum  
{
	TCP_CLOSED = 0, 
	TCP_CLOSED_AP_DISCONNECTED,
	TCP_CLOSED_AP_IP_OK,
	TCP_CLOSED_TCP_DISCONNECTED,
	TCP_OPEN,
} Tcp_status_t;
		
typedef enum
{
	WIFI_DISCONNECTED, 
	WIFI_CONNECTED, 
	WIFI_GOT_IP
} Wifi_status_t;

typedef enum
{
	WIFI_UNKNOWN,
	WIFI_NO_SIGNAL, 
	WIFI_SIGNAL_UNUSABLE, 
	WIFI_SIGNAL_NOT_GOOD, 
	WIFI_SIGNAL_OK,
	WIFI_SIGNAL_VERY_GOOD,
	WIFI_SIGNAL_AMAZING
} Wifi_signal_status_t;

typedef enum
{
	BLUFI_DISABLED = 0, 
	BLUFI_ENABLED = 1
} BluFi_status_t;

typedef enum
{
	RX_BYTES_AT_COMMAND,
	RX_BYTES_IPD_HEADER,
	RX_BYTES_IPD_DATA,
	RX_BYTES_IPD_DATA_DISCARD
} Rx_bytes_state_t;

// ************************************* Generic *****************************************

// Debug lib
void ESP_debug();

// Initialization
void ESP_init();
// Self-test
int8_t ESP_self_test();
// Test AT
int8_t ESP_test_at();
// Reset 
int8_t ESP_reset(uint8_t startup);
// Get RAM
int8_t ESP_get_RAM(uint16_t *ram);
// Test command
void ESP_test_command(char *command);
// ************************************* TCP *********************************************

// Get TCP status
int8_t ESP_tcp_update_status();
// Open TCP connection
int8_t ESP_tcp_open(char *ip, uint16_t port);
// Send TCP data
int8_t ESP_tcp_send_data(uint8_t *data, uint16_t length);
// Get Rx data from Buffer
int8_t ESP_tcp_receive_data(uint8_t **data, uint16_t *length);
// Get Rx data from SP buffer
int8_t ESP_tcp_cip_receive_data(); // Not used
// Flush data
int8_t ESP_tcp_flush_data();
// Close TCP connection
int8_t ESP_tcp_close();
// Get TCP status
Tcp_status_t ESP_tcp_status();
// Configure and get the SNTP time
int8_t ESP_tcp_sntp_time();
// ************************************* Wifi ********************************************


// Set the WiFi to station mode
int8_t ESP_wifi_set_station_mode();
// Connect to WiFi
int8_t ESP_wifi_connect(char *ssid, char *password);
// Disconnect WiFi
int8_t ESP_wifi_disconnect();
// Get WiFi status
int8_t ESP_wifi_update_status();
// Get WiFi status
Wifi_status_t ESP_wifi_status();
// Get WiFi signal
Wifi_signal_status_t ESP_wifi_signal_status();

// ************************************* BluFi ********************************************

// Start BluFi
int8_t ESP_blufi_start();
// Stop BluFi
int8_t ESP_blufi_stop();
// Send data through BluFi
int8_t ESP_blufi_send();
// Set BluFi status
void ESP_blufi_set_status(BluFi_status_t blufi_status);
// Get BluFi status
BluFi_status_t ESP_blufi_status();

// ************************************** AT COMMANDS **********************************************

// At command timeouts
#define ESP_TIMEOUT_AT_COMMAND		10
#define ESP_TIMEOUT_RESET_READY		5
#define ESP_TIMEOUT_RESET_WIFI		10
#define ESP_TIMEOUT_ST_CIPSTART		20
#define ESP_TIMEOUT_SEND_OK			30
#define ESP_TIMEOUT_AT_SMART_CON_1	62
#define ESP_TIMEOUT_AT_SMART_CON_2	40
#define ESP_TIMEOUT_AT_CWLAP		6
#define ESP_TIMEOUT_CONNECT_WIFI	20
#define ESP_TIMEOUT_INIT_RESPONSE	20

// At command wait for
#define ESP_WAIT_FOR_GENERAL		1
#define ESP_WAIT_FOR_TX_READY		2
#define ESP_WAIT_FOR_TX_RES			3
#define ESP_WAIT_FOR_RESET			4
#define ESP_WAIT_FOR_WIFI			5

// At command return code
#define ESP_RES_SNTP_TIME           24
#define ESP_RES_IPD					23
#define ESP_RES_CIFSR_STATION_IP	22
#define ESP_RES_SEND_CHAR			21
#define ESP_RES_CIPSTATUS			20
#define ESP_RES_STATUS				19
#define ESP_RES_SMART_CON_FAILED	18
#define ESP_RES_SMART_CON_CONNECTED	17
#define ESP_RES_INIT_RESPONSE		16
#define ESP_RES_NO_AP				15
#define ESP_RES_CWJAP				14
#define ESP_RES_CWJAP_DEF			13
#define ESP_RES_SYSRAM				12
#define ESP_RES_CWLAP				11
#define ESP_RES_WIFI_DISCONNECTED	10
#define ESP_RES_WIFI_GOT_IP			9
#define ESP_RES_WIFI_CONNECTED		8
#define ESP_RES_READY				7
#define ESP_RES_CLOSED_TCP_CON		6
#define ESP_RES_SEND_OK				5
#define ESP_RES_CLOSED				4
#define ESP_RES_ALREADY				3
#define ESP_RES_CONNECT				2
#define ESP_RES_OK					1
#define ESP_RES_NO_ERROR			0	// Neutral
#define ESP_RES_NOT_FOUND			-1
#define ESP_RES_ERROR				-2
#define ESP_RES_BUSY_P				-3
#define ESP_RES_BUSY_S				-4
#define ESP_RES_SEND_FAIL			-5
#define ESP_RES_GEN_UNKNOWN			-6
#define ESP_RES_UART_BUFFER_OVF		-7
#define ESP_RES_GEN_BUFFER_OVF		-8
#define ESP_RES_GEN_PARTIAL			-9
#define ESP_RES_IPD_BUFFER_OVF		-10
#define ESP_RES_IPD_CORRUPTED		-11
#define ESP_RES_IPD_PARTIAL			-12

#define NUM_OF_AT_COMMANDS		45
#define NUM_OF_AT_COMMANDS_RES	27

const char* const MEM_AT_COMMANDS[NUM_OF_AT_COMMANDS];
const char* const MEM_AT_COMMANDS_RES[NUM_OF_AT_COMMANDS_RES];

typedef enum
{
	AT_TEST, AT_RST, AT_SET_SNTP, AT_CIP_SSL_CONF, AT_CIPSTART_1_SSL,
	AT_CIPSTART_1, AT_CIPSTART_2, AT_CIPSTART_3, AT_CIPSTATUS, AT_CIPMUX_1,
	AT_CIPSERVERMAXCONN_1, AT_CIPSERVER_1_80, AT_CIPSEND, AT_CIPCLOSE_, AT_CIPCLOSE,
	AT_CWMODE_CUR_1, AT_CWMODE_CUR_2, AT_CWMODE_CUR_3, AT_CWMODE_DEF_1, AT_WPS_0,
	AT_WPS_1, AT_SYSRAM, AT_CWLAP, AT_CWQAP, AT_SMART_CON_START,
	AT_SMART_CON_STOP, AT_MDNS, AT_CWJAP_DEF_Q, AT_CWJAP_DEF_1, AT_CWJAP_DEF_2,
	AT_CWJAP_DEF_3,	AT_CWSAP_DEF_Q, AT_CWSAP_DEF_1, AT_CWSAP_DEF_2, AT_CWSAP_DEF_3,
	AT_CIFSR, AT_CIPRECVDATA, AT_BLUFI_START, AT_BLUFI_STOP, AT_BLUFI_SEND, 
    AT_SNTP_CONFIG, AT_SNTP_TIME, AT_COMMA, AT_EQUAL, AT_CAR_RET_NEW_LINE
} At_command_t;

typedef enum
{
	AT_RES_OK, AT_RES_ERROR, AT_RES_BUSY_P, AT_RES_BUSY_S, AT_RES_READY,
	AT_RES_SEND_CHAR, AT_RES_SEND_OK, AT_RES_SEND_FAIL, AT_RES_FAIL, AT_RES_CONNECT, AT_RES_ALREADY,
	AT_RES_CLOSED, AT_RES_IPD, AT_RES_CWLAP, AT_RES_CWJAP_DEF, AT_RES_CWJAP,
	AT_RES_NO_AP, AT_RES_SYSRAM, AT_RES_CIPSTATUS, AT_RES_STATUS, AT_RES_WIFI_CONNECTED,
	AT_RES_WIFI_DISCONNECTED, AT_RES_WIFI_GOT_IP, AT_RES_CIFSR_STATION_IP, 
    AT_RES_SNTP_TIME, AT_RES_SMART_CON_CONNECTED, AT_RES_SMART_CON_FAILED
} At_command_res_t;

#endif