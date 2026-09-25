#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_err.h"
#include "driver/gpio.h"

// User .h files
#include "secrets.h"
#include "MQTT/mqtt.h"

#define MQTT_TOPIC_RECEIVE  "humidifier/" AWS_IOT_CLIENT_ID "/cmd"
#define MQTT_TOPIC_PUBLISH  "humidifier/" AWS_IOT_CLIENT_ID "/data"

QueueHandle_t mqtt_queue;

esp_mqtt_client_handle_t client = NULL;

static const char *TAG = "mqtt";

extern const char amazon_root_pem[] asm("_binary_AmazonRootCA1_pem_start");
extern const char device_pem_key[] asm("_binary_device_pem_key_crt_crt_start");
extern const char private_pem_key[] asm("_binary_private_pem_key_start");

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        esp_mqtt_client_subscribe(client, MQTT_TOPIC_RECEIVE, 1);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED");
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED");
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED");
        break;
    case MQTT_EVENT_DATA: {





        
        break;
    }
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

esp_err_t esp_publish_hum(int hum, int rpm) {

    char json_buffer[64];
    int len = snprintf(json_buffer, sizeof(json_buffer), "{\"Humidity\": \"%d\", \"RPM\": \"%d\"}", hum, rpm);

    if (len < 0 || len >= sizeof(json_buffer)) {
        return ESP_FAIL;
    }
    int msg = esp_mqtt_client_publish(client, MQTT_TOPIC_PUBLISH, json_buffer, len, 0, 0);

    if (msg < 0) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtts://" AWS_IOT_ENDPOINT ":8883",
        .broker.verification.certificate = amazon_root_pem,
        .credentials.client_id = AWS_IOT_CLIENT_ID,
        .credentials.authentication.certificate = device_pem_key,
        .credentials.authentication.key = private_pem_key,
    };

    mqtt_queue = xQueueCreate(MQTT_QUEUE_LEN, sizeof(mqtt_msg_t));
    if (mqtt_queue == NULL) {
        return ESP_FAIL;
    }

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        return ESP_FAIL;
    }
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(client));

    return ESP_OK;
}
