/*
 * LIS3DSH.h
 *
 *  Created on: Sep 16, 2023
 *      Author: Duncan
 */


#ifndef INC_LIS3DSH_H_
#define INC_LIS3DSH_H_

#include <stdint.h>

#include "sst.h"
#include "spi_manager.h"



/*Output data rate selection*/
typedef enum LIS3DSH_ODR_e{
	LIS3DSH_ODR_PWR_DWN = 0,
	LIS3DSH_ODR_3p125HZ = 1,
	LIS3DSH_ODR_6p25HZ  = 2,
	LIS3DSH_ODR_12p5HZ  = 3,
	LIS3DSH_ODR_25Hz    = 4,
	LIS3DSH_ODR_50Hz    = 5,
	LIS3DSH_ODR_100Hz   = 6,
	LIS3DSH_ODR_400Hz   = 7,
	LIS3DSH_ODR_800Hz   = 8,
	LIS3DSH_ODR_1600Hz  = 9,
} LIS3DSH_ODR_t;

typedef enum LIS3DSH_DRVRState_e{
	LIS3DSH_INITIALISING ,
	LIS3DSH_IDLE,
	LIS3DSH_READING,
	LIS3DSH_FAULT,
} LIS3DSH_DRVRState_t;

typedef struct LIS3DSH_Results_s{
	int16_t x_g;
	int16_t y_g;
	int16_t z_g;
}LIS3DSH_Results_t;

typedef struct LIS3DSH_Config_s{
	uint8_t AxisEnable;
	uint8_t BDUMode;
	LIS3DSH_ODR_t DataRate;
} LIS3DSH_Config_t;


typedef struct LIS3DSH_Evnt_s{
	SST_Evt super;
}LIS3DSH_Evnt_t;

#define LIS3DSH_BUFF_SIZE (16u)

typedef struct LIS3DSH_task_s{
	SST_Task super; /*inherit SST task structure*/
	/*LIS3DSH driver specific variables*/
	LIS3DSH_DRVRState_t DrvrState;
	SST_TimeEvt pollTimer;
	SST_Task const *SPIDeviceAO; /*active object that managers the spi peripheral for comms to chip.*/
	LIS3DSH_Results_t Results;
	SPIManager_Evnt_t TxRxTransactionEvent;
	SPIManager_Job_t TxRxTransactionJob;
	uint8_t spiTxBuffer[LIS3DSH_BUFF_SIZE];
	uint8_t spiRxBuffer[LIS3DSH_BUFF_SIZE];
	uint8_t initStage; /*in the init state this walks through the initialisation steps of the device.*/
	uint8_t initAttempts; /*number of attempts to initialise the device.*/
	uint8_t ctrlReg4; /*desired value for control register 4*/
} LIS3DSH_task_t;


/*************************Public Function Prototypes ******************************************************/

void LIS3DSH_ctor(LIS3DSH_task_t * me, SST_Task const * const SPI_Manager_AO, GPIO_TypeDef * pcsGPIOPort, uint16_t csGPIOPin);

LIS3DSH_Results_t LIS3DSH_get_accel_xyz(LIS3DSH_task_t * me);
#endif /* INC_LIS3DSH_H_ */
