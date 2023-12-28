/*
 * spi_manager.h
 *
 *  Created on: Dec 3, 2023
 *      Author: Duncan
 */



#ifndef INC_SPI_MANAGER_H_
#define INC_SPI_MANAGER_H_


#include "sst.h"
#include "main.h"

#define SPIMANAGER_QUEUE_SIZE (16)

typedef enum SPIManager_State_e {
	SPI_MGR_BUSY, SPI_MGR_READY,
} SPIManager_State_t;

typedef struct {
	SST_Task const *pAOrequester; /*active object that requested the SPI transaction job*/
	GPIO_TypeDef * pcsGPIOPort; /*chip select port to use*/
	uint16_t csGPIOPin;  /*chip select pin to use*/
	uint8_t *txData;
	uint8_t *rxData;
	uint16_t lenData; /*Number of bytes in the job*/
	uint16_t timeoutCnt_ms; /*timeout time the job*/
} SPIManager_Job_t;

/*jobs are passed to the SPIManager in its event quest*/
typedef struct {
	SST_Evt super; /*inherit SST event*/
	SPIManager_Job_t *pJob;
} SPIManager_Evnt_t;

typedef struct SPIManager_Task_e {
	SST_Task super;
	/** add additional task data here*/
	SPIManager_State_t MgrState; /*internal state of the device*/
	SPI_HandleTypeDef *pSPIPeriph; /*pointer to the peripheral*/
	SST_TimeEvt JobTimeoutTimer;   /*time event object used to timeout jobs*/
	SPIManager_Job_t *pCurrentJob; /*current active job*/
	SPIManager_Job_t *pMgrJobs[SPIMANAGER_QUEUE_SIZE];
	uint32_t JobsHead;
	uint32_t JobsTail;
} SPIManager_Task_t;


/**public function prototypes**/
void SPIManager_ctor(SPIManager_Task_t *const me, SPI_HandleTypeDef *spiDevice);
void SPIManager_post_txrx_Request(SST_Task *const AO, SPIManager_Evnt_t *pEvent);
#endif /* INC_SPI_MANAGER_H_ */
