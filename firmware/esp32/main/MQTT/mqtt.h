#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

#define MSG_MAX_LEN 20
#define TOPIC_MAX_LEN 32
#define MQTT_QUEUE_LEN 5

typedef struct {
    char data[MSG_MAX_LEN];
    char topic[TOPIC_MAX_LEN];
} mqtt_msg_t;

extern QueueHandle_t mqtt_queue;

esp_err_t mqtt_app_start(void);

esp_err_t esp_publish_hum(int hum, int rpm);
