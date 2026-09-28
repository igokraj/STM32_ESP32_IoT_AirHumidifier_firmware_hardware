#include "main.hpp"
#include "stm32f4xx_hal.h"
#include "main.h"
#include "i2c.h"
#include "iwdg.h"
#include "tim.h"
#include "usart.h"
#include "uart.hpp"

// User files
#include "jsonParser.hpp"
#include "humiditySensor.hpp"
#include "digital_output.hpp"
#include "I2C_recovery.hpp"
#include "stackWatermark.h"


#define TIM1_ARR 3359

// Desired RPM_lvl;
int lvl = 0;

// Updated every loop iteration, watch this in the debugger instead of calling
// StackWatermark_GetMaxUsedBytes() from the Watch window (inferior function
// calls aren't reliably supported over SWD/OpenOCD)
volatile uint32_t stackUsedBytes = 0;

enum class SystemStatus_t {
    Waiting,
    Running,
    EmptyContainer,
    Error
};

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

// Values from the TIM3 interrupt in main.c USER BEGIN 4
extern volatile uint32_t lastPulseTime;
extern volatile uint32_t period;

uint16_t CalculateRPM() {

    // Copy the values, so both come from the same moment
    uint32_t p = period;
    uint32_t last = lastPulseTime;

    // Return 0 if there are no pulses, or the microcontroller hasn't seen one for FAN_STOP_TIMEOUT_MS
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

DigitalOutput RedLed(Red_LED_GPIO_Port, Red_LED_Pin);
DigitalOutput GreenLed(Green_LED_GPIO_Port, Green_LED_Pin);
DigitalOutput BlueLed(Blue_LED_GPIO_Port, Blue_LED_Pin);

void ApplyOutPuts(SystemStatus_t Status) {
    switch (Status) {
        case SystemStatus_t::Waiting:
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
            RedLed.off();
            BlueLed.off();
            GreenLed.off();
            break;
        case SystemStatus_t::Running:
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, SetPWM(lvl));
            RedLed.off();
            BlueLed.off();
            GreenLed.on();
            break;
        case SystemStatus_t::EmptyContainer:
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
            RedLed.off();
            BlueLed.on();
            GreenLed.off();
            break;
        case SystemStatus_t::Error:
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
            RedLed.on();
            BlueLed.off();
            GreenLed.off();
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

    float getDesiredHum() const {
        return desiredHum_;
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

    // Init these as false
    bool containerEmpty_ = false;
    bool systemFailed_ = false;
    bool sensorFailed_ = false;

    // Initial status: Waiting
    SystemStatus_t systemStatus_ = SystemStatus_t::Waiting;
};



void app_main() {

    static uint32_t now = 0;
    static uint32_t last_uart_mag = 0;

    Humidifier AirHumidifier(70.0f); // Set initial desired humidity to 70%
    HumiditySensor HTU21D(&hi2c1, 0x40); 
    uint8_t sensorFails = 0; // counter of the incorrect htu21d hum values

    espUart.Receive();   // arm the first byte, the interrupt does the rest

    while (1) {
    

// Wait for a JSON message and, once it's ready, parse it into variables (hum and rpm_lvl)

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

    espUart.release();   // Release the buffer so the UART can receive another message
}

        // Read the current humidity (set sensorFailed if the reading is invalid, clear it otherwise)
        float hum = HTU21D.readHumidity();
        if (hum == -1.0f) {
            if (sensorFails < 3) {
            sensorFails += 1;
            if (sensorFails == 3) {
                // This function is used to recover a jammed I2C bus during normal program operation (not after a restart).
                I2C1_Reinit();         
                sensorFails = 0;
                AirHumidifier.setSensorFailed(true);
            }
        }
        }
        else {
            sensorFails = 0;
            AirHumidifier.setCurrentHum(hum);
            AirHumidifier.setSensorFailed(false);
        }
     
        // Read the water level

        if (HAL_GPIO_ReadPin(Water_level_GPIO_Port, Water_level_Pin) == GPIO_PIN_SET) {
        AirHumidifier.setContainerEmpty(true);
        }
        else {
            AirHumidifier.setContainerEmpty(false);
        } 
        
        // Update system status according to the readings above
        AirHumidifier.updateStatus();

        // Apply outputs according to the current system status
        ApplyOutPuts(AirHumidifier.getSystemStatus());

        // Send a message to the ESP32 with the system's current characteristics
        now = HAL_GetTick();
        if (now - last_uart_mag > 5000) {
        espUart.Send(hum, AirHumidifier.getDesiredHum(), static_cast<int>(AirHumidifier.getSystemStatus()), CalculateRPM());
        last_uart_mag = now;
        }

        stackUsedBytes = StackWatermark_GetMaxUsedBytes();

        HAL_IWDG_Refresh(&hiwdg);

    }
}
