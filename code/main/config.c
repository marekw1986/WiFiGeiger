#include <stdio.h>
#include <string.h>
#include "lwip/apps/sntp.h"
#include "config.h"
#include "mqtt_log.h"

extern SemaphoreHandle_t configSemaphore;

static config_t config;

void ICACHE_FLASH_ATTR config_load_defaults (void) {
	
	if (xSemaphoreTake(configSemaphore, portMAX_DELAY) == pdTRUE) {
		strcpy(config.devname, "WiFiGeiger");
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
		xSemaphoreGive(configSemaphore);
	}
}

void config_apply_settings (void) {
	mqtt_client_stop();
    tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA);
	if (config.use_dhcp) {
		tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA);
	}
	else {
		tcpip_adapter_ip_info_t ip_info;
        ip_info.ip.addr = config.ip.addr;
        ip_info.gw.addr = config.gw.addr;
        ip_info.netmask.addr = config.netmask.addr;
        tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
	}
	
	sntp_stop();
    sntp_setservername(0, config.ntp1);
    sntp_setservername(1, config.ntp2);
    sntp_setservername(2, config.ntp3);
	//sntp_set_timezone(config.timezone);
	sntp_init();	
}

esp_err_t config_save_settings_to_flash (void) {
	nvs_handle_t my_handle;
    esp_err_t err;
    config_t tmp_config;
    
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;
    
    if (xSemaphoreTake(configSemaphore, portMAX_DELAY) == pdTRUE) {
		tmp_config = config;
		xSemaphoreGive(configSemaphore);
	}
	else {
		return ESP_FAIL;
	}    
    
    err = nvs_set_blob(my_handle, "config", &tmp_config, sizeof(tmp_config));
	if (err != ESP_OK) return err;
	
	err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;
    
    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t config_load_settings_from_flash (void) {
	nvs_handle_t my_handle;
    esp_err_t err;
    config_t tmp_config;
    
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return err;
    
    size_t required_size = sizeof(tmp_config);
    err = nvs_get_blob(my_handle, "config", &tmp_config, &required_size);
    if (err != ESP_OK) return err;
    
    if (xSemaphoreTake(configSemaphore, portMAX_DELAY) == pdTRUE) {
		config = tmp_config;
		xSemaphoreGive(configSemaphore);
	}
	else {
		return ESP_FAIL;
	}
	
	return ESP_OK;
}

esp_err_t config_get_current(config_t* out) {
	if (out == NULL) return ESP_FAIL;
	if (xSemaphoreTake(configSemaphore, portMAX_DELAY) == pdTRUE) {
		*out = config;
		xSemaphoreGive(configSemaphore);
		return ESP_OK;
	}
	else {
		return ESP_FAIL;
	}
}


esp_err_t config_set_new(const config_t in) {
	if (xSemaphoreTake(configSemaphore, portMAX_DELAY) == pdTRUE) {
		config = in;
		xSemaphoreGive(configSemaphore);
		return ESP_OK;
	}
	else {
		return ESP_FAIL;
	}
}
