/*
 * main.c
 *
 * Created: 2016-01-23 3:30:04 PM
 * Author : Gary
 */ 

#include "console.h"
#include "display.h"
#include "rtc.h"
#include "init.h"
#include "cal.h"
#include "menu.h"
#include "delay.h"

int main(void) {

	// init clocks
	clock_init();
	
	// init GPIO pin states
	gpio_init();

	// init serial console
	console_init();
	
	// wait for RTC to start up
	delay_ms(1000);
	
	// init RTC
	rtc_init();
	
	// init calibration
	cal_init();

	// init display (rtc_init(), cal_init() must must be called first!)
	display_init();

	// call menu
	menu();
}
