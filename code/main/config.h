#ifndef CONFIG_H
#define CONFIG_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"

typedef struct {
	ip4_addr_t ip;
	ip4_addr_t netmask;
	ip4_addr_t gw;
	ip4_addr_t dns1;
	ip4_addr_t dns2;
	uint8_t use_dhcp;
	char ntp1[64];
	char ntp2[64];
	char ntp3[64];
	char mqtt_server[64];
	char mqtt_topic[64];
	char password[34];
	int8_t timezone;
	uint8_t daylight; 
} config_t;

void config_load_defaults (void);
void config_apply_settings (void);
esp_err_t config_save_settings_to_flash (void);
esp_err_t config_load_settings_from_flash (void);
esp_err_t config_get_current(config_t* out);
esp_err_t config_set_new(const config_t in);



#endif
