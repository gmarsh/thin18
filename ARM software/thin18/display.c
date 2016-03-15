/*
 * display.c
 *
 * Created: 2016-01-05 1:57:42 AM
 *  Author: Gary
 */ 

#include <stdint.h>
#include <samd20.h>
#include <stdbool.h>
#include "display.h"
#include "rtc.h"
#include "cal.h"

// GPIO bits

#define PA_1H_ANODE_GPIO	(1<<8)
#define PA_10M_ANODE_GPIO	(1<<11)
#define PA_10H_ANODE_GPIO	(1<<7)
#define PA_1M_ANODE_GPIO	(1<<10)

#define C0_PA_GPIO	(1<<20)
#define C1_PA_GPIO	(1<<16)
#define C2_PA_GPIO	(1<<19)
#define C3_PA_GPIO	(1<<13)
#define C4_PA_GPIO	(1<<18)
#define C5_PA_GPIO	(1<<12)
#define C6_PA_GPIO	(1<<17)
#define C8_PA_GPIO	(1<<9)

#define C7_PB_GPIO	(1<<11)
#define C9_PB_GPIO	(1<<10)

#define INT_SENSE_PA_GPIO (1<<21)
#define MINUTE_SENSE_PA_GPIO (1<<0)
#define HOUR_SENSE_PA_GPIO (1<<1)


#define PA_ANODE_CLEAR_MASK		(PA_1H_ANODE_GPIO|PA_10M_ANODE_GPIO|PA_10H_ANODE_GPIO|PA_1M_ANODE_GPIO)
#define PA_CATHODE_CLEAR_MASK	(C0_PA_GPIO|C1_PA_GPIO|C2_PA_GPIO|C3_PA_GPIO|C4_PA_GPIO|C5_PA_GPIO|C6_PA_GPIO|C8_PA_GPIO)
#define PB_CATHODE_CLEAR_MASK	(C7_PB_GPIO|C9_PB_GPIO)

const uint32_t pa_anode_enable_mask[4] = {
	PA_1M_ANODE_GPIO,
	PA_10M_ANODE_GPIO,
	PA_1H_ANODE_GPIO,
	PA_10H_ANODE_GPIO
};

const uint32_t pa_cathode_enable_mask[4][10] = {
	{ 0,0,C5_PA_GPIO,C8_PA_GPIO,C0_PA_GPIO,C2_PA_GPIO,C4_PA_GPIO,C6_PA_GPIO,C3_PA_GPIO,C1_PA_GPIO},
	{ C5_PA_GPIO,0,0,C0_PA_GPIO,C8_PA_GPIO,C6_PA_GPIO,C4_PA_GPIO,C2_PA_GPIO,C1_PA_GPIO,C3_PA_GPIO},
	{ 0,0,C5_PA_GPIO,C8_PA_GPIO,C0_PA_GPIO,C2_PA_GPIO,C4_PA_GPIO,C6_PA_GPIO,C3_PA_GPIO,C1_PA_GPIO},
	{ C5_PA_GPIO,0,0,C0_PA_GPIO,C8_PA_GPIO,C6_PA_GPIO,C4_PA_GPIO,C2_PA_GPIO,C1_PA_GPIO,C3_PA_GPIO},
};

const uint32_t pb_cathode_enable_mask[4][10] = {
	{C9_PB_GPIO,C7_PB_GPIO,0,0,0,0,0,0,0,0},
	{0,C7_PB_GPIO,C9_PB_GPIO,0,0,0,0,0,0,0},
	{C9_PB_GPIO,C7_PB_GPIO,0,0,0,0,0,0,0,0},
	{0,C7_PB_GPIO,C9_PB_GPIO,0,0,0,0,0,0,0},
};


// display state structure
struct display_struct display;

/*
 
Main timer interrupt handler

*/

void TC0_Handler(void) {
	
	// overflow: turn off anode
	if (TC0->COUNT8.INTFLAG.bit.OVF) {
		PORT->Group[0].OUTCLR.reg = PA_ANODE_CLEAR_MASK;
		display.active_digit++;
		TC0->COUNT8.INTFLAG.reg = TC_INTFLAG_OVF;
	}
	
	// match/compare 0: switch cathodes
	if (TC0->COUNT8.INTFLAG.bit.MC0) {
		// turn on next cathode
		PORT->Group[0].OUTCLR.reg = PA_CATHODE_CLEAR_MASK;
		PORT->Group[1].OUTCLR.reg = PB_CATHODE_CLEAR_MASK;
		
		switch(display.active_digit & 0x03) {
			case 0:
				PORT->Group[0].OUTSET.reg = pa_cathode_enable_mask[0][display.displaytime.minutes];
				PORT->Group[1].OUTSET.reg = pb_cathode_enable_mask[0][display.displaytime.minutes];
			break;
			case 1:
				PORT->Group[0].OUTSET.reg = pa_cathode_enable_mask[1][display.displaytime.tenminutes];
				PORT->Group[1].OUTSET.reg = pb_cathode_enable_mask[1][display.displaytime.tenminutes];
			break;
			case 2:
				PORT->Group[0].OUTSET.reg = pa_cathode_enable_mask[2][display.displaytime.hours];
				PORT->Group[1].OUTSET.reg = pb_cathode_enable_mask[2][display.displaytime.hours];
			break;
			case 3:
				PORT->Group[0].OUTSET.reg = pa_cathode_enable_mask[3][display.displaytime.tenhours];
				PORT->Group[1].OUTSET.reg = pb_cathode_enable_mask[3][display.displaytime.tenhours];
			break;
		}

		TC0->COUNT8.INTFLAG.reg = TC_INTFLAG_MC0;
	}
	
	// match/compare 2: enable next anode, call display_update() and cal_update()
	if (TC0->COUNT8.INTFLAG.bit.MC1) {
		PORT->Group[0].OUTSET.reg = pa_anode_enable_mask[display.active_digit & 0x03];
		display_update();
		cal_update();
		TC0->COUNT8.INTFLAG.reg = TC_INTFLAG_MC1;
	}
}

// "Write time to RTC" interrupt

void TC1_Handler(void) {
	rtc_set_time(&display.rtc_handoff_time);
}



void display_init(void) {
	
	// set up vars
	display.active_digit = 0;
	display.hour_debounce_counter = 0;
	display.minute_pushed = false;
	display.minute_debounce_counter = 0;
	display.minute_pushed = false;

	display.prev_int_sense = true;
	display.rtc_handoff_counter = 0;
	
	// set up TC0 - use OSC32K (GCLK3) as time source
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK3 | GCLK_CLKCTRL_ID_TC0_TC1;
	// enable AHB clock
	PM->APBCMASK.reg |= PM_APBCMASK_TC0;
	
	// configure TC0 for 256Hz overflow (32768Hz / 1 / 128)
	TC0->COUNT8.CTRLA.reg = TC_CTRLA_RUNSTDBY | TC_CTRLA_PRESCALER_DIV1 | TC_CTRLA_WAVEGEN_NFRQ | TC_CTRLA_MODE_COUNT8 | (0<<TC_CTRLA_ENABLE_Pos);
	
	TC0->COUNT8.CC[0].reg = 0x08;
	while(TC0->COUNT8.STATUS.reg & TC_STATUS_SYNCBUSY);
	TC0->COUNT8.CC[1].reg = 0x10;
	while(TC0->COUNT8.STATUS.reg & TC_STATUS_SYNCBUSY);
	TC0->COUNT8.PER.reg = 0x7F;
	while(TC0->COUNT8.STATUS.reg & TC_STATUS_SYNCBUSY);
	TC0->COUNT8.COUNT.reg = 0x01;
	while(TC0->COUNT8.STATUS.reg & TC_STATUS_SYNCBUSY);
	TC0->COUNT8.CTRLBCLR.reg = TC_CTRLBCLR_DIR;	
	while(TC0->COUNT8.STATUS.reg & TC_STATUS_SYNCBUSY);
	TC0->COUNT8.CTRLC.reg = 0;
	while(TC0->COUNT8.STATUS.reg & TC_STATUS_SYNCBUSY);
	
	// fetch time from RTC into display.displaytime
	rtc_get_time(&display.displaytime);

	// enable TC IRQs in NVIC
	
	// TC0 is the 256Hz interrupt
	// use medium priority; RTC write interrupt is priority 3, UART is priority 1.
	NVIC_SetPriority(TC0_IRQn,2);
	NVIC_EnableIRQ(TC0_IRQn);
	
	// TC1's interrupt line is used to update the RTC.
	// TC1 isn't actually used for the function, we're just stealing its IRQ line.
	NVIC_SetPriority(TC1_IRQn,3);
	NVIC_EnableIRQ(TC1_IRQn);
	
	// clear CC and overflow interrupt flags
	TC0->COUNT8.INTFLAG.reg = TC_INTFLAG_MC1 | TC_INTFLAG_MC0 | TC_INTFLAG_OVF;
	// enable interrupt
	TC0->COUNT8.INTENSET.reg = TC_INTENSET_MC1 | TC_INTENSET_MC0 | TC_INTENSET_OVF;
	// and enable timer
	TC0->COUNT8.CTRLA.bit.ENABLE = 1;
	
	// lastly enable DC/DC!
	//PORT->Group[0].OUTSET.reg = (1<<6);
}



void display_update(void) {
	
	// step #1 - look for RTC /INT edge, int_edge = true if edge occurs.
	bool int_sense = ((PORT->Group[0].IN.reg & INT_SENSE_PA_GPIO) == 0);
	bool int_edge = !(display.prev_int_sense) && int_sense;
	display.prev_int_sense = int_sense;
	
	// step #2 - debounce buttons
	bool minute_push_edge = false;
	if ((PORT->Group[0].IN.reg & MINUTE_SENSE_PA_GPIO) == 0) {
		if (display.minute_debounce_counter < BUTTON_DEBOUNCE_COUNT) display.minute_debounce_counter++;
		if (display.minute_debounce_counter == BUTTON_DEBOUNCE_COUNT) {
			if (!display.minute_pushed) {
				minute_push_edge = true;
				display.minute_pushed = true;
			}
		}
	} else {
		if (display.minute_debounce_counter) display.minute_debounce_counter--;
		if (display.minute_debounce_counter == 0) display.minute_pushed = false;
	}
	
	bool hour_push_edge = false;
	if ((PORT->Group[0].IN.reg & HOUR_SENSE_PA_GPIO) == 0) {
		if (display.hour_debounce_counter < BUTTON_DEBOUNCE_COUNT) display.hour_debounce_counter++;
		if (display.hour_debounce_counter == BUTTON_DEBOUNCE_COUNT) {
			if (!display.hour_pushed) {
				hour_push_edge = true;
				display.hour_pushed = true;
			}
		}
	} else {
		if (display.hour_debounce_counter) display.hour_debounce_counter--;
		if (display.hour_debounce_counter == 0) display.hour_pushed = false;
	}
	
	// step #3 - increment time
	
	// priority #1 - if both buttons held, reset time to 0000
	if (display.hour_pushed && display.minute_pushed) {
		display.displaytime.seconds = 0;
		display.displaytime.minutes = 0;
		display.displaytime.tenminutes = 0;
		display.displaytime.hours = 0;
		display.displaytime.tenhours = 0;
		display.rtc_handoff_counter = RTC_HANDOFF_DELAY;
	}
	// priority #2 - if minute newly pushed, increment minutes
	else if (minute_push_edge) {
		display.displaytime.minutes++;
		display.displaytime.seconds = 0;
		display.rtc_handoff_counter = RTC_HANDOFF_DELAY;
	}
	// priority #3 - if hour newly pushed, increment hours
	else if (hour_push_edge) {
		display.displaytime.hours++;
		display.rtc_handoff_counter = RTC_HANDOFF_DELAY;
	}
	// priority #4 - if interrupt edge happens, increment seconds.
	else if (int_edge) {
		display.displaytime.seconds++;
	}
	
	// step #4 - handle time overflows from incrementing stuff
	if (display.displaytime.seconds >= 60) {
		display.displaytime.seconds = 0; display.displaytime.minutes++;
	}
	if (display.displaytime.minutes >= 10) {
		display.displaytime.minutes = 0; display.displaytime.tenminutes++;
	}
	if (display.displaytime.tenminutes >= 6) {
		display.displaytime.tenminutes = 0; display.displaytime.hours++;
	}
	if (display.displaytime.hours >= 10) {
		display.displaytime.hours = 0; display.displaytime.tenhours++;
	}
	if ((display.displaytime.tenhours >= 2) && (display.displaytime.hours >= 4)) {
		display.displaytime.hours = 0; display.displaytime.tenhours = 0;
	}
	
	
	// step #5 - copy time to handoff array when handoff counter expires,
	// and kick write-to-RTC interrupt
	if (display.rtc_handoff_counter) {
		if (--display.rtc_handoff_counter == 0) {
			display.rtc_handoff_time = display.displaytime;
			NVIC_SetPendingIRQ(TC1_IRQn);
		}
	}
}
