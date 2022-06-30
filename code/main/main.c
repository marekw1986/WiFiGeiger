/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "i2c_master.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "common.h"
#include "geiger.h"
#include "ds3231.h"
#include "wifi.h"
#include "http_server.h"
#include "mqtt_log.h"
#include "lwip/apps/sntp.h"
#include "config.h"
#include "wificgi.h"

#define GPIO_INPUT_BUTTON	0
#define GPIO_INPUT_RTC		14
#define GPIO_INPUT_PIN_SEL 	((1ULL<<GPIO_INPUT_RTC) | (1ULL<<GPIO_INPUT_CPM) | (1ULL<<GPIO_INPUT_BUTTON))

const char *TAG = "wiFiGeiger";

extern uint32_t uptime;

static os_timer_t soft_ap_restore_timer;

SemaphoreHandle_t xSemaphore = NULL;
SemaphoreHandle_t i2cSemaphore = NULL;

static void soft_ap_restore_timer_func(void* param) {
	ESP_LOGI(TAG, "Restoring Soft AP as operation mode");
	wifi_init_softap();
	esp_restart();
}

void gpio_isr_rtc_handler (void *arg) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	
	increment_uptime();
	geiger_1s_handle();
	xSemaphoreGiveFromISR( xSemaphore, &xHigherPriorityTaskWoken );
	portYIELD_FROM_ISR();
}

void gpio_isr_button_handler (void *arg) {
	static uint32_t button_timer = 0;
	
	if (!gpio_get_level(0)) {
		button_timer = get_uptime();
	}
	else {
		if ( (uint32_t)(get_uptime()-button_timer) > 10) {
			os_timer_disarm(&soft_ap_restore_timer);
			os_timer_setfn(&soft_ap_restore_timer, soft_ap_restore_timer_func, NULL);
			os_timer_arm(&soft_ap_restore_timer, 100, 0);	
		}
	}
}


void i2c_task_example(void *arg) {
	time_t now;
	struct tm timeinfo;

    while(1) {
		xSemaphoreTake( xSemaphore, portMAX_DELAY );
		
		//if (xSemaphoreTake(i2cSemaphore, portMAX_DELAY) == pdTRUE) {
		//	if (ds3231_getTime(&timeinfo)) {
		//		printf(asctime(&timeinfo));
		//	}
		//	else {printf("Time read FAILURE\r\n");}
		//	xSemaphoreGive(i2cSemaphore);
		//}
		//else {
		//	ESP_LOGI(TAG, "I2C semaphore taken");
		//}
		
		time(&now);
		localtime_r(&now, &timeinfo);	
		//printf("Sivert: %f, CPM: %d\r\n", cpm2sievert(geiger_get_cpm()), geiger_get_cpm());
		//printf(asctime(&timeinfo));
		//vTaskDelay(1000 / portTICK_RATE_MS);
		
    }
}


void app_main() {
	
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    //config_load_defaults();
    //err = config_save_settings_to_flash();
    //if (err != ESP_OK) printf("Error (%s) saving settings to NVS!\n", esp_err_to_name(err));
    
    err = config_load_settings_from_flash();
    if (err != ESP_OK) printf("Error (%s) reading settings from NVS!\n", esp_err_to_name(err));

    gpio_config_t io_conf;
    
    //interrupt of falling edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    //bit mask of the pins
    io_conf.pin_bit_mask = (1ULL<<GPIO_INPUT_RTC);
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);  
    
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL<<GPIO_INPUT_BUTTON);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    
    //install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_RTC, gpio_isr_rtc_handler, NULL);
    gpio_isr_handler_add(GPIO_INPUT_BUTTON, gpio_isr_button_handler, NULL); 
    
    i2c_master_init();
    
    ds3231_setSquarewaveFreq(DS3231_CTRL_SQWAVE_1HZ);
    ds3231_enableSquarewave();     
    
	xSemaphore = xSemaphoreCreateBinary();
	if( xSemaphore == NULL ) while(1);    
	i2cSemaphore = xSemaphoreCreateBinary();
	if( i2cSemaphore == NULL ) while(1);
	xSemaphoreGive(i2cSemaphore);
	
	esp_event_loop_create_default();
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, scan_end_event, NULL);    
    
    printf("Hello world!\n");
	geiger_init();
	  
	wifi_init();
	//wifi_init_sta();
	//wifi_init_apsta();
	//wifi_init_softap();
	esp_wifi_set_ps(WIFI_PS_NONE);
    
    //First we try to set system time from I2C RTC
    //In case if there is no SNTP to use
    if (xSemaphoreTake(i2cSemaphore, portMAX_DELAY) == pdTRUE) {
        struct tm current_time;
        uint8_t res = ds3231_getTime(&current_time);
        xSemaphoreGive(i2cSemaphore);
        if (res) {
            struct timeval tv;
            tv.tv_sec = mktime(&current_time);
            tv.tv_usec = 0;
            settimeofday(&tv, NULL);
        }
    }
    else {ESP_LOGI(TAG, "I2C semaphore taken");}
	
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, config.ntp1);
    sntp_setservername(1, config.ntp2);
    sntp_setservername(2, config.ntp3);
    sntp_setservername(3, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(sntp_sync_time_func);
    sntp_init();
    
    setenv("TZ", "GMT-1GMT-2,M3.5.0/2,M10.5.0/3", 1);
    tzset(); 
    
    http_server_init();
    mqtt_client_init();
    spi_filesystem_init();
    
    xTaskCreate(i2c_task_example, "i2c_task_example", 4096, NULL, 10, NULL);
}


