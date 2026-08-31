/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* --- Camera frame buffer -------------------------------------------------
 * DMA2 (used by DCMI) cannot access DTCM RAM (0x2000_0000-0x2000_FFFF) on
 * STM32F7 - only SRAM1/SRAM2/AXI SRAM are DMA-reachable. SRAM1 starts at
 * 0x2001_0000 and is 240 KB, comfortably fitting one QVGA RGB565 frame. */
#define CAMERA_FRAME_BUFFER_ADDR   0x20010000UL
#define CAMERA_FRAME_WIDTH         320U
#define CAMERA_FRAME_HEIGHT        240U
#define CAMERA_FRAME_SIZE          (CAMERA_FRAME_WIDTH * CAMERA_FRAME_HEIGHT * 2U) /* RGB565 = 2 bytes/px */

/* --- OV7670 control pins --------------------------------------------------
 * RESET (active LOW) and PWDN (active HIGH = powered down) are plain GPIO
 * outputs, not part of the DCMI/I2C peripherals. */
#define OV7670_RESET_Pin           GPIO_PIN_3
#define OV7670_RESET_GPIO_Port     GPIOD
#define OV7670_PWDN_Pin            GPIO_PIN_4
#define OV7670_PWDN_GPIO_Port      GPIOD

/* --- Continuity-test pin (P1 connector pin 5) ------------------------------
 * Uncomment CONNECTOR_PIN5_TEST to hold this pin HIGH forever for probing
 * with a multimeter instead of running the camera application. */
/* #define CONNECTOR_PIN5_TEST */
#define TEST_PIN5_Pin              GPIO_PIN_10
#define TEST_PIN5_GPIO_Port        GPIOH

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
