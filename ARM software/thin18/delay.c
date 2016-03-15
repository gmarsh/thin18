/*
 * delay_timer.c
 *
 * shitty delay functions that use TC4
 *
 * Created: 2016-02-06 9:53:34 PM
 *  Author: Gary
 */ 

#include <stdint.h>
#include <samd20.h>
#include "delay_timer.h"


#define CYCLES_PER_MS	8000
#define CYCLES_PER_US	8

void delay_ms(uint32_t ms) {
	delay_cycles (ms * CYCLES_PER_MS);
}

void delay_us(uint32_t us) {
	delay_cycles (us * CYCLES_PER_US);
}

