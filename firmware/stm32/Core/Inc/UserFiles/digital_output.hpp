#pragma once
#include "main.h"

// This is the class for handling output pins (e.g. 3x RGB LEDs, MOSFET gate, Buzzer)
class DigitalOutput {
    private:
    GPIO_TypeDef *port_;
    uint16_t pin_;

    public:
    DigitalOutput(GPIO_TypeDef *port, uint16_t pin) : port_(port), pin_(pin)
    {
    }

    void on() {
        HAL_GPIO_WritePin(port_, pin_, GPIO_PIN_SET);
    }
    void off() {
        HAL_GPIO_WritePin(port_, pin_, GPIO_PIN_RESET);
    }
};
