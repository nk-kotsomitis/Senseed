/*
* @file NRF_lib.c
*
* @brief High level library for the RF communication (with the NRF24L01 module).
*
*/

#include "PinsAndMacros.h"

#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <avr/wdt.h>
#include <util/delay.h>

#include "DataTypes.h"
#include "NRF24L01.h"
#include "Serialization_Deserialization.h"
#include "CONSOLE_log.h"
#include "NRF_lib.h"

#ifdef GATEWAY
#include "EVENT_LOGGER.h"
#endif

// Constants and macro definitions
#define NRF_LIB_MAX_RX_PACKETS 6
#define NRF_LIB_PAYLOAD NRF24_PAYLOAD_MAX

// Data types
typedef struct
{
	Rf_address_t tx_pipe;
	Rf_address_t rx_pipe_1;
	Rf_address_lsb_t rx_pipe_2;
	Rf_address_lsb_t rx_pipe_3;
	Rf_address_lsb_t rx_pipe_4;
	Rf_address_lsb_t rx_pipe_5;
	
} NRF_pipes_t;

// Static data
static NRF_pipes_t g_pipes = {0};

#ifdef GATEWAY

/************************************************************************************
******************************** GATEWAY ********************************************
************************************************************************************/

/*!
* @brief Library initialization. Opens Rx pipes and starts listening
*/
void NRF_init(Rf_address_t rx_pipe_1, Rf_address_t rx_pipe_2, Rf_address_t rx_pipe_3, Rf_address_t rx_pipe_4, Rf_address_t rx_pipe_5)
{		
	// Copy RF addresses for all Rx pipes
	memcpy(g_pipes.rx_pipe_1, rx_pipe_1, RF_ADDRESS_BYTES);
	memcpy(g_pipes.rx_pipe_2, rx_pipe_2, RF_ADDRESS_BYTES);
	memcpy(g_pipes.rx_pipe_3, rx_pipe_3, RF_ADDRESS_BYTES);
	memcpy(g_pipes.rx_pipe_4, rx_pipe_4, RF_ADDRESS_BYTES);
	memcpy(g_pipes.rx_pipe_5, rx_pipe_5, RF_ADDRESS_BYTES);
	
	// Initialize NRF24 and start listening
	NRF24_init();
	NRF24_open_pipe_rx(1, g_pipes.rx_pipe_1);
	NRF24_open_pipe_rx(2, g_pipes.rx_pipe_2);
	NRF24_open_pipe_rx(3, g_pipes.rx_pipe_3);
	NRF24_open_pipe_rx(4, g_pipes.rx_pipe_4);
	NRF24_open_pipe_rx(5, g_pipes.rx_pipe_5);
	// Start listening
	NRF24_go_rx();
}

/*!
* @brief Connects the gateway with a thing in the transmission direction.
*/
void NRF_connect_tx(ID_t id, TYPE_t type)
{
	NRF24_res_t res_tx = NRF24_TX_STATUS_ERROR;
	uint8_t tx_packet[NRF_LIB_PAYLOAD] = {0};
	Rf_address_t *payload_address_tx = NULL;
	
	// Serialize packet
	if (!SERIALIZE_rf_packet(tx_packet, id, type, &payload_address_tx))
	{
		// TODO: EVENT LOGGER
	}
	
	// Go to Tx mode
	NRF24_go_standby();
	NRF24_open_pipe_tx(*payload_address_tx);
	NRF24_go_tx();
	
	switch (type)
	{
		case TYPE_CONTROLLER_HP:
		case TYPE_CONTROLLER_LP:
		case TYPE_HYDRO:
			// Transmit packet
			res_tx = NRF24_packet_transmission(tx_packet);
			break;
			
		case TYPE_SOIL_SENSOR:
		case TYPE_ENV_SENSOR:	
		default:
			// TODO: EVENT LOGGER INTERNAL ERROR
			break;
	}
		
	if(res_tx != NRF24_TX_STATUS_TX_DS)
	{
		// TODO: EVENT LOGGER
	}
	
	// Start listening again
	NRF24_go_rx();
	
#if NRF_LIB_DEBUG
	CONSOLE_print_rf_packet(tx_packet, false, 0, 0, res_tx);
#endif
			
	return;
}

/*!
* @brief Connects the gateway with a thing in the reception direction.
*/
void NRF_connect_rx()
{	
	NRF24_res_t res_rx = NRF24_RX_STATUS_NO_PACKET;
	NRF24_res_t res_tx = NRF24_TX_STATUS_ERROR;
	
	uint8_t tx_packet[NRF_LIB_PAYLOAD] = {0};
	uint8_t num_of_packets = 0;
	uint8_t rx_pipes[NRF_LIB_MAX_RX_PACKETS] = {0, 0, 0, 0};
	uint8_t is_expired[NRF_LIB_MAX_RX_PACKETS] = {0, 0, 0, 0};
	uint8_t rx_packet_1[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_2[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_3[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_4[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_5[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_6[NRF_LIB_PAYLOAD] = {0};
	uint8_t *rx_packets[NRF_LIB_MAX_RX_PACKETS] = {0};
	rx_packets[0] = rx_packet_1;
	rx_packets[1] = rx_packet_2;
	rx_packets[2] = rx_packet_3;
	rx_packets[3] = rx_packet_4;
	rx_packets[4] = rx_packet_5;
	rx_packets[5] = rx_packet_6;
	Rf_address_t *payload_address_tx = NULL;
	bool is_ack_serialized = false;
	bool is_tx_mode_enabled = false;
			
	// Check for received packets (non-blocking)
	res_rx = NRF24_packet_reception(rx_packets, NRF_LIB_MAX_RX_PACKETS, &num_of_packets, rx_pipes, is_expired, false);
	
	// Process received packets
	for (uint8_t i = 0; i < num_of_packets; i++)
	{

#if NRF_LIB_DEBUG
		CONSOLE_print_rf_packet(rx_packets[i], true, i, is_expired[i], rx_pipes[i]);
#endif
		// IMPORTANT NOTE: Packets received from Rx Pipe 0 are not checked here. NRF24 should handle these.
		
		payload_address_tx = NULL;
		is_ack_serialized = false;
			
		// Deserialize packet and serialize ACK
		if(!DESERIALIZE_rf_packet(rx_packets[i], &is_ack_serialized, &payload_address_tx, g_time_epoch_y2k))
		{
			// TODO: EVENT LOGGER
			print_uint8("NRF_connect_rx - DESERIALIZE_rf_packet 1", is_ack_serialized);
			continue;
		}
	
		// Transmit MY ACK for non-expired packets
		if (is_ack_serialized && !is_expired[i])
		{		
			is_tx_mode_enabled = true;
						
			// Go to Tx mode
			NRF24_go_standby();
			NRF24_open_pipe_tx(*payload_address_tx);
			NRF24_go_tx();
						
			// Transmit My ACK
			res_tx = NRF24_packet_transmission(rx_packets[i]);
			
			if (res_tx != NRF24_TX_STATUS_TX_DS)
			{
				// NOTE: My ACK is for sensors, therefore re-transmission is not possible.
				// It seems there is nothing to do here
			}
#if NRF_LIB_DEBUG
			CONSOLE_print_rf_packet(rx_packets[i], false, 0, 0, res_tx);
#endif
		}
	}
	
	if (is_tx_mode_enabled)
	{
		// Before going to Rx mode again, check Rx FIFO
		res_rx = NRF24_packet_reception(rx_packets, NRF_LIB_MAX_RX_PACKETS, &num_of_packets, rx_pipes, is_expired, false);

		for (uint8_t i = 0; i < num_of_packets; i++)
		{
#if NRF_LIB_DEBUG			
			CONSOLE_print_rf_packet(rx_packets[i], true, i, is_expired[i], rx_pipes[i]);
#endif
			
			// TODO: UNCOMMENT !!!!!!!!!!!!!!!!!!!!!!
			
			// Deserialize packet. Since these are expired packets, My ACK is not considered
			if(!DESERIALIZE_rf_packet(rx_packets[i], &is_ack_serialized, &payload_address_tx, g_time_epoch_y2k))
			{
				// TODO: EVENT LOGGER
				print_uint8("NRF_connect_rx - DESERIALIZE_rf_packet 2", is_ack_serialized);
				continue;
			}

		}
		
		// Start listening again
		NRF24_go_rx();
	}
	
	return;
}

#else
/************************************************************************************
******************************** THING **********************************************
************************************************************************************/

/*!
* @brief Library initialization. Opens Rx pipes and starts listening
*/
void NRF_init(TYPE_t type, Rf_address_t tx_pipe, Rf_address_t rx_pipe)
{	
	// Copy RF addresses for Tx pipe and Rx pipe 1
	memcpy(g_pipes.tx_pipe, tx_pipe, RF_ADDRESS_BYTES);
	memcpy(g_pipes.rx_pipe_1, rx_pipe, RF_ADDRESS_BYTES);
	
	// NRF24 library init
	NRF24_init();
	NRF24_open_pipe_tx(g_pipes.tx_pipe);
	NRF24_open_pipe_rx(1, g_pipes.rx_pipe_1);
	
	// Start listening
	NRF24_go_rx();
}

/*!
* @brief Connects the thing with the gateway in the transmission direction.
*/
NRF_LIB_Connect_res_t NRF_connect_tx(ID_t id, TYPE_t type)
{	
	NRF_LIB_Connect_res_t res = NRF_LIB_CONNECT_ERROR;
	NRF24_res_t res_tx_rx = NRF24_TX_RX_STATUS_FAIL;
	// NRF24_res_t res_tx = NRF24_TX_STATUS_ERROR;
	
	uint8_t tx_packet[NRF_LIB_PAYLOAD] = {0};
	uint8_t num_of_packets = 0;
	uint8_t rx_pipes[NRF_LIB_MAX_RX_PACKETS] = {0, 0, 0, 0};
	uint8_t is_expired[NRF_LIB_MAX_RX_PACKETS] = {0, 0, 0, 0};
	uint8_t rx_packet_1[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_2[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_3[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_4[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_5[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_6[NRF_LIB_PAYLOAD] = {0};
	uint8_t *rx_packets[NRF_LIB_MAX_RX_PACKETS] = {0};
	rx_packets[0] = rx_packet_1;
	rx_packets[1] = rx_packet_2;
	rx_packets[2] = rx_packet_3;
	rx_packets[3] = rx_packet_4;
	rx_packets[4] = rx_packet_5;
	rx_packets[5] = rx_packet_6;
	bool is_tx_rx = false;
	
	// Serialize packet
	SERIALIZE_rf_packet(tx_packet, type);
		
	switch (type)
	{
		case TYPE_SOIL_SENSOR:
		case TYPE_ENV_SENSOR:
			is_tx_rx = true;
			
			// Transmit and wait for My ACK
			res_tx_rx = NRF24_packet_transmission_and_reception(tx_packet, rx_packets, NRF_LIB_MAX_RX_PACKETS, &num_of_packets, rx_pipes, is_expired);

			if (res_tx_rx != NRF24_TX_RX_STATUS_SUCCESS)
			{
				// TODO: EVENT Logger
			}
				
			break;
		
		case TYPE_CONTROLLER_HP:
		case TYPE_CONTROLLER_LP:
		case TYPE_HYDRO:
			is_tx_rx = false;
			
			// Tx mode
			NRF24_go_tx();
			
			// Transmit
			res_tx_rx = NRF24_packet_transmission(tx_packet);
			
			if (res_tx_rx != NRF24_TX_STATUS_TX_DS)
			{
				// TODO: EVENT Logger
			}
			
			// Non-blocking reception for expired packets
			NRF24_go_rx();
			NRF24_packet_reception(rx_packets, NRF_LIB_MAX_RX_PACKETS, &num_of_packets, rx_pipes, is_expired, false);
			
			break;
		
		default:
			break;
	}
	
	switch (res_tx_rx)
	{
		case NRF24_TX_STATUS_TX_DS:
			if (is_tx_rx)
			{
				// Tx Success, ACK Error
				res = NRF_LIB_CONNECT_SUCCESS_TX_ERROR_ACK;
			}
			else
			{
				// Success
				res = NRF_LIB_CONNECT_SUCCESS;
			}
			
#if NRF_LIB_DEBUG
			CONSOLE_print_rf_packet(tx_packet, false, 0, 0, NRF24_TX_STATUS_TX_DS);
#endif
			break;
			
		case NRF24_TX_RX_STATUS_SUCCESS:
			// Success
			res = NRF_LIB_CONNECT_SUCCESS;
			
#if NRF_LIB_DEBUG
			CONSOLE_print_rf_packet(tx_packet, false, 0, 0, NRF24_TX_STATUS_TX_DS);
#endif
			break;
		
		default:
			// Error (in all other cases)
			res = NRF_LIB_CONNECT_ERROR;
			
#if NRF_LIB_DEBUG
			CONSOLE_print_rf_packet(tx_packet, false, 0, 0, res_tx_rx);
#endif
			break;
	}

	// IMPORTANT NOTE: Received packets from pipe 0 should be handled at NRF24 lib.
	// Expired messages are don't care here.
	
	for (uint8_t i = 0; i < num_of_packets; i++)
	{
		
#if NRF_LIB_DEBUG
		CONSOLE_print_rf_packet(rx_packets[i], true, i, is_expired[i], rx_pipes[i]);
#endif

		// Deserialize packet
		if (!DESERIALIZE_rf_packet(rx_packets[i], id, type))
		{
			// TODO: EVENT LOGGER
			continue;
		}
	}

	return res;
}

/*!
* @brief Connects the thing with the gateway in the reception direction.
*/
NRF_LIB_Connect_res_t NRF_connect_rx(ID_t id, TYPE_t type)
{	
	NRF_LIB_Connect_res_t res = NRF_LIB_CONNECT_RX_NO_PACKET;
	NRF24_res_t res_rx = NRF24_RX_STATUS_NO_PACKET;
	
	uint8_t num_of_packets = 0;
	uint8_t rx_pipes[NRF_LIB_MAX_RX_PACKETS] = {0, 0, 0, 0};
	uint8_t is_expired[NRF_LIB_MAX_RX_PACKETS] = {0, 0, 0, 0};
	uint8_t rx_packet_1[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_2[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_3[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_4[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_5[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_6[NRF_LIB_PAYLOAD] = {0};
	uint8_t *rx_packets[NRF_LIB_MAX_RX_PACKETS] = {0};
	rx_packets[0] = rx_packet_1;
	rx_packets[1] = rx_packet_2;
	rx_packets[2] = rx_packet_3;
	rx_packets[3] = rx_packet_4;
	rx_packets[4] = rx_packet_5;
	rx_packets[5] = rx_packet_6;
		
	switch (type)
	{
		case TYPE_SOIL_SENSOR:
		case TYPE_ENV_SENSOR:		
		case TYPE_CONTROLLER_HP:
		case TYPE_CONTROLLER_LP:
		case TYPE_HYDRO:
			
			// Non-blocking reception
			res_rx = NRF24_packet_reception(rx_packets, NRF_LIB_MAX_RX_PACKETS, &num_of_packets, rx_pipes, is_expired, false);
			break;
		
		default:
			break;
	}
	
	// IMPORTANT NOTE: Received packets from pipe 0 should be handled at NRF24 lib.
	// Expired messages are don't care here.
	
	if (res_rx == NRF24_RX_STATUS_RX_DR)
	{
		
		for (uint8_t i = 0; i < num_of_packets; i++)
		{
		
	#if NRF_LIB_DEBUG
			CONSOLE_print_rf_packet(rx_packets[i], true, i, is_expired[i], rx_pipes[i]);
	#endif
			// Deserialize packet
			if (!DESERIALIZE_rf_packet(rx_packets[i], id, type))
			{
				// TODO: EVENT LOGGER
				continue;
			}
		}
		
		res = NRF_LIB_CONNECT_SUCCESS;
	}
	
	
	return res;
}

#endif

/************************************************************************************
******************************** COMMON *********************************************
************************************************************************************/

/*!
* @brief Turns on the RF module
*/
void NRF_power_on()
{
	NRF24_power_up();
}

/*!
* @brief Turns off the RF module
*/
void NRF_power_off()
{
	NRF24_power_down();
}

/************************************************************************************
******************************** TESTING ********************************************
************************************************************************************/

/*!
* @brief Thing RF Testing
*/
void NRF_TEST_transmitter()
{	
	print("NRF_TX_TEST started...\r\n");
	
	#define NRF_TX_TEST_IS_SENSOR_LOGIC 0
	#define NRF_TX_TEST_CYCLE_DELAY_MS 20
	#define NRF_TX_TEST_MAX_RX_PACKETS 4 
	
	ID_t id = atoi((const char *)STRINGIZE(UUID));
	static uint16_t cnt = 0;
	NRF24_res_t res_tx = 0;
	NRF24_res_t res = 0;
	
	Rf_address_t rx_pipe = TESTING_NRF_ADDR_THING_SENSOR_1;
	Rf_address_t tx_pipe = NRF_ADDRESS_GATEWAY_2_ENV_SENSORS;
	
	uint8_t rx_pipes[NRF_TX_TEST_MAX_RX_PACKETS] = {0, 0, 0, 0};
	uint8_t is_expired[NRF_TX_TEST_MAX_RX_PACKETS] = {0, 0, 0, 0};
	uint8_t num_of_packets = 0;
	uint8_t rx_packet_1[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_2[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_3[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_4[NRF_LIB_PAYLOAD] = {0};
	uint8_t *rx_packets[NRF_TX_TEST_MAX_RX_PACKETS] = {0};
	rx_packets[0] = rx_packet_1;
	rx_packets[1] = rx_packet_2;
	rx_packets[2] = rx_packet_3;
	rx_packets[3] = rx_packet_4;
		
	NRF24_init();
	NRF24_open_pipe_tx(tx_pipe);
	NRF24_open_pipe_rx(1, rx_pipe);
		
	while(1)
	{	
		memset(rx_packets[0], 0, NRF_LIB_PAYLOAD);
		memset(rx_packets[1], 0, NRF_LIB_PAYLOAD);
		memset(rx_packets[2], 0, NRF_LIB_PAYLOAD);
		memset(rx_packets[3], 0, NRF_LIB_PAYLOAD);
				
		uint8_t tx_packet[NRF_LIB_PAYLOAD] = {0};
		memset(tx_packet, 0, NRF_LIB_PAYLOAD);
		tx_packet[0] = (id >> 8);
		tx_packet[1] = id;
		tx_packet[2] = cnt++;

#if NRF_TX_TEST_IS_SENSOR_LOGIC

		// ****** THING - Sensor **********
		
		// Transmission and reception
		res_tx = NRF24_packet_transmission_and_reception(tx_packet, rx_packets, NRF_TX_TEST_MAX_RX_PACKETS, &num_of_packets, rx_pipes, is_expired);
		
#else
		// ****** THING - Controller ******
			
		// Transmission
		NRF24_go_tx();
		res_tx = NRF24_packet_transmission(tx_packet);
			
		// Reception for expired packets
		NRF24_go_rx();
		res = NRF24_packet_reception(rx_packets, NRF_TX_TEST_MAX_RX_PACKETS, &num_of_packets, rx_pipes, is_expired, false);
#endif
		
		switch (res_tx)
		{
			case NRF24_TX_STATUS_ERROR:
			case NRF24_TX_STATUS_ERROR_HW:
			case NRF24_TX_STATUS_MAX_RT:
			case NRF24_TX_STATUS_RX_DR:
				CONSOLE_print_rf_packet(tx_packet, false, 0, 0, res_tx);
				break;
				
			default:
				CONSOLE_print_rf_packet(tx_packet, false, 0, 0, NRF24_TX_STATUS_TX_DS);
				break;
		}
				
		for (uint8_t i = 0; i < num_of_packets; i++)
		{
			CONSOLE_print_rf_packet(rx_packets[i], true, i, is_expired[i], rx_pipes[i]);
		}
		
		
#if !NRF_TX_TEST_IS_SENSOR_LOGIC
		
		_delay_ms(1000);
		
		res = NRF24_packet_reception(rx_packets, NRF_TX_TEST_MAX_RX_PACKETS, &num_of_packets, rx_pipes, is_expired, true);

		for (uint8_t i = 0; i < num_of_packets; i++)
		{
			CONSOLE_print_rf_packet(rx_packets[i], true, i, is_expired[i], rx_pipes[i]);
		}
#endif
		
		if (res)
		{
			// Log
		}
		
		NRF24_power_down();
		wdt_reset();
		NRF24_print_stats(false);
		_delay_ms(NRF_TX_TEST_CYCLE_DELAY_MS);
		wdt_reset();
		NRF24_power_up();
	}
}

/*!
* @brief Gateway RF Testing
*/
void NRF_TEST_receiver()
{
	print("NRF_RX_TEST started...\r\n");
	
	#define NRF_RX_TEST_CYCLE_DELAY_MS 20
	#define NRF_RX_TEST_CONNECT_CYCLES_INTERVAL 200 // 4s
	#define NRF_RX_TEST_MAX_RX_PACKETS 4
	
	
	ID_t id = atoi((const char *)STRINGIZE(UUID));
	NRF24_res_t res_tx = 0;
	NRF24_res_t res = 0;
	uint16_t cycles = 0;
	
	uint8_t tx_packet[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_pipes[NRF_RX_TEST_MAX_RX_PACKETS] = {0, 0, 0, 0};
	uint8_t is_expired[NRF_RX_TEST_MAX_RX_PACKETS] = {0, 0, 0, 0};
	uint8_t num_of_packets = 0;
	uint8_t rx_packet_1[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_2[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_3[NRF_LIB_PAYLOAD] = {0};
	uint8_t rx_packet_4[NRF_LIB_PAYLOAD] = {0};
	uint8_t *rx_packets[NRF_RX_TEST_MAX_RX_PACKETS] = {0};
	rx_packets[0] = rx_packet_1;
	rx_packets[1] = rx_packet_2;
	rx_packets[2] = rx_packet_3;
	rx_packets[3] = rx_packet_4;
	
	Rf_address_t rx_pipe_1 = NRF_ADDRESS_GATEWAY_1_SOIL_SENSORS;
	Rf_address_lsb_t rx_pipe_2_lsb = {NRF_ADDRESS_GATEWAY_2_LSB_ENV_SENSORS};
	Rf_address_t tx_pipe = TESTING_NRF_ADDR_THING_SENSOR_1;
			
	NRF24_init();	
	NRF24_open_pipe_rx(1, rx_pipe_1);
	NRF24_open_pipe_rx(2, rx_pipe_2_lsb);
	NRF24_go_rx();
		
	while(1)
	{	
		cycles++;
		
		memset(rx_packets[0], 0, NRF_LIB_PAYLOAD);
		memset(rx_packets[1], 0, NRF_LIB_PAYLOAD);
		memset(rx_packets[2], 0, NRF_LIB_PAYLOAD);
		memset(rx_packets[3], 0, NRF_LIB_PAYLOAD);
		
		// ****** GATEWAY ******
		
		res = NRF24_packet_reception(rx_packets, NRF_RX_TEST_MAX_RX_PACKETS, &num_of_packets, rx_pipes, is_expired, true);
		
		for (uint8_t i = 0; i < num_of_packets; i++)
		{
			CONSOLE_print_rf_packet(rx_packets[i], true, i, is_expired[i], rx_pipes[i]);
			
			ID_t rx_id = (ID_t)((rx_packets[i][0] << 8) | rx_packets[i][1]);
			uint8_t rx_cnt = rx_packets[i][2];
			
			// Transmit MY ACK
			if (rx_id == 10001 && !is_expired[i])
			{
				memset(tx_packet, 0, NRF_LIB_PAYLOAD);
				tx_packet[0] = (rx_id >> 8);
				tx_packet[1] = rx_id;
				tx_packet[2] = rx_cnt;
				
				NRF24_go_standby();
				NRF24_open_pipe_tx(tx_pipe);
				NRF24_go_tx();
				res_tx = NRF24_packet_transmission(tx_packet);
				
				CONSOLE_print_rf_packet(tx_packet, false, 0, 0, res_tx);
			}		
		}	
		
		// Transmit packet
		if (cycles >= NRF_RX_TEST_CONNECT_CYCLES_INTERVAL)
		{
			cycles = 0;
			
			memset(tx_packet, 0, NRF_LIB_PAYLOAD);
			tx_packet[0] = (id >> 8);
			tx_packet[1] = id;
			tx_packet[2] = NRF_RX_TEST_CONNECT_CYCLES_INTERVAL;
			
			NRF24_go_standby();
			NRF24_open_pipe_tx(tx_pipe);
			NRF24_go_tx();
			res_tx = NRF24_packet_transmission(tx_packet);
			NRF24_go_rx();
			
			CONSOLE_print_rf_packet(tx_packet, false, 0, 0, res_tx);
		}
		
		memset(rx_packets[0], 0, NRF_LIB_PAYLOAD);
		memset(rx_packets[1], 0, NRF_LIB_PAYLOAD);
		memset(rx_packets[2], 0, NRF_LIB_PAYLOAD);
		memset(rx_packets[3], 0, NRF_LIB_PAYLOAD);
		
		// Before going to Rx mode again, check Rx FIFO
		res = NRF24_packet_reception(rx_packets, NRF_RX_TEST_MAX_RX_PACKETS, &num_of_packets, rx_pipes, is_expired, false);

		for (uint8_t i = 0; i < num_of_packets; i++)
		{
			CONSOLE_print_rf_packet(rx_packets[i], true, i, is_expired[i], rx_pipes[i]);
		}
		NRF24_go_rx();
		
		
		if (res)
		{
			// Log
		}
		
		wdt_reset();
		NRF24_print_stats(false);
		_delay_ms(NRF_RX_TEST_CYCLE_DELAY_MS);
		wdt_reset();
	}
	
}

/*** end of file ***/


/*


void NRF_packet_handle()
{
	uint8_t packet[NRF24_PAYLOAD_MAX];
	
	get_rx_data(packet);
	
	//NRF24_stop_listening();	// test
	NRF_IRQ = 0; // test
	
	uint8_t *address = NULL;
	bool is_ack_serialized = false;
	
	// Deserialize packet and serialize ACK
	if(!DESERIALIZE_rf_packet(packet, &is_ack_serialized, &address, g_time_epoch_y2k))
	{
		goto END;
	}
	
	// Debug
	//CONSOLE_print_rf_address(&packet[OFFSET_ADDRESS]);
	//CONSOLE_print_rf_address(address);
	
	if (!is_ack_serialized)
	{
		goto END;
	}
	
	// Go to Tx mode
	tx_mode(address); // packet[OFFSET_ADDRESS]
	
	// Transmit ACK
	int8_t res = NRF24_packet_transmission(packet);
	
	if (res == NRF24_TX_STATUS_TX_DS)
	{
		// Success D
		print_int16("OK", ((packet[OFFSET_UUID] << 8) | packet[OFFSET_UUID + 1]));
	}
	else
	{
		CONSOLE_print_rf_address(&packet[OFFSET_ADDRESS]);
		CONSOLE_print_rf_address(address);
		EVENT_LOGGER(g_time_epoch_y2k, EVT_ERROR, MODULE_NRF, 11, res);
	}
	
	END:
	//NRF24_stop_listening(); // TBC: is this required?
	rx_mode_all_pipes(g_pipes.rx_pipe_1, &g_pipes.rx_pipe_2, &g_pipes.rx_pipe_3, &g_pipes.rx_pipe_4, &g_pipes.rx_pipe_5);
}

*/