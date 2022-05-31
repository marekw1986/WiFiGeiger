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

#include "cJSON.h"

#include "common.h"
#include "mqtt_client.h"
#include "mqtt_log.h"
#include "geiger.h"
#include "ds3231.h"


extern const char *TAG;

esp_mqtt_client_config_t mqtt_cfg = {
	.uri = "mqtt://192.168.1.105:1883",
};

esp_mqtt_client_handle_t client;

os_timer_t mqtt_timer;

void mqtt_timer_func (void* arg);

/*
char* constructJSON(char* buf, uint16_t len) {
	struct tm time;
    long unsigned int timestamp = 0;
    
    if (ds3231_getTime(&time)) {
		timestamp = mktime(&time);
	}
    snprintf(buf, len, "{\"id\":\"wifigeiger1\",\"geiger\":{\"timestamp\":%lu,\"radiation\":%.4f}}", timestamp, cpm2sievert(geiger_get_cpm()));
    return buf;
}
*/


static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    char *data;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            data = constructJSON();
            msg_id = esp_mqtt_client_publish(client, "testTopic", data, 0, 1, 0);
            free(data);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            //msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
           // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            os_timer_disarm(&mqtt_timer);
            os_timer_setfn(&mqtt_timer, mqtt_timer_func, NULL);
            os_timer_arm(&mqtt_timer, 30000, 0);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			os_timer_disarm(&mqtt_timer);
            os_timer_setfn(&mqtt_timer, mqtt_timer_func, NULL);
            os_timer_arm(&mqtt_timer, 30000, 0);
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}


void mqtt_client_start(void) {
	client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);
}

void mqtt_timer_func (void* arg) {
	char *data;
	
	data = constructJSON();
	esp_mqtt_client_publish(client, "testTopic", data, 0, 1, 0);
	free(data);
}

