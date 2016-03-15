/*
 * init.c
 *
 * Created: 6/10/2015 9:23:11 PM
 *  Author: Gary
 */ 

#include <stdint.h>
#include "sam.h"
#include "init.h"

// OSC32K calibration
//#define NVMCTRL_OTP4   (0x00806020U)
#define SYSCTRL_FUSES_OSC32K_CAL_ADDR   (NVMCTRL_OTP4 + 4)
#define SYSCTRL_FUSES_OSC32K_CAL_Msk   (0x7Fu << SYSCTRL_FUSES_OSC32K_CAL_Pos)
#define SYSCTRL_FUSES_OSC32K_CAL_Pos   6


void clock_init(void){
	
	// use 8M oscillator as main clock, /1 prescaler
	// core switches to 8MHz operation after PRESC write
	SYSCTRL->OSC8M.bit.PRESC = 0;
	SYSCTRL->OSC8M.bit.ONDEMAND = 1;
	SYSCTRL->OSC8M.bit.RUNSTDBY = 0;
	
	// enable watchdog timer with a 1/2 second (16384 clock) timeout
	//WDT->CONFIG = WDT_CONFIG_WINDOW_8 | WDT_CONFIG_PER_16K;
	//WDT->CTRL = WDT_CTRL_ENABLE;

	// set up generic clock generators
	// GCLK0 = leave alone (8M core)
	// GCLK2 = leave alone (ULP 32K oscillator for WDT)
	// GCLK3 = OSC32K for display
	
	
	// GCLK1 = 10MHz calibration input (on PB23) - function H
	PORT->Group[1].PMUX[23/2].bit.PMUXO = PORT_PMUX_PMUXO_H_Val;
	PORT->Group[1].PINCFG[23].bit.PMUXEN = 1;
	
	GCLK->GENDIV.reg = GCLK_GENDIV_DIV(0) | GCLK_GENDIV_ID_GCLK1;
	GCLK->GENCTRL.reg =		(1 << GCLK_GENCTRL_RUNSTDBY_Pos) |
							(0 << GCLK_GENCTRL_DIVSEL_Pos) |
							(0 << GCLK_GENCTRL_OE_Pos) |
							(1 << GCLK_GENCTRL_GENEN_Pos) |
							GCLK_GENCTRL_SRC_GCLKIN |
							GCLK_GENCTRL_ID_GCLK1;
	

	// enable OSC32K
	SYSCTRL->OSC32K.reg =	((*(uint32_t *)SYSCTRL_FUSES_OSC32K_CAL_ADDR >> SYSCTRL_FUSES_OSC32K_CAL_Pos) << SYSCTRL_OSC32K_CALIB_Pos) |
							(2 << SYSCTRL_OSC32K_STARTUP_Pos) |
							(1 << SYSCTRL_OSC32K_RUNSTDBY_Pos) |
							(1 << SYSCTRL_OSC32K_EN32K_Pos) |
							(1 << SYSCTRL_OSC32K_ENABLE_Pos);
	// wait for it to start
	while ((SYSCTRL->PCLKSR.bit.OSC32KRDY) == 0);

	// configure GCLK3, pointed at OSC32K
	GCLK->GENDIV.reg = GCLK_GENDIV_DIV(0) | GCLK_GENDIV_ID_GCLK3;
	GCLK->GENCTRL.reg =		(1<<GCLK_GENCTRL_RUNSTDBY_Pos) |
							(0 << GCLK_GENCTRL_DIVSEL_Pos) |
							(0 << GCLK_GENCTRL_OE_Pos) |
							(1 << GCLK_GENCTRL_GENEN_Pos) |
							GCLK_GENCTRL_SRC_OSC32K |
							GCLK_GENCTRL_ID_GCLK3;

}

void gpio_init(void) {
	
	// gpio_init() - this function initializes all pins in GPIO mode with suitable default values.
	// subsequent pin multiplexing by peripherals should be done in the respective code.
	
	// Port A
	// Most outputs are either NC or tube drive outputs, both are driven low at startup.
	// Exceptions are:
	// PA31/PA30 = SWD = weak pulldowns
	// PA23 (SCL), PA22 (SDA), PA21 (INT) - inputs, external pull-ups
	// PA15 (RX from FTDI) - input, weak pull-up
	// PA14 (TX to FTDI) - output, driven high
	// PA05 (CBUS2), PA04 (CBUS3) - use weak pulldowns
	// PA1 (minute button), PA0 - use weak pullups
	
	PORT->Group[0].OUT.reg =	(1<<13)|(1<<1)|(1<<0);
	PORT->Group[0].DIR.reg =	(0<<31)|(0<<30)|(1<<29)|(1<<28)|(1<<27)|(1<<26)|(1<<25)|(1<<24)|\
								(0<<23)|(0<<22)|(0<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|(1<<16)|\
								(0<<15)|(1<14)|(1<<13)|(1<<12)|(1<<11)|(1<<10)|(1<<9)|(1<<8)|\
								(1<<7)|(1<<6)|(0<<5)|(0<<4)|(1<<3)|(1<<2)|(0<<1)|(0<<0);
	
	// PA31, 30 = SWDIO/SWCLK - multiplexed as peripheral G
	//PORT->Group[0].PMUX[31/2].bit.PMUXO = PORT_PMUX_PMUXO_G_Val;
	//PORT->Group[0].PMUX[30/2].bit.PMUXE = PORT_PMUX_PMUXE_G_Val;
	//PORT->Group[0].PINCFG[31].reg = (0<<PORT_PINCFG_DRVSTR_Pos)|(1<<PORT_PINCFG_PULLEN_Pos)|(1<<PORT_PINCFG_INEN_Pos)|(1<<PORT_PINCFG_PMUXEN_Pos);
	//PORT->Group[0].PINCFG[30].reg = (0<<PORT_PINCFG_DRVSTR_Pos)|(1<<PORT_PINCFG_PULLEN_Pos)|(1<<PORT_PINCFG_INEN_Pos)|(1<<PORT_PINCFG_PMUXEN_Pos);
	
	// PA23 = SCL = SC3[1]
	// PA22 = SDA = SC3[0]
	PORT->Group[0].PINCFG[23].reg = (0<<PORT_PINCFG_DRVSTR_Pos)|(0<<PORT_PINCFG_PULLEN_Pos)|(1<<PORT_PINCFG_INEN_Pos)|(0<<PORT_PINCFG_PMUXEN_Pos);
	PORT->Group[0].PINCFG[22].reg = (0<<PORT_PINCFG_DRVSTR_Pos)|(0<<PORT_PINCFG_PULLEN_Pos)|(1<<PORT_PINCFG_INEN_Pos)|(0<<PORT_PINCFG_PMUXEN_Pos);
	// PA21 = /INT
	PORT->Group[0].PINCFG[21].reg = (0<<PORT_PINCFG_DRVSTR_Pos)|(0<<PORT_PINCFG_PULLEN_Pos)|(1<<PORT_PINCFG_INEN_Pos)|(0<<PORT_PINCFG_PMUXEN_Pos);
	
	// PA15 = RX from FTDI = SC2[3] = function C
	// PA14 = TX to FTDI = SC2[2] = function C
	PORT->Group[0].PINCFG[15].reg = (0<<PORT_PINCFG_DRVSTR_Pos)|(1<<PORT_PINCFG_PULLEN_Pos)|(1<<PORT_PINCFG_INEN_Pos)|(0<<PORT_PINCFG_PMUXEN_Pos);
	PORT->Group[0].PINCFG[13].reg = (0<<PORT_PINCFG_DRVSTR_Pos)|(0<<PORT_PINCFG_PULLEN_Pos)|(0<<PORT_PINCFG_INEN_Pos)|(0<<PORT_PINCFG_PMUXEN_Pos);
	
	// PA05/PA04 (CBUS pins from FTDI) - enable pulldowns and inputs
	PORT->Group[0].PINCFG[5].reg = (0<<PORT_PINCFG_DRVSTR_Pos)|(1<<PORT_PINCFG_PULLEN_Pos)|(1<<PORT_PINCFG_INEN_Pos)|(0<<PORT_PINCFG_PMUXEN_Pos);
	PORT->Group[0].PINCFG[4].reg = (0<<PORT_PINCFG_DRVSTR_Pos)|(1<<PORT_PINCFG_PULLEN_Pos)|(1<<PORT_PINCFG_INEN_Pos)|(0<<PORT_PINCFG_PMUXEN_Pos);
	
	// PA1, PA0 = enable pullups and inputs
	PORT->Group[0].PINCFG[1].reg = (0<<PORT_PINCFG_DRVSTR_Pos)|(1<<PORT_PINCFG_PULLEN_Pos)|(1<<PORT_PINCFG_INEN_Pos)|(0<<PORT_PINCFG_PMUXEN_Pos);
	PORT->Group[0].PINCFG[0].reg = (0<<PORT_PINCFG_DRVSTR_Pos)|(1<<PORT_PINCFG_PULLEN_Pos)|(1<<PORT_PINCFG_INEN_Pos)|(0<<PORT_PINCFG_PMUXEN_Pos);
	

	// Port B
	// Inputs on this pin:
	
	// Most outputs are either NC or tube drive outputs, both are driven low at startup.
	// Only exception is PB23 (calibration input) - input, driven by external gate.

	PORT->Group[1].OUT.reg =	0;
	PORT->Group[1].DIR.reg =	(1<<31)|(1<<30)|(1<<29)|(1<<28)|(1<<27)|(1<<26)|(1<<25)|(1<<24)|\
								(0<<23)|(1<<22)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|(1<<16)|\
								(1<<15)|(1<14)|(1<<13)|(1<<12)|(1<<11)|(1<<10)|(1<<9)|(1<<8)|\
								(1<<7)|(1<<6)|(1<<5)|(1<<4)|(1<<3)|(1<<2)|(1<<1)|(1<<0);

	PORT->Group[1].PINCFG[23].reg = (0<<PORT_PINCFG_DRVSTR_Pos)|(0<<PORT_PINCFG_PULLEN_Pos)|(1<<PORT_PINCFG_INEN_Pos)|(0<<PORT_PINCFG_PMUXEN_Pos);



}

