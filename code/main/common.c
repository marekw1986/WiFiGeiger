#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
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

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "cJSON.h"
#include "common.h"
#include "geiger.h"
#include "config.h"
#include "ds3231.h"
#include "bme280.h"

extern const char *TAG;
extern SemaphoreHandle_t i2cSemaphore;

os_timer_t reset_timer;
uint32_t uptime = 0;

static void restart_timer_func(void* param);
static uint8_t validate_number(char *str);

uint32_t get_uptime(void) {
    return uptime;
}

inline void increment_uptime(void) {
    uptime++;
}

char* constructDataJSON(void) {
	cJSON *root;
	cJSON *geiger;
	cJSON *bme280;
	char *out;
	config_t config;
	
	if (config_get_current(&config) == ESP_FAIL) return NULL;
	
	root = cJSON_CreateObject();
    if (root == NULL) return NULL;
	cJSON_AddStringToObject(root, "id", config.devname);
	cJSON_AddStringToObject(root, "class", "EtherGeiger");
	if (uptime > 59) {
		cJSON_AddItemToObject(root, "geiger", geiger = cJSON_CreateObject());
		cJSON_AddNumberToObject(geiger, "timestamp", time(NULL));
		cJSON_AddNumberToObject(geiger, "radiation", cpm2sievert(geiger_get_cpm()));
	}
	
	bme_data_t bme_data;
	if (bme_get_data(&bme_data)) {
		cJSON_AddItemToObject(root, "bme280", bme280 = cJSON_CreateObject());
		cJSON_AddNumberToObject(bme280, "timestamp", bme_data.timestamp);
		cJSON_AddNumberToObject(bme280, "temperature", bme_data.temperature);
		cJSON_AddNumberToObject(bme280, "humidity", bme_data.humidity);
		cJSON_AddNumberToObject(bme280, "pressure", bme_data.pressure);
	}
	
	out = cJSON_Print(root);
	cJSON_Delete(root);
    if (out == NULL) return NULL;
	
	return out;
}

char* constructSettingsJSON(void) {
	cJSON *root;
	char *out;
	char buff[32];
	
	config_t config;
	
	if (config_get_current(&config) == ESP_FAIL) return NULL;
	
	root = cJSON_CreateObject();
    if (root == NULL) return NULL;
	cJSON_AddBoolToObject(root, "dhcp", config.use_dhcp);
	snprintf(buff, sizeof(buff)-1, IPSTR, IP2STR(&config.ip));
	cJSON_AddStringToObject(root, "devname", config.devname);
	cJSON_AddStringToObject(root, "ip", buff);
	snprintf(buff, sizeof(buff)-1, IPSTR, IP2STR(&config.netmask));
	cJSON_AddStringToObject(root, "netmask", buff);
	snprintf(buff, sizeof(buff)-1, IPSTR, IP2STR(&config.gw));
	cJSON_AddStringToObject(root, "gw", buff);
	snprintf(buff, sizeof(buff)-1, IPSTR, IP2STR(&config.dns1));
	cJSON_AddStringToObject(root, "dns1", buff);
	snprintf(buff, sizeof(buff)-1, IPSTR, IP2STR(&config.dns2));
	cJSON_AddStringToObject(root, "dns2", buff);
	cJSON_AddStringToObject(root, "ntp1", config.ntp1);
	cJSON_AddStringToObject(root, "ntp2", config.ntp2);
	cJSON_AddStringToObject(root, "ntp3", config.ntp3);
	cJSON_AddStringToObject(root, "mqtt_server", config.mqtt_server);
	cJSON_AddNumberToObject(root, "mqtt_port", config.mqtt_port);
	cJSON_AddStringToObject(root, "mqtt_topic", config.mqtt_topic);
	cJSON_AddNumberToObject(root, "timezone", config.timezone);
	cJSON_AddBoolToObject(root, "daylight", config.daylight);
	out = cJSON_Print(root);
	cJSON_Delete(root);
    if (out == NULL) return NULL;
	
	return out;
}

void spi_filesystem_init(void) {
    ESP_LOGI(TAG, "Initializing SPIFFS");
    
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/spiffs",
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    
    // Use settings defined above to initialize and mount SPIFFS filesystem.
    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
    
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
      
}


static void restart_timer_func(void* param) {
	esp_restart();
}


void set_reset_timer (void) {
	os_timer_disarm(&reset_timer);
	os_timer_setfn(&reset_timer, restart_timer_func, NULL);
	os_timer_arm(&reset_timer, 1000, 0);	
}

uint8_t is_password_valid(const char* pass) {
	config_t config;
	
	if (config_get_current(&config) == ESP_FAIL) return 0;	
	
    if (pass[0] == '\0') return 0;
    if ( strlen(pass) > (sizeof(config.password)-1) ) return 0;
    return 1;
}

static uint8_t validate_number(char *str) {
    while (*str) {
        if(!isdigit(*str)) { return 0; }
        str++;
    }
    return 1;
}


uint8_t is_valid_ip_address(const char *ip) {
    int num, dots = 0;
    char tmpstr[16];
    char *rest = NULL;
    char *ptr;
    
    if (ip == NULL) { return 0; }
    if ( strlen(ip) > (sizeof(tmpstr)-1) ) {return 0;}
    strncpy(tmpstr, ip, sizeof(tmpstr)-1);
	ptr = strtok_r(tmpstr, ".", &rest);
	if (ptr == NULL) { return 0; }
    while (ptr) {
        if (!validate_number(ptr)) { return 0; }
        num = atoi(ptr);
        if (num >= 0 && num <= 255) {
            ptr = strtok_r(NULL, ".", &rest);
            if (ptr != NULL) { dots++; }
        } else { return 0; }
    }
    if (dots != 3) { return 0; }
    return 1;
}

void sntp_sync_time_func(struct timeval *tv) {
	ESP_LOGI(TAG, "SNTP synchronized. Seconds: %lu", tv->tv_sec);
	time_t rawtime = tv->tv_sec;
	struct tm time;
	gmtime_r(&rawtime, &time);
	if (xSemaphoreTake(i2cSemaphore, portMAX_DELAY) == pdTRUE) {
		ds3231_setTime(&time);
		xSemaphoreGive(i2cSemaphore);
	}
	else {
		ESP_LOGI(TAG, "I2C semaphore taken");
	}
}
