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


#define LIS3DSH_READ (0x01 << 7) /*bit 7 sets LIS3DSH to read*/
/* MEMS REGISTER ADDRESS*/
#define LIS3DSH_OUT_T (0x0C)
#define LIS3DSH_INFO1 (0x0D)
#define LIS3DSH_INFO2 (0x0E)
#define LIS3DSH_WHO (0x0F)
#define LIS3DSH_STAT (0x18)
#define LIS3DSH_CTRL4 (0x20)
#define LIS3DSH_CTRL1 (0x21)
#define LIS3DSH_CTRL2 (0x22)
#define LIS3DSH_CTRL3 (0x23)
#define LIS3DSH_CTRL5 (0x24)
#define LIS3DSH_CTRL6 (0x25)
#define LIS3DSH_STATUS (0x27)
#define LIS3DSH_OUT_X_L (0x28)
#define LIS3DSH_OUT_X_H (0x29)
#define LIS3DSH_OUT_Y_L (0x2A)
#define LIS3DSH_OUT_Y_H (0x2B)
#define LIS3DSH_OUT_Z_L (0x2C)
#define LIS3DSH_OUT_Z_H (0x2D)

/* CTRL4 register 4 bit def msks*/
#define LIS3DSH_CTRL4_ODR_POS (0x04)
#define LIS3DSH_CTRL4_ODR_MSK (0x0F << LIS3DSH_CTRL4_ODR_POS)
#define LIS3DSH_CTRL4_BDU_POS (0x03)
#define LIS3DSH_CTRL4_BDU_MSK (0x01 << LIS3DSH_CTRL4_BDU_POS)
#define LIS3DSH_CTRL4_ZEN_POS (0x02)
#define LIS3DSH_CTRL4_ZEN_MSK (0x01 << LIS3DSH_CTRL4_ZEN_POS)
#define LIS3DSH_CTRL4_YEN_POS (0x01)
#define LIS3DSH_CTRL4_YEN_MSK (0x01 << LIS3DSH_CTRL4_YEN_POS)
#define LIS3DSH_CTRL4_XEN_POS (0x00)
#define LIS3DSH_CTRL4_XEN_MSK (0x01 << LIS3DSH_CTRL4_XEN_POS)

/* Block data update Mode*/
#define LIS3DSH_BDU_ENABLE (0x01u)
#define LIS3DSH_BDU_DISABLE (0x00u)

#define IS_A_LIS3DSH_BDU(u) (u == LIS3DSH_BDU_ENABLE || u == LIS3DSH_BDU_DISABLE)

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


void LIS3DSH_ctor(LIS3DSH_task_t * me, SST_Task const * const SPI_Manager_AO, GPIO_TypeDef * pcsGPIOPort, uint16_t csGPIOPin);

LIS3DSH_Results_t LIS3DSH_get_accel_xyz(LIS3DSH_task_t * me);
#endif /* INC_LIS3DSH_H_ */
