/*
* @file NRF_lib.h
*
* @brief High level library for the RF communication (with the NRF24L01 module).
*
*/
#ifndef NRF_LIB_H
#define NRF_LIB_H

#include "NRF24L01.h"

// Debug print
#define NRF_LIB_DEBUG 1

// Data types
typedef enum
{
	// PTX/PRX
	NRF_LIB_CONNECT_SUCCESS = 0,
		
	// PTX
	NRF_LIB_CONNECT_ERROR,
	NRF_LIB_CONNECT_SUCCESS_TX_ERROR_ACK,
	
	// PRX
	NRF_LIB_CONNECT_RX_NO_PACKET,
	
} NRF_LIB_Connect_res_t;

#ifdef GATEWAY

/************************************************************************************
******************************** GATEWAY ********************************************
************************************************************************************/

// Library initialization. Opens Rx pipes and starts listening
void NRF_init(Rf_address_t rx_pipe_1, Rf_address_lsb_t rx_pipe_2, Rf_address_lsb_t rx_pipe_3, Rf_address_lsb_t rx_pipe_4, Rf_address_lsb_t rx_pipe_5);
// Connects the gateway with a thing in the transmission direction.
void NRF_connect_tx(ID_t id, TYPE_t type);
// Connects the gateway with a thing in the reception direction.
void NRF_connect_rx();

#else
/************************************************************************************
******************************** THINGS *********************************************
************************************************************************************/

// Library initialization. Opens Rx pipes and starts listening
void NRF_init(TYPE_t type, Rf_address_t tx_pipe, Rf_address_t rx_pipe);
// Connects the thing with the gateway in the transmission direction.
NRF_LIB_Connect_res_t NRF_connect_tx(ID_t id, TYPE_t type);
// Connects the thing with the gateway in the reception direction.
NRF_LIB_Connect_res_t NRF_connect_rx(ID_t id, TYPE_t type);

#endif

/************************************************************************************
******************************** COMMON *********************************************
************************************************************************************/

// Turns on the RF module
void NRF_power_on();
// Turns off the RF module
void NRF_power_off();

/************************************************************************************
******************************** TESTING ********************************************
************************************************************************************/

// Thing RF Testing
void NRF_TEST_transmitter();
// Gateway RF Testing
void NRF_TEST_receiver();

#endif

/*** end of file ***/