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
#include "common.h"
#include "geiger.h"
#include "ds3231.h"
#include "wifi.h"
#include "http_server.h"
#include "mqtt_log.h"
#include "lwip/apps/sntp.h"

#define GPIO_INPUT_RTC		14
#define GPIO_INPUT_PIN_SEL 	((1ULL<<GPIO_INPUT_RTC) | (1ULL<<GPIO_INPUT_CPM))

const char *TAG = "wiFiGeiger";

SemaphoreHandle_t xSemaphore = NULL;

void gpio_isr_rtc_handler (void *arg) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	
	increment_uptime();
	geiger_1s_handle();
	xSemaphoreGiveFromISR( xSemaphore, &xHigherPriorityTaskWoken );
	portYIELD_FROM_ISR();
}


void i2c_task_example(void *arg) {
	time_t now;
	struct tm timeinfo;
    
    timeinfo.tm_year = 121;
    timeinfo.tm_mon = 2;
    timeinfo.tm_mday = 18;
    timeinfo.tm_sec = 0;
    timeinfo.tm_hour = 14;
    timeinfo.tm_min = 10;
    
    ds3231_setTime(&timeinfo);

    while(1) {
		xSemaphoreTake( xSemaphore, portMAX_DELAY );
		//if (ds3231_getTime(&timeinfo)) {
		//	printf(asctime(&timeinfo));
		//}
		//else {printf("Time read FAILURE\r\n");}
		time(&now);
		localtime_r(&now, &timeinfo);	
		printf("Sivert: %f, CPM: %d\r\n", cpm2sievert(geiger_get_cpm()), geiger_get_cpm());
		printf(asctime(&timeinfo));
		//vTaskDelay(1000 / portTICK_RATE_MS);
		
    }
}


void app_main() {
	
	uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    
    uart_driver_install(UART_NUM_0, 2048, 0, 0, NULL, 0);
	uart_param_config(UART_NUM_0, &uart_config);

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
    
    //install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_RTC, gpio_isr_rtc_handler, NULL); 
    
    i2c_master_init();
    
    ds3231_setSquarewaveFreq(DS3231_CTRL_SQWAVE_1HZ);
    ds3231_enableSquarewave();     
    
	xSemaphore = xSemaphoreCreateBinary();
	if( xSemaphore == NULL ) while(1);    
    
    printf("Hello world!\n");
	geiger_init();
	  
	wifi_init_sta();
	esp_wifi_set_ps(WIFI_PS_NONE);
	
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
    
    setenv("TZ", "GMT-1GMT-2,M3.5.0/2,M10.5.0/3", 1);
    tzset(); 
    
    http_server_init();
    mqtt_client_start(); 
    
    xTaskCreate(i2c_task_example, "i2c_task_example", 4096, NULL, 10, NULL);
}


