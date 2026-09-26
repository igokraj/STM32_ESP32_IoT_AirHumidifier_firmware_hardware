#include "main.hpp"
#include "stm32f4xx_hal.h"
#include "main.h"
#include "i2c.h"
#include "humiditySensor.hpp"
#include "iwdg.h"
#include "tim.h"
#include "usart.h"
#include "uart.hpp"
#include "jsonParser.hpp"



static uint32_t now = 0;
static uint32_t last_uart_mag = 0;

#define TIM1_ARR 3359

// Desired RPM_lvl;
int lvl = 0;

enum class SystemStatus_t {
    Waiting,
    Running,
    EmptyContainer,
    Error
};

// ***** TESTING *********

SystemStatus_t test_status;
float humidity_test;

uint32_t test_RPM;

bool Empty_test;

// ***********************

// Global, because the HAL callbacks below need to reach it
Uart espUart(&huart1);

extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == espUart.handle()) espUart.onByte();
}

// Overrun or noise stops the reception, so start it again
extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart == espUart.handle()) espUart.Receive();
}


#define TIM3_TICK_HZ        100000u   // 84 MHz / (839 + 1) -> 1 tick = 10 us
#define FAN_PULSES_PER_REV  2u        // check in the fan datasheet, most PC fans give 2
#define FAN_STOP_TIMEOUT_MS 500u      // no pulse for this long -> fan stopped

// Values from the TIM3 interrupt in main.c
extern volatile uint32_t lastPulseTime;
extern volatile uint32_t period;

uint16_t CalculateRPM() {

    // Copy the values, so both come from the same moment
    uint32_t p = period;
    uint32_t last = lastPulseTime;

    if (p == 0 || (HAL_GetTick() - last > FAN_STOP_TIMEOUT_MS)) {
        return 0;
    }

    // Calculate the RPM
    return (60u * TIM3_TICK_HZ) / (p * FAN_PULSES_PER_REV);
}


/*
RPM_lvl  |  PWM (%)   |  RPM
0        |  0         |  0
1        |  20        |  700
2        |  40        |  1400
3        |  60        |  2000
4        |  80        |  2600
5        |  100       |  3000
*/
uint32_t SetPWM(uint16_t RPM_lvl) {

    return (TIM1_ARR + 1) * RPM_lvl / 5;
}



void ApplyOutPuts(SystemStatus_t Status) {

    test_status = Status;

    switch (Status) {
        case SystemStatus_t::Waiting:

        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);

            break;
        case SystemStatus_t::Running:

        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, SetPWM(lvl));
            break;
        case SystemStatus_t::EmptyContainer:

        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
            break;
        case SystemStatus_t::Error:

        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
            break;
    }
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

    // Set these bool's as false when initialized
    bool containerEmpty_ = false;
    bool systemFailed_ = false;
    bool sensorFailed_ = false;

    // Set initialization status for: Waiting
    SystemStatus_t systemStatus_ = SystemStatus_t::Waiting;
};



void app_main() {

    Humidifier AirHumidifier(60.0f); // Set initialization humidity as 50%
    HumiditySensor HTU21D(&hi2c1, 0x40); 
    uint8_t sensorFails = 0;

    espUart.Receive();   // arm the first byte, the interrupt does the rest
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);

    while (1) {

if (espUart.hasLine()) {

    if (auto hum = jsonNumber(espUart.line(), "hum")) {
        if (*hum >= 0.0f && *hum <= 100.0f) {
            AirHumidifier.setDesiredHum(*hum);
        }
    }

    if (auto rpm_lvl = jsonNumber(espUart.line(), "rpm_lvl")) {
        lvl = static_cast<int>(*rpm_lvl);
        if (lvl > 5) {
            lvl = 5;
        }
        else if (lvl < 0) {
            lvl = 0;
        }
    }

    espUart.release();   // Release the buffer to the UART can send another message
}


        float hum = HTU21D.readHumidity();
        if (hum == -1.0f) {
            if (sensorFails < 3) {
            sensorFails += 1;
            if (sensorFails == 3) {
                AirHumidifier.setSensorFailed(true);
            }
        }
        }
        else {
            sensorFails = 0;
            AirHumidifier.setCurrentHum(hum);
            AirHumidifier.setSensorFailed(false);
        }
        // TEST
        humidity_test = hum;

        
        if (HAL_GPIO_ReadPin(Water_level_GPIO_Port, Water_level_Pin) == GPIO_PIN_SET) {
        AirHumidifier.setContainerEmpty(true);
        Empty_test = true; // TEST
        }
        else {
            AirHumidifier.setContainerEmpty(false);

            Empty_test = false; // TEST
        } 
        

        AirHumidifier.updateStatus();

        ApplyOutPuts(AirHumidifier.getSystemStatus());

        // TEST
        test_RPM = CalculateRPM();
        // ******

        now = HAL_GetTick();
        if (now - last_uart_mag > 5000) {
        espUart.Send(hum, static_cast<int>(AirHumidifier.getSystemStatus()), CalculateRPM());
        last_uart_mag = now;
        }

        HAL_IWDG_Refresh(&hiwdg);

    }
}
