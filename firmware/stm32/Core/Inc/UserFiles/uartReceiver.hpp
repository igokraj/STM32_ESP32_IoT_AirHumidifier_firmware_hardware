#pragma once
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_uart.h"

class UartReceiver {

    public:

    explicit UartReceiver(UART_HandleTypeDef *huart) : huart_(huart) {}

    void Receive() {
        HAL_UART_Receive_IT(huart_, &rxByte_, 1);
    }

    // Called from the HAL callback, after every received byte
    void onByte() {
        if (!lineReady_) {                       // while a line waits, new bytes are dropped
            char c = rxByte_;
            if (c == '\n') {
                if (!overflow_ && idx_ > 0) {
                    buf_[idx_] = '\0';
                    lineReady_ = true;
                }
                idx_ = 0;
                overflow_ = false;
            }
            else if (c != '\r') {
                if (idx_ < sizeof(buf_) - 1) buf_[idx_++] = c;
                else overflow_ = true;           // line too long, drop it
            }
        }
        Receive();                                 // arm the next byte
    }


    // Getters
    UART_HandleTypeDef *handle() const { return huart_; }
    bool hasLine() const { return lineReady_; }
    const char *line() const { return buf_; }
    void release() { lineReady_ = false; }      


 private:
    UART_HandleTypeDef *huart_;
    uint8_t rxByte_ = 0;
    char buf_[128];
    size_t idx_ = 0;
    bool overflow_ = false;
    volatile bool lineReady_ = false;            // shared with the interrupt
};

