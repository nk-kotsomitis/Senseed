/*
* @file NRF24L01.h
*
* @brief Low level library for the NRF24L01 module.
*
*/

#ifndef NRF24L01_H
#define NRF24L01_H

// Imports
typedef Rf_address_t NRF24_address_t;
typedef Rf_address_lsb_t NRF24_address_lsb_t;

#ifdef GATEWAY
// Timer
#define NRF24_TIMER TIMER_1
// Timer tick (ms)
#define NRF24_TIMER_TICK_MS TIMER_1_TICK_MS
// Led on
#define NRF24_LED_ON led_4_on
// Led off
#define NRF24_LED_OFF led_4_off
// Tx Timeout (ms)
#define NRF24_TX_TIMEOUT_MS 50
// Rx Timeout (ms)
#define NRF24_RX_TIMEOUT_MS 50
#else
// Timer
#define NRF24_TIMER TIMER_1
// Timer tick (ms)
#define NRF24_TIMER_TICK_MS TIMER_1_TICK_MS
// Led on
#define NRF24_LED_ON
// Led off
#define NRF24_LED_OFF
// Tx Timeout (ms)
#define NRF24_TX_TIMEOUT_MS 50
// Rx Timeout (ms)
#define NRF24_RX_TIMEOUT_MS 50
#endif


// Print Debug
#define NRF24_DEBUG 0
// Enable statistics
#define NRF24_STATISTICS 60
// ARD - Delay defined from end of transmission to start of next transmission
#define NRF24_ARD NRF24_ARD_1000US
// ARC - Number of retransmissions (0 - 15)
#define NRF24_ARC 5
// RF Channel
#define NRF24_CHANNEL 100

// Constants
#define NRF24_ADDRESS_BYTES 5
#define NRF24_PAYLOAD_MAX 32
#define NRF24_DATA_RATE_1MBPS 0
#define NRF24_DATA_RATE_2MBPS 1
#define NRF24_TX_TIMEOUT_TICKS (NRF24_TX_TIMEOUT_MS/NRF24_TIMER_TICK_MS - 1)
#define NRF24_RX_TIMEOUT_TICKS  (NRF24_RX_TIMEOUT_MS/NRF24_TIMER_TICK_MS - 1)

// Data types
// typedef uint8_t NRF24_address_t[NRF24_ADDRESS_BYTES];
// typedef uint8_t NRF24_address_lsb_t[1];

typedef enum
{
	NRF24_STATUS_SUCCESS = 0,
	
	// PRX Status
	NRF24_RX_STATUS_NO_PACKET,
	NRF24_RX_STATUS_RX_DR,
	
	// PTX Status
	NRF24_TX_STATUS_ERROR,
	NRF24_TX_STATUS_ERROR_HW,
	NRF24_TX_STATUS_TX_DS,
	NRF24_TX_STATUS_MAX_RT,
	NRF24_TX_STATUS_RX_DR,
	
	// PRX/PTX Status
	NRF24_TX_RX_STATUS_FAIL,
	NRF24_TX_RX_STATUS_SUCCESS,
		
	// Tx FIFO
	NRF24_TX_FIFO_ERROR,
	NRF24_TX_FIFO_FULL,
	NRF24_TX_FIFO_EMPTY,
	NRF24_TX_FIFO_DATA,

	// Rx FIFO
	NRF24_RX_FIFO_ERROR,
	NRF24_RX_FIFO_FULL,
	NRF24_RX_FIFO_EMPTY,
	NRF24_RX_FIFO_DATA
	
} NRF24_res_t;

typedef struct
{
	uint16_t counter;
	
	// TxTotal = TxDs + TxMaxRt + TxErrors + TxErrorsHW
	uint16_t tx_total;
	uint16_t tx_ds;
	uint16_t tx_max_rt;
	uint16_t tx_errors;
	uint16_t tx_errors_hw;
	
	// Independent
	uint16_t tx_errors_rx_dr;
	
	// RxTotal = RxValid + RxPipe0 + RxExpiredValid + RxExpiredPipe0
	uint16_t rx_total;
	uint16_t rx_valid;
	uint16_t rx_pipe_zero;
	uint16_t rx_expired_valid;
	uint16_t rx_expired_pipe_zero;
	
	// Independent
	uint16_t rx_error_irq;
	uint16_t rx_fifo_full;
		
} NRF24_statistics_t;


// Extern variables
volatile uint8_t NRF_IRQ;

// Library Initialization
void NRF24_init();
// Opens Rx pipe
void NRF24_open_pipe_rx(uint8_t pipe_num, NRF24_address_t address);
// Opens Tx pipe
void NRF24_open_pipe_tx(NRF24_address_t address);

// Go to Standby-I mode
void NRF24_go_standby();
// Go Rx mode, i.e. start listening
void NRF24_go_rx();
// Go Tx mode. Not used, since send() handles it.
void NRF24_go_tx();

// Transmits a packet
NRF24_res_t NRF24_packet_transmission(uint8_t *packet);
// Receives packets
NRF24_res_t NRF24_packet_reception(uint8_t *packet[], uint8_t max_packets, uint8_t *num_of_packets, uint8_t *rx_pipes, uint8_t *is_expired, bool blocking_flag);
// Transmits a packet and then waits to receive a packet
NRF24_res_t NRF24_packet_transmission_and_reception(uint8_t *tx_packet, uint8_t *rx_packets[], uint8_t max_packets, uint8_t *num_of_packets, uint8_t *rx_pipes, uint8_t *is_expired);

// Power down module
void NRF24_power_down();
// Power up module
void NRF24_power_up();

#if NRF24_STATISTICS
// Print statistics
void NRF24_print_stats(bool print_now);
// Get statistics
NRF24_statistics_t NRF24_get_stats();
#endif

// nRF24L01 Registers
#define CONFIG      0x00
#define EN_AA       0x01
#define EN_RXADDR   0x02
#define SETUP_AW    0x03
#define SETUP_RETR  0x04
#define RF_CH       0x05
#define RF_SETUP    0x06
#define STATUS      0x07
#define OBSERVE_TX  0x08
#define CD          0x09
#define RX_ADDR_P0  0x0A
#define RX_ADDR_P1  0x0B
#define RX_ADDR_P2  0x0C
#define RX_ADDR_P3  0x0D
#define RX_ADDR_P4  0x0E
#define RX_ADDR_P5  0x0F
#define TX_ADDR     0x10
#define RX_PW_P0    0x11
#define RX_PW_P1    0x12
#define RX_PW_P2    0x13
#define RX_PW_P3    0x14
#define RX_PW_P4    0x15
#define RX_PW_P5    0x16
#define FIFO_STATUS 0x17

// nRF24L01 Register Masks
#define MASK_RX_P_NO 0x0E
#define MASK_RX_DR  6
#define MASK_TX_DS  5
#define MASK_MAX_RT 4

// nRF24L01 Register Bits
#define EN_CRC      3
#define CRCO        2
#define PWR_UP      1
#define PRIM_RX     0
#define ENAA_P5     5
#define ENAA_P4     4
#define ENAA_P3     3
#define ENAA_P2     2
#define ENAA_P1     1
#define ENAA_P0     0
#define ERX_P5      5
#define ERX_P4      4
#define ERX_P3      3
#define ERX_P2      2
#define ERX_P1      1
#define ERX_P0      0
#define AW          0
#define ARD         4
#define ARC         0
#define PLL_LOCK    4
#define RF_DR_HIGH  3
#define RF_DR_LOW   5   // TBC
#define RF_PWR_HIGH 1
#define RF_PWR_LOW  2
#define LNA_HCURR   0
#define RX_DR       6
#define TX_DS       5
#define MAX_RT      4
#define RX_P_NO     1
#define TX_FULL     5
#define PLOS_CNT    4
#define ARC_CNT     0
#define TX_REUSE    6
#define FIFO_FULL   5
#define TX_EMPTY    4
#define RX_FULL     1
#define RX_EMPTY    0

// nRF24L01 SPI Commands
#define R_REGISTER    0x00
#define W_REGISTER    0x20
#define REGISTER_MASK 0x1F
#define R_RX_PAYLOAD  0x61
#define W_TX_PAYLOAD  0xA0
#define FLUSH_TX      0xE1
#define FLUSH_RX      0xE2
#define REUSE_TX_PL   0xE3
#define NOP           0xFF

#endif

/*** end of file ***/