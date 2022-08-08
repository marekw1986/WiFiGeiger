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
#include "config.h"
#include "mqtt_client.h"
#include "mqtt_log.h"
#include "geiger.h"
#include "ds3231.h"


static const char *TAG = "MQTT log";
static char mqtt_uri[512];
static char mqtt_topic[64];

esp_mqtt_client_config_t mqtt_cfg = {
	//.uri = "mqtt://192.168.1.105:1883",
};

esp_mqtt_client_handle_t client;
os_timer_t mqtt_timer;
static uint8_t mqtt_connected = 0;
static time_t mqtt_last_log_time = 0;

void mqtt_timer_func (void* arg);
static void connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);


static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    char *data;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            mqtt_connected = 1;
            if (get_uptime() > 59) {
				data = constructDataJSON();
				if (data) {
					msg_id = esp_mqtt_client_publish(client, mqtt_topic, data, 0, 1, 0);
					free(data);
					ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
				}
			}
			else {
				os_timer_disarm(&mqtt_timer);
				os_timer_setfn(&mqtt_timer, mqtt_timer_func, NULL);
				os_timer_arm(&mqtt_timer, 60000, 0);
			}
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            os_timer_disarm(&mqtt_timer);
            mqtt_connected = 0;
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
            mqtt_last_log_time = get_uptime();
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


void mqtt_client_init(void) {
	ESP_LOGI(TAG, "Itializing MQTT client");
	config_t config;
	if (config_get_current(&config) == ESP_FAIL) {
		ESP_LOGE(TAG, "Cant read configuration data");
		return;
	}
	if (strlen(config.mqtt_server) == 0) {
		ESP_LOGI(TAG, "MQTT server address empty - disabled");
		return;
	}
    //.uri = "mqtt://192.168.1.105:1883"
    if (strlen(config.mqtt_username) == 0) {
		snprintf(mqtt_uri, sizeof(mqtt_uri)-1, "mqtt://%s:%d", config.mqtt_server, config.mqtt_port);
	}
	else {
		snprintf(mqtt_uri, sizeof(mqtt_uri)-1, "mqtt://%s:%s@%s:%d", config.mqtt_username, config.mqtt_password, config.mqtt_server, config.mqtt_port);
	}
    mqtt_cfg.uri = mqtt_uri;
	//strncpy(mqtt_server, config.mqtt_server, sizeof(config.mqtt_server)-1);
	strncpy(mqtt_topic, config.mqtt_topic, sizeof(config.mqtt_topic)-1);
	client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &client));
}

void mqtt_client_deinit(void) {
	ESP_LOGI(TAG, "Deinitializing MQTT client");
	mqtt_client_stop();
	esp_mqtt_client_destroy(client);
	mqtt_connected = 0;
}

void mqtt_client_stop(void) {
	ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler));
	os_timer_disarm(&mqtt_timer);
	esp_mqtt_client_stop(client);
}

time_t mqtt_get_last_log_time(void) {
    return mqtt_last_log_time;
}

uint8_t mqtt_get_connection_status(void) {
    return mqtt_connected;
}

void mqtt_timer_func (void* arg) {
	char *data;
	
	data = constructDataJSON();
    if (data) {
        esp_mqtt_client_publish(client, mqtt_topic, data, 0, 1, 0);
        free(data);
    }
}

static void disconnect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	ESP_LOGI(TAG, "MQTT disconnect handler (STA lost connection with AP)");
	ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler));
	os_timer_disarm(&mqtt_timer);
	esp_mqtt_client_stop(client);
}

static void connect_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	ESP_LOGI(TAG, "MQTT connect handler (GOT IP)");
	config_t config;
	if (config_get_current(&config) == ESP_FAIL) return;
	if (strlen(config.mqtt_server) == 0) return;	//Ignore MQTT if server address was left empty
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &client));
    if (strlen(config.mqtt_username) == 0) {
		snprintf(mqtt_uri, sizeof(mqtt_uri)-1, "mqtt://%s:%d", config.mqtt_server, config.mqtt_port);
	}
	else {
		snprintf(mqtt_uri, sizeof(mqtt_uri)-1, "mqtt://%s:%s@%s:%d", config.mqtt_username, config.mqtt_password, config.mqtt_server, config.mqtt_port);
	}
    esp_mqtt_client_set_uri(client, mqtt_uri);
	esp_mqtt_client_start(client);
}
