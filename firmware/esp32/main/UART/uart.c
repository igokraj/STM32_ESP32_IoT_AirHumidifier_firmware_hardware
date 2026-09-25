#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "uart.h"

void uart_init(void)
{
const uart_port_t uart_num = UART_NUM_1;

const int uart_buffer_size = (1024 * 2);
uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 0,
};
// Configure UART parameters
ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
// Set UART pins
ESP_ERROR_CHECK(uart_set_pin(uart_num, 17, 18, UART_PIN_NO_CHANGE,
                             UART_PIN_NO_CHANGE));

// Install UART driver 
ESP_ERROR_CHECK(uart_driver_install(uart_num, uart_buffer_size, uart_buffer_size, 0, NULL, 0));

}

// Send JSON to STM32 followed by a "\n" character, so the STM knows where the message ends
esp_err_t uart_send_message(const char *data, size_t len)
{
    if (uart_write_bytes(UART_NUM_1, data, len) < 0 ||
        uart_write_bytes(UART_NUM_1, "\n", 1) < 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
