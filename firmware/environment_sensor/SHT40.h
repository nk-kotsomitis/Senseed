/*
* @file SHT40.h
*
* @brief SHT40 main library.
*
*/
#ifndef SHT40_H
#define SHT40_H

#include "SHT40/sht4x.h"

// Library init
void SHT40_init();
// Get temperature
uint16_t SHT40_get_measurements();


#endif

/*** end of file ***/