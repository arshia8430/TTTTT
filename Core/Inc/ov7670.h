/**
  ******************************************************************************
  * @file    ov7670.h
  * @brief   Minimal OV7670 driver: SCCB (I2C) register access + a fixed
  *          QVGA / RGB565 configuration to match the DCMI setup in main.c.
  *
  * Wiring assumed (see stm32f7xx_hal_msp.c / the connections table you
  * were given): SCCB on I2C3 (PA8=SCL, PC9=SDA), XCLK driven by the MCU on
  * PA5 (TIM2_CH1) - the sensor has NO onboard oscillator, so nothing on
  * this driver will work until that clock is running (see MX_TIM2_Init()
  * and HAL_TIM_PWM_Start() in main.c, called before OV7670_Init()).
  ******************************************************************************
  */
#ifndef __OV7670_H
#define __OV7670_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* 8-bit SCCB/I2C slave address (7-bit address 0x21 shifted left by 1) */
#define OV7670_I2C_ADDR      0x42U

/* A few register addresses referenced directly by this driver */
#define OV7670_REG_PID       0x0AU  /* Product ID MSB - must read 0x76 */
#define OV7670_REG_VER       0x0BU  /* Product ID LSB - must read 0x73 */
#define OV7670_REG_COM7      0x12U  /* Common control 7 (reset / format) */

/**
  * @brief  Resets, identifies, and configures the OV7670 for QVGA
  *         (320x240) RGB565 output over DCMI.
  * @param  hi2c: handle of the I2C peripheral wired to SIOC/SIOD (I2C3 here)
  * @retval HAL_OK on success; HAL_ERROR if the sensor didn't answer or its
  *         PID/VER didn't match an OV7670 (check wiring/XCLK first).
  */
HAL_StatusTypeDef OV7670_Init(I2C_HandleTypeDef *hi2c);

/**
  * @brief  Writes one SCCB/I2C register.
  */
HAL_StatusTypeDef OV7670_WriteReg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t value);

/**
  * @brief  Reads one SCCB/I2C register.
  */
HAL_StatusTypeDef OV7670_ReadReg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t *value);

#ifdef __cplusplus
}
#endif

#endif /* __OV7670_H */
