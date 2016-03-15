/*
 * cal.c
 *
 * Created: 2016-01-23 5:27:35 PM
 *  Author: Gary
 */ 


#include <samd20.h>
#include <stdint.h>
#include <stdbool.h>
#include "cal.h"


#define CAL_VALID_TIMEOUT	1024
volatile uint16_t cal_valid_counter;


// capture value mask - this also serves as the TC2 period
// value must be greater than cal frequency, and must be a multiple of 2 minus 1.
#define CAPTURE_VALUE_MASK	(0x00FFFFFF)

volatile uint32_t last_capture_value;
volatile bool last_capture_valid;

// FIFO of captures
#define CAPTURE_FIFO_SIZE	4
volatile uint16_t capture_fifo_readptr;
volatile uint16_t capture_fifo_writeptr;
volatile uint32_t capture_fifo[CAPTURE_FIFO_SIZE];
#define CAPTURE_FIFO_MASK (CAPTURE_FIFO_SIZE-1)
#define CAPTURE_FIFO_EMPTY (capture_fifo_readptr == capture_fifo_writeptr)
#define CAPTURE_FIFO_FULL (((capture_fifo_readptr - capture_fifo_writeptr) & CAPTURE_FIFO_MASK) == 1)


void cal_update(void) {
	
	// check for validity by checking for overflow flag, indicating timer is clocked.
	if (TC2->COUNT32.INTFLAG.bit.OVF) {
		cal_valid_counter = CAL_VALID_TIMEOUT;
		TC2->COUNT32.INTFLAG.reg = TC_INTFLAG_OVF;
	} else {
		if (cal_valid_counter) cal_valid_counter--;
	}

	
	// if calibration isn't valid, clear any other flags and return.
	if (cal_valid_counter == 0) {
		
		// clear any other pending interrupts
		TC2->COUNT32.INTFLAG.reg = TC_INTFLAG_MC1 | TC_INTFLAG_SYNCRDY;
		last_capture_valid = false;
		
		return;
	}

	uint32_t new_capture_value, capture_diff;

	// if MC1 flag is set, start read request
	// offset 0x1C is CC1 register
	if (TC2->COUNT32.INTFLAG.bit.MC1) {
		TC2->COUNT32.READREQ.reg = TC_READREQ_RREQ | TC_READREQ_ADDR(0x1C);
		TC2->COUNT32.INTFLAG.reg = TC_INTFLAG_MC1;
	}

	// if SYNCRDY flag is set, read CC1
	if (TC2->COUNT32.INTFLAG.bit.SYNCRDY) {
		new_capture_value = TC2->COUNT32.CC[1].reg;
		TC2->COUNT32.INTFLAG.reg = TC_INTFLAG_SYNCRDY;

		// if previous capture is valid, calculate time difference and write into FIFO
		if (last_capture_valid) {
			capture_diff = (new_capture_value - last_capture_value) & CAPTURE_VALUE_MASK;
			if (!CAPTURE_FIFO_FULL) {
				capture_fifo[capture_fifo_writeptr] = capture_diff;
				capture_fifo_writeptr = (capture_fifo_writeptr + 1) & CAPTURE_FIFO_MASK;
			}
		} else {
			last_capture_valid = true;
		}

		last_capture_value = new_capture_value;
	}

}

bool cal_ready(void) {
	return (cal_valid_counter > 0);
}

bool cal_fetch(uint32_t *value) {
	
	if (CAPTURE_FIFO_EMPTY) return false;

	*value = capture_fifo[capture_fifo_readptr];
	capture_fifo_readptr = (capture_fifo_readptr + 1) & CAPTURE_FIFO_MASK;

	return true;
}


void cal_init(void) {
	
	// CONFIGURE EIC
	
	// configure PA21 (INT from RTC, EXTINT5) input as EIC interrupt
	PORT->Group[0].PMUX[21/2].bit.PMUXO = PORT_PMUX_PMUXO_A_Val;
	PORT->Group[0].PINCFG[21].bit.PMUXEN = 1;
	
	// enable EIC clock
	PM->APBAMASK.reg |= PM_APBAMASK_EIC;
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(EIC_GCLK_ID) | GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN(0);
	
	// configure EXTINT5 to pass through EXTINT5 without any filtering/retiming
	EIC->CONFIG[0].bit.FILTEN5 = 0;
	EIC->CONFIG[0].bit.SENSE5 = EIC_CONFIG_SENSE5_HIGH_Val;
	// enable event for this interrupt
	EIC->EVCTRL.bit.EXTINTEO5 = 1;

	// enable EIC!
	EIC->CTRL.reg = EIC_CTRL_ENABLE;
	
	
	// CONFIGURE EVENT SYSTEM
	
	// enable event system clock
	PM->APBCMASK.reg |= PM_APBCMASK_EVSYS;
	
	// enable event channel 0 CLK
	// use GCLK generator 0 for now, we'll change this to GCLK1 later
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK1 | GCLK_CLKCTRL_ID_EVSYS_CHANNEL_0;
	
	// event generator 0x11 is EXTINT5 and is a copy of what's on the IO pin.
	// resynchronize to the local GCLK and detect a falling edge.
	EVSYS->CHANNEL.reg =	EVSYS_CHANNEL_EDGSEL_FALLING_EDGE |
							EVSYS_CHANNEL_PATH_RESYNCHRONIZED |
							EVSYS_CHANNEL_EVGEN(0x11) |
							EVSYS_CHANNEL_CHANNEL(0);
	
	// event user 0x02 is TC2
	// note that EVSYS_USER_CHANNEL(1) actually configures channel 0!
	EVSYS->USER.reg = EVSYS_USER_CHANNEL(1) | EVSYS_USER_USER(0x02);
	
	
	// CONFIGURE TC2
	
	// Use GCLK0 to clock timer while we configure it, we'll change it to GCLK1 later
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_TC2_TC3;
	// enable APB clock (also enable TC3 clock, though it's probably not needed)
	PM->APBCMASK.reg |= PM_APBCMASK_TC2 | PM_APBCMASK_TC3;
	
	// 32 bit mode, top=CC0
	TC2->COUNT32.CTRLA.reg = TC_CTRLA_RUNSTDBY | TC_CTRLA_PRESCALER_DIV1 | TC_CTRLA_WAVEGEN_MFRQ | TC_CTRLA_MODE_COUNT32 | (0<<TC_CTRLA_ENABLE_Pos);
	while(TC2->COUNT32.STATUS.bit.SYNCBUSY);
	
	// overflow every 16M samples, this makes subtracting time easy.
	TC2->COUNT32.CC[0].reg = CAPTURE_VALUE_MASK;
	while(TC2->COUNT32.STATUS.bit.SYNCBUSY);
	
	// zero counter
	TC2->COUNT32.COUNT.reg = 0;
	while(TC2->COUNT32.STATUS.bit.SYNCBUSY);
	
	// clear ONESHOT and DIR bits, so we count upwards and repeat
	TC2->COUNT32.CTRLBCLR.reg = TC_CTRLBCLR_ONESHOT | TC_CTRLBCLR_DIR;
	while(TC2->COUNT32.STATUS.bit.SYNCBUSY);
	
	// enable capture channel 1
	TC2->COUNT32.CTRLC.reg = TC_CTRLC_CPTEN1;
	while(TC2->COUNT32.STATUS.bit.SYNCBUSY);
	
	// enable event input
	TC2->COUNT32.EVCTRL.reg = TC_EVCTRL_TCEI | TC_EVCTRL_EVACT_OFF;
	while(TC2->COUNT32.STATUS.bit.SYNCBUSY);
	
	// clear any pending interrupt flags
	TC2->COUNT32.INTFLAG.reg = TC_INTFLAG_MC1 | TC_INTFLAG_OVF | TC_INTFLAG_SYNCRDY;
	while(TC2->COUNT32.STATUS.bit.SYNCBUSY);
	
	// and fire away!
	TC2->COUNT32.CTRLA.bit.ENABLE = 1;
	while(TC2->COUNT32.STATUS.bit.SYNCBUSY);
	
	// now switch GCLKs for event and timer to GCLK1
	//GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK1 | GCLK_CLKCTRL_ID_TC2_TC3;
	//GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK1 | GCLK_CLKCTRL_ID_EVSYS_CHANNEL_0;
	
	
}


void cal_input_select(bool use_cal_input) {
	
	if (use_cal_input) {
		GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK1 | GCLK_CLKCTRL_ID_TC2_TC3;
		GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK1 | GCLK_CLKCTRL_ID_EVSYS_CHANNEL_0;
	} else {
		GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_TC2_TC3;
		GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_EVSYS_CHANNEL_0;
	}
	
}