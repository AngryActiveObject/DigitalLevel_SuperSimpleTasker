/*
 * bsp.c
 *
 *  Created on: Nov 20, 2023
 *      Author: Duncan
 */


#include "bsp.h"
#include "main.h"
#include "sst.h"
#include "tim.h"
#include "blinky.h"
#include "spi_manager.h"


static void MX_SPI1_Init(void);
static void MX_GPIO_Init(void);

void BSP_init_SPIManager_Task(void);
void BSP_init_blinky_task(void);

/*task configuration*/


/************************SPI task config**********************************/

SPI_HandleTypeDef hspi1; /*spi device handler (initialised in HAL_SPI init functions*/

#define SPIMANAGER_IRQn (80u)
#define SPIMANAGER_IRQHandler HASH_RNG_IRQHandler
#define SPIMANAGER_TASK_PRIORITY ((SST_TaskPrio)2u)
#define SPIHANDLER_MSG_QUEUELEN (10u)

static SPIManager_Task_t SpiMgrInstance;
static SST_Evt const *spiMsgQueue[SPIHANDLER_MSG_QUEUELEN];
static SST_Task *const AO_SpiMgr = &(SpiMgrInstance.super); /*Scheduler task pointer*/

void SPIMANAGER_IRQHandler(void) {
	SST_Task_activate(AO_SpiMgr); /*trigger the task on interrupt.*/
}

void BSP_init_SPIManager_Task(void) {
	SPIManager_ctor(&SpiMgrInstance, &hspi1);

	SST_Task_setIRQ(AO_SpiMgr, SPIMANAGER_IRQn);

	NVIC_EnableIRQ(SPIMANAGER_IRQn);

	SST_Task_start(AO_SpiMgr, SPIMANAGER_TASK_PRIORITY, spiMsgQueue,
	SPIHANDLER_MSG_QUEUELEN, 0);
}

/*immutable txrx complete event signal*/
static const SST_Evt TxxRxCompleteEventSignal = { .sig = SPI_TXRXCOMPLETE_SIG };
static SST_Evt const *const pTxRxCompleteEventSignal = &TxxRxCompleteEventSignal;

/*SPI device needs to post calback events to the SPI managers*/
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
	if (hspi == &hspi1) {
		SST_Task_post(AO_SpiMgr, pTxRxCompleteEventSignal);
	}
}

/*implement the SPI IRQ handler*/
void SPI1_IRQHandler(void)
{
HAL_SPI_IRQHandler(&hspi1);
}

/*****************************LIS3DSH Task Config************************/
#define LIS3DSH_IRQn (DCMI_IRQn)
#define LIS3DSH_IRQHandler DCMI_IRQHandler
#define LIS3DSH_TASK_PRIORITY ((SST_TaskPrio)1u)
#define LIS3DSH_MSG_QUEUELEN (2u)

static LIS3DSH_task_t LIS3DSHInstance;

static SST_Evt const *LIS3DSHMsgQueue[LIS3DSH_MSG_QUEUELEN];
static SST_Task *const AO_LIS3DSH = &(LIS3DSHInstance.super); /*Scheduler task pointer*/

void LIS3DSH_IRQHandler(void) {
	SST_Task_activate(AO_LIS3DSH); /*trigger the task on interrupt.*/
}

void BSP_init_LIS3DSH_Task(void) {

	LIS3DSH_ctor(&LIS3DSHInstance, AO_SpiMgr,
	CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin);

	SST_Task_setIRQ(AO_LIS3DSH, LIS3DSH_IRQn);

	NVIC_EnableIRQ(LIS3DSH_IRQn);

	SST_Task_start(AO_LIS3DSH, LIS3DSH_TASK_PRIORITY, LIS3DSHMsgQueue,
	LIS3DSH_MSG_QUEUELEN, 0);
}


LIS3DSH_Results_t LIS3DSH_read(void)
{
	/*unprotected read of accel data. (doesn't ensure the 3 values come from the same sample)*/
	return LIS3DSH_get_accel_xyz(&LIS3DSHInstance);
}

/*****************************Blinky Task Config************************/

#define BLINKY_IRQn (79u)
#define BLINKY_IRQHANDLER0 UNUSED_IRQHandler0
#define BLINKY_TASK_PRIORITY ((SST_TaskPrio)1u)

static BlinkyTask_T BlinkyInstance;
static SST_Task *const AO_Blink = &(BlinkyInstance.super); /*Scheduler task pointer*/

#define BLINKY_MSG_QUEUELEN (10u)
static SST_Evt const *blinkyMsgQueue[BLINKY_MSG_QUEUELEN]; /*initialised in the SST_Task_Start function*/

void BLINKY_IRQHANDLER0(void) {
	SST_Task_activate(AO_Blink); /*trigger the task on interrupt.*/
}

void BSP_init_blinky_task(void) {
	Blinky_ctor(&BlinkyInstance);

	SST_Task_setIRQ(AO_Blink, BLINKY_IRQn);

	NVIC_EnableIRQ(BLINKY_IRQn); /*RNG interrupt number is for the blinky task*/

	SST_Task_start(AO_Blink, BLINKY_TASK_PRIORITY, blinkyMsgQueue,
	BLINKY_MSG_QUEUELEN, 0); /*no intial event*/
}

void BSP_init(void) {
	MX_GPIO_Init();
	MX_SPI1_Init();
	MX_TIM4_Init();
	BSP_init_SPIManager_Task();
	BSP_init_blinky_task();
	BSP_init_LIS3DSH_Task();
}



void set_blue_LED_duty(uint16_t duty) {
	TIM4->CCR4 = duty;
}
void set_red_LED_duty(uint16_t duty) {
	TIM4->CCR3 = duty;
}

void set_orange_LED_duty(uint16_t duty) {
	TIM4->CCR2 = duty;
}

void set_green_LED_duty(uint16_t duty) {
	TIM4->CCR1 = duty;
}

void DBC_fault_handler(char const *const module, int const label) {
	/*
	 * NOTE: add here your application-specific error handling
	 */
	(void) module;
	(void) label;

	/* set PRIMASK to disable interrupts and stop SST right here */
	__asm volatile ("cpsid i");

	set_red_LED_duty(0u); /*turn off red led*/

#ifndef NDEBUG
	/* blink LED*/
#endif
	NVIC_SystemReset();
}

/* CUBEMx auto generated functions */
void HAL_MspInit(void) {

	__HAL_RCC_SYSCFG_CLK_ENABLE();
	__HAL_RCC_PWR_CLK_ENABLE();

}

/**
 * @brief SPI MSP Initialization
 * This function configures the hardware resources used in this example
 * @param hspi: SPI handle pointer
 * @retval None
 */
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	if (hspi->Instance == SPI1) {

		__HAL_RCC_SPI1_CLK_ENABLE();

		__HAL_RCC_GPIOA_CLK_ENABLE();
		/**SPI1 GPIO Configuration
		 PA5     ------> SPI1_SCK
		 PA6     ------> SPI1_MISO
		 PA7     ------> SPI1_MOSI
		 */
		GPIO_InitStruct.Pin = SPI1_SCK_Pin | SPI1_MISO_Pin | SPI1_MOSI_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = SPI1_SCK_Pin | SPI1_MISO_Pin | SPI1_MOSI_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);


	}

}

/**
 * @brief SPI MSP De-Initialization
 * This function freeze the hardware resources used in this example
 * @param hspi: SPI handle pointer
 * @retval None
 */
void HAL_SPI_MspDeInit(SPI_HandleTypeDef *hspi) {
	if (hspi->Instance == SPI1) {

		__HAL_RCC_SPI1_CLK_DISABLE();

		/**SPI1 GPIO Configuration
		 PA5     ------> SPI1_SCK
		 PA6     ------> SPI1_MISO
		 PA7     ------> SPI1_MOSI
		 */
		HAL_GPIO_DeInit(GPIOA, SPI1_SCK_Pin | SPI1_MISO_Pin | SPI1_MOSI_Pin);

	}

}

void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 8;
	RCC_OscInitStruct.PLL.PLLN = 336;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void) {

	/* USER CODE BEGIN SPI1_Init 0 */

	/* USER CODE END SPI1_Init 0 */

	/* USER CODE BEGIN SPI1_Init 1 */

	/* USER CODE END SPI1_Init 1 */
	/* SPI1 parameter configuration*/
	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
	hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 10;
	if (HAL_SPI_Init(&hspi1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN SPI1_Init 2 */
	NVIC_EnableIRQ(SPI1_IRQn);
	/* USER CODE END SPI1_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */
	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin,
			GPIO_PIN_SET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOD,
	LD4_Pin | LD3_Pin | LD5_Pin | LD6_Pin | Audio_RST_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : CS_I2C_SPI_Pin */
	GPIO_InitStruct.Pin = CS_I2C_SPI_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(CS_I2C_SPI_GPIO_Port, &GPIO_InitStruct);
	HAL_GPIO_WritePin(CS_I2C_SPI_GPIO_Port, CS_I2C_SPI_Pin, GPIO_PIN_SET); /*set chip select high initially*/

	/*Configure GPIO pin : OTG_FS_PowerSwitchOn_Pin */
	GPIO_InitStruct.Pin = OTG_FS_PowerSwitchOn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(OTG_FS_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : PDM_OUT_Pin */
	GPIO_InitStruct.Pin = PDM_OUT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	/*  HAL_GPIO_Init(PDM_OUT_GPIO_Port, &GPIO_InitStruct);*/

	/*Configure GPIO pin : B1_Pin */
	GPIO_InitStruct.Pin = B1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : I2S3_WS_Pin */
	GPIO_InitStruct.Pin = I2S3_WS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
	HAL_GPIO_Init(I2S3_WS_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : BOOT1_Pin */
	GPIO_InitStruct.Pin = BOOT1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(BOOT1_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : CLK_IN_Pin */
	GPIO_InitStruct.Pin = CLK_IN_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(CLK_IN_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : LD4_Pin LD3_Pin LD5_Pin LD6_Pin
	 Audio_RST_Pin */
	GPIO_InitStruct.Pin = LD4_Pin | LD3_Pin | LD5_Pin | LD6_Pin | Audio_RST_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/*Configure GPIO pins : I2S3_MCK_Pin I2S3_SCK_Pin I2S3_SD_Pin */
	GPIO_InitStruct.Pin = I2S3_MCK_Pin | I2S3_SCK_Pin | I2S3_SD_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : OTG_FS_OverCurrent_Pin */
	GPIO_InitStruct.Pin = OTG_FS_OverCurrent_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(OTG_FS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : Audio_SCL_Pin Audio_SDA_Pin */
	GPIO_InitStruct.Pin = Audio_SCL_Pin | Audio_SDA_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : MEMS_INT2_Pin */
	GPIO_InitStruct.Pin = MEMS_INT2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(MEMS_INT2_GPIO_Port, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */
	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

