#ifndef MQTT_LIB_H
#define MQTT_LIB_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#include "MQTT_paho_lib/MQTTConnect.h"
#include "MQTT_paho_lib/MQTTFormat.h"
#include "MQTT_paho_lib/MQTTPacket.h"
#include "MQTT_paho_lib/MQTTPublish.h"
#include "MQTT_paho_lib/MQTTSubscribe.h"
#include "MQTT_paho_lib/MQTTUnsubscribe.h"
#include "MQTT_paho_lib/StackTrace.h"

#include "DataTypes.h"
#include "ESP32.h"
#include "THINGS_lib.h"
#include "Serialization_Deserialization.h"
#include "CONSOLE_log.h"
//#include "MCP79410.h"
#include "EVENT_LOGGER.h"

#define TESTING_INTEGRATION 1

/* ----------------------------------------- 
	Library Configuration
	----------------------------------------
	// Version
	- RabbitMQ MQTT protocol v3.1.1
	- Paho Library MQTT protocol v3
	
	// Dependencies
	- RTC clock
	- Data types of ID, TOPICS
	- Gateway ID (integer, not string)
	- LED on/off and blink frequency (Optional)
	
	// Exports
	MQTT_Broker_state()
	MQTT_Broker_subscription_state()
	
	// Notes
	General concepts of library: 
		- ID: the identification number of sender/receiver (i.e. gateway or thing). This number is serialized/de-serialized at MQTT packet's payload
		- Topic: The MQTT topic. It is the 'type' of ID
		- State: The state of MQTT packet
	
	// TODOs
		- IP/Port, Virtual Host, UserName and Password
		- g_wifi_credentials_flag
		- g_time_epoch_y2k // Note: The clock will be shared with NRF? 
		- Will Message, Retain, Clean Session
		- Statistics
		
	// TODOs - TBC
		- IMPORTANT! What to do to avoid buffer overflows
			1. Decrease the rabbitMQ queue size to e.g. 10. So, when GW connects, only 10 messages will be received.
			2. ?
		- After a PUBACK timeout (of a Tx PUBLISH), because of e.g. buffer overflow, what to do? Re-send PUBLISH with DUP?
*/	

// Timer
time_t g_time_epoch_y2k;

// IDs and Topics
#define MQTT_ID_PROTOCOL 2 // This can NOT be 0, 1, 10, 11, 12,...
#define MQTT_ID_GATEWAY ID_GATEWAY
typedef ID_t MQTT_Id_t;
typedef TOPIC_t MQTT_Topic_t;
typedef msgTypes_t MQTT_Packet_t;
	
// LED
#define MQTT_CONNECTION_LED_ON	// INTERNET_LED_ON
#define MQTT_CONNECTION_LED_BLINK // INTERNET_LED_BLINK_MODE
#define MQTT_CONNECTION_LED_SLOW // INTERNET_LED_SLOW	
#define MQTT_CONNECTION_LED_NORMAL // INTERNET_LED_NORMAL
#define MQTT_CONNECTION_LED_FAST // INTERNET_LED_FAST
#define MQTT_CONNECTION_LED_SUPER_FAST // INTERNET_LED_SUPER_FAST

// Protocol
#define MQTT_KEEP_ALIVE_INTERVAL 60
#define MQTT_PING_INTERVAL 20
#define MQTT_CLEAN_SESSION 0
#define MQTT_RETAINED_MESSAGES 0
#define MQTT_NUM_OF_TOPICS_MAX 10
#define MQTT_TOPIC_LENGTH_MAX 20
#define MQTT_WILL_MESSAGE_LENGTH_MAX 50
#define MQTT_INPUT_PARAMS_LENGTH_MAX 50

// Buffers
#define MQTT_QUEUE_SIZE 128
#define MQTT_TX_BUFFER_SIZE 2048 // NOTE: max value 32767 because the length variable is int16
#define MQTT_RX_BUFFER_SIZE 1024 // This buffer MUST be >= ESP_BUFFER_IPD_SIZE at ESP library.

/* ----------------------------------------- */

// Protocol Fixed Values - DO NOT CHANGE
#define MQTT_PACKET_LENGTH_MIN 2
#define MQTT_PACKET_LENGTH_FIXED 4

// Timeouts (in seconds? TBC)
#define MQTT_TIMEOUT_KEEP_DURATION 10 // TBC
#define MQTT_TIMEOUT_CONNACK 20
#define MQTT_TIMEOUT_PINGRES 5
#define MQTT_TIMEOUT_SUBACK 10
#define MQTT_TIMEOUT_UNSUBACK 5
#define MQTT_TIMEOUT_RX_PUBACK 10
#define MQTT_TIMEOUT_DISCONNECT 10 // Time to wait for CLOSED after transmitting DISCONNECT

// Other
#define MQTT_QUEUE_MASK MQTT_QUEUE_SIZE-1
#define MQTT_PUBLISH_PAYLOAD_OFFSET 32
#define RX_DIRECTION 0
#define TX_DIRECTION 1
#define REMAINING_LENGTH_MULTI_MAX 2097152 //128*128*128

typedef enum
{
	MQTT_BROKER_NO_CONNECTION,
	MQTT_BROKER_CONNECTION_OK,
	MQTT_BROKER_CONNECTION_OK_WAIT_FOR_RETAINED_MESSAGES,
	MQTT_BROKER_CONNECTION_REQUEST_IN_PROGRESS,
	MQTT_BROKER_CONNECTION_REQUESTED,
	MQTT_BROKER_ERROR_TIMEOUT,
	MQTT_BROKER_ERROR_PROTOCOL_VERSION,
	MQTT_BROKER_ERROR_IDENTIFIER_REJECTED,
	MQTT_BROKER_ERROR_SERVER_UNAVAILABLE,
	MQTT_BROKER_ERROR_BAD_USER_OR_PASS,
	MQTT_BROKER_ERROR_NOT_AUTH,
	MQTT_BROKER_ERROR_UNDEFINED,
	MQTT_BROKER_ERROR_RESTART
} MQTT_Broker_t;

typedef enum
{
	MQTT_NO_SUBSCRIPTION,
	MQTT_SUBSCRIPTION_GRANTED_OK,
	MQTT_SUBSCRIPTION_REQUEST_IN_PROGRESS,
	MQTT_SUBSCRIPTION_REQUESTED,
	MQTT_SUBSCRIPTION_RESPONDED,
	MQTT_SUBSCRIPTION_ERROR_TIMEOUT,
	MQTT_SUBSCRIPTION_ERROR_PROTOCOL,
	MQTT_SUBSCRIPTION_ERROR_IN_QUEUE,
	MQTT_SUBSCRIPTION_ERROR_IN_SUBACK,
	
} MQTT_Subscription_t;

typedef struct
{
	char server_ip[MQTT_INPUT_PARAMS_LENGTH_MAX];
	uint16_t server_port;
	char virtual_host[MQTT_INPUT_PARAMS_LENGTH_MAX];
	char client_id[MQTT_INPUT_PARAMS_LENGTH_MAX];
	char username[MQTT_INPUT_PARAMS_LENGTH_MAX];
	char vhost_username[MQTT_INPUT_PARAMS_LENGTH_MAX];
	char password[MQTT_INPUT_PARAMS_LENGTH_MAX];
	time_t last_ping_ts;
	bool ping_requested;
	// The eye catcher for this structure. NOTE: Must be MQTC
	char struct_id[4];
	// The version number of this structure.  NOTE: Must be 0
	int struct_version;
	// Version of MQTT to be used.  3 = 3.1 4 = 3.1.1
	unsigned char MQTTVersion;
	unsigned short keepAliveInterval;
	unsigned char cleansession;
	unsigned char willFlag;
	char willTopicName[MQTT_TOPIC_LENGTH_MAX];
	char willMessage[MQTT_WILL_MESSAGE_LENGTH_MAX];
	unsigned char returnCode;
	unsigned char sessionFlag;
	// MQTTPacket_connectData MQTTconnectData;
	// MQTTPacket_willOptions will;
} MQTT_Connect_Fields_t;


typedef enum
{
	CONNECT_idle = 1,
	CONNECT_ok_CONNACK_idle,
	CONNECT_ok_CONNACK_timeout,
	CONNACK_ok_DELETE,
	// Disconnect
	DISCONNECT_idle, // 5
	DISCONNECT_ok_CLOSED_in_progress,
	//DISCONNECT_ok_CONNECT_idle, // Note: This is equal to CONNECT_idle
	// Ping
	PINGREQ_idle, // 7
	PINGREQ_ok_PINGRES_idle,
	PINGREQ_ok_PINGRES_timeout,
	PINGRES_ok_DELETE,
	// Subscribe
	SUBSCRIBE_idle, // 11
	SUBSCRIBE_ok_SUBACK_idle,
	SUBSCRIBE_ok_SUBACK_timeout,
	SUBACK_ok_DELETE,
	// Un-Subscribe
	UNSUBSCRIBE_idle, // 15
	UNSUBSCRIBE_ok_UNSUBACK_idle,
	UNSUBSCRIBE_ok_UNSUBACK_timeout,
	UNSUBACK_ok_DELETE,
	// Publish
	PUBLISH_TX_idle, // 19
	PUBLISH_TX_ok_PUBACK_idle,
	PUBLISH_TX_ok_PUBACK_timeout_DELETE,
	PUBLISH_TX_ok_PUBACK_ok_DELETE,
	// Puback
	PUBLISH_RX_PUBACK_idle,
	PUBLISH_RX_PUBACK_ok_DELETE
} Element_state_t;

typedef struct
{
	MQTT_Id_t id;
	MQTT_Topic_t topic;
	Element_state_t state;
	uint16_t mess_id;
	uint8_t dup;
	time_t timestamp;
} Element_t;

typedef struct
{
	Element_t buffer[MQTT_QUEUE_SIZE];
	uint16_t client_message_id;
	uint16_t head;
	uint16_t tail;
	uint16_t free_elements;
} Queue_t;

typedef enum 
{
	CONNECTION_INIT = 0,
	CONNECTION_BLUFI_START,
	CONNECTION_BLUFI_WAIT_RESPONSE,
	CONNECTION_BLUFI_GOT_RESPONSE, // Unused
	CONNECTION_BLUFI_STOP,
	CONNECTION_TCP_INIT,
	CONNECTION_TCP_CLOSE,
	CONNECTION_TCP_OPEN,
	CONNECTION_MQTT_BROKER_INIT,
	CONNECTION_MQTT_BROKER_OK_SUBSCRIBE,
	CONNECTION_OK,
	CONNECTION_GO_OFFLINE
} Connection_fsm_t;

typedef struct
{
	uint32_t RxTotal;
	uint32_t RxValid;
	uint32_t RxErrors;
	uint32_t RxErrorUnknown;
	uint32_t RxAckErrors;
	
} RxStatistics_t;

// ******************************** Main functions ********************************************
// Debug
void MQTT_debug();
// Library initializations
void MQTT_init();

// Connection
void MQTT_connection();
// Transmission
void MQTT_packet_transmission();
// Reception
void MQTT_packet_reception();
// Checks and update packets status in queue (Timeouts, DUP,...)
void MQTT_packet_update();
// Checks if there is anything to add to queue
void MQTT_packet_transmission_queue();

// Get the broker status
MQTT_Broker_t MQTT_Broker_state();
// Get the broker subscription state
MQTT_Subscription_t MQTT_Broker_subscription_state();

#endif