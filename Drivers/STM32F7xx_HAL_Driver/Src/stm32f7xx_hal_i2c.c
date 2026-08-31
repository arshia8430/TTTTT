/**
 ******************************************************************************
 * @file    stm32f7xx_hal_i2c.c
 * @brief   Polling I2C support used by the OV7670 camera application.
 ******************************************************************************
 *
 * The project previously contained an empty file at this path.  CubeIDE did
 * compile that file, but the resulting object had no definitions for the HAL
 * I2C functions used by main.c and ov7670.c, which caused the link failure.
 */

#include "stm32f7xx_hal.h"

/* This camera only uses short polling transfers.  Keeping the transaction
 * implementation here also makes the project independent of an accidentally
 * omitted generated I2C driver source file. */
#define I2C_ERROR_FLAGS (I2C_ISR_NACKF | I2C_ISR_BERR | I2C_ISR_ARLO | I2C_ISR_OVR)

static HAL_StatusTypeDef I2C_WaitForFlag(I2C_HandleTypeDef *hi2c,
                                         uint32_t flag, uint32_t timeout);
static HAL_StatusTypeDef I2C_StartTransfer(I2C_HandleTypeDef *hi2c,
                                            uint16_t address, uint16_t length,
                                            uint32_t direction);
static HAL_StatusTypeDef I2C_FinishTransfer(I2C_HandleTypeDef *hi2c,
                                             uint32_t timeout);

HAL_StatusTypeDef HAL_I2C_Init(I2C_HandleTypeDef *hi2c)
{
  if ((hi2c == NULL) || (hi2c->Instance == NULL))
  {
    return HAL_ERROR;
  }

  HAL_I2C_MspInit(hi2c);

  __HAL_I2C_DISABLE(hi2c);
  hi2c->Instance->TIMINGR = hi2c->Init.Timing;
  hi2c->Instance->CR1 = I2C_CR1_PE;
  hi2c->ErrorCode = HAL_I2C_ERROR_NONE;
  hi2c->State = HAL_I2C_STATE_READY;
  hi2c->Mode = HAL_I2C_MODE_NONE;
  hi2c->Lock = HAL_UNLOCKED;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c,
                                          uint16_t DevAddress, uint8_t *pData,
                                          uint16_t Size, uint32_t Timeout)
{
  HAL_StatusTypeDef status;
  uint16_t index;

  if ((hi2c == NULL) || ((pData == NULL) && (Size != 0U)) || (Size > 255U))
  {
    return HAL_ERROR;
  }

  hi2c->State = HAL_I2C_STATE_BUSY_TX;
  status = I2C_StartTransfer(hi2c, DevAddress, Size, 0U);
  for (index = 0U; (status == HAL_OK) && (index < Size); ++index)
  {
    status = I2C_WaitForFlag(hi2c, I2C_ISR_TXIS, Timeout);
    if (status == HAL_OK)
    {
      hi2c->Instance->TXDR = pData[index];
    }
  }
  if (status == HAL_OK)
  {
    status = I2C_FinishTransfer(hi2c, Timeout);
  }
  hi2c->State = HAL_I2C_STATE_READY;
  return status;
}

HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *hi2c,
                                         uint16_t DevAddress, uint8_t *pData,
                                         uint16_t Size, uint32_t Timeout)
{
  HAL_StatusTypeDef status;
  uint16_t index;

  if ((hi2c == NULL) || ((pData == NULL) && (Size != 0U)) || (Size > 255U))
  {
    return HAL_ERROR;
  }

  hi2c->State = HAL_I2C_STATE_BUSY_RX;
  status = I2C_StartTransfer(hi2c, DevAddress, Size, I2C_CR2_RD_WRN);
  for (index = 0U; (status == HAL_OK) && (index < Size); ++index)
  {
    status = I2C_WaitForFlag(hi2c, I2C_ISR_RXNE, Timeout);
    if (status == HAL_OK)
    {
      pData[index] = (uint8_t)hi2c->Instance->RXDR;
    }
  }
  if (status == HAL_OK)
  {
    status = I2C_FinishTransfer(hi2c, Timeout);
  }
  hi2c->State = HAL_I2C_STATE_READY;
  return status;
}

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c,
                                    uint16_t DevAddress, uint16_t MemAddress,
                                    uint16_t MemAddSize, uint8_t *pData,
                                    uint16_t Size, uint32_t Timeout)
{
  HAL_StatusTypeDef status;
  uint16_t index;
  uint16_t address_bytes = (MemAddSize == I2C_MEMADD_SIZE_16BIT) ? 2U : 1U;

  if ((hi2c == NULL) || ((pData == NULL) && (Size != 0U)) ||
      (MemAddSize != I2C_MEMADD_SIZE_8BIT && MemAddSize != I2C_MEMADD_SIZE_16BIT) ||
      ((uint32_t)Size + address_bytes > 255U))
  {
    return HAL_ERROR;
  }

  hi2c->State = HAL_I2C_STATE_BUSY_TX;
  status = I2C_StartTransfer(hi2c, DevAddress, Size + address_bytes, 0U);
  if ((status == HAL_OK) && (address_bytes == 2U))
  {
    status = I2C_WaitForFlag(hi2c, I2C_ISR_TXIS, Timeout);
    if (status == HAL_OK)
    {
      hi2c->Instance->TXDR = (uint8_t)(MemAddress >> 8);
    }
  }
  if (status == HAL_OK)
  {
    status = I2C_WaitForFlag(hi2c, I2C_ISR_TXIS, Timeout);
    if (status == HAL_OK)
    {
      hi2c->Instance->TXDR = (uint8_t)MemAddress;
    }
  }
  for (index = 0U; (status == HAL_OK) && (index < Size); ++index)
  {
    status = I2C_WaitForFlag(hi2c, I2C_ISR_TXIS, Timeout);
    if (status == HAL_OK)
    {
      hi2c->Instance->TXDR = pData[index];
    }
  }
  if (status == HAL_OK)
  {
    status = I2C_FinishTransfer(hi2c, Timeout);
  }
  hi2c->State = HAL_I2C_STATE_READY;
  return status;
}

static HAL_StatusTypeDef I2C_StartTransfer(I2C_HandleTypeDef *hi2c,
                                            uint16_t address, uint16_t length,
                                            uint32_t direction)
{
  uint32_t flags = hi2c->Instance->ISR;

  if ((flags & I2C_ISR_BUSY) != 0U)
  {
    hi2c->ErrorCode = HAL_I2C_ERROR_BERR;
    return HAL_BUSY;
  }

  hi2c->Instance->ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF |
                        I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF;
  hi2c->Instance->CR2 = ((uint32_t)address & I2C_CR2_SADD) |
                        ((uint32_t)length << I2C_CR2_NBYTES_Pos) |
                        direction | I2C_CR2_AUTOEND | I2C_CR2_START;
  return HAL_OK;
}

static HAL_StatusTypeDef I2C_FinishTransfer(I2C_HandleTypeDef *hi2c,
                                             uint32_t timeout)
{
  HAL_StatusTypeDef status = I2C_WaitForFlag(hi2c, I2C_ISR_STOPF, timeout);

  if (status == HAL_OK)
  {
    hi2c->Instance->ICR = I2C_ICR_STOPCF;
  }
  return status;
}

static HAL_StatusTypeDef I2C_WaitForFlag(I2C_HandleTypeDef *hi2c,
                                         uint32_t flag, uint32_t timeout)
{
  uint32_t start = HAL_GetTick();

  while ((hi2c->Instance->ISR & flag) == 0U)
  {
    uint32_t errors = hi2c->Instance->ISR & I2C_ERROR_FLAGS;
    if (errors != 0U)
    {
      hi2c->Instance->ICR = errors;
      hi2c->ErrorCode = (errors & I2C_ISR_NACKF) ? HAL_I2C_ERROR_AF : HAL_I2C_ERROR_BERR;
      return HAL_ERROR;
    }
    if ((timeout != HAL_MAX_DELAY) && ((HAL_GetTick() - start) > timeout))
    {
      hi2c->ErrorCode = HAL_I2C_ERROR_TIMEOUT;
      return HAL_TIMEOUT;
    }
  }
  return HAL_OK;
}
