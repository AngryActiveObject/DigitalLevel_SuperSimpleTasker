/**
 ******************************************************************************
 * @file    LIS3DSH.c
 * @author  Duncan Hennion
 * @brief   Driver task for LIS3DSH MEMS chip
 **/
#include "LIS3DSH.h"

#include "dbc_assert.h"
#include "bsp.h"

DBC_MODULE_NAME("LIS3DSH")

#define LIS3DSH_DEFAULT_TIMEOUT_MS (10u)
#define LIS3DSH_MAX_INIT_ATTEMPTS (3u)
#define LIS3DSH_POLL_MS (5u)

/*private function prototypes*/
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
/*public function declarations*/
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

	me->ctrlReg4 = LIS3DSH_ODR_100Hz << LIS3DSH_CTRL4_ODR_POS;
	me->ctrlReg4 |= LIS3DSH_CTRL4_XEN_MSK | LIS3DSH_CTRL4_YEN_MSK
			| LIS3DSH_CTRL4_ZEN_MSK;
}

LIS3DSH_Results_t LIS3DSH_get_accel_xyz(LIS3DSH_task_t * me)
{
	LIS3DSH_Results_t results;
	results.x_g = me->Results.x_g;
	results.y_g = me->Results.y_g;
	results.z_g = me->Results.z_g;
	return results;
}

/*private function declarations*/
static void LIS3DSH_init_Handler(LIS3DSH_task_t *const me,
		SST_Evt const *const ie) {
	(void) ie;
	LIS3DSH_init_stage0(me);
}

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

static void LIS3DSH_init_stage0(LIS3DSH_task_t *const me) {
	/*write control registers configuration to the device*/
	uint8_t spiTxBuffer[] = { LIS3DSH_CTRL4, me->ctrlReg4 };
	me->DrvrState = LIS3DSH_INITIALISING;
	me->initStage = 1;
	LIS3DSH_txrx_SPI(me, spiTxBuffer, sizeof(spiTxBuffer));
}

static void LIS3DSH_init_stage1(LIS3DSH_task_t *const me) {
	/*call a read of the control register to check it has been written*/
	me->initStage = 2;
	uint8_t spiTxBuffer[] = { LIS3DSH_READ | LIS3DSH_CTRL4, 0x00u };
	LIS3DSH_txrx_SPI(me, spiTxBuffer, sizeof(spiTxBuffer));
}

static void LIS3DSH_init_stage2(LIS3DSH_task_t *const me) {
	/*check read back register is equal to the desired config*/
	if (me->spiRxBuffer[1] == me->ctrlReg4) {
		/*initialisation complete*/
		me->DrvrState = LIS3DSH_IDLE; /*move to idle state*/
		SST_TimeEvt_arm(&(me->pollTimer), LIS3DSH_POLL_MS, LIS3DSH_POLL_MS); /*arm the timer to start polling data*/
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

static void LIS3DSH_reading_Handler(LIS3DSH_task_t *const me,
		SST_Evt const *const e) {
	switch (e->sig) {
	case SPI_TXRXCOMPLETE_SIG: {

		me->Results.x_g = 0;
		me->Results.x_g = (int16_t) (me->spiRxBuffer[2] << 8
				| me->spiRxBuffer[1]);

		me->Results.y_g = 0;
		me->Results.y_g = (int16_t) (me->spiRxBuffer[4] << 8
				| me->spiRxBuffer[3]);

		me->Results.z_g = 0;
		me->Results.z_g = (int16_t) (me->spiRxBuffer[6] << 8
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

static void LIS3DSH_fault_Handler(LIS3DSH_task_t *const me,
		SST_Evt const *const e) {
	/*do nothing*/
	(void) me;
	(void) e;
}

static void LIS3DSH_fault_enter(LIS3DSH_task_t *const me) {
	me->DrvrState = LIS3DSH_FAULT;
	me->Results.x_g = 0;
	me->Results.y_g = 0;
	me->Results.z_g = 0;
	SST_TimeEvt_disarm(&(me->pollTimer));
}

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
