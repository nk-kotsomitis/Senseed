#include "MQTT_lib.h"

// Imports 
time_t g_time_epoch_y2k = 0;

// Broker
static MQTT_Connect_Fields_t g_broker_connection = {0};
static MQTT_Broker_t g_broker_connection_status = {0};
static MQTT_Subscription_t g_broker_subscription_status = MQTT_NO_SUBSCRIPTION;
static uint8_t g_broker_subscription_num_of_topics = 0;
static bool g_disconnect_flag = false;

// Connection state (FSM)
static Connection_fsm_t g_connection_state = CONNECTION_INIT;
static Connection_fsm_t g_connection_state_prev = CONNECTION_INIT;

// Buffers
static uint8_t MQTT_buffer_tx[MQTT_TX_BUFFER_SIZE] = {0};
static int16_t MQTT_buffer_tx_len = 0;

// TC2 timer
uint32_t tc2 = 0;
static bool start_tc = false;

// Queue
static Queue_t g_queue = {0};
const static Element_t ELEMENT_ZERO = {0};

// Statistics
RxStatistics_t g_rx_statistics = {0};
RxStatistics_t ZERO_RX_STATISTICS = {0};

// Debug
static time_t last_debug_call = 0;

// ******************************* Connection functions **************************************
// Server settings initializations
static void server_settings_init();

// ******************************* Queue functions *******************************************
// Adds element to queue
static bool queue_add_packet(MQTT_Packet_t packet);
// Adds element to queue
static bool queue_add_publish(MQTT_Id_t id, MQTT_Topic_t topic);
// Adds element to queue
static bool queue_add_puback(uint16_t message_id);
// Adds element to queue
static bool queue_add_element(Element_t element);
// Removes element from queue and "clean" 0s from tail towards head
static void queue_remove_element(uint16_t element_idx);
// Update the state of an element in queue
static bool queue_find_and_update_element(Element_state_t state, Element_state_t next_state, uint16_t message_id, bool check_message_id);
// Update the state of an element in queue
static bool queue_find_and_update_element_by_id(Element_state_t state, Element_state_t next_state, MQTT_Id_t id);
// Checks if element is 'OK' (TBD what OK means) in order to add it in queue
static bool is_queue_element_ok_to_add(Element_t element);
// Gets the next message id for transmition
static uint16_t queue_get_message_id();

// *********************************** TX functions *******************************************
// Send Connect
static int8_t mqtt_send_connect();
// Send Disconnect
static int8_t mqtt_send_disconnect();
// Send Ping
static int8_t mqtt_send_ping();
// Send Publish
static int8_t mqtt_send_publish(Element_t element);
// Send Subscribe
static int8_t mqtt_send_subscribe(Element_t element);
// Send Un-subscribe
static int8_t mqtt_send_unsubscribe();
// Send Puback
static int8_t mqtt_send_puback(Element_t element);

// *********************************** RX functions *******************************************
// Process all data received from lower level
static bool process_rx_data(uint8_t *data_buffer, uint32_t data_buffer_length);
// Checks and returns the MQTT remaining length bytes and the type of packet
static bool process_mqtt_packet_length_and_type(uint8_t *buffer, uint16_t length, uint32_t *mqtt_packet_length, uint8_t *mqtt_packet_type);
// De-serialize the MQTT packet
static bool process_mqtt_packet(uint8_t *buffer, uint32_t length, uint8_t mqtt_type);
// De-serialize the message id from a PUBACK
static uint16_t deserialize_puback_message_id(uint8_t *buffer);
// De-serialize the return code from CONNACK
static bool deserialize_connack(unsigned char return_code, unsigned char session);

// *********************************** Debug functions *****************************************
static void debug_queue(MQTT_Packet_t packet);

void MQTT_debug()
{
    ESP_tcp_open(g_broker_connection.server_ip, g_broker_connection.server_port);
    
    queue_add_packet(CONNECT);
    MQTT_packet_transmission();
    MQTT_packet_reception();
    MQTT_packet_update();

    queue_add_packet(PINGREQ);
    MQTT_packet_transmission();
    MQTT_packet_reception();
    MQTT_packet_update();
}

// *********************************** Functions Definitions ************************************

void MQTT_init()
{    
	// Server Init
	server_settings_init();
	
	// Protocol Configuration
	g_broker_connection.struct_version = 0;
	g_broker_connection.MQTTVersion = 3;
	g_broker_connection.keepAliveInterval = MQTT_KEEP_ALIVE_INTERVAL;
	g_broker_connection.cleansession = MQTT_CLEAN_SESSION;
	g_broker_connection.willFlag = 0;
		
	// TODO: Impleement will message
	//g_mqtt_connection.willTopicName = GW_TOPIC_WILL;
	//g_mqtt_connection.willMessage = GW_WILL_MESSAGE;
	//g_mqtt_connection.will.retained = 0;
	//g_mqtt_connection.will.qos = MQTT_QOS_WILL_MESS;
	g_broker_connection.returnCode = 0;
	g_broker_connection.sessionFlag = 0;		
	
	// Queue Configuration
	g_queue.head = g_queue.tail = 0;
	g_queue.client_message_id = 32700; // TBC
	g_queue.free_elements = MQTT_QUEUE_SIZE;
	
	// Statistics
	g_rx_statistics = ZERO_RX_STATISTICS;
}


void MQTT_connection(void)
{
	int8_t res = ESP_RES_NO_ERROR;	
		
	if (g_connection_state != g_connection_state_prev)
	{
		CONSOLE_log("FSM ", g_connection_state);
	}
	
	// Store previous connection state
	g_connection_state_prev = g_connection_state;
	
	switch (g_connection_state)
	{
		case CONNECTION_INIT:
			MQTT_CONNECTION_LED_SLOW;
            
            // NOTE: 500000000ns (500ms)/ 17066.6666667ns (per tick) = 29296 ticks
            if (TC2_Timer32bitCounterGet() >= 29296)
            {
                // Clear counter
                TC2_Timer32bitCounterSet(0); 
                // Update WiFi status
                res = ESP_wifi_update_status();
            }

            // Check if BluFi is enabled
			if (ESP_blufi_status())
			{
				// Go to the next state - start BluFi
				g_connection_state = CONNECTION_BLUFI_START;
			}
            // Check WiFi status
			else if (ESP_wifi_status() == WIFI_GOT_IP)
            {
                // WiFi is OK, go to TCP init
                g_connection_state = CONNECTION_TCP_INIT;
            }
            else
			{
                // Wait for WiFi to be connected...
			}
            
			break;
		
		// Start BluFi
		case CONNECTION_BLUFI_START:
			MQTT_CONNECTION_LED_SLOW;
			// Note: At this state, there is only FORWARD direction
			
			// Start BluFi
			if ((res = ESP_blufi_start()) == ESP_RES_OK);
			{
				// BluFi is started, go to next state
				g_connection_state = CONNECTION_BLUFI_WAIT_RESPONSE;
			}
						
			break;
		
		// BluFi - wait for a response
		case CONNECTION_BLUFI_WAIT_RESPONSE:
			MQTT_CONNECTION_LED_SLOW;
            // Note: At this state, there is only FORWARD direction
                                    
            // Check WiFi status
            if (ESP_wifi_status() == WIFI_GOT_IP)
            {
                ESP_blufi_send();
                ESP_blufi_set_status(BLUFI_DISABLED);
                
                // WiFi is OK, go to BluFi stop
                g_connection_state = CONNECTION_BLUFI_STOP;
            }
            
			break;
				
		case CONNECTION_BLUFI_STOP:
			MQTT_CONNECTION_LED_SLOW;
            // Note: At this state, there is only FORWARD direction
                        
            // Stop BluFi
			if ((res = ESP_blufi_stop()) == ESP_RES_OK);
			{
				// BluFi is stopped, go to next state
				g_connection_state = CONNECTION_TCP_INIT;
			}
            
			break;
		
		
		case CONNECTION_TCP_INIT: // 5
			MQTT_CONNECTION_LED_FAST;
		
			// Note: In case WDT reset, ESP will be probably still connected!
			
			if (ESP_blufi_status() || ESP_wifi_status() != WIFI_GOT_IP)
			{					
				// Go to init state
				g_connection_state = CONNECTION_INIT;
				break;
			}
			
			res = ESP_tcp_update_status();
									
			// Check if TCP connection is open
			if (ESP_tcp_status() == TCP_OPEN)
			{
				// Go to TCP close
				g_connection_state = CONNECTION_TCP_CLOSE;
			}
			else
			{
				// Go to TCP open
				g_connection_state = CONNECTION_TCP_OPEN;
			}
					
			break;
		
			case CONNECTION_TCP_CLOSE: // 6
				MQTT_CONNECTION_LED_FAST;
				
				// Close TCP connection
				res = ESP_tcp_close();
							
				if (ESP_tcp_status() != TCP_OPEN)
				{
					// Next state
					g_connection_state = CONNECTION_TCP_OPEN;
				}
				else
				{
					// Previous state
					g_connection_state = CONNECTION_TCP_INIT;
				}				
				break;

		case CONNECTION_TCP_OPEN: // 7
			MQTT_CONNECTION_LED_FAST;
						
			// Open TCP connection
			res = ESP_tcp_open(g_broker_connection.server_ip, g_broker_connection.server_port);
			
			if (res == ESP_RES_OK || res == ESP_RES_ALREADY)
			{
				// FOR DEBUGGING ONLY
				ESP_tcp_update_status();
			}
			
			if (ESP_tcp_status() == TCP_OPEN)
			{
				// Next state
				g_connection_state = CONNECTION_MQTT_BROKER_INIT;
                start_tc = false;
                                                
				// TCP connection is OK
				queue_add_packet(CONNECT);
				g_disconnect_flag = false; // IMPORTANT: Clear this flag after queue_add_packet(CONNECT);
				g_broker_connection_status = MQTT_BROKER_CONNECTION_REQUEST_IN_PROGRESS;
				break;
			}
			else
			{
				// Previous state
				g_connection_state = CONNECTION_TCP_INIT;
			}
			break;
		
		// Connect to broker
		case CONNECTION_MQTT_BROKER_INIT: // 8
			MQTT_CONNECTION_LED_SUPER_FAST;
			
			// Go to TCP Init
			if (ESP_tcp_status() != TCP_OPEN || ESP_blufi_status())
			{					
				g_connection_state = CONNECTION_TCP_INIT;
			}
            
			// Wait for CONNACK or a timeout
			if (g_broker_connection_status == MQTT_BROKER_CONNECTION_REQUEST_IN_PROGRESS || g_broker_connection_status == MQTT_BROKER_CONNECTION_REQUESTED)
			{
				break;
			}

			if (g_broker_connection_status == MQTT_BROKER_CONNECTION_OK_WAIT_FOR_RETAINED_MESSAGES)
			{	
                if(!start_tc)
                {
                    start_tc = true;
                    TC2_Timer32bitCounterSet(0); 
                }
                
                // NOTE: 500000000ns (500ms)/ 17066.6666667ns (per tick) = 29296 ticks
				if (TC2_Timer32bitCounterGet() >= 29296)
				{
					// Next state
					g_connection_state = CONNECTION_MQTT_BROKER_OK_SUBSCRIBE;
					g_broker_connection_status = MQTT_BROKER_CONNECTION_OK;
					g_broker_subscription_status = MQTT_SUBSCRIPTION_REQUEST_IN_PROGRESS;
					queue_add_packet(SUBSCRIBE);
				}
			}
			else
			{
				// Error! Restart TCP connection! Go to TCP Init
				g_connection_state = CONNECTION_TCP_INIT;
			}	
						
			break;
		
		// Subscribe to topics and poll unexpected messages
		case CONNECTION_MQTT_BROKER_OK_SUBSCRIBE: // 9
			MQTT_CONNECTION_LED_SUPER_FAST;
			
			// Go to TCP Init
			if (g_broker_connection_status != MQTT_BROKER_CONNECTION_OK || ESP_tcp_status() != TCP_OPEN || ESP_blufi_status())
			{					
				g_connection_state = CONNECTION_TCP_INIT;
			}
			
			// Waiting for SUBACK or a timeout
			if (g_broker_subscription_status == MQTT_SUBSCRIPTION_REQUEST_IN_PROGRESS || g_broker_subscription_status == MQTT_SUBSCRIPTION_REQUESTED)
			{
				break;
			}
			
			if (g_broker_subscription_status == MQTT_SUBSCRIPTION_GRANTED_OK)
			{
				// Next state
				g_connection_state = CONNECTION_OK;
                //last_debug_call = g_time_epoch_y2k; // Debug
			}
			else
			{
				// Error! Restart TCP connection! Go to TCP Init
				g_connection_state = CONNECTION_TCP_INIT;
			}
			break;
		
		
		// TCP and broker are connected! :)
		case CONNECTION_OK:
			MQTT_CONNECTION_LED_ON;

			// Go to TCP Init
			if (g_broker_connection_status != MQTT_BROKER_CONNECTION_OK || ESP_tcp_status() != TCP_OPEN ||  ESP_wifi_status() != WIFI_GOT_IP || ESP_blufi_status())
			{								
				MQTT_CONNECTION_LED_BLINK;
				g_connection_state = CONNECTION_TCP_INIT;
			}
			
			if (!g_broker_connection.ping_requested && g_time_epoch_y2k - g_broker_connection.last_ping_ts >= MQTT_PING_INTERVAL)
			{
				// Send PING to keep connection alive
				queue_add_packet(PINGREQ);
				g_broker_connection.ping_requested = true;
                
                //queue_add_publish(ID_GATEWAY, MQTT_TOPIC_GATEWAY);
			}
            
            // Debug
            if (g_time_epoch_y2k - last_debug_call >= 3600)
            {
                last_debug_call = g_time_epoch_y2k;
                res = ESP_tcp_sntp_time();
                //ESP_wifi_disconnect();
                //queue_add_packet(DISCONNECT);
            }
			break;
		
		default:
			break;		
	}
	
	if (res < ESP_RES_NO_ERROR)
	{
		EVENT_LOGGER(g_time_epoch_y2k, EVT_WARN, MODULE_MQTT, 50, res);
	}

	return;
}

void MQTT_packet_transmission()
{	
	if (ESP_tcp_status() != TCP_OPEN || g_disconnect_flag)
	{
		return;
	}
	
	int8_t res = ESP_RES_NO_ERROR;
		
	for (uint16_t element = g_queue.tail; element != g_queue.head; element++, element &= MQTT_QUEUE_MASK)
	{		
		if (g_queue.buffer[element].id)
		{
			bool check_broker = true;
			
			switch(g_queue.buffer[element].state)
			{
				case CONNECT_idle:
					check_broker = false;
					if ((res = mqtt_send_connect()) == ESP_RES_SEND_OK)
					{
						g_queue.buffer[element].state = CONNECT_ok_CONNACK_idle;
						g_queue.buffer[element].timestamp = g_time_epoch_y2k;
						g_broker_connection_status = MQTT_BROKER_CONNECTION_REQUESTED;
					}
					break;
				
				case PUBLISH_TX_idle:				
					if ((res = mqtt_send_publish(g_queue.buffer[element])) == ESP_RES_SEND_OK)
					{
						g_queue.buffer[element].state = PUBLISH_TX_ok_PUBACK_idle;
						g_queue.buffer[element].timestamp = g_time_epoch_y2k;
					}
					else
					{
						g_queue.buffer[element].state = PUBLISH_TX_idle;
					}
					break;
				
				case PUBLISH_RX_PUBACK_idle:
					if ((res = mqtt_send_puback(g_queue.buffer[element])) == ESP_RES_SEND_OK)
					{
						g_queue.buffer[element].state = PUBLISH_RX_PUBACK_ok_DELETE;
						g_queue.buffer[element].timestamp = g_time_epoch_y2k;
					}
					break;
				
				case SUBSCRIBE_idle:
					if ((res = mqtt_send_subscribe(g_queue.buffer[element])) == ESP_RES_SEND_OK)
					{
						g_queue.buffer[element].state = SUBSCRIBE_ok_SUBACK_idle;
						g_queue.buffer[element].timestamp = g_time_epoch_y2k;
						g_broker_subscription_status = MQTT_SUBSCRIPTION_REQUESTED;
					}
					break;
					
				case UNSUBSCRIBE_idle:
					if ((res = mqtt_send_unsubscribe(g_queue.buffer[element])) == ESP_RES_SEND_OK)
					{
						g_queue.buffer[element].state = UNSUBSCRIBE_ok_UNSUBACK_idle;
						g_queue.buffer[element].timestamp = g_time_epoch_y2k;
					}
					break;
					
				case DISCONNECT_idle:
					if ((res = mqtt_send_disconnect()) == ESP_RES_SEND_OK)
					{
						g_disconnect_flag = true;
						g_queue.buffer[element].state = DISCONNECT_ok_CLOSED_in_progress;
						g_queue.buffer[element].timestamp = g_time_epoch_y2k;
					}
					break;

				case PINGREQ_idle:
					if ((res = mqtt_send_ping(g_queue.buffer[element])) == ESP_RES_SEND_OK)
					{
						g_queue.buffer[element].state = PINGREQ_ok_PINGRES_idle;
						g_queue.buffer[element].timestamp = g_time_epoch_y2k;
					}
					break;
                    
				default:
					break;
			}
			
			// Success
			if (res == ESP_RES_SEND_OK)
			{
				CONSOLE_mqtt(MQTT_buffer_tx, (uint16_t)MQTT_buffer_tx_len, TX_DIRECTION);
			}
			// Error
			else if (res != ESP_RES_SEND_OK && 
					 res != ESP_RES_NO_ERROR &&
					 res != ESP_RES_UART_BUFFER_OVF && 
					 ESP_wifi_status() == WIFI_GOT_IP && 
					 ESP_tcp_status() == TCP_OPEN && 
					 !((g_broker_connection_status != MQTT_BROKER_CONNECTION_OK) & check_broker))
			{
				EVENT_LOGGER(g_time_epoch_y2k, EVT_WARN, MODULE_MQTT, 51, res);
			}
			// Warning
			else if (res < ESP_RES_NO_ERROR && res != ESP_RES_UART_BUFFER_OVF)
			{
				// Note: UART Buffer full is handled and reported by Reception!
				EVENT_LOGGER(g_time_epoch_y2k, EVT_WARN, MODULE_MQTT, 52, res);
			}
					
			if (res == ESP_RES_UART_BUFFER_OVF || g_disconnect_flag)
			{
				// 1 - Without UART, nothing can be sent!
				// 2 - DO NOT SEND other control packets after disconnect
				return;	
			}
		}
	}
}

void MQTT_packet_reception()
{	
	uint8_t *buf_ptr = NULL;
	uint16_t buf_length = 0;
	int8_t res = ESP_RES_NO_ERROR;
	bool restart_connection_flag = false;
		
	do
	{			
		// Receive data
		res = ESP_tcp_receive_data(&buf_ptr, &buf_length);
		
		if (res == ESP_RES_IPD || res == ESP_RES_IPD_BUFFER_OVF || (res == ESP_RES_UART_BUFFER_OVF && buf_length))
		{            
			// Process data
			if(!process_rx_data(buf_ptr, buf_length))
			{
				restart_connection_flag = true;
			}
		}
		
		// ESP_RES_GEN_PARTIAL
		
		if (res == ESP_RES_IPD_BUFFER_OVF || res == ESP_RES_IPD_CORRUPTED || res == ESP_RES_IPD_PARTIAL || res == ESP_RES_UART_BUFFER_OVF)
		{
			EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 53, res);
			restart_connection_flag = true;
		}
		
		// Flush data
		ESP_tcp_flush_data();
				
	} while (res != ESP_RES_NOT_FOUND && res != ESP_RES_UART_BUFFER_OVF);
	
	if (restart_connection_flag)
	{
		queue_add_packet(DISCONNECT);
	}
}

void MQTT_packet_update()
{
	time_t time_now = g_time_epoch_y2k;
		
	for (uint16_t element = g_queue.tail; element != g_queue.head; element++, element &= MQTT_QUEUE_MASK)
	{
		if (g_queue.buffer[element].id)
		{
			bool timeout_flag = false;
			Element_state_t timeout_deleted = 0;
			
			switch(g_queue.buffer[element].state)
			{
				case CONNECT_ok_CONNACK_idle:
					// Check for CONNACK timeout
					if (time_now - g_queue.buffer[element].timestamp >= MQTT_TIMEOUT_CONNACK)
					{
						g_queue.buffer[element].timestamp = g_time_epoch_y2k;
						g_queue.buffer[element].state = CONNECT_ok_CONNACK_timeout;
						g_broker_connection_status = MQTT_BROKER_ERROR_TIMEOUT;
						timeout_flag = true;
					}
					
					break;
				
				case PINGREQ_ok_PINGRES_idle:
					// Check for PINGRES timeout
					if (time_now - g_queue.buffer[element].timestamp >= MQTT_TIMEOUT_PINGRES)
					{
						g_queue.buffer[element].timestamp = g_time_epoch_y2k;
						g_queue.buffer[element].state = PINGREQ_ok_PINGRES_timeout;
						g_broker_connection.ping_requested = false;
						timeout_flag = true;
					}
					break;
					
				case SUBSCRIBE_ok_SUBACK_idle:
					// Check for SUBACK timeout
					if (time_now - g_queue.buffer[element].timestamp >= MQTT_TIMEOUT_SUBACK)
					{
						g_queue.buffer[element].timestamp = g_time_epoch_y2k;
						g_queue.buffer[element].state = SUBSCRIBE_ok_SUBACK_timeout;
						g_broker_subscription_status = MQTT_SUBSCRIPTION_ERROR_TIMEOUT;
						timeout_flag = true;
					}
					break;
					
				case UNSUBSCRIBE_ok_UNSUBACK_idle:
					// Check for UNSUBACK timeout
					if (time_now - g_queue.buffer[element].timestamp >= MQTT_TIMEOUT_UNSUBACK)
					{
						g_queue.buffer[element].timestamp = g_time_epoch_y2k;
						g_queue.buffer[element].state = UNSUBSCRIBE_ok_UNSUBACK_timeout;
						timeout_flag = true;
					}
					break;
				
				case PUBLISH_TX_ok_PUBACK_idle:
					// Check for PUBACK timeout
					if (time_now - g_queue.buffer[element].timestamp >= MQTT_TIMEOUT_RX_PUBACK)
					{
						// g_queue.buffer[element].timestamp = g_time_epoch_y2k;
						//g_queue.buffer[element].state = PUBLISH_TX_ok_PUBACK_timeout_DELETE;
												
						// Failure
						THINGS_tx_to_server_fail((ID_t)g_queue.buffer[element].id, (TOPIC_t)g_queue.buffer[element].topic);
						
						timeout_flag = true;
						timeout_deleted = g_queue.buffer[element].state;
						queue_remove_element(element);
					}
					break;	
								
				// Timeouts
				case CONNECT_ok_CONNACK_timeout:
				case PINGREQ_ok_PINGRES_timeout:
				case SUBSCRIBE_ok_SUBACK_timeout:
				case UNSUBSCRIBE_ok_UNSUBACK_timeout:
					if (time_now - g_queue.buffer[element].timestamp >= MQTT_TIMEOUT_KEEP_DURATION)
					{
						timeout_deleted = g_queue.buffer[element].state;
						queue_remove_element(element);
					}
					break;
				
				// RE-CONNECT
				case DISCONNECT_ok_CLOSED_in_progress:
				
					if (g_disconnect_flag)
					{
						// This means that connection is NOT CLOSED after sending DISCONNECT, so force broker restart
						if (time_now - g_queue.buffer[element].timestamp >= MQTT_TIMEOUT_DISCONNECT)
						{
							g_queue.buffer[element].state = CONNECT_idle;
							g_broker_connection_status = MQTT_BROKER_ERROR_RESTART;
							g_disconnect_flag = false;
							timeout_flag = true;
						}
						
					}
					// This means that connection is CLOSED
					else
					{
						g_queue.buffer[element].state = CONNECT_idle;
					}				
					break;
				
				// Deletes - Tx PUBLISH SUCCESS (Gateway -> FE)
				case PUBLISH_TX_ok_PUBACK_ok_DELETE:					
					//print_uint16("TX_TO_SERVER_SUCCESS ", g_queue.buffer[element].mess_id);
					// Success
					THINGS_tx_to_server_success((ID_t)g_queue.buffer[element].id, (TOPIC_t)g_queue.buffer[element].topic);
					queue_remove_element(element);
					break;
					
				// Deletes
				case CONNACK_ok_DELETE:
				case PUBLISH_RX_PUBACK_ok_DELETE:
				case PINGRES_ok_DELETE:
				case SUBACK_ok_DELETE:
				case UNSUBACK_ok_DELETE:
					queue_remove_element(element);
					break;
					
				default:
					// The rest states are handled by other function (i.e. transmission or reception)
					break;
			}
			
			if (timeout_deleted)
			{
				EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 54, timeout_deleted);
			}
			
			if (timeout_flag)
			{
				EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 55, g_queue.buffer[element].state);	
			}
		}			
	}
}

void MQTT_packet_transmission_queue()
{
	bool res = false;
	MQTT_Id_t id = 0;
	MQTT_Topic_t topic = MQTT_TOPIC_UNDEFINED;
	uint8_t struct_idx = 0;

#if TESTING_INTEGRATION
    THINGS_TESTING_integration();
#endif
    
	do 
	{
		// Check if there is something to transmit
		if ((res = THINGS_tx_mqtt_queue((ID_t *)&id, (TOPIC_t *)&topic, &struct_idx)))
		{
			// Add to MQTT Queue
			 if (queue_add_publish(id, topic))
			 {
				THINGS_tx_mqtt_queue_ok((ID_t)id, (TOPIC_t)topic, struct_idx);	 
			 }			
		}
				
	} while (res);
}

MQTT_Broker_t MQTT_Broker_state()
{
	return g_broker_connection_status;
}

MQTT_Subscription_t MQTT_Broker_subscription_state()
{
	return g_broker_subscription_status;
}


//************************************************************************************
//************************************* Static functions *****************************
//************************************************************************************

// *********************************** Connection functions *******************************

static void server_settings_init()
{
	const char *COLON = ":";
	char *server_ip;
	uint16_t server_port;
	char *virtual_host;
	char *client_id;
	char *broker_username;;
	char *broker_password;
		
	// Get server settings
	THINGS_get_server_settings(&server_ip, &server_port, &virtual_host, &client_id, &broker_username, &broker_password);
		
	// Set server settings
	strcpy(g_broker_connection.server_ip, server_ip);
	strcpy(g_broker_connection.virtual_host, virtual_host);
	strcpy(g_broker_connection.client_id, client_id);
	strcpy(g_broker_connection.username, broker_username);
	strcpy(g_broker_connection.vhost_username, virtual_host);
	strcat(g_broker_connection.vhost_username, COLON);
	strcat(g_broker_connection.vhost_username, broker_username);
	strcpy(g_broker_connection.password, broker_password);
	g_broker_connection.server_port = server_port;
}

// *********************************** Queue functions ************************************


static bool queue_add_packet(MQTT_Packet_t packet)
{	
	Element_state_t state;
	
	switch(packet)
	{
		case CONNECT:
			state = CONNECT_idle;
			break;
		case SUBSCRIBE:
			state = SUBSCRIBE_idle;
			break;
		case UNSUBSCRIBE:
			state = UNSUBSCRIBE_idle;
			break;
		case PINGREQ:
			state = PINGREQ_idle;
			break;
		case DISCONNECT:
			state = DISCONNECT_idle;
			break;
		default:
			// Error!
			return false;
	}
	
	Element_t element = {0};
		
	element.id = MQTT_ID_PROTOCOL;
	element.topic = MQTT_TOPIC_UNDEFINED;
	element.state = state;
	element.mess_id = queue_get_message_id();
	
	return queue_add_element(element);
}

static bool queue_add_publish(MQTT_Id_t id, MQTT_Topic_t topic)
{
	Element_t element = {0};
	
	element.id = id;
	element.topic = topic;
	element.state = PUBLISH_TX_idle;
	element.mess_id = queue_get_message_id();
	
	return queue_add_element(element);
}

static bool queue_add_puback(uint16_t message_id)
{
	Element_t element = {0};
	
	element.id = MQTT_ID_GATEWAY;
	element.topic = MQTT_TOPIC_UNDEFINED;
	element.state = PUBLISH_RX_PUBACK_idle;
	element.mess_id = message_id;
	
	return queue_add_element(element);
}

static bool queue_add_element(Element_t element)
{	
	// Check if new element is valid
	if (!is_queue_element_ok_to_add(element))
	{
		return false;
	}
	
	// Add to queue
	g_queue.buffer[g_queue.head] = element;
	g_queue.buffer[g_queue.head].timestamp = g_time_epoch_y2k;
	
	// Update number of free elements
	if (g_queue.free_elements > 0)
		g_queue.free_elements -= 1;
	
	// Update circular buffer	
	g_queue.head++;
	g_queue.head &= MQTT_QUEUE_MASK;
	
		
	// Check for Overflow
	if (g_queue.head == g_queue.tail)
	{
		EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 60, 0);
		debug_queue(0);
		
		// Delete the last element		
		queue_remove_element(g_queue.tail);
	}
	
	return true;
}

static void queue_remove_element(uint16_t element_idx)
{
	// Delete element
	g_queue.buffer[element_idx] = ELEMENT_ZERO;
	
	// Increase number of empty elements
	if (g_queue.free_elements < MQTT_QUEUE_SIZE)
	g_queue.free_elements += 1;
	
	// Clean queue from 0s
	if (element_idx == g_queue.tail)
	{
		do
		{
			g_queue.tail++;
			g_queue.tail &= MQTT_QUEUE_MASK;
		} while (!g_queue.buffer[g_queue.tail].id  &&  g_queue.tail != g_queue.head);
	}
}

static bool queue_find_and_update_element(Element_state_t state, Element_state_t next_state, uint16_t message_id, bool check_message_id)
{	
	for (uint16_t row = g_queue.tail; row != g_queue.head; row++, row &= MQTT_QUEUE_MASK)
	{
		// Consider to also check ID TBC
		if (g_queue.buffer[row].state == state)
		{
			//CONSOLE_queue(g_queue.buffer[row].id, g_queue.buffer[row].topic, g_queue.buffer[row].state, g_queue.buffer[row].mess_id);
			
			if (check_message_id)
			{				
				if (g_queue.buffer[row].mess_id == message_id)
				{
					g_queue.buffer[row].state = next_state;
					g_queue.buffer[row].timestamp = g_time_epoch_y2k;
					return true;
				}
			}
			else
			{
				g_queue.buffer[row].state = next_state;
				g_queue.buffer[row].timestamp = g_time_epoch_y2k;
				return true;
			}
		}
	}
	
	return false;
}

static bool queue_find_and_update_element_by_id(Element_state_t state, Element_state_t next_state, MQTT_Id_t id)
{
	for (uint16_t row = g_queue.tail; row != g_queue.head; row++, row &= MQTT_QUEUE_MASK)
	{
		if (g_queue.buffer[row].id == id && g_queue.buffer[row].state == state)
		{			
			g_queue.buffer[row].state = next_state;
			g_queue.buffer[row].timestamp = g_time_epoch_y2k;
			return true;	
		}
	}
	
	return false;
}

static bool is_queue_element_ok_to_add(Element_t element)
{
	// Case 1: Only 1 CONNACK, DISCONNECT, PING, SUBSCRIBE, UNSUBSCRIBE can be present in queue
	// Case 2: TODO: PUBLISH - Check at which cases a second PUBLISH with the same Id is allowed
	
	bool res = true;
	bool report = true;
		
	for (uint16_t row = g_queue.tail; row != g_queue.head; row++, row &= MQTT_QUEUE_MASK)
	{
		if (g_queue.buffer[row].id && g_queue.buffer[row].id == element.id)
		{
			switch(g_queue.buffer[row].state)
			{
				case PUBLISH_TX_idle:
				case PUBLISH_TX_ok_PUBACK_idle:
				case PUBLISH_TX_ok_PUBACK_timeout_DELETE:
				case PUBLISH_TX_ok_PUBACK_ok_DELETE:
					if (element.state == PUBLISH_TX_idle)
					{
						res = false;
						report = false;
						goto EXIT;
					}
					break;
				
				case CONNECT_idle:
				case CONNECT_ok_CONNACK_idle:
				
				if (element.state == CONNECT_idle)
				{
					res = false;
					report = false;
					goto EXIT;
				}
				break;
				
				case DISCONNECT_idle:
				case DISCONNECT_ok_CLOSED_in_progress:
				if (element.state == DISCONNECT_idle || element.state == CONNECT_idle)
				{
					res = false;
					report = false;
					goto EXIT;
				}
				break;
				
				case PINGREQ_idle:
				case PINGREQ_ok_PINGRES_idle:
				
				if (element.state == PINGREQ_idle)
				{
					res = false;
					goto EXIT;
				}
				break;
				
				case SUBSCRIBE_idle:
				case SUBSCRIBE_ok_SUBACK_idle:
				
				if (element.state == SUBSCRIBE_idle)
				{
					res = false;
					report = false;
					goto EXIT;
				}
				break;
				
				case UNSUBSCRIBE_idle:
				case UNSUBSCRIBE_ok_UNSUBACK_idle:
				// Not implemented
				break;
				
				default:
				break;
			}
		}		
	}
	
	EXIT:
	
	if (!res && report)
	{
		EVENT_LOGGER(g_time_epoch_y2k, EVT_WARN, MODULE_MQTT, 61, element.state);
	}
	
	return res;
}

static uint16_t queue_get_message_id()
{	
	// Increase message ID
	if (g_queue.client_message_id < 65535)
	{
		g_queue.client_message_id++;
	}
	else
	{
		// NOTE: Do not use Message ID 0. It is reserved as an invalid Message ID
		g_queue.client_message_id = 1;
	}
	
	return g_queue.client_message_id;
}

// *********************************** Transmission functions ************************************

static int8_t mqtt_send_connect()
{
	int8_t res = ESP_RES_NO_ERROR;
		
	MQTTPacket_connectData _connect_data = MQTTPacket_connectData_initializer;
	_connect_data.struct_version = g_broker_connection.struct_version;
	_connect_data.MQTTVersion = g_broker_connection.MQTTVersion;
	_connect_data.keepAliveInterval = g_broker_connection.keepAliveInterval;
	_connect_data.cleansession = g_broker_connection.cleansession;
	_connect_data.willFlag = g_broker_connection.willFlag;
	_connect_data.clientID.cstring = g_broker_connection.client_id;
	_connect_data.username.cstring = g_broker_connection.vhost_username;
	_connect_data.password.cstring = g_broker_connection.password;
	
	// TODO: Will message
	
	// Serialize connect packet
	MQTT_buffer_tx_len = MQTTSerialize_connect(MQTT_buffer_tx, MQTT_TX_BUFFER_SIZE, &_connect_data);
	
	// Report error
	if (MQTT_buffer_tx_len <= 0)
	{
		EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 70, 0);
		return res;
	}
	
	// Send data
	res = ESP_tcp_send_data(MQTT_buffer_tx, (uint16_t)MQTT_buffer_tx_len);
				
	return res;
}

static int8_t mqtt_send_disconnect()
{
	int8_t res = ESP_RES_NO_ERROR;
	
	// Check if broker is connected
	if (g_broker_connection_status != MQTT_BROKER_CONNECTION_OK)
	{
		return res;
	}
	
	// Serialize MQTT disconnect
	MQTT_buffer_tx_len = MQTTSerialize_disconnect(MQTT_buffer_tx, MQTT_TX_BUFFER_SIZE);
	
	if (MQTT_buffer_tx_len <= 0)
	{
		EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 71, 0);
		return res;
	}
	
	// Send data
	res = ESP_tcp_send_data(MQTT_buffer_tx, (uint16_t)MQTT_buffer_tx_len);
	
	return res;
}

static int8_t mqtt_send_ping()
{
	int8_t res = ESP_RES_NO_ERROR;
	
	// Check if broker is connected
	if (g_broker_connection_status != MQTT_BROKER_CONNECTION_OK)
	{
		return res;
	}
		
	// Send MQTT Ping
	MQTT_buffer_tx_len = MQTTSerialize_pingreq(MQTT_buffer_tx, MQTT_TX_BUFFER_SIZE);
	
	if (MQTT_buffer_tx_len <= 0)
	{
		EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 72, 0);
		return res;
	}
	
	// Send data
	res = ESP_tcp_send_data(MQTT_buffer_tx, (uint16_t)MQTT_buffer_tx_len);
				
	return res;		
}

const char *TEMP = STRINGIZE(UUID)"/"TOPIC_GW_STR_GATEWAY;

static int8_t mqtt_send_publish(Element_t element)
{
	int8_t res = ESP_RES_NO_ERROR;
	
	// Check if broker is connected
	if (g_broker_connection_status != MQTT_BROKER_CONNECTION_OK)
	{
		return res;
	}
	
	// Set PUBLISH fields
	int qos = MQTT_QOS_PUBLISH;
	unsigned char retained = MQTT_RETAINED_MESSAGES;
	int payloadlen = 0;
	MQTTString topic;
	topic.lenstring.data = NULL;
	topic.lenstring.len = 0;
	topic.cstring = NULL;
	
	Json_res_t res_json = JSON_NO_ERROR;
	if ((res_json = SERIALIZE_mqtt_publish(&topic.cstring, (char *)(MQTT_buffer_tx + MQTT_PUBLISH_PAYLOAD_OFFSET), (MQTT_TX_BUFFER_SIZE - MQTT_PUBLISH_PAYLOAD_OFFSET) , &payloadlen, element.id, element.topic)) != JSON_SUCCESS)
	{
		EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 77, res_json);
		return res;
	}
		
	//print("MYDEBUG\r\n");
	//print_uint8(topic.cstring, strlen(topic.cstring));
	//print_uint8((char *)GW_TOPIC_CONTROLLER_HP, strlen(GW_TOPIC_CONTROLLER_HP));
		
	// JSON_serialize((char *)(MQTT_buffer_tx + MQTT_PAYLOAD_OFFSET), &g_mqtt_publish.payloadlen, element.id, element.type);
		
	// Serialize MQTT Publish
	// NOTE: MyPublish.payload_buf is replaced with buffer + MQTT_PAYLOAD_OFFSET. And at MQTTSerialize_publish, memcpy is replaced with memmove
	MQTT_buffer_tx_len = MQTTSerialize_publish(	MQTT_buffer_tx,
											MQTT_TX_BUFFER_SIZE,
											element.dup,
											qos,
											retained,
											element.mess_id,
											topic,
											MQTT_buffer_tx + MQTT_PUBLISH_PAYLOAD_OFFSET,
											payloadlen);
	
	if (MQTT_buffer_tx_len <= 0)
	{
		EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 73, 0);
		return res;
	}
		
	// Send data	
	res = ESP_tcp_send_data(MQTT_buffer_tx, (uint16_t)MQTT_buffer_tx_len);
	
	return res;	
}
	
static int8_t mqtt_send_subscribe(Element_t element)
{
	int8_t res = ESP_RES_NO_ERROR;
		
	// Check if broker is connected
	if (g_broker_connection_status != MQTT_BROKER_CONNECTION_OK)
	{
		return res;
	}
	
	uint8_t num_of_topics = 0;
	int qos[MQTT_NUM_OF_TOPICS_MAX] = {0};
	char *topics[MQTT_NUM_OF_TOPICS_MAX] = {NULL};
	
	Json_res_t res_json;
	if ((res_json = SERIALIZE_mqtt_subscribe(topics, &num_of_topics, qos)) != JSON_SUCCESS)
	{
		return res;
	}
	
	g_broker_subscription_num_of_topics = num_of_topics;
	
	// Convert topic filters from char array to MQTTString for PAHO compatibility
	MQTTString _topic_filter[MQTT_NUM_OF_TOPICS_MAX] = {0};
	
	for (uint8_t i = 0; i < num_of_topics; i++)
	{		
		_topic_filter[i].cstring = topics[i];
	}
	
	int dup = 0;

	// Serialize MQTT Subscribe
	MQTT_buffer_tx_len = MQTTSerialize_subscribe(MQTT_buffer_tx, MQTT_TX_BUFFER_SIZE, dup, element.mess_id, (int)num_of_topics, _topic_filter, qos);
	
	// Report error
	if (MQTT_buffer_tx_len <= 0)
	{
		EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 74, 0);
		return res;
	}
	
	// Send data
	res = ESP_tcp_send_data(MQTT_buffer_tx, (uint16_t)MQTT_buffer_tx_len);
	
	return res;
}

static int8_t mqtt_send_unsubscribe()
{
	int8_t res = ESP_RES_NO_ERROR;
	
	// Check if broker is connected
	if (g_broker_connection_status != MQTT_BROKER_CONNECTION_OK)
	{
		return res;
	}
	
	// TODO: Not implemented
	
	// Serialize MQTT Un-subscribe
	MQTT_buffer_tx_len = 0; // MQTTSerialize_unsubscribe(MQTT_buffer_tx, MQTT_BUFFER_TX_SIZE, dup, messID, numberOfTopicsRequested, topicFilter);
	
	// Report error
	if(MQTT_buffer_tx_len <= 0)
	{
		EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 75, 0);
		return res;
	}
	
	// Send data	
	res = ESP_tcp_send_data(MQTT_buffer_tx, (uint16_t)MQTT_buffer_tx_len);
		
	return res;
}

static int8_t mqtt_send_puback(Element_t element)
{
	int8_t res = ESP_RES_NO_ERROR;
	
	// Check if broker is connected
	if (g_broker_connection_status != MQTT_BROKER_CONNECTION_OK)
	{
		return res;
	}
		
	// Serialize MQTT Puback										
	MQTT_buffer_tx_len = MQTTSerialize_puback(MQTT_buffer_tx, MQTT_TX_BUFFER_SIZE, element.mess_id);
	
	// Report error	
	if (MQTT_buffer_tx_len <= 0)
	{
		EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 76, 0);
		return res;
	}
	
	// Send data
	res = ESP_tcp_send_data(MQTT_buffer_tx, (uint16_t)MQTT_buffer_tx_len);
		
	return res;
}

// *********************************** Reception functions ************************************

bool process_rx_data(uint8_t *data_buffer, uint32_t data_buffer_length)
{
	uint8_t mqtt_packet_type = 0;
	uint8_t *mqtt_packet = data_buffer;
	uint32_t mqtt_packet_len = data_buffer_length;
		
	if (data_buffer == NULL || !data_buffer_length)
	{
		EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 80, 0);
		return true; // This is an error, but we do not want to restart connection. This is something from ESP
	}
		
	while(data_buffer_length) 
	{
		if(!process_mqtt_packet_length_and_type(mqtt_packet, data_buffer_length, &mqtt_packet_len, &mqtt_packet_type))
		{
			g_rx_statistics.RxErrorUnknown++;
			EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 81, 0);
			
			// NOTE: All remaining bytes are discarded
			return false;
		}
		
		g_rx_statistics.RxTotal++;
		
		if(!process_mqtt_packet(mqtt_packet, mqtt_packet_len, mqtt_packet_type))
		{
			g_rx_statistics.RxErrors++;
			return false;
		}
		else
		{
			g_rx_statistics.RxValid++;
		}
								
		while (mqtt_packet_len--)
		{
			data_buffer_length--;
			mqtt_packet++;
		}	
	}
	
	return true;
}

bool process_mqtt_packet(uint8_t *buffer, uint32_t length, uint8_t mqtt_type)
{
	int8_t res = 0;
	
	switch(mqtt_type)
	{
		case CONNACK:
		{
			if ((res = MQTTDeserialize_connack(&g_broker_connection.sessionFlag, &g_broker_connection.returnCode, buffer, length)) != 1)
			{
				EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 85, res);
				return false;
			}
			
			// Update element state
			if(!queue_find_and_update_element(CONNECT_ok_CONNACK_idle, CONNACK_ok_DELETE, 0, false))
			{
				EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 86, 0);
				return false;		
			}
			
			// Update broker state
			if(!deserialize_connack(g_broker_connection.returnCode, g_broker_connection.sessionFlag))
			{
				EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 87, g_broker_connection.returnCode);
				return false;
			}
		}
		break;
		
		case PUBLISH:
		{
			unsigned char dup;
			int qos;
			unsigned char retained;
			unsigned short messID;
			MQTTString topic_str;
			unsigned char *payload_pt;
			int payloadlen;
			
			if((res = MQTTDeserialize_publish(&dup, &qos, &retained, &messID, &topic_str, &payload_pt, &payloadlen, buffer, length)) != 1)
			{
				EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 88, res);
				return false;
			}
			
			// Add PUBACK regardless of what received! Only message id is required.
			queue_add_puback(messID);
			
			Json_res_t res;
			if ((res = DESERIALIZE_mqtt_publish(topic_str.lenstring.data, (int)topic_str.lenstring.len, (char *)payload_pt, payloadlen)) != JSON_SUCCESS)
			{
				EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 89, res);
				return false;
			}
						
			MQTT_Id_t id = 0;
			if (THINGS_is_thing_deleted(&id))
			{
				// Case 1: Rx GW PUBLISH for delete, after a while (either a disconnection or simultaneously reception or FE error) Rx Thing PUBLISH - A warning is generated! TBC
				// N/A
				
				// Case 2: After thing is deleted, remove Tx PUBLISH from queue - No error/warning is generated - OK
				queue_find_and_update_element_by_id(PUBLISH_TX_idle, PUBLISH_TX_ok_PUBACK_ok_DELETE, id);
			}
			

									
			break;
		}
		
		case PUBACK:
		{
			// De-serialize message id from received puback
		 	uint16_t puback_mess_id = deserialize_puback_message_id(buffer);
			
			// Update element state
			if(!queue_find_and_update_element(PUBLISH_TX_ok_PUBACK_idle, PUBLISH_TX_ok_PUBACK_ok_DELETE, puback_mess_id, true))
			{
				EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 90, 0);
				return false;
			}
						
			break;
		}
		case SUBACK:
		{
			uint16_t mess_id;
			int32_t num_of_topics_requested = MQTT_NUM_OF_TOPICS_MAX;
			int32_t qos_granted[MQTT_NUM_OF_TOPICS_MAX];
			int32_t num_of_topics_granted;
			
			g_broker_subscription_status = MQTT_SUBSCRIPTION_RESPONDED;
			
			// De-serialize SUBACK
			if((res = MQTTDeserialize_suback((unsigned short *)&mess_id, num_of_topics_requested, &num_of_topics_granted, qos_granted, buffer, length)) != 1)
			{
				g_broker_subscription_status = MQTT_SUBSCRIPTION_ERROR_PROTOCOL;	
				EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 91, res);					
				return false;
			}
						
			// Update element state
			if (!queue_find_and_update_element(SUBSCRIBE_ok_SUBACK_idle, SUBACK_ok_DELETE, mess_id, true))
			{				
				EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 92, 0);
			}
						
			g_broker_subscription_status = MQTT_SUBSCRIPTION_GRANTED_OK;
				
			// Check that the number of topics granted is equal to the number of topics requested
			if (g_broker_subscription_num_of_topics != num_of_topics_granted)
			{
				g_broker_subscription_status = MQTT_SUBSCRIPTION_ERROR_IN_SUBACK;
				EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 93, num_of_topics_granted);
			}
				
			// Check that requested QOS is the expected
			for (uint8_t i = 0; i < num_of_topics_granted; i++)
			{
				if (qos_granted[i] != MQTT_QOS_SUBSCRIBE)
				{
					g_broker_subscription_status = MQTT_SUBSCRIPTION_ERROR_IN_SUBACK;
					EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 94, 0);
				}
			}
					
			break;
		}
		case UNSUBACK:
			// Untested
			if ((res = MQTTDeserialize_unsuback(0, buffer, length)) != 1)
			{
				EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 95, res);
				return false;
			}
			
			// Update status
			if(!queue_find_and_update_element(UNSUBSCRIBE_ok_UNSUBACK_idle, UNSUBACK_ok_DELETE, 0, false))
			{
				EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 96, res);
				return false;
			}
			
			break;
			
		case PINGRESP:
			// Broker is alive
			g_broker_connection_status = MQTT_BROKER_CONNECTION_OK;
			g_broker_connection.last_ping_ts = g_time_epoch_y2k;
			g_broker_connection.ping_requested = false;
			
			// Update element state
			if(!queue_find_and_update_element(PINGREQ_ok_PINGRES_idle, PINGRES_ok_DELETE, 0, false))
			{
				EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_MQTT, 97, res);
				return false;
			}
			
			break;
		
		default:
			break;
	}
		
	CONSOLE_mqtt(buffer, length, RX_DIRECTION);
	
	return true;
}


bool process_mqtt_packet_length_and_type(uint8_t *buffer, uint16_t length, uint32_t *mqtt_packet_length, uint8_t *mqtt_packet_type)
{
	*mqtt_packet_length = 0;
	
	// Check if length is zero
	if (!length)
	{
		return false;
	}
	
	*mqtt_packet_type = (buffer[0] & 0xF0) >> 4;
	
	// Check if packet type is correct
	if( *mqtt_packet_type < CONNECT || *mqtt_packet_type > DISCONNECT)
	{
		return false;
	}
	
	uint32_t remaining_length = 0;
	uint32_t multiplier = 1;
	uint8_t byte = 0;
	
	switch (*mqtt_packet_type)
	{
		case CONNACK:
			// Length
			if (length < MQTT_PACKET_LENGTH_FIXED)
			{
				return false;
			}
				
			// Fixed header
			if (buffer[0] & 0x0F || buffer[1] != 0x02 )
			{
				return false;
			}
				
			// Variable header
			if (buffer[2] & 0xFE || buffer[3] > 0x05)
			{
				return false;
			}
				
			*mqtt_packet_length = MQTT_PACKET_LENGTH_FIXED;
			break;
		
		case PUBLISH:
			if (length < MQTT_PACKET_LENGTH_MIN)
			{
				return false;
			}
				
			// Fixed header - First byte
			// NOT CHECKED application specific
			
			
			// Fixed header - Second (or more) byte(s)
			do 
			{
				byte++;	
				
				if (byte + 1 > length)
				{
					return false;
				}
						
				remaining_length += (buffer[byte] & 127) * multiplier;
				multiplier *= 128;
				
				if (multiplier > REMAINING_LENGTH_MULTI_MAX)
				{
					// Malformed remaining length
					return false;
				}
				
			} while ((buffer[byte] & 128) != 0);
			
			if ((remaining_length + byte + 1) > length)
			{
				return false;
			}
				
			*mqtt_packet_length = remaining_length + byte + 1;
			break;
		
		case PUBACK:
		case UNSUBACK:
				
			// Length
			if (length < MQTT_PACKET_LENGTH_FIXED)
			{
				return false;
			}
				
			// Fixed header
			if (buffer[0] & 0x0F || buffer[1] != 0x02 )
			{
				return false;
			}
				
			*mqtt_packet_length = MQTT_PACKET_LENGTH_FIXED;
			break;	

		case SUBACK:
			if (length < MQTT_PACKET_LENGTH_MIN)
			{
				return false;
			}
			
			// Fixed header - First byte
			if (buffer[0] & 0x0F)
			{
				return false;
			}
			
			// Fixed header - Second (or more) byte(s)
			do
			{
				byte++;
				
				if (byte + 1 > length)
				{
					return false;
				}
				
				remaining_length += (buffer[byte] & 127) * multiplier;
				multiplier *= 128;
				
				if (multiplier > REMAINING_LENGTH_MULTI_MAX)
				{
					// Malformed remaining length
					return false;
				}
				
			} while ((buffer[byte] & 128) != 0);
			
			if ((remaining_length + byte + 1) > length)
			{
				return false;
			}
			
			*mqtt_packet_length = remaining_length + byte + 1;
			
			break;

		case DISCONNECT:
		case PINGRESP:
			if (length < MQTT_PACKET_LENGTH_MIN)
			{
				return false;
			}
				
			// Fixed header
			if (buffer[0] & 0x0F || buffer[1])
			{
				return false;
			}
				
			*mqtt_packet_length = MQTT_PACKET_LENGTH_MIN;
			break;
				
		default:
			return false;
	}
	
	return true;
}

uint16_t deserialize_puback_message_id(uint8_t *buffer)
{
	return ((buffer[2] << 8) | buffer[3]);
}

bool deserialize_connack(unsigned char return_code, unsigned char session)
{	
	bool res = true;
	
	switch (return_code)
	{
		case 0: // Connection accepted
			g_broker_connection_status = MQTT_BROKER_CONNECTION_OK_WAIT_FOR_RETAINED_MESSAGES;
			break;
		case 1: // Connection refused, unacceptable protocol version
			g_broker_connection_status = MQTT_BROKER_ERROR_PROTOCOL_VERSION;
			break;
		case 2: // Connection refused, identifier rejected
			g_broker_connection_status = MQTT_BROKER_ERROR_IDENTIFIER_REJECTED;
			break;
		case 3: // Connection refused, server unavailable
			g_broker_connection_status = MQTT_BROKER_ERROR_SERVER_UNAVAILABLE;
			break;
		case 4: // Connection refused, bad user name or password
			g_broker_connection_status = MQTT_BROKER_ERROR_BAD_USER_OR_PASS;
			break;
		case 5: // Connection refused, not authorized
			g_broker_connection_status = MQTT_BROKER_ERROR_NOT_AUTH;
			break;
		default: // Undefined
			g_broker_connection_status = MQTT_BROKER_ERROR_UNDEFINED;
			res = false;
			break;
	}
	
	return res;
}

// ********************************** Debug functions ***********************************

static void debug_queue(MQTT_Packet_t packet)
{
	print("DEBUG QUEUE starting...\r\n");
	
	uint16_t counter_empty_elements_by_id = 0;
	uint16_t counter_empty_elements_by_state = 0;
	uint16_t counter_empty_elements_by_topic = 0;
	
	for (uint16_t row = 0; row < MQTT_QUEUE_SIZE; row++)
	{
		if (!g_queue.buffer[row].id)
		{
			counter_empty_elements_by_id++;
		}
				
		if (!g_queue.buffer[row].state)
		{
			counter_empty_elements_by_state++;
		}
			
		if (!g_queue.buffer[row].topic)
		{
			counter_empty_elements_by_topic++;
		}
		
		switch(packet)
		{				
			case SUBSCRIBE:
				if (g_queue.buffer[row].state >= SUBSCRIBE_idle && g_queue.buffer[row].state <= SUBACK_ok_DELETE)
				{
					CONSOLE_queue(g_queue.buffer[row].id, g_queue.buffer[row].topic, g_queue.buffer[row].state, g_queue.buffer[row].mess_id);
				}
				break;
				
			default:
				CONSOLE_queue(g_queue.buffer[row].id, g_queue.buffer[row].topic, g_queue.buffer[row].state, g_queue.buffer[row].mess_id);
				break;
		}
	
	}
	
	print_uint16("DEBUG QUEUE NUM OF ELEMENTS BY ID ", MQTT_QUEUE_SIZE - counter_empty_elements_by_id);
	print_uint16("DEBUG QUEUE NUM OF ELEMENTS BY TOPIC ", MQTT_QUEUE_SIZE - counter_empty_elements_by_topic);
	print_uint16("DEBUG QUEUE NUM OF ELEMENTS BY STATE ", MQTT_QUEUE_SIZE - counter_empty_elements_by_state);
	print_uint16("DEBUG QUEUE HEAD - TAIL ", g_queue.head - g_queue.tail);
	print_uint16("DEBUG QUEUE FREE ELEMENTS ", g_queue.free_elements);
	
	print("DEBUG QUEUE ended!\r\n");
	// if (counter_empty_elements > 64)
	// return;
	
	//char s[20];
	//print(ctime(&g_system_time_epoch_y2k));
	//print("   DEBUG QUEUE empty elements: ");
	//print(utoa(MyQueue.free_elements, s, 10));
	//print("\t actual empty elements ");
	//print(utoa(counter_empty_elements, s, 10));
	//print("\t actual DUP elements ");
	//print(utoa(counter_dup_elements, s, 10));
	//print("\r\n");
}

