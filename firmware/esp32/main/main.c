#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "nvs_flash.h"

// User .h files
#include "UART/uart.h"
#include "WIFI/wifi.h"

#define ECHO_TASK_STACK_SIZE   4096

static const char *TAG = "AirHumidifier";

void app_main(void)
{

     /* Initialize NVS: required by the WiFi driver (radio calibration data),
       and used by ota.c to store the black-listed firmware version */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    xTaskCreate(echo_task, "uart_echo_task", ECHO_TASK_STACK_SIZE, NULL, 10, NULL);


    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
}