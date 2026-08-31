/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ov7670.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

DCMI_HandleTypeDef hdcmi;
DMA_HandleTypeDef hdma_dcmi;

I2C_HandleTypeDef hi2c3;

PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* USER CODE BEGIN PV */
/* Frame buffer deliberately placed at a fixed SRAM1 address, NOT a normal
 * .bss array, so it can never end up in DTCM (see main.h for why). */
static uint8_t * const camera_frame_buffer = (uint8_t *)CAMERA_FRAME_BUFFER_ADDR;

/* Set by HAL_DCMI_FrameEventCallback() when a snapshot capture is complete */
static volatile uint8_t frame_ready = 0;

/* 8-byte header sent before every frame so the PC-side script can find the
 * start of a frame and know its size: "AA 55 AA 55" + width(LE16) + height(LE16) */
static const uint8_t frame_header[4] = {0xAA, 0x55, 0xAA, 0x55};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_DCMI_Init(void);
static void MX_I2C3_Init(void);
static void MX_USB_OTG_FS_PCD_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
static void CameraSendFrame_UART(void);
static void USART1_Write(const uint8_t *data, uint32_t length);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_DCMI_Init();
  MX_I2C3_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */

  /* Start XCLK (PA5, ~12 MHz) before touching the camera at all: the OV7670
   * needs this clock running before it will ACK on SCCB, and long before it
   * can produce PCLK/HSYNC/VSYNC. */
  HAL_Delay(10);

#ifdef CONNECTOR_PIN5_TEST
  /* Continuity-check mode: hold P1 connector pin 5 (PH10) HIGH forever and
   * do nothing else, so you can probe it with a multimeter. Disabled unless
   * CONNECTOR_PIN5_TEST is defined in main.h. */
  {
    GPIO_InitTypeDef testPinInit = {0};
    __HAL_RCC_GPIOH_CLK_ENABLE();
    testPinInit.Pin   = TEST_PIN5_Pin;
    testPinInit.Mode  = GPIO_MODE_OUTPUT_PP;
    testPinInit.Pull  = GPIO_NOPULL;
    testPinInit.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(TEST_PIN5_GPIO_Port, &testPinInit);
    HAL_GPIO_WritePin(TEST_PIN5_GPIO_Port, TEST_PIN5_Pin, GPIO_PIN_SET);
  }
  while (1)
  {
    /* stay here forever: probe P1 pin 5 with a multimeter, it must read ~3.3V */
  }
#endif /* CONNECTOR_PIN5_TEST */

  /* Normal operation: bring up the camera over SCCB (I2C3) */
  if (OV7670_Init(&hi2c3) != HAL_OK)
  {
    /* Camera did not answer on I2C3 (PA8=SCL/PC9=SDA) or returned the wrong
     * ID. Check SCL/SDA wiring, VCC/GND, and that XCLK (PA5) is toggling
     * before assuming the code is wrong. */
    Error_Handler();
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    frame_ready = 0;

    /* Capture exactly one frame into camera_frame_buffer */
    if (HAL_DCMI_Start_DMA(&hdcmi, DCMI_MODE_SNAPSHOT,
                            (uint32_t)camera_frame_buffer,
                            CAMERA_FRAME_SIZE / 4) != HAL_OK)
    {
      Error_Handler();
    }

    /* Wait for HAL_DCMI_FrameEventCallback() to flag completion */
    uint32_t wait_start = HAL_GetTick();
    while (!frame_ready)
    {
      if ((HAL_GetTick() - wait_start) > 2000)
      {
        /* Timed out waiting for VSYNC/HSYNC/PCLK: camera not sending data.
         * Re-check D0-D7/HSYNC/VSYNC/PIXCLK wiring against the pin table. */
        break;
      }
    }

    if (frame_ready)
    {
      CameraSendFrame_UART();
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

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
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 432;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief DCMI Initialization Function
  * @param None
  * @retval None
  */
static void MX_DCMI_Init(void)
{

  /* USER CODE BEGIN DCMI_Init 0 */

  /* USER CODE END DCMI_Init 0 */

  /* USER CODE BEGIN DCMI_Init 1 */

  /* USER CODE END DCMI_Init 1 */
  hdcmi.Instance = DCMI;
  /* OV7670 drives HSYNC/VSYNC pins directly (no embedded BT.656 sync codes),
   * so hardware synchronization must be used. This also requires PA4
   * (DCMI_HSYNC) and PG9 (DCMI_VSYNC) to be configured, see
   * HAL_DCMI_MspInit() in stm32f7xx_hal_msp.c. */
  hdcmi.Init.SynchroMode = DCMI_SYNCHRO_HARDWARE;
  hdcmi.Init.PCKPolarity = DCMI_PCKPOLARITY_FALLING;
  /* OV7670 HREF/VSYNC are active-high by default (MVFP/COM10 registers in
   * ov7670.c left at default polarity). If the captured image looks torn,
   * shifted, or blank, try flipping PCKPolarity to RISING first. */
  hdcmi.Init.VSPolarity = DCMI_VSPOLARITY_HIGH;
  hdcmi.Init.HSPolarity = DCMI_HSPOLARITY_HIGH;
  hdcmi.Init.CaptureRate = DCMI_CR_ALL_FRAME;
  hdcmi.Init.ExtendedDataMode = DCMI_EXTEND_DATA_8B;
  hdcmi.Init.SyncroCode.FrameEndCode = 0;
  hdcmi.Init.SyncroCode.FrameStartCode = 0;
  hdcmi.Init.SyncroCode.LineStartCode = 0;
  hdcmi.Init.SyncroCode.LineEndCode = 0;
  hdcmi.Init.JPEGMode = DCMI_JPEG_DISABLE;
  hdcmi.Init.ByteSelectMode = DCMI_BSM_ALL;
  hdcmi.Init.ByteSelectStart = DCMI_OEBS_ODD;
  hdcmi.Init.LineSelectMode = DCMI_LSM_ALL;
  hdcmi.Init.LineSelectStart = DCMI_OELS_ODD;
  if (HAL_DCMI_Init(&hdcmi) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DCMI_Init 2 */

  /* USER CODE END DCMI_Init 2 */

}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.Timing = 0x20404768;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief USART1 Initialization Function (used to stream frames to the PC
  *        over the ST-LINK Virtual COM Port, PA9=TX / PB7=RX)
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  /* USART1 uses PA9 (TX) and PB7 (RX), both alternate function 7. */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = GPIO_PIN_7;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* PCLK2 is 108 MHz. With 16x oversampling, BRR = 108 MHz / 921600,
   * rounded to the nearest integer (117). */
  USART1->CR1 = 0U;
  USART1->CR2 = 0U;
  USART1->CR3 = 0U;
  USART1->BRR = (108000000U + (921600U / 2U)) / 921600U;
  USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

/**
  * @brief USB_OTG_FS Initialization Function
  * @param None
  * @retval None
  */
static void MX_USB_OTG_FS_PCD_Init(void)
{

  /* USER CODE BEGIN USB_OTG_FS_Init 0 */

  /* USER CODE END USB_OTG_FS_Init 0 */

  /* USER CODE BEGIN USB_OTG_FS_Init 1 */

  /* USER CODE END USB_OTG_FS_Init 1 */
  hpcd_USB_OTG_FS.Instance = USB_OTG_FS;
  hpcd_USB_OTG_FS.Init.dev_endpoints = 6;
  hpcd_USB_OTG_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_OTG_FS.Init.dma_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_OTG_FS.Init.Sof_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.vbus_sensing_enable = DISABLE;
  hpcd_USB_OTG_FS.Init.use_dedicated_ep1 = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_OTG_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_OTG_FS_Init 2 */

  /* USER CODE END USB_OTG_FS_Init 2 */

}

/**
  * @brief TIM2 Initialization Function - free-running 50% duty PWM on
  *        TIM2_CH1 (PA5), used purely as the OV7670's XCLK source.
  *        Timer clock here is 108 MHz (APB1 x2); ARR=8 (period=9 counts)
  *        gives 108 MHz / 9 = 12 MHz, inside the sensor's 10-48 MHz XCLK
  *        range. CCR=4 gives ~44/56% duty, which is fine for a clock input.
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_TIM2_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* TIM2 runs from APB1's doubled 108 MHz timer clock.  PSC=0 and ARR=8
   * produce a 12 MHz XCLK; CCR1=4 gives a near-50% duty cycle. */
  TIM2->CR1 = 0U;
  TIM2->PSC = 0U;
  TIM2->ARR = 8U;
  TIM2->CCR1 = 4U;
  TIM2->CCMR1 = (6U << TIM_CCMR1_OC1M_Pos) | TIM_CCMR1_OC1PE;
  TIM2->CCER = TIM_CCER_CC1E;
  TIM2->EGR = TIM_EGR_UG;
  TIM2->CR1 = TIM_CR1_ARPE | TIM_CR1_CEN;
}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PH13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* OV7670_RESET idle HIGH (not held in reset), OV7670_PWDN idle LOW
   * (powered up) before anything else touches the camera. */
  HAL_GPIO_WritePin(OV7670_RESET_GPIO_Port, OV7670_RESET_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(OV7670_PWDN_GPIO_Port, OV7670_PWDN_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = OV7670_RESET_Pin | OV7670_PWDN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OV7670_RESET_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
  * @brief  Called by the HAL DCMI IRQ handler once a full snapshot frame
  *         has landed in camera_frame_buffer.
  */
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
  frame_ready = 1;
}

/**
  * @brief  Sends the last captured frame to the PC over USART1
  *         (header + raw RGB565 bytes). Blocking, ~1.6s per frame at
  *         921600 baud -> this is a "does it work" link, not a live feed.
  *         Swap this for the USB CDC class later for real frame rates.
  */
static void CameraSendFrame_UART(void)
{
  uint8_t size_hdr[4];
  size_hdr[0] = (uint8_t)(CAMERA_FRAME_WIDTH & 0xFF);
  size_hdr[1] = (uint8_t)((CAMERA_FRAME_WIDTH >> 8) & 0xFF);
  size_hdr[2] = (uint8_t)(CAMERA_FRAME_HEIGHT & 0xFF);
  size_hdr[3] = (uint8_t)((CAMERA_FRAME_HEIGHT >> 8) & 0xFF);

  USART1_Write(frame_header, sizeof(frame_header));
  USART1_Write(size_hdr, sizeof(size_hdr));
  USART1_Write(camera_frame_buffer, CAMERA_FRAME_SIZE);
}

/**
 * @brief Send bytes synchronously through USART1.
 * @note TXE only signals that the data register is empty, which permits the
 *       next byte to be queued while the previous byte is shifting out.
 */
static void USART1_Write(const uint8_t *data, uint32_t length)
{
  while (length-- != 0U)
  {
    while ((USART1->ISR & USART_ISR_TXE) == 0U)
    {
    }
    USART1->TDR = *data++;
  }
}

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
