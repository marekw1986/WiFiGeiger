/*
 * geiger.h
 *
 * Created: 2014-04-20 12:01:23
 *  Author: Marek
 */ 


#ifndef GEIGER_H_
#define GEIGER_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Deklaracje zmiennych zewnêtrznych
extern volatile uint16_t geiger_pulses[];



//Deklaracje funkcji
void geiger_init (void);
uint16_t geiger_get_cpm (void);
void geiger_1s_handle (void);
double cpm2sievert (uint16_t cpm);



#endif /* GEIGER_H_ */
