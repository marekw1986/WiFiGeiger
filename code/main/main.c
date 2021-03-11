/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_master.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "geiger.h"

void app_main()
{
    printf("Hello world!\n");
	geiger_init();
    
    //I2C Master init
    //i2c_master_init();
    
    xTaskCreate(i2c_task_example, "i2c_task_example", 2048, NULL, 10, NULL);
}


