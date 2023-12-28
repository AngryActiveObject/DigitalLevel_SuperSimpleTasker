/*
 * bsp.h
 *
 *  Created on: Nov 20, 2023
 *      Author: Duncan
 */

#ifndef INC_BSP_H_
#define INC_BSP_H_

#include <stdint.h>
#include "LIS3DSH.h"

void SystemClock_Config(void);
void BSP_init(void);


void set_blue_LED_duty(uint16_t duty);
void set_red_LED_duty(uint16_t duty);
void set_orange_LED_duty(uint16_t duty);
void set_green_LED_duty(uint16_t duty);
LIS3DSH_Results_t LIS3DSH_read(void);

/*Event signals for all project task queues shall use the same type*/
typedef enum project_sigs_e{
	/*blinky event signals*/
	BLINKYTIMER,
	/*spi handler event signals*/
	SPI_TXRXREQ_SIG,
	SPI_TXRXCOMPLETE_SIG,
	SPI_TIMEOUT_SIG,
	/*LIS3DSH event signals*/
	LIS3DSH_POLL_SIG,
	/**/
	PRJ_SIGS_MAX,
} project_sigs_t;

#endif /* INC_BSP_H_ */
