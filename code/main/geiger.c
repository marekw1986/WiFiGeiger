/*
 * geiger.c
 *
 * Created: 2014-04-20 12:01:47
 *  Author: Marek
 */ 

#include <stdlib.h>
#include "driver/gpio.h"
#include "esp_system.h"
#include "geiger.h"

#define GPIO_INPUT_CPM		4
#define PRESCALER 			0.0057

volatile uint16_t geiger_pulses[60];
volatile uint8_t geiger_pulses_index = 0;
volatile uint16_t pulses = 0;

static void geiger_isr_cpm_handler (void *arg);


void geiger_init (void) {
	gpio_config_t io_conf;
    uint8_t i;
    
    for (i=0; i<60; i++) {
        geiger_pulses[i] = 0;
    }

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    //bit mask of the pins
    io_conf.pin_bit_mask = (1ULL<<GPIO_INPUT_CPM);
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);     
    
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_CPM, geiger_isr_cpm_handler, NULL);   

}


uint16_t geiger_get_cpm (void) {
	uint16_t result = 0;
	uint8_t i;
	
	for (i=0; i < 60; i++) {
		result += geiger_pulses[i];
	}
	
	return result;
}


void geiger_1s_handle (void) {
	//TODO: Disable interrupt!!!!
	geiger_pulses[geiger_pulses_index] = pulses;
	pulses = 0;
	geiger_pulses_index++;
	if (geiger_pulses_index > 59) geiger_pulses_index = 0;
}


double cpm2sievert (uint16_t cpm) {
	return (PRESCALER * cpm);
}


static void geiger_isr_cpm_handler (void *arg) {
	pulses++;
} 
