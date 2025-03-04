/*
* @file NRF24L01.c
*
* @brief Low level library for the NRF24L01 module.
*
*/

#include "PinsAndMacros.h"
#include "DataTypes.h"
#include "SPI_atmega328P.h"
#include "NRF24L01.h"

#include <stdbool.h>
#include <util/delay.h>

// Data types
typedef enum
{
	NRF24_ARD_250US = 1,
	NRF24_ARD_500US,
	NRF24_ARD_750US,
	NRF24_ARD_1000US,
	NRF24_ARD_1250US,
	NRF24_ARD_1500US,
	NRF24_ARD_1750US,
	NRF24_ARD_2000US,
	NRF24_ARD_2250US,
	NRF24_ARD_2500US,
	NRF24_ARD_2750US,
	NRF24_ARD_3000US,
	NRF24_ARD_3250US,
	NRF24_ARD_3500US,
	NRF24_ARD_3750US,
} NRF24_ARD_t;

typedef enum
{
	NRF24_RF_PWR_18DBM = 0,
	NRF24_RF_PWR_12DBM,
	NRF24_RF_PWR_6DBM,
	NRF24_RF_PWR_0DBM	
} NRF24_Rf_power_t;

// Constants and macro definitions
// Power on reset delay (10.3ms)
#define NRF24_POWER_ON_RESET_DELAY_MS 12
// Start-up delay (1.5ms)
#define NRF24_STARTUP_DELAY_US 1500

// Extern data
volatile uint8_t NRF_IRQ = 0;

// Static data
static bool gb_rx_data_available = false;

#if NRF24_STATISTICS
#include "CONSOLE_log.h"
static NRF24_statistics_t g_stats = {};
#endif

// Private function prototypes
// Goes to Tx mode and sends a packet
static void send(uint8_t * value, uint8_t len);
// Checks the status after an interrupt is triggered. Clears Tx status flags and flushes Tx FIFO.
static NRF24_res_t check_ack();
// Clears Tx interrupt flags and flushes Tx FIFO
static void send_clear();
// Checks if there are data available after an interrupt is triggered.
static bool rx_data_available(int8_t *rx_pipe);
// Checks if there are data available that are already in FIFO (prior transmission)
static bool rx_data_available_in_fifo(int8_t *rx_pipe);
// Clear Rx interrupt flags
static void rx_data_clear();
// Get Rx data and Rx pipe
static void get_rx_data(uint8_t *data);
// Extracts the Rx pipe number from status register
static int8_t status_reg_2_pipe_number(uint8_t status);
// Get Rx FIFO status
static NRF24_res_t rx_fifo_status();
// Get Tx FIFO status
static NRF24_res_t tx_fifo_status();
// Get status register
static uint8_t get_status();
// Flush Rx FIFO - Unused
// static void flush_rx();
// Flush Tx FIFO
static void flush_tx();


#if NRF24_DEBUG
// Debugging functions
static void debug_all(char *fun_name);
static void debug_ce_pin();
static void debug_config_reg();
static void debug_status_reg();
static void debug_observe_tx_reg();
static void debug_fifo_status_reg();
static void debug_register_bit(char *reg_name, char *bit_name, uint8_t reg, uint8_t bit_pos);
static void debug_register(char *reg_name, uint8_t reg);
#endif /* NRF24_DEBUG */

// Register Functions
static void modify_register_bit(uint8_t reg, uint8_t bit_pos, uint8_t bit_value);
static void read_register(uint8_t reg, uint8_t * value, uint8_t len);
static void config_register(uint8_t reg, uint8_t value);
static void write_register(uint8_t reg, uint8_t * value, uint8_t len);


/************************************************************************************
*************************** Public functions ****************************************
************************************************************************************/

/*!
* @brief Module initialization. Initializes the SPI library and registers, and goes to Standby-I mode
*/
void NRF24_init()
{	
	// IMPORTANT NOTE: IRQ interrupt should be enabled!
	
	RF_CE_INIT;
	RF_CS_INIT;
	RF_CE_LOW;
	RF_CS_HIGH;
	
	// SPI lib init	
	SPI_init();
	
	// Power on	reset delay 10.3ms
	_delay_ms(NRF24_POWER_ON_RESET_DELAY_MS);
		
	// Auto Retransmit Delay (ARD) and Auto Retransmit Count (ARC)
	config_register(SETUP_RETR, (NRF24_ARD << ARD) | (NRF24_ARC << ARC));
		
	//  Sets the frequency channel nRF24L01 operates on (Bits 6:0)
	config_register(RF_CH, NRF24_CHANNEL);
	
	// RF Setup Register
	// PLL_LOCK, RF_DR, RF_PWR, LNA_HCURR
	//configRegister(RF_SETUP, (1 << RF_DR_LOW) );     //10  -> 250KBPS   | 00  -> 1MBPS
	//configRegister(RF_SETUP, (0 << RF_DR_HIGH) );
	//configRegister(RF_SETUP, (0 << RF_PWR_HIGH) );	// PAlevel
	// debug_register("RF_SETUP", RF_SETUP);
	// config_register(RF_SETUP, (1 << PLL_LOCK) | (NRF24_DATA_RATE_1MBPS << RF_DR_HIGH) | ( NRF24_RF_PWR_0DBM << RF_PWR_HIGH) | (1 << LNA_HCURR));
		
	// Set CONFIG register and go to Standby-I mode
	config_register(CONFIG, ((0 << MASK_RX_DR) | (0 << MASK_TX_DS) | (0 << MASK_MAX_RT) | (1 << EN_CRC) | (0 << CRCO)| (1 << PWR_UP) | (0 << PRIM_RX)));
	
	// Start-up delay (1.5 ms)
	_delay_us(NRF24_STARTUP_DELAY_US);
	
	// Clear masks
	modify_register_bit(STATUS, TX_DS, 1);
	modify_register_bit(STATUS, MAX_RT, 1);
	modify_register_bit(STATUS, RX_DR, 1);

#if NRF24_STATISTICS
	g_stats.counter = 0;
	g_stats.tx_total = 0;
	g_stats.tx_ds = 0;
	g_stats.tx_max_rt = 0;
	g_stats.tx_errors = 0;
	g_stats.tx_errors_hw = 0;
	g_stats.tx_errors_rx_dr = 0;
	g_stats.rx_total = 0;
	g_stats.rx_valid = 0;
	g_stats.rx_pipe_zero = 0;
	g_stats.rx_expired_valid = 0;
	g_stats.rx_expired_pipe_zero = 0;
	g_stats.rx_error_irq = 0;
	g_stats.rx_fifo_full = 0;
#endif

#if NRF24_PRINT_DEBUG
	debug_all("NRF24_init");
#endif
}



/*!
* @brief Opens an Rx pipe. NOTE: Pipe 0 should not be enabled. It is used for transmission.
*
* @param[in] pipe_num The number of pipe (0 to 5)
* @param[in] address The address pointer
*
*/
void NRF24_open_pipe_rx(uint8_t pipe_num, NRF24_address_t address)
{
	switch (pipe_num)
	{
		case 0:
		// NOTE: DO NOT USE Rx pipe 0. Tx pipe will overwrite it.
		write_register(RX_ADDR_P0, address, NRF24_ADDRESS_BYTES);
		config_register(RX_PW_P0, NRF24_PAYLOAD_MAX);
		modify_register_bit(EN_RXADDR, ERX_P0, 1);
		break;
		
		case 1:
		write_register(RX_ADDR_P1, address, NRF24_ADDRESS_BYTES);
		config_register(RX_PW_P1, NRF24_PAYLOAD_MAX);
		modify_register_bit(EN_RXADDR, ERX_P1, 1);
		break;
		
		case 2:
		write_register(RX_ADDR_P2, address, 1);
		config_register(RX_PW_P2, NRF24_PAYLOAD_MAX);
		modify_register_bit(EN_RXADDR, ERX_P2, 1);
		break;
		
		case 3:
		write_register(RX_ADDR_P3, address, 1);
		config_register(RX_PW_P3, NRF24_PAYLOAD_MAX);
		modify_register_bit(EN_RXADDR, ERX_P3, 1);
		break;
		
		case 4:
		write_register(RX_ADDR_P4, address, 1);
		config_register(RX_PW_P4, NRF24_PAYLOAD_MAX);
		modify_register_bit(EN_RXADDR, ERX_P4, 1);
		break;
		
		case 5:
		write_register(RX_ADDR_P5, address, 1);
		config_register(RX_PW_P5, NRF24_PAYLOAD_MAX);
		modify_register_bit(EN_RXADDR, ERX_P5, 1);
		break;
		
		default:
		break;
	}

}

/*!
* @brief Opens the Tx pipe (and Rx pipe 0)
*	     Set RX_ADDR_P0 equal to this address to handle automatic acknowledge if
*		 this is a PTX device with Enhanced ShockBurst™ enabled.
*
* @param[in] pipe_num The number of pipe (0 to 5)
* @param[in] address The address pointer
*
*/
void NRF24_open_pipe_tx(NRF24_address_t address)
{
	// Write address in Rx pipe 0 and Tx pipe
	write_register(TX_ADDR, address, NRF24_ADDRESS_BYTES);
	write_register(RX_ADDR_P0, address, NRF24_ADDRESS_BYTES);
	// Set the payload size for Rx pipe 0
	config_register(RX_PW_P0, NRF24_PAYLOAD_MAX);
	
	// NOTE: Rx pipe 0, is enabled and disabled just before and after transmission
	
	#if NRF24_PRINT_DEBUG
	debug_all("NRF24_open_pipe_tx");
	#endif
}


/*!
* @brief Go to Standby-I mode and clears interrupt flags.
*/
void NRF24_go_standby()
{
	// Flush Tx FIFO
	// flush_tx();
	// flush_rx();
	
	// Go to Standby-I mode
	RF_CE_LOW;
	
	// Clear masks
	modify_register_bit(STATUS, TX_DS, 1);
	modify_register_bit(STATUS, MAX_RT, 1);
	modify_register_bit(STATUS, RX_DR, 1);
}

/*!
* @brief Go to Rx mode. (The PWR_UP bit, the PRIM_RX bit and the CE pin should be set high to go to Rx mode)
*/
void NRF24_go_rx()
{
	#if NRF24_DEBUG
	debug_register("EN_RXADDR", EN_RXADDR);
	debug_register("EN_AA", EN_AA);
	debug_register("RX_ADDR_P1", RX_ADDR_P1);
	debug_register("RX_ADDR_P2", RX_ADDR_P2);
	debug_register("RX_PW_P1", RX_PW_P1);
	debug_register("RX_PW_P2", RX_PW_P2);
	#endif
	
	RF_CE_LOW;
	modify_register_bit(CONFIG, PRIM_RX, 1);
	RF_CE_HIGH;
	
	// Tpece2csn 4us Min (Delay from CE pos. edge to CSN low)
	_delay_us(4);
}

/*!
* @brief Goes to Tx mode. Tx mode will change when CE is set high (after writing Tx payload) at NRF24_send()
*	     After going to Tx mode, the interrupt is checked for received packets and sets a flag.
*	     These packets should be read directly after transmission and flagged as "expired".
*/
void NRF24_go_tx()
{
	RF_CE_LOW;
	modify_register_bit(CONFIG, PRIM_RX, 0);
	
	// NOTE: CE is set high after writing Tx payload at NRF24_send()
	
	// Check if there is any Rx packet and clear flags
	if (NRF_IRQ)
	{
		gb_rx_data_available = true;
		// Clear RX_DR flag and interrupt
		NRF_IRQ = 0;
		modify_register_bit(STATUS, RX_DR, 1);

		// NOTE: These packets should be read after transmission and flagged as expired.
	}
}

/*!
* @brief High-level packet transmission
*
* @return PTX Status: NRF24_TX_STATUS_ERROR, NRF24_TX_STATUS_ERROR_HW, NRF24_TX_STATUS_TX_DS, NRF24_TX_STATUS_MAX_RT, NRF24_TX_STATUS_RX_DR
*/
NRF24_res_t NRF24_packet_transmission(uint8_t *tx_packet)
{	
	NRF24_res_t res = NRF24_TX_STATUS_ERROR;
	bool ack_received = false;

	// Transmit packet	
	send(tx_packet, NRF24_PAYLOAD_MAX);	

	// Clear timer and wait for ACK until timer expires
	NRF24_TIMER = 0;
	while(NRF24_TIMER < NRF24_TX_TIMEOUT_TICKS)
	{			
		if(NRF_IRQ)
		{
			NRF_IRQ = 0;
	
			// Check status register
			res = check_ack();
					
			ack_received = true;
			//NRF_LED_RX_PACKET_OFF;
			break;
		}	
	}
			
	// Perform actions after transmitting a packet
	send_clear();

#if NRF24_STATISTICS
	if (!ack_received)
	{
		//print_uint16("ERROR_TX_HW_NO_ACK", 0);
		g_stats.tx_errors++;
	}
#endif
	
	return res;
}

/*!
* @brief High-level packet reception
*
* @return PRX Status: NRF24_RX_STATUS_NO_PACKET, NRF24_RX_STATUS_RX_DR
*/
NRF24_res_t NRF24_packet_reception(uint8_t *packet[], uint8_t max_packets, uint8_t *num_of_packets, uint8_t *rx_pipes, uint8_t *is_expired, bool blocking_flag)
{
	// NRF24_res_t res = NRF24_RX_STATUS_NO_PACKET;
	*num_of_packets = 0;
	uint8_t pkt_idx = 0;
	int8_t rx_pipe = 0;
	
	// Check if Rx FIFO is FULL
	if (rx_fifo_status() == NRF24_RX_FIFO_FULL)
	{
#if NRF24_STATISTICS
		// Rx FIFO FULL
		g_stats.rx_fifo_full++;
#endif
	}
	
	if (rx_data_available_in_fifo(&rx_pipe))
	{	
#if NRF24_STATISTICS
		if (!rx_pipe)
		{

			g_stats.rx_total++;
			g_stats.rx_expired_pipe_zero++;
			// CONSOLE_log("ERROR_RX_P0", NRF_IRQ);
		}
		else
		{
			// Expired Packet valid
			g_stats.rx_total++;
			g_stats.rx_expired_valid++;
		}
#endif
		
		// Get data
		get_rx_data(packet[pkt_idx]);
		
		rx_pipes[pkt_idx] = rx_pipe;
		is_expired[pkt_idx] = 1;
		*num_of_packets += 1;
		pkt_idx++;
	}
	
	// Clear timer
	NRF24_TIMER = 0;
	do
	{
		if (NRF_IRQ)
		{
			NRF_IRQ = 0;
												
			do 
			{	
				// Get the Rx pipe number
				if(!rx_data_available(&rx_pipe))
				{
#if NRF24_STATISTICS
					g_stats.rx_error_irq++;
					// CONSOLE_log("ERROR_RX_IRQ", rx_pipe);
#endif
				}

#if NRF24_STATISTICS				
				if (!rx_pipe)
				{
					

					g_stats.rx_total++;
					g_stats.rx_pipe_zero++;
					// CONSOLE_log("ERROR_RX_P0", NRF_IRQ);	
				}
				else
				{
					// Packet valid
					g_stats.rx_total++;
					g_stats.rx_valid++;
				}
#endif				
				// Get data
				get_rx_data(packet[pkt_idx]);
				
				rx_pipes[pkt_idx] = rx_pipe;
				is_expired[pkt_idx] = 0;
				*num_of_packets += 1;
				pkt_idx++;
							
			} while (rx_fifo_status() != NRF24_RX_FIFO_EMPTY && *num_of_packets <= max_packets);
				
			rx_data_clear();
				
			NRF24_LED_OFF;
			
			return NRF24_RX_STATUS_RX_DR;
		}
		
		if (!blocking_flag)
		{
			break;
		}
				
	} while(NRF24_TIMER < NRF24_RX_TIMEOUT_TICKS);
		
	return NRF24_RX_STATUS_NO_PACKET;
}

/*!
* @brief High-level packet transmission and reception
*
* @return PRX/PTX Status: NRF24_TX_RX_STATUS_FAIL, NRF24_TX_RX_STATUS_SUCCESS, packet_transmission, packet_reception
*/
NRF24_res_t NRF24_packet_transmission_and_reception(uint8_t *tx_packet, uint8_t *rx_packets[], uint8_t max_packets, uint8_t *num_of_packets, uint8_t *rx_pipes, uint8_t *is_expired)
{
	NRF24_res_t res = NRF24_TX_RX_STATUS_FAIL;
		
	// Tx mode
	NRF24_go_tx();
		
	if ((res = NRF24_packet_transmission(tx_packet)) != NRF24_TX_STATUS_TX_DS)
	{
		return res;
	}
		
	// Rx mode
	NRF24_go_rx();
		
	if ((res = NRF24_packet_reception(rx_packets, max_packets, num_of_packets, rx_pipes, is_expired, true)) != NRF24_RX_STATUS_RX_DR)
	{
		return res;
	}
	
	return NRF24_TX_RX_STATUS_SUCCESS;
}


/*!
* @brief Go to Power Down mode.
*/
void NRF24_power_down()
{
	// Clear CE to go to standby-I mode (if it not already low)
	RF_CE_LOW;
	// Clear PWR_UP to go to power down mode
	modify_register_bit(CONFIG, PWR_UP, 0);
}

/*!
* @brief Go from Power Down mode to Standby-I mode
*/
void NRF24_power_up()
{
	// Set PWR_UP bit to go to Standby-I mode
	modify_register_bit(CONFIG, PWR_UP, 1);
	// Start-up delay
	_delay_us(NRF24_STARTUP_DELAY_US);
}


#if NRF24_STATISTICS

// Print statistics
void NRF24_print_stats(bool print_now)
{		
	g_stats.counter++;
	
	if (g_stats.counter <= NRF24_STATISTICS && !print_now)
	{
		return;
	}
	
	// Print statistics
	CONSOLE_print_rf_statistics(g_stats);
		
	g_stats.counter = 0;
}

// Get statistics
NRF24_statistics_t NRF24_get_stats()
{
	return g_stats;
}

#endif

/************************************************************************************
******************************** Static functions ***********************************
************************************************************************************/

/*!
* @brief Goes to Tx mode, transmits a packet and then returns to Standby-I mode.
*		 (The PWR_UP bit should be set high, the PRIM_RX bit should be set low, 
*		 a payload should be in the TX FIFO and a high pulse on the CE for more than 10µs).
*/
static void send(uint8_t * value, uint8_t len)
{
		
#if NRF24_STATISTICS
	g_stats.tx_total++;
#endif
	
#if NRF24_PRINT_DEBUG
	debug_all("NRF24_send");
#endif

	// Enable Rx pipe 0
	modify_register_bit(EN_RXADDR, ERX_P0, 1);
			
	// Write Tx payload command
	RF_CS_LOW;
	SPI_fast_shift(W_TX_PAYLOAD); 
	SPI_transmit_sync(value, len);
	RF_CS_HIGH;
	
	// Go Tx mode (Start transmission by setting a high pulse on the CE for more than 10us)
	RF_CE_HIGH;
	_delay_us(20);
	// Standby-I mode after the transmission
	RF_CE_LOW;
		
	// Tx (Setting) mode is 130us. There is no reason to add delay here.
	// _delay_us(110); 
	
	/*	
	Since CE is 0 after the 10us pulse, it returns to standby-I mode. If CE was remained 1 after the 10us pulse, 
	the next action is determined by the status of the TX FIFO.If the TX FIFO is not empty it remains in TX mode, 
	transmitting the next packet. If the TX FIFO is empty it goes into standby-II mode.
	
	IMPORTANT NOTE: It is important to never keep the nRF24L01 in TX mode for more than 4ms at a time.
	*/
}

/*!
* @brief Check the status register after a transmission is made and an interrupt is fired
*		 
*		 Checks the status register for an ACK, after a packet is transmitted.
*		 If the received ACK packet is empty, only the TX_DS IRQ is asserted.
*		 If the ACK packet contains a payload, both TX_DS IRQ and RX_DR IRQ are asserted simultaneously.
*
*		 If the MAX_RT bit in the STATUS register is set high, the payload in TX FIFO is NOT removed !!!
*
*		 IMPORTANT: The 3 bit pipe information in the STATUS register is updated during the IRQ pin high to low
*		 transition. If the STATUS register is read during an IRQ pin high to low transition, the pipe
*		 information is unreliable. So, only read the register after IRQ interrupt is triggered.
*/
static NRF24_res_t check_ack()
{
	NRF24_res_t res = NRF24_TX_STATUS_ERROR_HW;
	uint8_t status = 0;
		
	// Disable Rx pipe 0
	modify_register_bit(EN_RXADDR, ERX_P0, 0);
	
	// Get status register
	status = get_status();
		
	if(status & (1 << TX_DS))
	{
		
#if NRF24_DEBUG
		print_uint8("TX_DS", status); // 46
#endif

#if NRF24_STATISTICS
	g_stats.tx_ds++;
#endif

		res = NRF24_TX_STATUS_TX_DS;
	}
	else if(status & (1 << MAX_RT))
	{
		
#if NRF24_DEBUG
		print_uint8("TX_MAX_RT", status);
#endif	

#if NRF24_STATISTICS
	g_stats.tx_max_rt++;
#endif
		// NOTE: Flush Tx FIFO in case of MAX_RT
		res = NRF24_TX_STATUS_MAX_RT;
	}
	else
	{
		
#if NRF24_DEBUG
		print_uint8("ERROR_TX_HW", status);
#endif

#if NRF24_STATISTICS
	g_stats.tx_errors_hw++;
#endif
		res = NRF24_TX_STATUS_ERROR_HW;
	}
	
	if(status & (1 << RX_DR))
	{
		
#if NRF24_DEBUG
		print_uint8("RX_DR", status);
#endif	

#if NRF24_STATISTICS
		g_stats.tx_errors_rx_dr++;
#endif
	
		res = NRF24_TX_STATUS_RX_DR;
		
		// NOTE: This will be triggered only if there is payload in ACK (N/A for current library's implementation, so it is an error). 
		// In case it is triggered, TX_DS should also be 1.
	}
	
	return res;
}

/*!
* @brief Clears Tx interrupt flags and flushes Tx FIFO if needed
*/
static void send_clear()
{
	// IMPORTANT NOTE: Disable Rx pipe 0 here ONLY in case the IRQ after transmission is not fired!
	
	NRF24_LED_OFF;
	
	// Flush Tx FIFO
	if (tx_fifo_status() != NRF24_TX_FIFO_EMPTY)
	{
		flush_tx();	
	}

	// Clear masks
	modify_register_bit(STATUS, TX_DS, 1);
	modify_register_bit(STATUS, MAX_RT, 1);

#if NRF24_PRINT_DEBUG
	debug_all("NRF24_send_clear");
#endif

}


/*!
* @brief Checks the RX_DR flag and the Rx pipe number
*		 
*		 The RX_DR IRQ is asserted by a new packet arrival event. The procedure for handling this interrupt should be: 
*		 1) read payload through SPI
*		 2) clear RX_DR IRQ
*		 3) read FIFO_STATUS to check if there are more payloads available in RX FIFO
*		 4) if there are more data in RX FIFO, repeat from step 1
*
* @return Returns true if there is a Rx data, false otherwise.
*/
static bool rx_data_available(int8_t *rx_pipe)
{
	//RF_CE_LOW;
	
	uint8_t status = 0;
	RF_CS_LOW;                               
	status = SPI_fast_shift(NOP);               
	RF_CS_HIGH;     
		
	bool is_rx_data = (status & (1 << RX_DR));
		
	*rx_pipe = status_reg_2_pipe_number(status);
		
	return is_rx_data;
}

/*!
* @brief Checks if Rx (expired) packets are pending in Rx Fifo, prior to a transmission
*
* @return Returns true if there is a Rx data, false otherwise.
*/
static bool rx_data_available_in_fifo(int8_t *rx_pipe)
{
	*rx_pipe = -1;
	
	if (gb_rx_data_available)
	{
		gb_rx_data_available = false;
		
		RF_CS_LOW;
		uint8_t status = SPI_fast_shift(NOP);
		RF_CS_HIGH;
		
		*rx_pipe = status_reg_2_pipe_number(status);
		
		return true;
	}
	
	return false;
}

/*!
* @brief Clears flags after receiving Rx data.
*/
static void rx_data_clear()
{
	// Flush Rx FIFO
	// flush_rx();
	
	// Go to Standby-I mode in order to clear interrupt flags
	RF_CE_LOW;
	
	// Clear RX_DR
	modify_register_bit(STATUS, RX_DR, 1);
	modify_register_bit(STATUS, TX_DS, 1);
	
	// Go to Rx mode again
	RF_CE_HIGH;
}

/*!
* @brief Get Rx data
*/
static void get_rx_data(uint8_t *data)
{
	// Read RX-payload command
	RF_CS_LOW;
	SPI_fast_shift(R_RX_PAYLOAD);
	SPI_transfer_sync(data, data, NRF24_PAYLOAD_MAX);
	RF_CS_HIGH;
}

/*!
* @brief Get the Rx pipe number from register
*
* @return Returns the pipe number or -1 if Rx FIFO is empty
*/
static int8_t status_reg_2_pipe_number(uint8_t status)
{
	/*
	Register RX_P_NO
	000-101: Data Pipe Number
	110: Not Used
	111: RX FIFO Empty
	*/
	
	int8_t rx_pipe = ((status & MASK_RX_P_NO) >> RX_P_NO);
	
	if (rx_pipe == 7)
	{
		// RX FIFO Empty
		rx_pipe = -1;
	}
	
	return rx_pipe;
}

/*!
* @brief Get the Rx FIFO status
*
* @return Returns the status NRF24_STATUS_RX_FIFO_FULL, NRF24_STATUS_RX_FIFO_EMPTY, NRF24_STATUS_RX_FIFO_DATA
*/
NRF24_res_t rx_fifo_status()
{
	NRF24_res_t res = NRF24_RX_FIFO_ERROR;
	uint8_t status = 0;
		
	read_register(FIFO_STATUS, &status, 1);
	
	// RX FIFO full flag. 1: RX FIFO full. 0: Available locations in RX FIFO
	if (status & (1 << RX_FULL))
	{
		res = NRF24_RX_FIFO_FULL;
	}
	// 0: Data in RX FIFO, 1:RX FIFO empty
	else if (status & (1 << RX_EMPTY))
	{
		res = NRF24_RX_FIFO_EMPTY;
	}
	else
	{
		res = NRF24_RX_FIFO_DATA;
	}
	
	// print_uint8("NRF24_rx_fifo_status", res);
	
	return res;
}

/*!
* @brief Get the Tx FIFO status
*
* @return Returns the status NRF24_TX_FIFO_FULL, NRF24_TX_FIFO_EMPTY, NRF24_TX_FIFO_DATA
*/
NRF24_res_t tx_fifo_status()
{
	NRF24_res_t res = NRF24_TX_FIFO_ERROR;
	uint8_t status = 0;
	
	read_register(FIFO_STATUS, &status, 1);
	
	// TX FIFO full flag. 1: TX FIFO full. 0: Available	locations in TX FIFO.
	if (status & (1 << TX_FULL))
	{
		res = NRF24_TX_FIFO_FULL;
	}
	// TX FIFO empty flag. 1: TX FIFO empty. 0: Data in TX FIFO.
	else if (status & (1 << TX_EMPTY))
	{
		res = NRF24_TX_FIFO_EMPTY;
	}
	else
	{
		res = NRF24_TX_FIFO_DATA;
	}
	
	// print_uint8("TX FIFO", res);
	
	return res;
}

/************************************************************************************
*********************** Debug and helper functions **********************************
************************************************************************************/

// Get status
static uint8_t get_status()
{
	RF_CS_LOW;
	uint8_t status = SPI_fast_shift(NOP);
	RF_CS_HIGH;
	return status;
}

#if 0 // Unused
// Flush Rx FIFO when in Rx mode. Should not be executed during transmission of acknowledge, that is, acknowledge package will not be completed.
static void flush_rx()
{
	//  Flush RX FIFO, used in RX mode
	RF_CS_LOW;
	SPI_fast_shift(FLUSH_RX);
	RF_CS_HIGH;
}
#endif

// Flush Tx FIFO when in Tx mode
static void flush_tx()
{
	// Flush TX FIFO, used in TX mode
	RF_CS_LOW;
	SPI_fast_shift(FLUSH_TX);
	RF_CS_HIGH;
}

#if NRF24_DEBUG

static void debug_all(char *fun_name)
{
	print("\r\n");
	print(fun_name);
	print("\r\n");
	
	debug_ce_pin();
	debug_config_reg();
	debug_status_reg();
	debug_fifo_status_reg();
}

static void debug_ce_pin()
{
	print_uint8("DEBUG CE ", RF_CE_STATE);
}

static void debug_config_reg()
{
	uint8_t reg = 0;
	read_register(CONFIG, &reg, 1);
	print_uint8("DEBUG CONFIG", reg);	
}


static void debug_status_reg()
{
	uint8_t status;
	RF_CS_LOW;
	status = SPI_fast_shift(NOP);
	RF_CS_HIGH;
	print_uint8("DEBUG STATUS", status);
}


static void debug_observe_tx_reg()
{
	uint8_t reg = 0;
	read_register(OBSERVE_TX, &reg, 1);
	print_uint8("DEBUG OBSERVE_TX", reg);
}

static void debug_fifo_status_reg()
{
	uint8_t reg = 0;
	read_register(FIFO_STATUS, &reg, 1);
	print_uint8("DEBUG FIFO_STATUS", reg);
}

static void debug_register_bit(char *reg_name, char *bit_name, uint8_t reg, uint8_t bit_pos)
{
	// Read register
 	uint8_t status = 0;
 	read_register(reg, &status, 1);
 	// Check bit
 	uint8_t result = status & (1 << bit_pos);
 	
	print("DEBUG REG ");
	print(reg_name);
	print(" BIT ");
	print(bit_name);
	print_uint8(":", result);
}

static void debug_register(char *reg_name, uint8_t reg)
{
	// Read register
	uint8_t status = 0;
	read_register(reg, &status, 1);
		
	print("DEBUG REG ");
	print(reg_name);
	print_uint8(":", status);
}
#endif /* NRF24_DEBUG */

// Modify only 1 bit of a register
static void modify_register_bit(uint8_t reg, uint8_t bit_pos, uint8_t bit_value)
{
	uint8_t reg_value = 0;

	// Read register
	RF_CS_LOW;
	SPI_fast_shift(R_REGISTER | (REGISTER_MASK & reg));
	SPI_transfer_sync(&reg_value, &reg_value, 1);
	RF_CS_HIGH;
	
	if (bit_value)
	{
		// Set value
		reg_value = reg_value | (1 << bit_pos);
	}
	else
	{
		// Clear value
		reg_value = reg_value & ~(1 << bit_pos);
	}
	
	// Write register
	RF_CS_LOW;
	SPI_fast_shift(W_REGISTER | (REGISTER_MASK & reg));
	SPI_fast_shift(reg_value);
	RF_CS_HIGH;
}

// Read register
static void read_register(uint8_t reg, uint8_t *value, uint8_t len)
{
	RF_CS_LOW;
	SPI_fast_shift(R_REGISTER | (REGISTER_MASK & reg));
	SPI_transfer_sync(value,value,len);
	RF_CS_HIGH;
}

// Write 1 byte register
static void config_register(uint8_t reg, uint8_t value)
{
	RF_CS_LOW;
	SPI_fast_shift(W_REGISTER | (REGISTER_MASK & reg));
	SPI_fast_shift(value);
	RF_CS_HIGH;
}

// Write multiple-byte register
static void write_register(uint8_t reg, uint8_t * value, uint8_t len)
{
	RF_CS_LOW;
	SPI_fast_shift(W_REGISTER | (REGISTER_MASK & reg));
	SPI_transmit_sync(value, len);
	RF_CS_HIGH;
}

/*** end of file ***/