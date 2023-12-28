/** \file spi_manager.c
 ******************************************************************************
 * @file    spi_manager.c
 * @brief   This file provides code for managing a single SPI device
 ******************************************************************************
 * The SPI manager provides methods to share a SPI device via a single interfacing task.
 * THe driver depends on the STM32 SPI (Interrupt mode) and GPIO HAL drivers.
 * The manager contains an internal queue of SPIMANAGER_QUEUE_SIZE SPI transactions.
 * Will provide event callbacks to the threads which request SPI jobs so these threads
 * need to implement handlers for the SPI_TXRXCOMPLETE_SIG and SPI_TIMEOUT_SIG event signals
 * When a job is provided to the spi manager it is expected that the
 ******************************************************************************
 ******************************************************************************
 */


#include "spi_manager.h"

#include "dbc_assert.h" /* Design By Contract (DBC) assertions */
#include <string.h>
#include "bsp.h"


DBC_MODULE_NAME("spi_mgr")

/*****private data********/

/* move this into task setup module function*/

/*immutable txrx complete event signal*/
static const SST_Evt TxxRxCompleteEventSignal = { .sig = SPI_TXRXCOMPLETE_SIG };
static SST_Evt const *const pTxRxCompleteEventSignal = &TxxRxCompleteEventSignal;

/*immutable txrx timeout event signal*/
static const SST_Evt txTimeoutEventSignal = { .sig = SPI_TIMEOUT_SIG };
static SST_Evt const *const ptxTimeoutEventSignal = &txTimeoutEventSignal;

/*****Private Function Prototypes*****/


static void SPIManager_task_Handler(SPIManager_Task_t *const me,
		SPIManager_Evnt_t const *const e);

static void SPIManager_init_Handler(SPIManager_Task_t *const me,
		SPIManager_Evnt_t const *const ie);

 void SPIManager_start_txrx(SPIManager_Task_t *const me,
		SPIManager_Job_t *pJob);

 void SPIManager_txrx_complete_Handler(SPIManager_Task_t *const me);

 void SPIManager_txrx_Req_Handler(SPIManager_Task_t *const me,
		const SPIManager_Evnt_t *const e);

 void SPIManager_Timeout_Handler(SPIManager_Task_t *const me);

 HAL_StatusTypeDef SPIManager_enqueue_Job(SPIManager_Task_t *const me,
		SPIManager_Job_t *pJob);

 SPIManager_Job_t* SPIManager_dequeue_Job(SPIManager_Task_t *const me);

/*****Function Declarations*****/
void SPIManager_ctor(SPIManager_Task_t *const me, SPI_HandleTypeDef *spiDevice) {
	/*construct internal objects*/
	SST_Task_ctor(&(me->super), (SST_Handler) &SPIManager_init_Handler,
			(SST_Handler) &SPIManager_task_Handler);
	SST_TimeEvt_ctor(&(me->JobTimeoutTimer), SPI_TIMEOUT_SIG, &(me->super));

	/*initialise simple fields*/
	me->pCurrentJob = NULL;
	me->JobsHead = 0;
	me->JobsTail = 0;
	me->MgrState = SPI_MGR_READY;
	memset(me->pMgrJobs, 0u, SPIMANAGER_QUEUE_SIZE * sizeof(SPIManager_Job_t));
	me->pSPIPeriph = spiDevice;
}

/*public function to allow external tasks to post a tx rx event request*/
void SPIManager_post_txrx_Request(SST_Task *const AO, SPIManager_Evnt_t *pEvent) {

	DBC_ASSERT(0, (AO != NULL) && (pEvent != NULL) && (pEvent->pJob != NULL));

	SST_Task_post(AO, SST_EVT_DOWNCAST(SST_Evt, pEvent));
}

/*The init event handler does nothing currently as everything is initialised in the constructor*/
void SPIManager_init_Handler(SPIManager_Task_t *const me,
		SPIManager_Evnt_t const *const ie) {
	(void) ie;
	(void) me;
}

void SPIManager_task_Handler(SPIManager_Task_t *const me,
		SPIManager_Evnt_t const *const e) {

	switch (e->super.sig) {
	case SPI_TXRXREQ_SIG: {
		SPIManager_txrx_Req_Handler(me, e);
		break;
	}
	case SPI_TXRXCOMPLETE_SIG: {
		SPIManager_txrx_complete_Handler(me);
		break;
	}
	case SPI_TIMEOUT_SIG: {
		SPIManager_Timeout_Handler(me);
		break;
	}
	default: {
		DBC_ERROR(200);
	}
	}
}

 void SPIManager_start_txrx(SPIManager_Task_t *const me,
		SPIManager_Job_t *pJob) {

	DBC_ASSERT(1,
			(me != NULL) && (pJob->pAOrequester != NULL));

	HAL_GPIO_WritePin(pJob->pcsGPIOPort, pJob->csGPIOPin, GPIO_PIN_RESET); /*set the chip select pin low*/

	HAL_StatusTypeDef result = HAL_SPI_TransmitReceive_IT(me->pSPIPeriph,
			pJob->txData, pJob->rxData, pJob->lenData);
	me->pCurrentJob = pJob;
	me->MgrState = SPI_MGR_BUSY;
	SST_TimeEvt_arm(&(me->JobTimeoutTimer), pJob->timeoutCnt_ms, 0u);

	DBC_ASSERT(2, result != HAL_ERROR);
}

 void SPIManager_txrx_complete_Handler(SPIManager_Task_t *const me) {

	/*its expected that the spi manager is in the busy state if it gets a SPI_TXRXCOMPLETE_SIG*/
	DBC_ASSERT(10, (me != NULL) && (me->MgrState== SPI_MGR_BUSY));

	HAL_GPIO_WritePin(me->pCurrentJob->pcsGPIOPort, me->pCurrentJob->csGPIOPin,
			GPIO_PIN_SET); /*set the chip select pin high*/

	/*Post tx complete signal back to the requesting thread*/
	SST_Task_post((SST_Task* const ) me->pCurrentJob->pAOrequester,
			pTxRxCompleteEventSignal);

	/*disarm the timout timer*/
	SST_TimeEvt_disarm(&me->JobTimeoutTimer); /*finished so disarm the timeout timer*/

	/*check for new job to do*/
	SPIManager_Job_t *newJob = SPIManager_dequeue_Job(me);
	if (newJob == NULL) {
		me->MgrState = SPI_MGR_READY; /*goto ready state ready to receive more jobs*/
		me->pCurrentJob = NULL;
	} else {
		SPIManager_start_txrx(me, newJob);
	}
}

 void SPIManager_txrx_Req_Handler(SPIManager_Task_t *const me,
		const SPIManager_Evnt_t *const e) {
	DBC_ASSERT(20,
			(me != NULL) && (e != NULL) && (e->pJob != NULL));

	if (me->MgrState == SPI_MGR_BUSY) {
		/*save job for when previous job has completed*/

		HAL_StatusTypeDef enqueueResult = SPIManager_enqueue_Job(me, e->pJob);
		DBC_ASSERT(21, enqueueResult != HAL_ERROR); /*assert there was space in the queue*/

	} else {
		SPIManager_start_txrx(me, e->pJob);
	}
}

/* Handles timeout events from the SPIManagers internal timeout timer */
 void SPIManager_Timeout_Handler(SPIManager_Task_t *const me) {

	/*it's expected that the spi manager is in the busy state if it gets a SPI_TIMEOUT_SIG*/
	DBC_ASSERT(30,
			(me != NULL) && (me->pCurrentJob != NULL) && (me->pCurrentJob->pAOrequester != NULL) && (me->MgrState == SPI_MGR_BUSY));

	HAL_SPI_Abort(me->pSPIPeriph);

	SST_Task_post((SST_Task* const ) me->pCurrentJob->pAOrequester,
			ptxTimeoutEventSignal); /*Post tx timeout signal back to the requesting thread*/

	me->MgrState = SPI_MGR_READY; /*free the manager for other tasks*/
}

 HAL_StatusTypeDef SPIManager_enqueue_Job(SPIManager_Task_t *const me,
		SPIManager_Job_t *pJob) {
	uint32_t tmpHead = me->JobsHead;

	/*increment and wrap the next index*/
	uint32_t tmpNext = tmpHead + 1u;
	if (SPIMANAGER_QUEUE_SIZE == tmpNext) {
		tmpNext = 0;
	}

	/*loose one cell in the buffer here but simplifies buffer full/empty detection*/
	if (tmpNext == me->JobsTail) {
		return HAL_ERROR; /*buffer full*/
	}

	me->pMgrJobs[tmpHead] = pJob;
	me->JobsHead = tmpNext;
	return HAL_OK;
}

 SPIManager_Job_t* SPIManager_dequeue_Job(SPIManager_Task_t *const me) {
	{
		uint32_t tmpTail = me->JobsTail;
		uint32_t tmpTailNext = (uint16_t) tmpTail + 1u;
		/*check for empty queue*/
		if (me->JobsHead == tmpTail) {
			return NULL;
		}

		/*wrap next tail*/
		if (SPIMANAGER_QUEUE_SIZE == tmpTailNext) {
			tmpTailNext = 0;
		}

		me->JobsTail = tmpTailNext;
		return me->pMgrJobs[tmpTail];
	}
}

