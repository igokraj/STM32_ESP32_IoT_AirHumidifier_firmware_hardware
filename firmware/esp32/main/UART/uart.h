#pragma once

#include <stddef.h>
#include "esp_err.h"

void uart_init(void);

esp_err_t uart_send_message(const char *data, size_t len);
