#include "main.hpp"
#include "stm32f4xx_hal.h"
#include "main.h"
#include "i2c.h"
#include "humiditySensor.hpp"
#include "iwdg.h"
#include "tim.h"
#include "usart.h"
#include "uartReceiver.hpp"
#include "jsonParser.hpp"

enum class SystemStatus_t {
    Waiting,
    Running,
    EmptyContainer,
    Error
};

uint16_t targetRPM = 0;

// Global, because the HAL callbacks below need to reach it
UartReceiver espUartReceiver(&huart1);

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == espUartReceiver.handle()) espUartReceiver.onByte();
}

// Overrun or noise stops the reception, so start it again
extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart == espUartReceiver.handle()) espUartReceiver.Receive();
}

class Humidifier {

public:
    Humidifier(float desiredHum) : desiredHum_(desiredHum) {}

    // Getters
    SystemStatus_t getSystemStatus() const {
        return systemStatus_;
    }

    // Setters
    void setCurrentHum(float currentHum) {
        currentHum_ = currentHum;
    }

    void setDesiredHum(float desiredHum) {
        desiredHum_ = desiredHum;
    }

    void setContainerEmpty(bool containerEmpty) {
        containerEmpty_ = containerEmpty;
    }

    void setSensorFailed(bool sensorFailed) {
        sensorFailed_ = sensorFailed;
    }

    
    void updateStatus() {

        if (sensorFailed_) {
            systemStatus_ = SystemStatus_t::Error;
            return;
        }

        if (containerEmpty_) {
            systemStatus_ = SystemStatus_t::EmptyContainer;
            return;
        }

        if (currentHum_ < desiredHum_) {
            systemStatus_ = SystemStatus_t::Running;
        }
        else if (currentHum_ >= desiredHum_) {
            systemStatus_ = SystemStatus_t::Waiting;
        }
    }

private:
    float currentHum_ = 0.0f;   // measured humidity [%]
    float desiredHum_;          // setpoint humidity [%]

    bool containerEmpty_ = false;
    bool systemFailed_ = false;
    bool sensorFailed_ = false;

    // Set inicialization status for: Waiting
    SystemStatus_t systemStatus_ = SystemStatus_t::Waiting;
};


void app_main() {

    Humidifier AirHumidifier(50.0f); // Set inicialization humidity as 50%
    HumiditySensor HTU21D(&hi2c1, 0x40); 
    uint8_t sensorFails = 0;

    espUartReceiver.Receive();   // arm the first byte, the interrupt does the rest

    while (1) {

if (espUartReceiver.hasLine()) {

    if (auto hum = jsonNumber(espUartReceiver.line(), "hum")) {
        if (*hum >= 0.0f && *hum <= 100.0f) {
            AirHumidifier.setDesiredHum(*hum);
        }
    }

    if (auto rpm = jsonNumber(espUartReceiver.line(), "rpm")) {
        if (*rpm >= 3000.0f) {
            targetRPM = 3000;
        }
        else if (*rpm > 0.0f) {
            targetRPM = static_cast<uint16_t>(*rpm);
        }
        else {
            targetRPM = 0;
        }
    }

    espUartReceiver.release();   // zwolnij bufor, żeby mogła przyjść następna wiadomość
}


        float hum = HTU21D.readHumidity();
        if (hum == -1.0f) {
            if (sensorFails < 3)
            sensorFails += 1;
        }
        else {
            sensorFails = 0;
            AirHumidifier.setCurrentHum(hum);
        }
        
        
        AirHumidifier.setSensorFailed(sensorFails >= 3);

        AirHumidifier.setContainerEmpty(HAL_GPIO_ReadPin(Water_level_GPIO_Port, Water_level_Pin) == GPIO_PIN_RESET);

        AirHumidifier.updateStatus();

        HAL_IWDG_Refresh(&hiwdg);

    }
}
