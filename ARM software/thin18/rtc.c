/*
 * rtc.c
 *
 * Created: 2016-01-07 10:29:14 PM
 *  Author: Gary
 */ 

#include <samd20.h>
#include <stdint.h>
#include <stdbool.h>
#include "rtc.h"


// ISL12022 registers
#define RTC_ADDRESS_READ	0xDF
#define RTC_ADDRESS_WRITE	0xDE


// ISL12022 structure
struct rtc_data_struct rtc_data;



void rtc_init(void) {

	/* step #1: initialize SERCOM */
	
	// enable clock to SERCOM3 - 8MHz core clock, 32KHz slow clock
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM3_GCLK_ID_CORE) | GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN(0);
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM3_GCLK_ID_SLOW) | GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN(2);
	PM->APBCMASK.reg |= PM_APBCMASK_SERCOM3;
	
	// configure PA22/PA23 as SC3[0] and SC3[1] respectively
	PORT->Group[0].PMUX[23/2].bit.PMUXO = PORT_PMUX_PMUXO_C_Val;
	PORT->Group[0].PMUX[22/2].bit.PMUXE = PORT_PMUX_PMUXE_C_Val;
	PORT->Group[0].PINCFG[23].bit.PMUXEN = 1;
	PORT->Group[0].PINCFG[22].bit.PMUXEN = 1;
	
	// initialize SERCOM3 into TWI mode
	// high/low time periods are (5+N) GCLK cycles
	// values of 5 give 400KHz TWI speed
	// values of 15 give 200KHz TWI speed
	// values of 75 gives 50KHz TWI speed

	SERCOM3->I2CM.BAUD.reg = \
		(15<<SERCOM_I2CM_BAUD_BAUD_Pos)| \
		(15<<SERCOM_I2CM_BAUD_BAUDLOW_Pos);
	while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);
	
	
	// enable TWI mode
	SERCOM3->I2CM.CTRLA.reg = \
		(3<<SERCOM_I2CM_CTRLA_SDAHOLD_Pos) |\
		(0<<SERCOM_I2CM_CTRLA_INACTOUT_Pos) |\
		(0<<SERCOM_I2CM_CTRLA_PINOUT_Pos) |\
		SERCOM_I2CM_CTRLA_MODE_I2C_MASTER |\
		(0<<SERCOM_I2CM_CTRLA_ENABLE_Pos);
	while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);
	
	// don't enable smart mode
	SERCOM3->I2CM.CTRLB.reg = 0;
	while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);
	
	// force TWI unit into idle mode
	SERCOM3->I2CM.STATUS.bit.BUSSTATE = 1;
	while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);

	// enable!
	SERCOM3->I2CM.CTRLA.bit.ENABLE = 1;
	while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);

	// force TWI unit into idle mode
	SERCOM3->I2CM.STATUS.bit.BUSSTATE = 1;
	while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);
	
	/* step #2: initialize RTC chip itself */
	
	// read entire RTC chip
	rtc_read(0x00,(uint8_t *) &rtc_data,sizeof(rtc_data));
	
	// check config
	
	uint8_t csr_int_cache = rtc_data.csr.csr_int;

	
	// WRTC=1 (enable RTC)
	// FOBATB=1 (disable /INT output)
	// FO=10 (1Hz frequency output)
	rtc_data.csr.csr_int = (1<<6)|(1<<4)|(10<<0);
	// clear timestamps, VDD brownout = 2.8V, battery brownouts = see datasheet.
	rtc_data.csr.csr_pwr_vdd = (1<<7)|(2<<0);
	rtc_data.csr.csr_pwr_vbat = (0<<6)|(3<<3)|(3<<0);
	// rest of CSR registers is trimming - leave alone!
	
	// disable alarms
	rtc_data.alarm.alarm_sca0 = 0;
	rtc_data.alarm.alarm_mna0 = 0;
	rtc_data.alarm.alarm_hra0 = 0;
	rtc_data.alarm.alarm_dta0 = 0;
	rtc_data.alarm.alarm_moa0 = 0;
	rtc_data.alarm.alarm_dwa0 = 0;
	
	// disable DST
	rtc_data.dstcr.dstcr_DstMoFd = 0;
	
	// write everything back to RTC
	rtc_write(0x07,&rtc_data.csr.csr_sr,42);
	
	// if RTCF was set, write new time (00:00, jan 1, 2016)
	if (csr_int_cache & 0x01) {
		rtc_data.rtc.rtc_sc = 0x00;
		rtc_data.rtc.rtc_mn = 0x00;
		rtc_data.rtc.rtc_hr = 0x00;
		rtc_data.rtc.rtc_dt = 0x01;
		rtc_data.rtc.rtc_mo = 0x01;
		rtc_data.rtc.rtc_yr = 0x16;
		rtc_data.rtc.rtc_dw = 0;
		rtc_write(0x00,(uint8_t *) &rtc_data, 7);
	}
	
}

bool rtc_read(uint8_t addr, uint8_t *data, uint8_t count) {
	
	// fail gracefully if count is zero
	if (count == 0) return true;
	
	// write chip address
	SERCOM3->I2CM.ADDR.reg = RTC_ADDRESS_WRITE;
	while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);
	while(SERCOM3->I2CM.INTFLAG.bit.MB == 0);
	if (SERCOM3->I2CM.STATUS.reg & (SERCOM_I2CM_STATUS_RXNACK|SERCOM_I2CM_STATUS_BUSERR|SERCOM_I2CM_STATUS_ARBLOST)) {
		goto rtc_read_fail;
	}
	
	// write register address
	SERCOM3->I2CM.DATA.reg = addr;
	while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);
	while(SERCOM3->I2CM.INTFLAG.bit.MB == 0);
	if (SERCOM3->I2CM.STATUS.reg & (SERCOM_I2CM_STATUS_RXNACK|SERCOM_I2CM_STATUS_BUSERR|SERCOM_I2CM_STATUS_ARBLOST)) {
		goto rtc_read_fail;
	}
	
	// writing RTC_ADDRESS_READ sends repeated start,
	// sends address and receives first byte.
	SERCOM3->I2CM.ADDR.reg = RTC_ADDRESS_READ;
	while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);

	while(count--) {
		// wait for data byte to arrive
		while(SERCOM3->I2CM.INTFLAG.bit.SB == 0);
		
		// grab data
		*data++ = SERCOM3->I2CM.DATA.reg;

		if (count) {
			// if we have another byte to receive, ACK this transmission and start another xfer
			SERCOM3->I2CM.CTRLB.reg = (0 << SERCOM_I2CM_CTRLB_ACKACT_Pos) | SERCOM_I2CM_CTRLB_CMD(2);
			while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);
		} else {
			// we're done receiving, so NACK and STOP.
			SERCOM3->I2CM.CTRLB.reg = (1 << SERCOM_I2CM_CTRLB_ACKACT_Pos) | SERCOM_I2CM_CTRLB_CMD(3);
			while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);
		}
	}

	
	return true;
	
	
rtc_read_fail:
	
	SERCOM3->I2CM.CTRLB.reg = (1 << SERCOM_I2CM_CTRLB_ACKACT_Pos) | SERCOM_I2CM_CTRLB_CMD(3);
	while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);
	
	return false;
}

// rtc_write(addr, *data, count)
// writes data to RTC

bool rtc_write(uint8_t addr, uint8_t *data, uint8_t count) {
	
	bool retval = true;

	SERCOM3->I2CM.ADDR.reg = RTC_ADDRESS_WRITE;
	while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);
	while(SERCOM3->I2CM.INTFLAG.bit.MB == 0);
	if (SERCOM3->I2CM.STATUS.reg & (SERCOM_I2CM_STATUS_RXNACK|SERCOM_I2CM_STATUS_BUSERR|SERCOM_I2CM_STATUS_ARBLOST)) {
		retval = false;
		goto rtc_write_fail;
	}
	
	// write register address
	SERCOM3->I2CM.DATA.reg = addr;
	while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);
	while(SERCOM3->I2CM.INTFLAG.bit.MB == 0);
	if (SERCOM3->I2CM.STATUS.reg & (SERCOM_I2CM_STATUS_RXNACK|SERCOM_I2CM_STATUS_BUSERR|SERCOM_I2CM_STATUS_ARBLOST)) {
		retval = false;
		goto rtc_write_fail;
	}
	
	// write data
	while(count--) {
		SERCOM3->I2CM.DATA.reg = *data++;
		while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);
		while(SERCOM3->I2CM.INTFLAG.bit.MB == 0);
		
		if (SERCOM3->I2CM.STATUS.reg & (SERCOM_I2CM_STATUS_RXNACK|SERCOM_I2CM_STATUS_BUSERR|SERCOM_I2CM_STATUS_ARBLOST)) {
			retval = false;
			break;
		}
	}

rtc_write_fail:

	// issue STOP
	SERCOM3->I2CM.CTRLB.reg = SERCOM_I2CM_CTRLB_CMD(3);
	while(SERCOM3->I2CM.STATUS.bit.SYNCBUSY);
	
	return retval;
}


void rtc_get_time(struct time_struct *ts) {
	
	// read time from RTC
	rtc_read(0,(uint8_t *) &rtc_data.rtc,7);
	
	ts->seconds = (((rtc_data.rtc.rtc_sc & 0x70) >> 4) * 10) + (rtc_data.rtc.rtc_sc & 0x0F);
	ts->minutes = rtc_data.rtc.rtc_mn & 0x0F;
	ts->tenminutes = (rtc_data.rtc.rtc_mn & 0xF0) >> 4;
	ts->hours = rtc_data.rtc.rtc_hr & 0x0F;
	ts->tenhours = (rtc_data.rtc.rtc_hr & 0xF0) >> 4;
}

void rtc_set_time(struct time_struct *ts) {
	
	uint8_t seconds_tmp = ts->seconds;
	
	// convert seconds to BCD
	rtc_data.rtc.rtc_sc = 0;
	while(seconds_tmp >= 10) {
		rtc_data.rtc.rtc_sc += 0x10;
		seconds_tmp -= 10;
	}
	rtc_data.rtc.rtc_sc += seconds_tmp;
	
	rtc_data.rtc.rtc_mn = (ts->tenminutes << 4) + ts->minutes;
	rtc_data.rtc.rtc_hr = (ts->tenhours << 4) + ts->hours;
	
	rtc_write(0,(uint8_t *) &rtc_data.rtc,7);
}
