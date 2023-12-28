/** \file spi_manager.c
 ******************************************************************************
 * @file    spi_manager.c
 * @brief   This file provides code for managing a single SPI device
 ******************************************************************************
 * The SPI manager provides methods to share a SPI device via a single interfacing task.
 * THe driver depends on the STM32 SPI (Interrupt mode) and GPIO HAL drivers.
 * Implemented via an event driven state machine (built with a switch case and state variable)
 * with two states, SPI_MGR_BUSY and SPI_MGR_READY.
 * The manager contains an internal queue of SPIMANAGER_QUEUE_SIZE SPI transactions.
 * It will provide event callbacks to the threads which request SPI jobs so these threads
 * need to implement handlers for the SPI_TXRXCOMPLETE_SIG and SPI_TIMEOUT_SIG event signals
 * When a job is provided to the spi manager it is expected that the contents of the tx and
 * rx buffers in the request message remain untouched and valid  before the 
 * SPI_TXRXCOMPLETE_SIG response is received from the manager.
 * @note 
 * The user needs to post TxRx complete signal events from the SPI device driver e.g.
 * void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
	if (hspi == &hspi1) {
		SST_Task_post(AO_SpiMgr, pTxRxCompleteEventSignal);
	}
}
 ******************************************************************************
 ******************************************************************************
 */

#include "spi_manager.h"

#include "dbc_assert.h" /* Design By Contract (DBC) assertions */
#include <string.h>
#include "bsp.h"

DBC_MODULE_NAME("spi_mgr")

/******************************private data*******************************************/

/*immutable txrx complete event signal*/
static const SST_Evt TxxRxCompleteEventSignal = { .sig = SPI_TXRXCOMPLETE_SIG };
static SST_Evt const *const pTxRxCompleteEventSignal = &TxxRxCompleteEventSignal;

/*immutable txrx timeout event signal*/
static const SST_Evt txTimeoutEventSignal = { .sig = SPI_TIMEOUT_SIG };
static SST_Evt const *const ptxTimeoutEventSignal = &txTimeoutEventSignal;

/***********************Private Function Prototypes********************************/

static void SPIManager_task_Handler(SPIManager_Task_t *const me,
		SPIManager_Evnt_t const *const e);

static void SPIManager_init_Handler(SPIManager_Task_t *const me,
		SPIManager_Evnt_t const *const ie);

void SPIManager_start_txrx(SPIManager_Task_t *const me, SPIManager_Job_t *pJob);

void SPIManager_txrx_complete_Handler(SPIManager_Task_t *const me);

void SPIManager_txrx_Req_Handler(SPIManager_Task_t *const me,
		const SPIManager_Evnt_t *const e);

void SPIManager_Timeout_Handler(SPIManager_Task_t *const me);

HAL_StatusTypeDef SPIManager_enqueue_Job(SPIManager_Task_t *const me,
		SPIManager_Job_t *pJob);

SPIManager_Job_t* SPIManager_dequeue_Job(SPIManager_Task_t *const me);

/**********************Public Function Declarations*********************************/

/**
 * @brief SPIManager_ctor - Constructs a new SPIManager object
 * @param me - SPIManager instance variable.
 * @param pspiDevice - pointer to the spi handle this manager is responsible for
 */
void SPIManager_ctor(SPIManager_Task_t *const me, SPI_HandleTypeDef *pspiDevice) {
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
	me->pSPIPeriph = pspiDevice;
}

/**
 * @brief SPIManager_post_txrx_Request - Posts a request for a txrx job to the spi manager
 * @param AO - Pointer to the SPI manager active object the request is to be made to. 
 * @param pEvent - pointer to a SPIManager_Evnt_t object that contains a SPI_TXRXREQ_SIG request
 */
void SPIManager_post_txrx_Request(SST_Task *const AO, SPIManager_Evnt_t *pEvent) {

/*ensure the contents of the request are valid and of SPI_TXRXREQ_SIG type*/
	DBC_ASSERT(0,
			(AO != NULL) && (pEvent != NULL) && (pEvent->pJob != NULL) && (pEvent->super.sig == SPI_TXRXREQ_SIG));

	SST_Task_post(AO, SST_EVT_DOWNCAST(SST_Evt, pEvent));
}

/**********************Private Function Declarations*********************************/

/*The init event handler does nothing currently as everything is initialised in the constructor*/
void SPIManager_init_Handler(SPIManager_Task_t *const me,
		SPIManager_Evnt_t const *const ie) {
	(void) ie;
	(void) me;
}

/**
 * @brief SPIManager_task_Handler - Task handler called by the SST kernel on receipt of an event.
 * each event runs its own handler that adjusts the SPI Manager state accordingly.
 * @param me - me pointer
 * @param e - pointer to event that triggered this call.  
 */
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

/**
 * @brief SPIManager_txrx_Req_Handler - event handler for SPI_TXRXREQ_SIG which is 
 * received on external requests for a txrx job.
 * The handler either immediately calls the low level drivers if the device is not busy or
 * enqueues the job for sending when the SPI device becomes free. 
 * @param me - me pointer
 * @param e - pointer to event that triggered this call. 
 */
void SPIManager_txrx_Req_Handler(SPIManager_Task_t *const me,
		const SPIManager_Evnt_t *const e) {

	DBC_ASSERT(20, (me != NULL) && (e != NULL) && (e->pJob != NULL));

	if (me->MgrState == SPI_MGR_BUSY) {
		/*save job for when previous job has completed*/

		HAL_StatusTypeDef enqueueResult = SPIManager_enqueue_Job(me, e->pJob);
		DBC_ASSERT(21, enqueueResult != HAL_ERROR); /*assert there was space in the queue*/

	} else {
		SPIManager_start_txrx(me, e->pJob);
	}
}

/**
 * @brief SPIManager_start_txrx - Helper function which wraps the HAL call to the SPI txrx function. 
 * Additionally it unsets the jobs desired chip select pin
 * And arms a timeout counter. 
 * @param me - me pointer
 * @param e - pointer to event that triggered this call. 
 */
void SPIManager_start_txrx(SPIManager_Task_t *const me, SPIManager_Job_t *pJob) {

	DBC_ASSERT(1, (me != NULL) && (pJob->pAOrequester != NULL));

	HAL_GPIO_WritePin(pJob->pcsGPIOPort, pJob->csGPIOPin, GPIO_PIN_RESET); /*set the chip select pin low*/

	HAL_StatusTypeDef result = HAL_SPI_TransmitReceive_IT(me->pSPIPeriph,
			pJob->txData, pJob->rxData, pJob->lenData);
	me->pCurrentJob = pJob;
	me->MgrState = SPI_MGR_BUSY;
	SST_TimeEvt_arm(&(me->JobTimeoutTimer), pJob->timeoutCnt_ms, 0u);

	DBC_ASSERT(2, result != HAL_ERROR);
}

/**
 * @brief SPIManager_txrx_complete_Handler - event handler called when a SPI_TXRXCOMPLETE_SIG
 * And arms a timeout counter. 
 * @param me - me pointer
 */
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

/**
 * @brief SPIManager_Timeout_Handler - event handler called when the JobTimeoutTimer posts a Timer event.
 * This occurs when a job takes longer than the jobs timeoutCnt_ms to complete. It aboerts the current job 
 * and moves the device back to the ready state. 
 * And arms a timeout counter. 
 * @param me - me device pointer
 */
void SPIManager_Timeout_Handler(SPIManager_Task_t *const me) {

	/*it's expected that the spi manager is in the busy state if it gets a SPI_TIMEOUT_SIG*/
	DBC_ASSERT(30,
			(me != NULL) && (me->pCurrentJob != NULL) && (me->pCurrentJob->pAOrequester != NULL) && (me->MgrState == SPI_MGR_BUSY));

	HAL_SPI_Abort(me->pSPIPeriph);

	SST_Task_post((SST_Task* const ) me->pCurrentJob->pAOrequester,
			ptxTimeoutEventSignal); /*Post tx timeout signal back to the requesting thread*/

	me->MgrState = SPI_MGR_READY; /*free the manager for other tasks*/
}

/**
 * @brief SPIManager_enqueue_Job - Enqueues the job for later in the managers internal rolling FIFO buffer.
 * @param me - me device pointer 
 * @param pJob - pointer to the job to store in the queue.
 * @return - returns HAL_ERROR if the buffer is full.
 */
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

/**
 * @brief SPIManager_dequeue_Job - pop a job from the managers internal rolling FIFO buffer.
 * returns NULL if the queue is empty.
 * @param me - me device pointer 
 * @param pJob - pointer to the job to store in the queue.
 * @return - returns a pointer to the job taken from the queue, returns NULL if the queue is empty.
 **/
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

