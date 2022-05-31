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

extern const char *TAG;

uint32_t uptime = 0;

uint32_t get_uptime(void) {
    return uptime;
}

inline void increment_uptime(void) {
    uptime++;
}

char* constructJSON(void) {
	cJSON *root;
	cJSON *geiger;
	char *out;
	
	root = cJSON_CreateObject();
	cJSON_AddStringToObject(root, "id", "ethergeiger1");
	cJSON_AddItemToObject(root, "geiger", geiger = cJSON_CreateObject());
	cJSON_AddNumberToObject(geiger, "timestamp", time(NULL));
	cJSON_AddNumberToObject(geiger, "radiation", cpm2sievert(geiger_get_cpm()));
	out = cJSON_Print(root);
	cJSON_Delete(root);
	
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
