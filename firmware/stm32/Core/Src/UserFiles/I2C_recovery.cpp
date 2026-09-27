#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_gpio.h"
#include "I2C_recovery.hpp"
#include "i2c.h"

/* Recovers the I2C bus at startup, in case the transmission was cut off
mid-transfer by a microcontroller reset and SDA got stuck low. */

void I2C1_BusRecovery(void) {

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

#define I2C_SCL_HIGH() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET)
#define I2C_SCL_LOW() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET)
#define I2C_SDA_HIGH() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET)
#define I2C_SDA_LOW() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET)
#define I2C_SDA_READ() HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7)

  I2C_SCL_HIGH();
  I2C_SDA_HIGH();
  HAL_Delay(1);

  // If SDA is stuck low, the bus stuck and needs a recovery
  if (I2C_SDA_READ() == GPIO_PIN_RESET) {

    /* Clock SCL up to 9 times so the slave can finish whatever byte it was sending and release the line */
    for (uint8_t i = 0; i < 9; i++) {
      I2C_SCL_LOW();
      HAL_Delay(1);
      I2C_SCL_HIGH();
      HAL_Delay(1);

      if (I2C_SDA_READ() == GPIO_PIN_SET) {
        break;
      }
    }

    // Manually generate a STOP condition
    I2C_SDA_LOW();
    HAL_Delay(1);
    I2C_SCL_HIGH();
    HAL_Delay(1);
    I2C_SDA_HIGH();
    HAL_Delay(1);
  }


  
  // Leave the pins back in their reset state - MX_I2C1_Init() will switch them to SDA and SLC.
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

// Full recovery for a bus that jammed during normal operation
void I2C1_Reinit(void) {
  HAL_I2C_DeInit(&hi2c1);
  I2C1_BusRecovery();
  MX_I2C1_Init();
}
