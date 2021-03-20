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
#include "driver/gpio.h"
#include "i2c_master.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "geiger.h"
#include "ds3231.h"

#define GPIO_INPUT_RTC		14
#define GPIO_INPUT_PIN_SEL 	((1ULL<<GPIO_INPUT_RTC) | (1ULL<<GPIO_INPUT_CPM))

SemaphoreHandle_t xSemaphore = NULL;

uint32_t uptime = 0;


void gpio_isr_rtc_handler (void *arg) {
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	
	uptime++;
	geiger_1s_handle();
	xSemaphoreGiveFromISR( xSemaphore, &xHigherPriorityTaskWoken );
	portYIELD_FROM_ISR();
}


void i2c_task_example(void *arg) {
	struct tm time;
    
    time.tm_year = 121;
    time.tm_mon = 2;
    time.tm_mday = 18;
    time.tm_hour = 14;
    time.tm_min = 10;
    time.tm_hour = 0;
    
    ds3231_setTime(&time);

    while(1) {
		xSemaphoreTake( xSemaphore, portMAX_DELAY );
		//ds3231_getTime(&time);
		printf("Time: %lu, CPM: %d\r\n", (long unsigned int)uptime, geiger_get_cpm());
		//vTaskDelay(1000 / portTICK_RATE_MS);
		
    }
}


void app_main() {

    gpio_config_t io_conf;
    
    //interrupt of rising edge
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
    
    xTaskCreate(i2c_task_example, "i2c_task_example", 2048, NULL, 10, NULL);
}


