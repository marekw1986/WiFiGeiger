#include <stdio.h>
#include <string.h>
#include "config.h"

config_t config;

void ICACHE_FLASH_ATTR config_load_defaults (void) {
	IP4_ADDR(&(config.ip), 192, 168, 1, 42);
	IP4_ADDR(&(config.netmask), 255, 255, 255, 0);
	IP4_ADDR(&(config.gw), 192, 168, 1, 1);
	IP4_ADDR(&(config.dns1), 8, 8, 8, 8);
	IP4_ADDR(&(config.dns2), 8, 8, 4, 4);
	config.use_dhcp = 1;
	strcpy(config.ntp1, "ntp1.tp.pl");
	strcpy(config.ntp2, "ntp2.tp.pl");
	strcpy(config.ntp3, "ntp.nask.pl");
	strcpy(config.mqtt_server, "192.168.1.95");
	strcpy(config.mqtt_topic, "testTopic");
	strcpy(config.password, "s3cr3t");
	config.timezone = 2;
	config.daylight = 0;
}

void config_apply_settings (void) {
	if (config.use_dhcp) {
		//wifi_station_dhcpc_start();
	}
	else {
		//wifi_station_dhcpc_stop();
		//wifi_set_ip_info(STATION_IF, &(config.station_ip));
		//espconn_dns_setserver(0, &(config.dns1));
		//espconn_dns_setserver(1, &(config.dns2));
	}
	
	//sntp_stop();
	//sntp_setservername(0, config.ntp1);
	//sntp_setservername(1, config.ntp2);
	//sntp_setservername(2, config.ntp3);
	//sntp_set_timezone(config.timezone);
	//sntp_init();	
}

esp_err_t config_save_settings_to_flash (void) {
	nvs_handle_t my_handle;
    esp_err_t err;
    
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;
    
    err = nvs_set_blob(my_handle, "config", &config, sizeof(config));
	if (err != ESP_OK) return err;
	
	err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;
    
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t config_load_settings_from_flash (void) {
	nvs_handle_t my_handle;
    esp_err_t err;
    
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;
    
    size_t required_size = sizeof(config);
    nvs_get_blob(my_handle, "config", &config, &required_size);
    if (err != ESP_OK) return err;
	
	return ESP_OK;
}
