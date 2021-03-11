/*
 * geiger.c
 *
 * Created: 2014-04-20 12:01:47
 *  Author: Marek
 */ 

#include <stdlib.h>
#include "geiger.h"

#define PRESCALER 0.0057

volatile uint16_t geiger_pulses[60];

void geiger_init (void) {
    uint8_t i;
    for (i=0; i<60; i++) {
        geiger_pulses[i] = 0;
    }
    //TMR2 = 0;
    //OpenTimer2(T2_ON | T2_PS_1_1 | T2_SOURCE_EXT, 0xFFFF);
}

uint16_t cpm (void) {
	uint16_t result = 0;
	uint8_t i;
	
	for (i=0; i < 60; i++) {
		result += geiger_pulses[i];
	}
	
	return result;
}

double cpm2sievert (uint16_t cpm) {
	return (PRESCALER * cpm);
}
