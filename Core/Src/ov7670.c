/**
  ******************************************************************************
  * @file    ov7670.c
  * @brief   Minimal OV7670 driver (see ov7670.h).
  ******************************************************************************
  */
#include "ov7670.h"

/* Register, value pairs applied after reset to get QVGA (320x240) RGB565
 * output. This is the commonly-used "safe" subset (format + timing window
 * registers); auto-exposure/gain/white-balance are left enabled at their
 * power-on defaults. If the image is shifted, rolling, or the wrong colors,
 * HSTART/HSTOP/VSTART/VSTOP/VREF (0x17/0x18/0x19/0x1a/0x03) and CLKRC (0x11)
 * are the registers worth hand-tuning on the bench - exact values vary
 * slightly between OV7670 module batches. */
static const uint8_t ov7670_regs[][2] = {
    {0x12, 0x14}, /* COM7:  QVGA output, RGB format                        */
    {0x40, 0xD0}, /* COM15: RGB565, full 0..255 output range               */
    {0x3A, 0x04}, /* TSLB:  correct byte ordering for RGB565               */
    {0x11, 0x01}, /* CLKRC: internal prescaler /2                          */
    {0x0C, 0x00}, /* COM3:  defaults (no DCW scaling - using COM7's QVGA   */
                  /*        hardware window instead)                       */
    {0x3E, 0x00}, /* COM14: no manual scaling / normal PCLK                */
    {0x04, 0x00}, /* COM1:  disable CCIR656                                */
    {0x17, 0x16}, /* HSTART                                                */
    {0x18, 0x04}, /* HSTOP                                                 */
    {0x32, 0x80}, /* HREF                                                  */
    {0x19, 0x02}, /* VSTART                                                */
    {0x1A, 0x7B}, /* VSTOP                                                 */
    {0x03, 0x06}, /* VREF                                                  */
    {0x13, 0xC7}, /* COM8:  fast AEC, AEC step, AGC + AWB + AEC enabled    */
};

HAL_StatusTypeDef OV7670_WriteReg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(hi2c, OV7670_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                              &value, 1, 100);
}

HAL_StatusTypeDef OV7670_ReadReg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t *value)
{
    /* SCCB register reads are two separate transactions (write the
     * register pointer, then a plain read) rather than HAL's combined
     * Mem_Read, which some OV7670 modules don't answer correctly. */
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(hi2c, OV7670_I2C_ADDR, &reg, 1, 100);
    if (status != HAL_OK)
    {
        return status;
    }
    return HAL_I2C_Master_Receive(hi2c, OV7670_I2C_ADDR, value, 1, 100);
}

HAL_StatusTypeDef OV7670_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t pid = 0, ver = 0;

    /* COM7 bit7 = reset all registers to defaults */
    if (OV7670_WriteReg(hi2c, OV7670_REG_COM7, 0x80) != HAL_OK)
    {
        return HAL_ERROR;
    }
    HAL_Delay(30); /* datasheet: allow time after reset before further SCCB access */

    if (OV7670_ReadReg(hi2c, OV7670_REG_PID, &pid) != HAL_OK ||
        OV7670_ReadReg(hi2c, OV7670_REG_VER, &ver) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (pid != 0x76 || ver != 0x73)
    {
        /* Answered on the bus, but isn't an OV7670 (or XCLK/timing is off
         * enough that reads are unreliable) */
        return HAL_ERROR;
    }

    for (unsigned int i = 0; i < sizeof(ov7670_regs) / sizeof(ov7670_regs[0]); i++)
    {
        if (OV7670_WriteReg(hi2c, ov7670_regs[i][0], ov7670_regs[i][1]) != HAL_OK)
        {
            return HAL_ERROR;
        }
        HAL_Delay(1);
    }

    HAL_Delay(30); /* let AGC/AWB settle before the first capture */
    return HAL_OK;
}
