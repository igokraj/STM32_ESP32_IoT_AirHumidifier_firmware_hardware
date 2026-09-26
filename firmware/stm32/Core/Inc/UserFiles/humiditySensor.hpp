#pragma once

#include "stm32f4xx_hal.h"

class HumiditySensor {
public:

    HumiditySensor(I2C_HandleTypeDef *hi2c, uint8_t address)
        : hi2c_(hi2c), address_(address << 1) {}

    float readHumidity() {
        uint8_t data[3];

        if (HAL_I2C_Master_Transmit(hi2c_, address_, &humCmd_, 1, TIMEOUT_MS) != HAL_OK) {
            return -1.0f;
        }
        if (HAL_I2C_Master_Receive(hi2c_, address_, data, 3, TIMEOUT_MS) != HAL_OK) {
            return -1.0f;
        }

        uint16_t raw = ((data[0] << 8) | data[1]) & 0xFFFC;
        return -6.0f + (125.0f * raw / 65536.0f);
    }

private:
    static constexpr uint32_t TIMEOUT_MS = 100;
    uint8_t humCmd_ = 0xE5;  

    I2C_HandleTypeDef *hi2c_;
    uint8_t address_; // I2C address of the HTU21D sensor
};