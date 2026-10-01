#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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

/* Reads one line (up to '\n') sent by STM32 into buf. Blocks until a whole line arrives.
 * Returns line length (without '\n'), or -1 if the line did not fit in buf. */
int uart_receive_message(char *buf, size_t size)
{
    size_t idx = 0;
    bool overflow = false;
    uint8_t ch;

    while (1) {
        // Wait for one byte (blocks the task, does not waste CPU)
        if (uart_read_bytes(UART_NUM_1, &ch, 1, portMAX_DELAY) != 1) {
            continue;
        }

        if (ch == '\n') {               // end of message
            if (overflow) {
                return -1;              // message was too long, drop it
            }
            if (idx == 0) {
                continue;               // empty line, ignore
            }
            buf[idx] = '\0';            // make it a C string
            return (int)idx;
        }

        if (ch == '\r') {               // ignore carriage return, if STM32 sends "\r\n"
            continue;
        }

        if (idx < size - 1) {
            buf[idx++] = (char)ch;      // store the byte
        } else {
            overflow = true;            // buffer full, skip the rest until '\n'
        }
    }
}