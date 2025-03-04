#include "ESP32.h"
#include "CONSOLE_log.h"
#include "MQTT_lib.h"

// Static variables
static Buffers_t g_buffers = {0};
static uint16_t g_ram = 0;

static BluFi_status_t g_blufi_status = BLUFI_DISABLED;
static Wifi_status_t g_wifi_status = WIFI_DISCONNECTED;
static Wifi_signal_status_t  g_wifi_signal_status = WIFI_NO_SIGNAL;
static Tcp_status_t g_tcp_status = TCP_CLOSED_AP_DISCONNECTED;

// Constant variables
static const char IPD_STR[] = "+IPD,";
static const char SEND_CHAR_STR[] = "> ";
static const char NL_STR = '\n';
static const char CR_STR = '\r';

//************************************************************************************
//******************************* STATIC FUNCTIONS ***********************************
//************************************************************************************
// Serialize "ciprecvdata"
static void serialize_cip_recv_data(char *buf, uint16_t length);
// Serialize WiFi connect
static void serialize_wifi_connect(char *buf, char *ssid, char *password);
// Serialize "cipstatus"
static void serialize_cipstatus(char *buf);
// Serialize "cipstart"
static void serialize_cipstart(char *buf, char *ip, uint16_t port);
// Serialize "cipsend"
static void serialize_cipsend(char *buf, uint16_t cipsend_length);
// Serialize "blufisend"
static void serialize_blufisend(char *buf, uint16_t blufisend_length);
// De-serialize TCP status
static void deserialize_tcp_status(char *line);
// De-serialize WiFi signal and status
static void deserialize_wifi_signal_and_status(char *line);
// De-serialize station IP
static void deserialize_station_ip(char *line);
// De-serialize SNTP time
static void deserialize_sntp_time(char *line);
// De-serialize RAM
static void deserialize_RAM(char *line);
// Wait for answer
static int8_t wait_for_answer(uint8_t wait_for, uint8_t retries_sec);
// Check for line
static int8_t is_line_found(uint8_t wait_for);
// Line process
static int8_t line_proccess(uint8_t wait_for);
// Actions when connection is closed
static void connection_closed();

//************************************************************************************
//************************************* UART *****************************************
//************************************************************************************

static bool ESP_rx_available()
{
    uint16_t rxBytes = SERCOM2_USART_ReadCountGet();
    
    if (rxBytes > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static void ESP_sendbuf(char * str)
{
	UART1_transmit_buf(str);
}

static void ESP_sendbuf_P(const char* str)
{	
	UART1_transmit_buf_P(str);
}

static void ESP_sendc(uint8_t data)
{
	UART1_transmit(data);
}

static bool ESP_is_buffer_overflow()
{
	return UART_rb1.ovf_flag;
}

//************************************************************************************
//************************************* GENERIC **************************************
//************************************************************************************

void ESP_debug()
{
    ESP_test_command("AT+CIPSNTPCFG=1,0\r\n");
    ESP_test_command("AT+CIPSNTPTIME?\r\n");
    //ESP_test_command("AT+BLUFINAME=\"MyESP32\"\r\n");
    ESP_test_at();
    ESP_wifi_set_station_mode();
    ESP_wifi_connect("COSMOTE-805961", "zeta1990");
    ESP_tcp_update_status();
    uint16_t ram = 0;
    ESP_get_RAM(&ram);
    ESP_tcp_open("135.181.146.21", 1883);
    ESP_tcp_update_status();
    ESP_wifi_update_status();
    ESP_test_at();
    ESP_tcp_close();
}


void ESP_init()
{	
	// Clear buffers
	g_buffers.general[0] = g_buffers.general_len = g_buffers.ipd[0] = g_buffers.ipd_len = 0;
	    
    // TODO: Set this to one for production
	ESP_reset(0); 

    // TODO: Delete this in production
	ESP_blufi_set_status(BLUFI_DISABLED);
	//ESP_wifi_disconnect();
}

int8_t ESP_self_test()
{
	int8_t res = ESP_RES_NOT_FOUND;
	uint16_t ram = 0;
	
	res = ESP_get_RAM(&ram);
	
	if (res == ESP_RES_OK && ram >= ESP_RAM_LIMIT)
	{
		// Self-Test OK
		return res;
	}
		
	// Reset
	res = ESP_reset(0);
	
	return res;
}

int8_t ESP_test_at()
{
	int8_t res = ESP_RES_NOT_FOUND;
	
	ESP_sendbuf_P(MEM_AT_COMMANDS[AT_TEST]);
	
	res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
	
	return res;
}

int8_t ESP_reset(uint8_t startup)
{
	int8_t res = ESP_RES_NOT_FOUND;
	
	if (!startup)
	{
		ESP_sendbuf_P(MEM_AT_COMMANDS[AT_RST]);
		
		// Wait for OK
		res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);	
	}
	
	// Wait for "ready"
	res = wait_for_answer(ESP_WAIT_FOR_RESET, ESP_TIMEOUT_RESET_READY);
	
	if ( res == ESP_RES_READY )
	{			
		if (startup)
		{
			// Wait for WIFI DISCONNECTED
			res = wait_for_answer(ESP_WAIT_FOR_WIFI, ESP_TIMEOUT_RESET_WIFI);	
		}
		
		if (res == ESP_RES_READY || res == ESP_RES_WIFI_DISCONNECTED)
		{
			// Wait for WIFI CONNECTED
			res = wait_for_answer(ESP_WAIT_FOR_WIFI, ESP_TIMEOUT_RESET_WIFI);	
			
			if( res == ESP_RES_WIFI_CONNECTED)
			{				
				// Wait for WIFI GOT IP
				res = wait_for_answer(ESP_WAIT_FOR_WIFI, ESP_TIMEOUT_RESET_WIFI);
				
				if (res == ESP_RES_WIFI_GOT_IP)
				{
					// SUCCESS
				}
				
			}
		} 
	}
		
	return res;
	
/*
Case 1 (startup = 0):
-------------------------------
AT+RST

OK
WIFI DISCONNECT
<??72?#@?yxJ?A?fw?y??>"??BfC??
ready
WIFI CONNECTED					//optional
WIFI GOT IP						//optional

Case 2 (startup = 1):
-------------------------------
<??72?#@?yxJ?A?fw?y??>"??BfC??
ready
WIFI DISCONNECT
WIFI CONNECTED					//optional
WIFI GOT IP						//optional

*/
}

int8_t ESP_get_RAM(uint16_t *ram)
{
	*ram = 0;
	
	int8_t res = ESP_RES_NOT_FOUND;
	
	ESP_sendbuf_P(MEM_AT_COMMANDS[AT_SYSRAM]);
	
	res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
	
	//print("ESP RAM 0");
	
	if ( res == ESP_RES_SYSRAM)
	{
		*ram = g_ram;
		
		// Wait for OK
		res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
		
		if (res == ESP_RES_OK)
		{
			// Success!
		}
	}
    	
	return res;
}

void ESP_test_command(char *command)
{
    int8_t res = ESP_RES_NOT_FOUND;
    
    ESP_sendbuf(command);
    
    res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);  
}

int8_t ESP_tcp_update_status()
{
	int8_t res = ESP_RES_NOT_FOUND;

	char cipstatus_buf[32] = {0};
		
	serialize_cipstatus(cipstatus_buf);
	
	// AT+CIPSTATUS
	ESP_sendbuf(cipstatus_buf);
	
	res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_ST_CIPSTART);
	
	// STATUS:<stat>
	if (res == ESP_RES_STATUS)
	{
		res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_ST_CIPSTART);
		
		// +CIPSTATUS:<linkID>,<type>,<remoteIP>,<remoteport>,<localport>,<tetype>
		if (res == ESP_RES_CIPSTATUS)
		{
			res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_ST_CIPSTART);
		}
	}
		
	return res;
}

//************************************************************************************
//************************************* TCP  *****************************************
//************************************************************************************

int8_t ESP_tcp_open(char *ip, uint16_t port)
{
	int8_t res = ESP_RES_NOT_FOUND;
	
	char cipstart_buf[64] = {0};
		
	serialize_cipstart(cipstart_buf, ip, port);
	
	// AT+CIPSTART=\"TCP\",\"192.168.33.10\",1883,20\r\n
	ESP_sendbuf(cipstart_buf);
	
	// CONNECT or ALREADY CONNECTED
	res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_ST_CIPSTART);

	switch(res)
	{
		// CONNECT
		case ESP_RES_CONNECT:
			
			g_tcp_status = TCP_OPEN;
			
			// OK
			res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
			
			if (res == ESP_RES_OK)
			{
				
			}
			break;
			
		// ALREADY CONNECTED
		case ESP_RES_ALREADY:
			
			g_tcp_status = TCP_OPEN;
			
			res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
					
			if (res == ESP_RES_ERROR)
			{
				res = ESP_RES_ALREADY;
			}
			
		break;
				
		default:
			break;
	}
	
	return res;
}



int8_t ESP_tcp_send_data(uint8_t *data, uint16_t length)
{		
	int8_t res = ESP_RES_NOT_FOUND;
	
	// Check that TCP connection is open
	if (g_tcp_status != TCP_OPEN)
	{
		return ESP_RES_CLOSED_TCP_CON;
	}
	
	// Check that there is no buffer overflow
	if (ESP_is_buffer_overflow())
	{
		return ESP_RES_UART_BUFFER_OVF;
	}
	
	char cipsend_buf[32] = {0};
		
	serialize_cipsend(cipsend_buf, length);
	
	// AT+CIPSEND=19
	ESP_sendbuf(cipsend_buf);
	
	res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
	
	
	if (res == ESP_RES_OK)
	{
		// Wait for "> " and then send data.	
		res = wait_for_answer(ESP_WAIT_FOR_TX_READY, ESP_TIMEOUT_SEND_OK);
		
		if (res == ESP_RES_SEND_CHAR)
		{
			
			// print_uint16("TX_READY ", length);
			
			for( uint16_t i = 0;  i < length; i++)
			{
				ESP_sendc(data[i]);
			}
			
			res = wait_for_answer(ESP_WAIT_FOR_TX_RES, ESP_TIMEOUT_SEND_OK);
			
			if (res == ESP_RES_SEND_OK)
			{
				// SUCCESS
			}
		}
	}
		
	return res;
}

int8_t ESP_tcp_receive_data(uint8_t **data, uint16_t *length)
{
	int8_t res = ESP_RES_NOT_FOUND;
		
	// Return buffered data first
	if (g_buffers.ipd_len)
	{
		*data = g_buffers.ipd; 
		*length = g_buffers.ipd_len;
		
		if (!g_buffers.ipd_buffer_full)
		{
			res = ESP_RES_IPD;
		}
		else
		{
			res = ESP_RES_IPD_BUFFER_OVF;
		}
		
	}
	// Check for new data
	else if(ESP_rx_available() || ESP_is_buffer_overflow())
	{
		res = is_line_found(ESP_WAIT_FOR_GENERAL);
		
		if (res == ESP_RES_GEN_UNKNOWN || res == ESP_RES_GEN_BUFFER_OVF)
		{
			res = line_proccess(ESP_WAIT_FOR_GENERAL);
		}
		else if (res == ESP_RES_IPD || res == ESP_RES_IPD_BUFFER_OVF || (res == ESP_RES_UART_BUFFER_OVF && g_buffers.ipd_len))
		{
			*data = g_buffers.ipd;
			*length = g_buffers.ipd_len;
		}	
	}
	
	return res;
}

int8_t ESP_tcp_cip_receive_data()
{
    int8_t res = ESP_RES_NOT_FOUND;
	
    uint16_t rx_length = 4;
    
    char cip_recv_data_buf[64] = {0};
    
    serialize_cip_recv_data(cip_recv_data_buf, rx_length);
    
    // AT+CIPRECVDATA=4
	ESP_sendbuf(cip_recv_data_buf);
	
	// Rx data
	res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
    
}

int8_t ESP_tcp_flush_data()
{
	int8_t res = ESP_RES_OK;
	
	// Clear Rx IPD buffer
	g_buffers.ipd_len = 0;
	g_buffers.ipd_buffer_full = false;
	
	return res;
}
int8_t ESP_tcp_close()
{
	int8_t res = ESP_RES_NOT_FOUND;
	
	ESP_sendbuf_P(MEM_AT_COMMANDS[AT_CIPCLOSE]);
	
	// CLOSED or ERROR
	res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
	
	if (res == ESP_RES_CLOSED)
	{
		g_tcp_status = TCP_CLOSED;
		
		res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
		
		// OK
		if (res == ESP_RES_OK)
		{
			
		}
	}
	
	return res;
}

Tcp_status_t ESP_tcp_status()
{
	return g_tcp_status;
}

int8_t ESP_tcp_sntp_time()
{
    int8_t res = ESP_RES_NOT_FOUND;
	
    // AT+CIPSNTPCFG=1,0
	ESP_sendbuf_P(MEM_AT_COMMANDS[AT_SNTP_CONFIG]);
	
	// OK
	res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
	
	if (res == ESP_RES_OK)
	{
        // AT+CIPSNTPTIME?
        ESP_sendbuf_P(MEM_AT_COMMANDS[AT_SNTP_TIME]);
        
        // +CIPSNTPTIME:Fri Sep 15 15:26:37 2023
		res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
		
		if (res == ESP_RES_SNTP_TIME)
		{
			res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
            
            if(res == ESP_RES_OK)
            {
                // Success
            }
		}
	}
	
	return res;
}

//************************************************************************************
//************************************* WIFI *****************************************
//************************************************************************************

int8_t ESP_wifi_set_station_mode()
{
    int8_t res = ESP_RES_NOT_FOUND;
                
    // AT+CWMODE=1
	ESP_sendbuf_P(MEM_AT_COMMANDS[AT_CWMODE_DEF_1]);
    
    // Wait for response
	res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
    
    if (res == ESP_RES_OK)
    {
        // Success
    }
}

int8_t ESP_wifi_connect(char *ssid, char *password)
{   
    int8_t res = ESP_RES_NOT_FOUND;
    
    char wifi_connect_buf[128] = {0};
    
    serialize_wifi_connect(wifi_connect_buf, ssid, password);
            
    // AT+CWJAP_DEF?
	ESP_sendbuf(wifi_connect_buf);
    
    // Wait for response
	res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
    
}

// Disconnect WiFi
int8_t ESP_wifi_disconnect()
{
    int8_t res = ESP_RES_NOT_FOUND;
                
    // AT+CWQAP
	ESP_sendbuf_P(MEM_AT_COMMANDS[AT_CWQAP]);
    
    // Wait for response
	res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
    
    if (res == ESP_RES_OK)
    {
        ESP_blufi_set_status(BLUFI_ENABLED);
        // Success!
    }
}

int8_t ESP_wifi_update_status(void)
{	
	int8_t res = ESP_RES_NOT_FOUND;
	
	// AT+CWJAP_DEF?
	ESP_sendbuf_P(MEM_AT_COMMANDS[AT_CWJAP_DEF_Q]);
	
	// Wait for No App or +CWJAP:
	res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
	
	if ( res == ESP_RES_NO_AP  ||  res == ESP_RES_CWJAP )
	{
		// Wait for OK
		res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
		
		if(res == ESP_RES_OK)
		{	
			// AT+CIFSR
			ESP_sendbuf_P(MEM_AT_COMMANDS[AT_CIFSR]);
			
			// Wait for +CIFSR:STAIP	
			res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
			
			if (res == ESP_RES_CIFSR_STATION_IP)
			{
				// Wait for OK
				res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
			}
		}
	}
    
	return res;
}

Wifi_status_t ESP_wifi_status()
{
	return g_wifi_status;
}

Wifi_signal_status_t ESP_wifi_signal_status()
{
	return g_wifi_signal_status;
}

//************************************************************************************
//************************************* BluFi ****************************************
//************************************************************************************

// Start BluFi
int8_t ESP_blufi_start()
{
    int8_t res = ESP_RES_NOT_FOUND;
    
    // AT+BLUFI=1
    ESP_sendbuf_P(MEM_AT_COMMANDS[AT_BLUFI_START]);

    // Wait for OK
    res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);

    if (res == ESP_RES_OK)
    {
        // Success!
    }
}

// Stop BluFi
int8_t ESP_blufi_stop()
{
    int8_t res = ESP_RES_NOT_FOUND;
    
    // AT+BLUFI=0
	ESP_sendbuf_P(MEM_AT_COMMANDS[AT_BLUFI_STOP]);
	
	// Wait for OK
	res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);
    
    if (res == ESP_RES_OK)
	{
        // Success!
    }
}

int8_t ESP_blufi_send()
{
    int8_t res = ESP_RES_NOT_FOUND;
    uint16_t length = 2;
    char s[10];
    char cipsend_buf[32] = {0};
		
	serialize_blufisend(cipsend_buf, length);
	
	// AT+BLUFISEND=1
	ESP_sendbuf(cipsend_buf);
    
    // Wait for "> " and then send data.	
    res = wait_for_answer(ESP_WAIT_FOR_TX_READY, ESP_TIMEOUT_SEND_OK);

    if (res == ESP_RES_SEND_CHAR)
    {        
        sprintf(s, "W%d", ESP_wifi_status());
        ESP_sendbuf(s);
             
        res = wait_for_answer(ESP_WAIT_FOR_GENERAL, ESP_TIMEOUT_AT_COMMAND);

        if (res == ESP_RES_OK)
        {
            // SUCCESS
        }
    }

}

void ESP_blufi_set_status(BluFi_status_t blufi_status)
{
    g_blufi_status = blufi_status;
}

BluFi_status_t ESP_blufi_status()
{
    return g_blufi_status;
}

//************************************************************************************
//************************************* SERIALIZE ************************************
//************************************************************************************

static void serialize_cip_recv_data(char *buf, uint16_t length)
{
    buf[0] = 0x00;
	char s[10];
    
	strcat(buf, MEM_AT_COMMANDS[AT_CIPRECVDATA]);
    sprintf(s, "%d", length);
	strcat(buf, s);
	strcat(buf, MEM_AT_COMMANDS[AT_CAR_RET_NEW_LINE]);
}

static void serialize_wifi_connect(char *buf, char *ssid, char *password)
{
    buf[0] = 0x00;
	
	strcat(buf, MEM_AT_COMMANDS[AT_CWJAP_DEF_1]);
	strcat(buf, ssid);
	strcat(buf, MEM_AT_COMMANDS[AT_CWJAP_DEF_2]);
    strcat(buf, password);
	strcat(buf, MEM_AT_COMMANDS[AT_CWJAP_DEF_3]);
}

static void serialize_cipstatus(char *buf)
{
	buf[0] = 0x00;	
	strcat(buf, MEM_AT_COMMANDS[AT_CIPSTATUS]);
}

static void serialize_cipstart(char *buf, char *ip, uint16_t port)
{
	char s[20];
	buf[0] = 0x00;
	
	strcat(buf, MEM_AT_COMMANDS[AT_CIPSTART_1]);
	strcat(buf, ip);
	strcat(buf, MEM_AT_COMMANDS[AT_CIPSTART_2]);
    sprintf(s, "%d", port);
    strcat(buf, s);
	strcat(buf, MEM_AT_COMMANDS[AT_CIPSTART_3]);
}

static void serialize_cipsend(char *buf, uint16_t cipsend_length)
{
	char s[20] = {0};
	buf[0] = 0x00;
		
	strcat(buf, MEM_AT_COMMANDS[AT_CIPSEND]);
    sprintf(s, "%d", cipsend_length);
    strcat(buf, s);
	strcat(buf, MEM_AT_COMMANDS[AT_CAR_RET_NEW_LINE]);
}

static void serialize_blufisend(char *buf, uint16_t blufisend_length)
{
    char s[20] = {0};
	buf[0] = 0x00;
		
	strcat(buf, MEM_AT_COMMANDS[AT_BLUFI_SEND]);
    sprintf(s, "%d", blufisend_length);
    strcat(buf, s);
	strcat(buf, MEM_AT_COMMANDS[AT_CAR_RET_NEW_LINE]);
}

//************************************************************************************
//*********************************** DESERIALIZE ************************************
//************************************************************************************

static void deserialize_tcp_status(char *line)
{	
	uint8_t len = strlen(line);
	
	char status_str[2] = {};
	
	//STATUS:4

	if (len >= 8)
	{
		status_str[0] = line[7];
			
		int8_t status =  atoi(status_str);
		
		/*
        0: The ESP32 station is not initialized.
        1: The ESP32 station is initialized, but not started a Wi-Fi connection yet.
        2: The ESP32 station is connected to an AP and its IP address is obtained.
        3: The ESP32 station has created a TCP/SSL transmission.
        4: All of the TCP/UDP/SSL connections of the ESP32 station are disconnected.
        5: The ESP32 station started a Wi-Fi connection, but was not connected to an AP or disconnected from an AP.
		*/
				
		switch (status)
		{
            case 0:
                g_tcp_status = TCP_CLOSED;
                break;
            
            case 1:
                g_tcp_status = TCP_CLOSED;
                break;
                
			case 2:
				g_tcp_status = TCP_CLOSED_AP_IP_OK;
				break;	
			
			case 3:
				g_tcp_status = TCP_OPEN;
				break;
			
			case 4:
				g_tcp_status = TCP_CLOSED_TCP_DISCONNECTED;
				break;
			
			case 5:
				g_tcp_status = TCP_CLOSED_AP_DISCONNECTED;
				break;
			
			default:
				break;
		}
	}
}

static void deserialize_wifi_signal_and_status(char * line)
{   
    // +CWJAP:<ssid>,<bssid>,<channel>,<rssi>,<pci_en>,<reconn_interval>,<listen_interval>,<scan_mode>,<pmf>
	// +CWJAP:"COSMOTE-012292","28:ff:3e:01:22:92",6,-70,0,1,3,0,1
	// +CWJAP:1\r\n
	// No Ap\r\n
	// +CWJAP_DEF:"COSMOTE-012292","28:ff:3e:01:22:92",12,31
/*
		-30 dBm	Amazing	
		-67 dBm	Very Good	
		-70 dBm	Okay	
		-80 dBm	Not Good	
		-90 dBm	Unusable	
*/	
	
	const char del[2] = ",";
	char * token = NULL;
	char * token_list[20];
	int index = 0;
	
	uint16_t len = strlen(line);
	char wifi[4] = {0,0,0,0};
	int16_t wifi_rssi = 0;
	
	if( len > 10 )
	{		
		token = strtok(line, del);
		token_list[index++] = token;
		
		while( token != NULL )
		{
			token = strtok(NULL, del);
			token_list[index++] = token;
		}
		
		strcpy(wifi, token_list[index - 7]);

		wifi_rssi = atoi(wifi);
	
		if (wifi_rssi >= -30 && wifi_rssi < 0)
		{
			g_wifi_signal_status = WIFI_SIGNAL_AMAZING;
			
			if (!g_wifi_status)
			{
				g_wifi_status = WIFI_CONNECTED;
			}
		}
		else if (wifi_rssi >= -67 && wifi_rssi < -30)
		{
			g_wifi_signal_status = WIFI_SIGNAL_VERY_GOOD;
			
			if (!g_wifi_status)
			{
				g_wifi_status = WIFI_CONNECTED;
			}
		}
		else if (wifi_rssi >= -70 && wifi_rssi < -67)
		{
			g_wifi_signal_status = WIFI_SIGNAL_OK;
			if (!g_wifi_status)
			{
				g_wifi_status = WIFI_CONNECTED;
			}
		}
		else if (wifi_rssi >= -80 && wifi_rssi < -70)
		{
			g_wifi_signal_status = WIFI_SIGNAL_NOT_GOOD;
			if (!g_wifi_status)
			{
				g_wifi_status = WIFI_CONNECTED;
			}
		}
		else if (wifi_rssi >= -90 && wifi_rssi < -80)
		{
			g_wifi_signal_status = WIFI_SIGNAL_UNUSABLE;
			g_wifi_status = WIFI_DISCONNECTED;
		}
		else
		{
			g_wifi_signal_status = WIFI_NO_SIGNAL;
			g_wifi_status = WIFI_DISCONNECTED;
		}
	}
	else
	{
		g_wifi_signal_status = WIFI_NO_SIGNAL;
		g_wifi_status = WIFI_DISCONNECTED;
	}
    
    //CONSOLE_log_int("WiFi Rssi: ", (int32_t)wifi_rssi);
    //CONSOLE_log("WiFi Signal: ", (uint32_t)g_wifi_signal_status);
    //CONSOLE_log("WiFi Status: ", (uint32_t)g_wifi_status);
}

static void deserialize_station_ip(char *line)
{
	// +CIFSR:STAIP,"192.168.43.207"
    // +CIFSR:STAMAC,"cc:50:e3:a1:4b:e0"

	char ip[20];
	const char quote = '"';
	const char no_ip [] = "0.0.0.0";
	
	memset(ip, '\0', sizeof(ip));
	char * start = strchr((const char *)line, quote);
	char * end = strrchr((const char *)line, quote);
	strncpy(ip, (const char *)start + 1, end - (start + 1));
	
	if (strcmp((const char *)ip, no_ip))
	{
		g_wifi_status = WIFI_GOT_IP;
	}
	else
	{
		// g_wifi_status = WIFI_DISCONNECTED;
	}
	
	//print_int8("WIFI STATUS ", g_wifi_status);
}

static void deserialize_sntp_time(char *line)
{
    // +CIPSNTPTIME:Fri Sep 15 15:26:37 2023
    
    //const char* ascii_time = "Sat Sep 15 14:45:30 2023";
    char s[50];
    char ascii_time[50];
    struct tm timeinfo = {};
    const char* short_months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char day[4], month[4], day_num[3], year[5], time_str[9]= {};
    uint16_t hour, minute, second = 0;
    uint32_t res = 0;
    
    char *colon_position = strchr(line, ':');
    
    if (colon_position != NULL) 
    {
        // Calculate the length of the first part
        size_t first_part_length = colon_position - line;
        
        // Create a buffer for the first part and copy it
        char first_part[first_part_length + 1];
        strncpy(first_part, line, first_part_length);
        first_part[first_part_length] = '\0'; // Null-terminate the string

        strncpy(ascii_time, colon_position + 1, 25);
        ascii_time[24] = '\0';
       
        
        // Parse the asciitime style time string manually, e.g. "Fri Sep 15 15:26:37 2023"
        res = sscanf(ascii_time, "%s %s %s %s %s", day, month, day_num, &time_str, year);

        if (res == 5) 
        {
            // Parse the time, e.g. 15:26:37
            int length = strlen(time_str);
            int current_value = 0;
            int value_multiplier = 1;

            for (int i = length - 1; i >= 0; i--) 
            {
                if (time_str[i] == ':') 
                {
                    if (i == 2) 
                    { // Colons are at positions 2 and 5
                        minute = current_value;
                    } 
                    else if (i == 5) 
                    {
                        second = current_value;
                    }
                    current_value = 0;
                    value_multiplier = 1;
                } 
                else 
                {
                    current_value += (time_str[i] - '0') * value_multiplier;
                    value_multiplier *= 10;
                }
            }
            // The remaining part of the string represents the seconds
            hour = current_value;

            // Convert the month abbreviation to its corresponding number
            for (int i = 0; i < 12; i++) 
            {
                if (strcmp(month, short_months[i]) == 0) 
                {
                    timeinfo.tm_mon = i;
                    break;
                }
            }

            // Adjust values for the struct tm
            timeinfo.tm_mday = atoi(day_num);
            timeinfo.tm_hour = hour;
            timeinfo.tm_min = minute;
            timeinfo.tm_sec = second;
            timeinfo.tm_year = atoi(year) - 1900;

            // Convert the tm struct to epoch time
            time_t sntp_time = Time2Epoch(&timeinfo);
            
            if (sntp_time > 1000000000)
            {
                /*
                print("SNTP TIME: ");
                sprintf(s, "%u", sntp_time);
                print(s);
                print("\r\n");
                */
                
                g_time_epoch_y2k = sntp_time;
                
                // Success
                return;
            }
            

        }           
    }
    
    print("Failed to parse the time string\r\n");
    sprintf(s, "%d", res);
    print(s);
    print("\r\n");
        
    return;
}

static void deserialize_RAM(char *line)
{
	// +SYSRAM:30976, 156432\r\n
	const uint8_t str_offset = 8;
	
	uint8_t i = 0;
	char ram_str[30];

	do
	{
		ram_str[i] = line[i + str_offset];
		
	} while(  (line[str_offset + 1 + i++] != ',' ) );

	ram_str[i] = 0x00;
	
	g_ram = atoi(ram_str);
		
	return;
}

//************************************************************************************
//*************************** AT COMMAND FUNCTIONS ***********************************
//************************************************************************************

static int8_t is_line_found(uint8_t wait_for)
{
	/*	
		Returns
		ESP_RES_NOT_FOUND		No byte
		
		ESP_RES_UNKNOWN			AT COMMAND
		ESP_RES_IPD				IPD packet
		
		ESP_RES_UART_BUFFER_OVF		
		ESP_RES_IPD_BUFFER_OVF	
		ESP_RES_IPD_CORRUPTED_PKT // TODO
		ESP_RES_PARTIAL_GEN_PKT // TODO	
		ESP_RES_PARTIAL_IPD_PKT // TODO
		ESP_RES_GEN_BUFFER_OVF
		
		Event Logger
		#1 - 80 - Error! UART buffer full
		#2 - 81 - Error! AT COMMAND buffer is full
		#3 - 82 - Error! IPD buffer full
		#4 - 83 - Error! IPD number is corrupted
		#5 - 84 - Error! Partial IPD packet. This is an error from ESP8266. This library do not store partial IPD packets		
	*/
	
	int8_t res = ESP_RES_NOT_FOUND;
		
	// Clear Rx buffer
	g_buffers.general[0] = 0x00;
	g_buffers.general_len = 0;
			
	// Static local variables for IPD
	Rx_bytes_state_t rx_bytes_type = RX_BYTES_AT_COMMAND;
	uint16_t ipd_rx_bytes = 0;
	
	char ipd_rx_bytes_str[ESP_BUFFER_IPD_NUMBER_MAX_LEN] = {0};
	uint8_t ipd_str_index = 0;
	uint8_t ipd_rx_bytes_str_index = 0;
	uint8_t byte = 0;
	bool end_of_line = false;
		
	//SERCOM5_USART_Write("TEST1", 5);
        
    // Clear TC0
    TC0_Timer32bitCounterSet(0);
    uint32_t tc0 = TC0_Timer32bitCounterGet();
    
    // NOTE: 100000000ns (100ms)/ 17066.6666667ns (per tick) = 5859 ticks
    
	//while(ESP_TIMER < ESP_TIMEOUT_BYTE_TICKS)
    while(TC0_Timer32bitCounterGet() <= 5859) // 100 ms
	{        
		while((UART_rb1.head != UART_rb1.tail) || UART_read_polling())
		{
            // Clear TC0
            TC0_Timer32bitCounterSet(0);
            
			// Get byte
			byte = UART_rb1.buf[UART_rb1.tail];
																
			switch (rx_bytes_type)
			{
				case RX_BYTES_AT_COMMAND:
					
					res = ESP_RES_GEN_PARTIAL;
					
					// **** Copy byte to Rx buffer **********
					g_buffers.general[g_buffers.general_len++] = (char)byte;
					
					// Check for buffer overflow
					if (g_buffers.general_len >= ESP_BUFFER_SIZE)
					{
						g_buffers.general_buffer_full = true;
						res = ESP_RES_GEN_BUFFER_OVF;
						end_of_line = true;
						break;
					}
								
					// Check for IPD
					if(byte == IPD_STR[ipd_str_index])
					{
						// If found comma ',' change state
						if(ipd_str_index == 4)
						{
							g_buffers.general_len -= strlen(IPD_STR); // TBC
							ipd_rx_bytes_str_index = 0;
							res = ESP_RES_IPD;
							rx_bytes_type = RX_BYTES_IPD_HEADER;
							break;
						}
						
						ipd_str_index++;
					}
					else
					{
						ipd_str_index = 0;
					}
				
					// Check if is end of line (i.e. '\r\n' or '> ')
					if (g_buffers.general_len >= 2)
					{
						//if (wait_for == ESP_WAIT_FOR_TX_READY && g_buffers.general[g_buffers.general_len - 1] == SEND_CHAR_STR[1] && g_buffers.general[g_buffers.general_len - 2] == SEND_CHAR_STR[0])
                        if (wait_for == ESP_WAIT_FOR_TX_READY && g_buffers.general[g_buffers.general_len - 1] == SEND_CHAR_STR[0])
						{	
							// Add String null terminator
							g_buffers.general[g_buffers.general_len++] = 0x00;
							res = ESP_RES_GEN_UNKNOWN;
							end_of_line = true;
						}

						if (wait_for != ESP_WAIT_FOR_TX_READY && g_buffers.general[g_buffers.general_len - 1] == NL_STR && g_buffers.general[g_buffers.general_len - 2] == CR_STR)
						{
							// Add String null terminator
							g_buffers.general[g_buffers.general_len++] = 0x00;
							res = ESP_RES_GEN_UNKNOWN;
							end_of_line = true;
						}
						
					}
					break;

				case RX_BYTES_IPD_HEADER:
					
					res = ESP_RES_IPD_PARTIAL;
					
					ipd_rx_bytes_str[ipd_rx_bytes_str_index++] = byte;
					
					if (ipd_rx_bytes_str_index > ESP_BUFFER_IPD_NUMBER_MAX_LEN + 1)
					{
						res = ESP_RES_IPD_CORRUPTED; // Something bad happened!
						end_of_line = true;
					}
					
					if(byte == ':')
					{
						// Convert number of bytes received from IPD
						ipd_rx_bytes_str[ipd_rx_bytes_str_index - 1] = 0x00;
						ipd_rx_bytes = atoi(ipd_rx_bytes_str);
						
						if (g_buffers.ipd_buffer_full)
						{
							rx_bytes_type = RX_BYTES_IPD_DATA_DISCARD;
						}
						else
						{
							rx_bytes_type = RX_BYTES_IPD_DATA;	
						}
					}
								
					break;

				case RX_BYTES_IPD_DATA:
					
					g_buffers.ipd[g_buffers.ipd_len++] = byte;					
					ipd_rx_bytes--;
					
					if (!ipd_rx_bytes)
					{
						rx_bytes_type = RX_BYTES_AT_COMMAND;
						res = ESP_RES_IPD;
						end_of_line = true;
						break;
					}
							
					if (g_buffers.ipd_len >= ESP_BUFFER_IPD_SIZE)
					{
						rx_bytes_type = RX_BYTES_IPD_DATA_DISCARD;
						g_buffers.ipd_buffer_full = true;
					}			
					break;
				
				case RX_BYTES_IPD_DATA_DISCARD:
					
					ipd_rx_bytes--;
					res = ESP_RES_IPD_BUFFER_OVF;
					
					if (!ipd_rx_bytes)
					{
						end_of_line = true;
						break;
					}
					break;
			}
			
			UART_rb1.tail++;
			UART_rb1.tail &= UART_BUFFER_MASK_1;				
            
#ifdef DEBUG_PRINT
                        
			if (rx_bytes_type == RX_BYTES_IPD_DATA || rx_bytes_type == RX_BYTES_IPD_DATA_DISCARD)
			{
				print_ascii_c_nl(byte);
			}
			else if (rx_bytes_type == RX_BYTES_AT_COMMAND && ipd_rx_bytes_str_index)
			{
				print_ascii_c_nl(byte);
			} 
			else
			{
				print_c(byte);
			}
            
			if (end_of_line && rx_bytes_type == RX_BYTES_AT_COMMAND && ipd_rx_bytes_str_index) { print("\r\n"); }
#endif	
										
			if (end_of_line)
			{
				goto EXIT;
			}
		}
				
		// UART buffer full
		if (UART_rb1.ovf_flag && UART_rb1.tail == UART_rb1.head)
		{
			UART_rb1.ovf_flag = 0;
			res = ESP_RES_UART_BUFFER_OVF;
			goto EXIT;
		}
	}
	
	EXIT:
	
	// Check for keyword (i.e. "CLOSED\r\n")
	if (UART_rb1.key_found_flag)
	{
		connection_closed();
		UART_rb1.key_found_flag = 0;
	}
	
	return res;
}

static int8_t wait_for_answer(uint8_t wait_for, uint8_t retries_sec)
{
	//-1:NOT FOUND,  0:ERROR,  1:OK
	int8_t  res = ESP_RES_NOT_FOUND;
	int16_t retries_100_ms = retries_sec * 10;
	
	do
	{
		res = is_line_found(wait_for);
		
        //char temp[32] = {};
        //sprintf((char*)temp, "%d L \r\n", res);
        //SERCOM5_USART_Write(temp, strlen((const char*)temp));
        
		if (res == ESP_RES_GEN_UNKNOWN)
		{
			res = line_proccess(wait_for);
            
		}
		else
		{               
			ESP_WDT_RESET;
			retries_100_ms--;
			if(!retries_100_ms)
			{
				break;
			}
		}
				
	} while (res == ESP_RES_NOT_FOUND || res == ESP_RES_BUSY_P || res == ESP_RES_BUSY_S || res == ESP_RES_IPD || res == ESP_RES_IPD_BUFFER_OVF);
	
	return res;
}

/*

if (is_line_found(wait_for) == ESP_RES_UNKNOWN)
{
	res = line_proccess(wait_for);
}
*/


static int8_t line_proccess(uint8_t wait_for)
{
	int8_t res = ESP_RES_NOT_FOUND;
                
	// CLOSED
	if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_CLOSED]))
	{
		res = ESP_RES_CLOSED;
		connection_closed();
	}

	// WIFI DISCONNECTED
	if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_WIFI_DISCONNECTED]))
	{
		g_wifi_status = WIFI_DISCONNECTED;
		// g_tcp_status = TCP_CLOSED_AP_DISCONNECTED;
	}
	
	// WIFI CONNECTED
	if(strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_WIFI_CONNECTED]))
	{
		g_wifi_status = WIFI_CONNECTED;
	}
	
	// WIFI GOT IP
	if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_WIFI_GOT_IP]))
	{
		g_wifi_status = WIFI_GOT_IP;
	}
		
	switch(wait_for)
	{
		case ESP_WAIT_FOR_GENERAL:
		
			if(!strcmp(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_OK]))
			{
				return ESP_RES_OK;
			}
			else if (!strcmp(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_ERROR]))
			{
				return ESP_RES_ERROR;
			}
			else if (!strcmp(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_BUSY_P]))
			{
				return ESP_RES_BUSY_P;
			}
			else if (!strcmp(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_BUSY_S]))
			{
				return ESP_RES_BUSY_S;
			}
			else if (!strcmp(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_CONNECT]))
			{
				return ESP_RES_CONNECT;
			}
			else if (!strcmp(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_ALREADY]))
			{
				return ESP_RES_ALREADY;
			}
			else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_CWLAP]))
			{
				return ESP_RES_CWLAP;
			}
			else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_SYSRAM]))
			{
				deserialize_RAM(g_buffers.general);
				return ESP_RES_SYSRAM;
			}
			else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_CWJAP]))
			{
				deserialize_wifi_signal_and_status(g_buffers.general);
				return ESP_RES_CWJAP;
			}
			else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_CIFSR_STATION_IP]))
			{
				deserialize_station_ip(g_buffers.general);
				return ESP_RES_CIFSR_STATION_IP;
			}
			else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_CIPSTATUS]))
			{
				// +CIPSTATUS:0,"TCP","135.181.146.21",1883,56003,0
				//deserialize_tcp_cipstatus(g_buffers.general); // Not implemented!
				return ESP_RES_CIPSTATUS;
			}
			else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_STATUS]))
			{
                // STATUS:3
				deserialize_tcp_status(g_buffers.general);
				return ESP_RES_STATUS;
			}
			else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_NO_AP]))
			{
				deserialize_wifi_signal_and_status(g_buffers.general);
				return ESP_RES_NO_AP;
			}
            else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_SNTP_TIME]))
			{
				deserialize_sntp_time(g_buffers.general);
				return ESP_RES_SNTP_TIME;
			}
			else
			{
                //SERCOM5_USART_Write("TEST NOT FOUND", 14);
				return ESP_RES_NOT_FOUND;
			}
			break;
		
		case ESP_WAIT_FOR_TX_READY: // Waiting for '>' (i.e. tx ready)
            
			if(strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_SEND_CHAR]))
			{
				return ESP_RES_SEND_CHAR;
			}
			break;
		
		case ESP_WAIT_FOR_TX_RES:  // Waiting for response after sending a packet
		
            if(strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_SEND_OK]))
            {
                return ESP_RES_SEND_OK;
            }
            else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_SEND_FAIL]))
            {
                return ESP_RES_SEND_FAIL;
            }
            else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_ERROR]))
            {
                return ESP_RES_ERROR;
            }
            else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_BUSY_P]))
            {
                return ESP_RES_BUSY_P;
            }
            else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_BUSY_S]))
            {
                return ESP_RES_BUSY_S;
            }
            else
            {
                return ESP_RES_NOT_FOUND;
            }
            break;
		
		case ESP_WAIT_FOR_RESET: // Waiting after reset
		
            if(strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_READY]))
            {
                return ESP_RES_READY;
            }
            else
            {
                return ESP_RES_NOT_FOUND;
            }
            break;
		
		case ESP_WAIT_FOR_WIFI: // Waiting for WiFi related response
		
            if(strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_WIFI_CONNECTED]))
            {
                return ESP_RES_WIFI_CONNECTED;
            }
            else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_WIFI_GOT_IP]))
            {
                return ESP_RES_WIFI_GOT_IP;
            }
            else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_WIFI_DISCONNECTED]))
            {
                return ESP_RES_WIFI_DISCONNECTED;
            }
            else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_CWJAP]))
            {
                deserialize_wifi_signal_and_status(g_buffers.general);
                return ESP_RES_CWJAP;
            }
            else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_SMART_CON_CONNECTED]))
            {
                return ESP_RES_SMART_CON_CONNECTED;
            }
            else if (strstr(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_SMART_CON_FAILED]))
            {
                return ESP_RES_SMART_CON_FAILED;
            }
            else if (!strcmp(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_BUSY_P]))
            {
                return ESP_RES_BUSY_P;
            }
            else if (!strcmp(g_buffers.general, MEM_AT_COMMANDS_RES[AT_RES_BUSY_S]))
            {
                return ESP_RES_BUSY_S;
            }
            else
            {
                return ESP_RES_NOT_FOUND;
            }
            break;
		
		default:
            break;
	}
	
	return res;
}

static void connection_closed()
{
	g_tcp_status = TCP_CLOSED;
	// g_wifi_status = WIFI_DISCONNECTED;
}

// ************************************** AT COMMANDS **********************************************

const char at_test[] = "AT\r\n";
const char at_reset[] = "AT+RST\r\n";
const char at_cip_set_sntp[] = "AT+CIPSNTPCFG=1,3\r\n";
const char at_cip_ssl_conf[] = "AT+CIPSSLCCONF=0\r\n";
const char at_cipstart_1_ssl[] = "AT+CIPSTART=\"SSL\",\"";
const char at_cipstart_1[] = "AT+CIPSTART=\"TCP\",\"";
const char at_cipstart_2[] = "\",";
const char at_cipstart_3[] = "\r\n";
const char at_cipstatus[] = "AT+CIPSTATUS\r\n";
const char at_cipmux_1[] = "AT+CIPMUX=1\r\n";
const char at_cipservermaxconn_1[] = "AT+CIPSERVERMAXCONN=1\r\n";
const char at_cipserver_1_80[] = "AT+CIPSERVER=1,80\r\n";
const char at_cipsend[] = "AT+CIPSEND=";
const char at_cipclose_[] = "AT+CIPCLOSE";
const char at_cipclose[] = "AT+CIPCLOSE\r\n";
const char at_cwmode_cur_1[] = "AT+CWMODE_CUR=1\r\n";
const char at_cwmode_cur_2[] = "AT+CWMODE_CUR=2\r\n";
const char at_cwmode_cur_3[] = "AT+CWMODE_CUR=3\r\n";
const char at_cwmode_def_1[] = "AT+CWMODE=1\r\n";
const char at_wps_0[] = "AT+WPS=0\r\n";
const char at_wps_1[] = "AT+WPS=1\r\n";
const char at_sysram[] = "AT+SYSRAM?\r\n";
const char at_cwlap[] = "AT+CWLAP\r\n";
const char at_cwqap[] = "AT+CWQAP\r\n"; // DIsconnects from the AP (temporary)
const char at_smart_con_start[] = "AT+CWSTARTSMART=1\r\n";
const char at_smart_con_stop[] = "AT+CWSTOPSMART\r\n";
const char at_mdns[] = "AT+MDNS=1,\"mynetwork\",\"unit\",8080\r\n";
const char at_cwjap_def_Q[] = "AT+CWJAP?\r\n";
const char at_cwjap_def_1[]	= "AT+CWJAP=\"";
const char at_cwjap_def_2[]	= "\",\"";
const char at_cwjap_def_3[]	= "\"\r\n";
const char at_cwsap_def_Q[] = "AT+CWSAP_DEF?\r\n";
const char at_cwsap_def_1[] = "AT+CWSAP_DEF=\"";
const char at_cwsap_def_2[]	= "\",\"";
const char at_cwsap_def_3[]	= "\",1,3\r\n";
const char at_cifsr[] = "AT+CIFSR\r\n";
const char at_ciprecvdata[] = "AT+CIPRECVDATA=";
const char at_blufi_start[] = "AT+BLUFI=1\r\n";
const char at_blufi_stop[] = "AT+BLUFI=0\r\n";
const char at_blufi_send[] = "AT+BLUFISEND=";
const char at_sntp_config[] = "AT+CIPSNTPCFG=1,0\r\n";
const char at_sntp_time[] = "AT+CIPSNTPTIME?\r\n";
const char at_comma[] = ",";
const char at_equal[] = "=";
const char at_carr_return_new_line[]= "\r\n";

const char* const MEM_AT_COMMANDS[NUM_OF_AT_COMMANDS] = {
	at_test, at_reset, at_cip_set_sntp, at_cip_ssl_conf, at_cipstart_1_ssl,
	at_cipstart_1, at_cipstart_2, at_cipstart_3, at_cipstatus, at_cipmux_1,
	at_cipservermaxconn_1, at_cipserver_1_80, at_cipsend, at_cipclose_, at_cipclose,
	at_cwmode_cur_1, at_cwmode_cur_2, at_cwmode_cur_3, at_cwmode_def_1, at_wps_0,
	at_wps_1, at_sysram, at_cwlap, at_cwqap, at_smart_con_start,
	at_smart_con_stop, at_mdns, at_cwjap_def_Q,	at_cwjap_def_1, at_cwjap_def_2,
	at_cwjap_def_3, at_cwsap_def_Q, at_cwsap_def_1, at_cwsap_def_2, at_cwsap_def_3,
	at_cifsr, at_ciprecvdata, at_blufi_start, at_blufi_stop, at_blufi_send, at_sntp_config,
    at_sntp_time, at_comma,  at_equal, at_carr_return_new_line
};

// ************************************** AT COMMANDS RESPONSES ******************************************

const char atRes_ok[] = "OK\r\n";
const char atRes_error[] = "ERROR\r\n";
const char atRes_busy_p[] = "busy p...\r\n";
const char atRes_busy_s[] = "busy s...\r\n";
const char atRes_ready[] = "ready\r\n";
const char atRes_send_char[] = ">";
const char atRes_send_ok[] = "SEND OK\r\n";
const char atRes_send_fail[] = "SEND FAIL\r\n";
const char atRes_fail[] = "FAIL\r\n";
const char atRes_connect[] = "CONNECT\r\n";
const char atRes_already_connected[] = "ALREADY CONNECTED\r\n";
const char atRes_closed[] = "CLOSED\r\n";
const char atRes_ipd[] = "+IPD";
const char atRes_cwlap[] = "CWLAP:(";
const char atRes_cwjap_def[] = "+CWJAP_DEF:";
const char atRes_cwjap[] = "+CWJAP:";
const char atRes_no_ap[] = "No AP";
const char atRes_sysram[] = "+SYSRAM:";
const char atRes_cipstatus[] = "+CIPSTATUS:";
const char atRes_status[] = "STATUS:";
const char atRes_wifi_connected[] = "WIFI CONNECTED\r\n";
const char atRes_wifi_disconected[]	= "WIFI DISCONNECT\r\n";
const char atRes_wifi_got_ip[] = "WIFI GOT IP\r\n";
const char atRes_cifsr_station_ip[] = "+CIFSR:STAIP";
const char atRes_sntp_time[] = "+CIPSNTPTIME:";
const char atRes_smart_config_connected[] = "smartconfig connected wifi\r\n";
const char atRes_smart_config_failed[] = "smartconfig connect fail\r\n";

const char* const MEM_AT_COMMANDS_RES[NUM_OF_AT_COMMANDS_RES] = {
	atRes_ok, atRes_error, atRes_busy_p, atRes_busy_s, atRes_ready,
	atRes_send_char, atRes_send_ok, atRes_send_fail, atRes_fail, atRes_connect, atRes_already_connected,
	atRes_closed, atRes_ipd, atRes_cwlap, atRes_cwjap_def, atRes_cwjap,
	atRes_no_ap, atRes_sysram, atRes_cipstatus, atRes_status, atRes_wifi_connected,
	atRes_wifi_disconected, atRes_wifi_got_ip, atRes_cifsr_station_ip, atRes_sntp_time, 
    atRes_smart_config_connected, atRes_smart_config_failed
};
