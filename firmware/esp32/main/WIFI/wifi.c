#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_timer.h"
#include "driver/gptimer.h"

// User .h files
#include "secrets.h"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* Timer which is used to keep trying to connect to WiFi till success */
static gptimer_handle_t reconnect_timer = NULL;

/* Callback function which is alerted by the timer */
static bool reconnect_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
    esp_wifi_connect();
    return false;
}

// Delay between each connection tries
#define WIFI_RECONNECT_DELAY (30 * 1000 * 1000) 

#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "wifi";

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) 
    {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) 
    {   
        ESP_LOGI(TAG, "WiFi connection failed. Next connection attempt in %d seconds...", WIFI_RECONNECT_DELAY / (1000 * 1000));
        ESP_ERROR_CHECK(gptimer_start(reconnect_timer));

    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {   
        gptimer_stop(reconnect_timer);
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

// ------------------- CALLBACK TIMER --------------------

// The gptimer_set_alarm_action() function is used to configure the timer's alarm action. When the timer count value reaches the specified alarm value, an alarm event (reconnect_cb callback function -> esp_wifi_connect();) will be triggered. It is so the timer does not try to connect continuously. Instead it tries to connect in specified intervals to save energy.

    gptimer_config_t timer_config = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT, // Select the default clock source
    .direction = GPTIMER_COUNT_UP,      // Counting direction is up
    .resolution_hz = 1 * 1000 * 1000,   // Resolution is 1 MHz, i.e., 1 tick equals 1 microsecond
};
    // Create a timer instance
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &reconnect_timer));

    gptimer_alarm_config_t alarm_config = {
    .reload_count = 0,      // When the alarm event occurs, the timer will automatically reload to 0
    .alarm_count = WIFI_RECONNECT_DELAY, // Set the actual alarm period 
    .flags.auto_reload_on_alarm = true, // Enable auto-reload function
};

// Set the timer's alarm action
ESP_ERROR_CHECK(gptimer_set_alarm_action(reconnect_timer, &alarm_config));

gptimer_event_callbacks_t cbs = {
    .on_alarm = reconnect_cb, // Call the user callback function when the alarm event occurs
};

// Register timer event callback functions, allowing user context to be carried
ESP_ERROR_CHECK(gptimer_register_event_callbacks(reconnect_timer, &cbs, NULL));
// Enable the timer
ESP_ERROR_CHECK(gptimer_enable(reconnect_timer));

// ------------------------------------------

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    /* Waiting until the connection is established (WIFI_CONNECTED_BIT).
     * The bit is set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s",
                 WIFI_SSID);
    }
}
