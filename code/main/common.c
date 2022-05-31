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
#include <time.h>

#include "cJSON.h"
#include "common.h"
#include "geiger.h"

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
