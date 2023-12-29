/** \file LIS3DSH.c
 ******************************************************************************
 * @file    LIS3DSH.c
 * @brief   This file provides a state machine to setup and poll a LIS3DSH mems accelerometer
 ******************************************************************************
 * This module uses an active object and event driven state machine to walkthrough the setup 
 * and polling of an LIS3DSH mems device. The device has an initialisation state and a idle and reading state. 
 * The initialisation state writes the configuration registers on the device and then reads them back and verifies them. 
 * The idle state waits for a polling event signal from the timer armed after initialisation, when the polling event is received 
 * a txrx request is made to the downstream SPI_manager to read the output registers of the device. The device then enters the 
 * reading state until the data has been received after which it collects the results into the devices internal structure and
 * returns to the idle state waiting for the next polling event. 
 * @note 
 * The LIS3DSH device has to wait for a SPI_TXRXCOMPLETE_SIG or SPI_TIMEOUT_SIG before the data in its rxBuffer is valid. During a 
 * transaction no changes to the tx or rxBuffers are allows as they may be modified by the SPI_manager device. This rule prevents race 
 * conditions without requiring copies of data into and out of message queues. 
 * @todo - more assertions + design by contract.
}
 ******************************************************************************
 ******************************************************************************
 */
#include "LIS3DSH.h"

#include "dbc_assert.h"
#include "bsp.h"

DBC_MODULE_NAME("LIS3DSH")

#define LIS3DSH_DEFAULT_TIMEOUT_MS (10u)
#define LIS3DSH_MAX_INIT_ATTEMPTS (3u)
#define LIS3DSH_POLL_MS (10u)

/*************************Register definitions*******************/
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


/*********************private function prototypes****************************/
static void LIS3DSH_init_Handler(LIS3DSH_task_t *const me,
		SST_Evt const *const ie);

static void LIS3DSH_task_Handler(LIS3DSH_task_t *const me,
		SST_Evt const *const e);

static void LIS3DSH_initialising_Handler(LIS3DSH_task_t *const me,
		SST_Evt const *const e);

static void LIS3DSH_init_stage0(LIS3DSH_task_t *const me);

static void LIS3DSH_init_stage1(LIS3DSH_task_t *const me);

static void LIS3DSH_init_stage2(LIS3DSH_task_t *const me);

static void LIS3DSH_idle_Handler(LIS3DSH_task_t *const me,
		SST_Evt const *const e);

static void LIS3DSH_reading_Handler(LIS3DSH_task_t *const me,
		SST_Evt const *const e);

static void LIS3DSH_fault_Handler(LIS3DSH_task_t *const me,
		SST_Evt const *const e);

static void LIS3DSH_fault_enter(LIS3DSH_task_t *const me);

static void LIS3DSH_txrx_SPI(LIS3DSH_task_t *const me, uint8_t *txData,
		uint16_t len);
/*************************public function declarations*************************/

/**
 * @brief LIS3DSH_ctor - constructor for a LIS3DSH device driver
 * @param me - me device pointer 
 * @param SPIDeviceAO - pointer to spi device manager the LIS3DSH drive shall use to communicate with the device
 * @param pcsGPIOPort - pointer to the GPIO port used for the LIS3DSH chip select pin
 * @param csGPIOPin - LIS3DSH chip select pin 
 * 
 */
void LIS3DSH_ctor(LIS3DSH_task_t *me, SST_Task const *const SPIDeviceAO,
		GPIO_TypeDef *pcsGPIOPort, uint16_t csGPIOPin) {

	SST_Task_ctor(&(me->super), (SST_Handler) &LIS3DSH_init_Handler,
			(SST_Handler) &LIS3DSH_task_Handler);

	SST_TimeEvt_ctor(&(me->pollTimer), LIS3DSH_POLL_SIG, &(me->super));

	/*link in SPI device and setup the txrx transaction event used for comms*/
	me->SPIDeviceAO = SPIDeviceAO;
	me->TxRxTransactionEvent.super.sig = SPI_TXRXREQ_SIG;
	me->TxRxTransactionEvent.pJob = &(me->TxRxTransactionJob);
	me->TxRxTransactionJob.csGPIOPin = csGPIOPin;
	me->TxRxTransactionJob.pcsGPIOPort = pcsGPIOPort;
	me->TxRxTransactionJob.pAOrequester = (SST_Task const*) &(me->super);
	me->TxRxTransactionJob.rxData = (me->spiRxBuffer); /*internal link to buffer*/
	me->TxRxTransactionJob.txData = (me->spiTxBuffer);
	me->TxRxTransactionJob.lenData = 0u; /*no data for now*/
	me->TxRxTransactionJob.timeoutCnt_ms = LIS3DSH_DEFAULT_TIMEOUT_MS; /* default SPI timout*/

	/*initial state of the device is initialising*/
	me->DrvrState = LIS3DSH_INITIALISING;
	me->initStage = 1; /*initial stage is one as the first stage is always performed in init handler*/
	me->initAttempts = 0;
	
	/** @todo allow additional configuration options */
	me->ctrlReg4 = LIS3DSH_ODR_100Hz << LIS3DSH_CTRL4_ODR_POS;
	me->ctrlReg4 |= LIS3DSH_CTRL4_XEN_MSK | LIS3DSH_CTRL4_YEN_MSK
			| LIS3DSH_CTRL4_ZEN_MSK;
}

/**
 * @brief LIS3DSH_get_accel_xyz - Raw read of the LIS3DSH data (results may not be from the same polling event)
 * @param me - me device pointer 
 * @return - x y z acceleration results structure LIS3DSH_Results_t
 */
LIS3DSH_Results_t LIS3DSH_get_accel_xyz(LIS3DSH_task_t * me)
{
	/*default configuration has full scale of 2g
	 * this makes the fixed point format Q14*/
	LIS3DSH_Results_t results;
	results.x_gQ14 = me->Results.x_gQ14;
	results.x_gQ14 = me->Results.x_gQ14;
	results.x_gQ14 = me->Results.x_gQ14;
	return results;
}

/***************************private function declarations****************************/
/**
 * @brief LIS3DSH_get_accel_xyz - Raw read of the LIS3DSH data (results may not be from the same polling event)
 * @param me - me device pointer 
 * @return - x y z acceleration results structure LIS3DSH_Results_t
 */
static void LIS3DSH_init_Handler(LIS3DSH_task_t *const me,
		SST_Evt const *const ie) {
	(void) ie;
	LIS3DSH_init_stage0(me);
}

/**
 * @brief LIS3DSH_task_Handler - Core active object task handler, implements a switch case style state machine 
 * and calls the appropriate state handler.
 * @param me - me device pointer 
 * @param e- event passed from the kernel 
 */
static void LIS3DSH_task_Handler(LIS3DSH_task_t *const me,
		SST_Evt const *const e) {
	/*state driven switch, event signal is checked in each state.*/
	switch (me->DrvrState) {
	case LIS3DSH_INITIALISING: {
		LIS3DSH_initialising_Handler(me, e);
		break;
	}
	case LIS3DSH_IDLE: {
		LIS3DSH_idle_Handler(me, e);
		break;
	}
	case LIS3DSH_READING: {
		LIS3DSH_reading_Handler(me, e);
		break;
	}
	case LIS3DSH_FAULT: {
		LIS3DSH_fault_Handler(me, e);
		break;
	}
	default: {
		DBC_ERROR(100);
		break;
	}
	}
}

/**
 * @brief LIS3DSH_initialising_Handler - Handler used when the device is in the initialising state. 
 * This handler walks the device through the 3 initialisation states. 
 * 0-> tx write configuration to the device 
 * 1-> request a read of the configuration 
 * 2-> validate the registers have been written succesfully.
 * @param me - me device pointer 
 * @param e- event passed from the kernel 
 */
static void LIS3DSH_initialising_Handler(LIS3DSH_task_t *const me,
		SST_Evt const *const e) {
	switch (e->sig) {
	case SPI_TXRXCOMPLETE_SIG: {
		switch (me->initStage) {
		case 1: {
			/*call a read of the control register to check it has been written*/
			LIS3DSH_init_stage1(me);
			break;
		}
		case 2: {
			/*verify the register is as expected, otherwise retry*/
			LIS3DSH_init_stage2(me);
			break;
		}
		default: {
			DBC_ERROR(200);
			break;
		}
		}
		break;
	}
	case SPI_TIMEOUT_SIG: {
		LIS3DSH_fault_enter(me);
		break;
	}
	case LIS3DSH_POLL_SIG: {
		/*if we get a polling signal in initialisation just ignore it*/
		break;
	}
	default: {
		DBC_ERROR(210);
		break;
	}
	}
}


/**
 * @brief LIS3DSH_init_stage0 - requests the write of the configuration registers.
 * @param me - me device pointer 
 */
static void LIS3DSH_init_stage0(LIS3DSH_task_t *const me) {
	/*write control registers configuration to the device*/
	uint8_t spiTxBuffer[] = { LIS3DSH_CTRL4, me->ctrlReg4 };
	me->DrvrState = LIS3DSH_INITIALISING;
	me->initStage = 1;
	LIS3DSH_txrx_SPI(me, spiTxBuffer, sizeof(spiTxBuffer));
}

/**
 * @brief LIS3DSH_init_stage1 - requests a read of the configuration registers.
 * @param me - me device pointer 
 */
static void LIS3DSH_init_stage1(LIS3DSH_task_t *const me) {
	/*call a read of the control register to check it has been written*/
	me->initStage = 2;
	uint8_t spiTxBuffer[] = { LIS3DSH_READ | LIS3DSH_CTRL4, 0x00u };
	LIS3DSH_txrx_SPI(me, spiTxBuffer, sizeof(spiTxBuffer));
}

/**
 * @brief LIS3DSH_init_stage2 - verifies the configuration registers. 
 * if verification is succesful the driver state is moved to the IDLE state and the driver begins the polling the device. 
 * If verification is unsuccesful the initialisation events are repeats by LIS3DSH_MAX_INIT_ATTEMPTS
 * @param me - me device pointer 
 */
static void LIS3DSH_init_stage2(LIS3DSH_task_t *const me) {
	/*check read back register is equal to the desired config*/
	if (me->spiRxBuffer[1] == me->ctrlReg4) {
		/*initialisation complete*/
		me->DrvrState = LIS3DSH_IDLE; /*move to idle state*/
		SST_TimeEvt_arm(&(me->pollTimer), 1u, LIS3DSH_POLL_MS); /*arm the timer to start polling data*/
	} else {
		me->initStage = 0;
		me->initAttempts++;
		if (me->initAttempts >= LIS3DSH_MAX_INIT_ATTEMPTS) {
			LIS3DSH_fault_enter(me);
		} else {
			LIS3DSH_init_stage0(me); /*try again*/
		}
	}
}

/**
 * @brief LIS3DSH_idle_Handler - On receipt of a polling event while in the idle state,
 * the device triggers the read of the OUT X,Y,Z registers in one transaction to read the accelerations.
 * The device modes to the READING state in this event. 
 * @param me - me device pointer 
 * @param e- event passed from the kernel 
 */
static void LIS3DSH_idle_Handler(LIS3DSH_task_t *const me,
		SST_Evt const *const e) {
	switch (e->sig) {
	case SPI_TXRXCOMPLETE_SIG: {
		/*shouldn't get a complete signal in idle state*/
		break;
	}
	case SPI_TIMEOUT_SIG: {
		/*shouldn't get a timeout signal in idle handler*/
		LIS3DSH_fault_enter(me);
		break;
	}
	case LIS3DSH_POLL_SIG: {
		/*trigger a new request for data to the device*/
		/*txrx 7 bytes ( 1 for read instruction 6 more to get the 6 result registers into read buffer*/
		me->DrvrState = LIS3DSH_READING; /*enter the reading state*/
		uint8_t spiTxBuffer[7] = { LIS3DSH_READ | LIS3DSH_OUT_X_L };
		LIS3DSH_txrx_SPI(me, spiTxBuffer, sizeof(spiTxBuffer));
		break;
	}
	default: {
		DBC_ERROR(210);
		break;
	}
	}

}

/**
 * @brief LIS3DSH_reading_Handler - When the TXRXComplete signal is received in the reading state. 
 * The handler converts and stores the results as raw int16_t values.  
 * If a TIMEOUT event is received the device driver enters a faulty state. 
 * @param me - me device pointer 
 * @param e- event passed from the kernel 
 **/
static void LIS3DSH_reading_Handler(LIS3DSH_task_t *const me,
		SST_Evt const *const e) {
	switch (e->sig) {
	case SPI_TXRXCOMPLETE_SIG: {

		me->Results.x_gQ14 = 0;
		me->Results.x_gQ14 = (int16_t) (me->spiRxBuffer[2] << 8
				| me->spiRxBuffer[1]);

		me->Results.y_gQ14 = 0;
		me->Results.y_gQ14 = (int16_t) (me->spiRxBuffer[4] << 8
				| me->spiRxBuffer[3]);

		me->Results.z_gQ14 = 0;
		me->Results.z_gQ14 = (int16_t) (me->spiRxBuffer[6] << 8
				| me->spiRxBuffer[5]);

		me->DrvrState = LIS3DSH_IDLE;

		break;
	}
	case SPI_TIMEOUT_SIG: {
		/*shouldn't get a timeout signal in idle handler*/
		LIS3DSH_fault_enter(me);
		break;
	}
	case LIS3DSH_POLL_SIG: {
		/*shouldn't get a complete signal in idle state unless read is extremely slow.
		 * ignore this request and wait for timeout from SPI*/
		break;
	}
	default: {
		DBC_ERROR(210);
		break;
	}
	}
}

/**
 * @brief LIS3DSH_fault_Handler - In the fault handler state, the device does nothing.
 * @param me - me device pointer 
 */
static void LIS3DSH_fault_Handler(LIS3DSH_task_t *const me,
		SST_Evt const *const e) {
	/*do nothing*/
	(void) me;
	(void) e;
}

/**
 * @brief LIS3DSH_fault_enter - Helper function to push the device driver into the fault state. 
 * Write default values to the results registers and disarms the polling timer. 
 * @param me - me device pointer 
 */
static void LIS3DSH_fault_enter(LIS3DSH_task_t *const me) {
	me->DrvrState = LIS3DSH_FAULT;
	me->Results.x_gQ14 = 0;
	me->Results.y_gQ14 = 0;
	me->Results.z_gQ14 = 0;
	SST_TimeEvt_disarm(&(me->pollTimer));
}


/**
 * @brief LIS3DSH_fault_enter - Sends a txrx request to the SPIManager that is configured for this device. 
 * Flushes the rx buffer and copies the required data into the device drivers TxBuffer.
 * @note once called this function may be called and the buffers not used until a response has been received from the SPIManager 
 * @param me - me device pointer 
 * @param txData - pointer to the transmit data buffer 
 * @param len - length of the transmitted data. 
 */
static void LIS3DSH_txrx_SPI(LIS3DSH_task_t *const me, uint8_t *txData,
		uint16_t len) {
	/**@pre whenever this is called the LIS3DSH handler cannot call it again
	 *  until a response has been received from the SPI_manager because the
	 *   buffer is used by the spi device until complete*/

	me->TxRxTransactionJob.lenData = len;
	/*write tx data into tx buffer and clear out receipt buffer*/
	for (uint32_t i = 0; i < len; i++) {
		me->spiTxBuffer[i] = txData[i];
		me->spiRxBuffer[i] = 0;
	}
	SPIManager_post_txrx_Request((SST_Task* const ) me->SPIDeviceAO,
			&(me->TxRxTransactionEvent));
}
