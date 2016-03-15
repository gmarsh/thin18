/*
 * rtc.h
 *
 * Created: 2016-01-07 10:26:44 PM
 *  Author: Gary
 */ 


#ifndef RTC_H_
#define RTC_H_

#include <stdint.h>
#include <stdbool.h>
#include "display.h"

// RTC structure

struct rtc_rtc_struct {
	uint8_t rtc_sc;
	uint8_t rtc_mn;
	uint8_t rtc_hr;
	uint8_t rtc_dt;
	uint8_t rtc_mo;
	uint8_t rtc_yr;
	uint8_t rtc_dw;
};

struct rtc_csr_struct {
	uint8_t csr_sr;
	uint8_t csr_int;
	uint8_t csr_pwr_vdd;
	uint8_t csr_pwr_vbat;
	uint8_t csr_itr0;
	uint8_t csr_alpha;
	uint8_t csr_beta;
	uint8_t csr_fatr;
	uint8_t csr_fdtr;
};

struct rtc_alarm_struct {
	uint8_t alarm_sca0;
	uint8_t alarm_mna0;
	uint8_t alarm_hra0;
	uint8_t alarm_dta0;
	uint8_t alarm_moa0;
	uint8_t alarm_dwa0;
	};

struct rtc_tsv2b_struct {
	uint8_t tsv2b_vsc;
	uint8_t tsv2b_vmn;
	uint8_t tsv2b_vhr;
	uint8_t tsv2b_vdt;
	uint8_t tsv2b_vmo;
	};

struct rtc_tsb2v_struct {
	uint8_t tsb2v_bsc;
	uint8_t tsb2v_bmn;
	uint8_t tsb2v_bhr;
	uint8_t tsb2v_bdt;
	uint8_t tsb2v_bmo;
};

struct rtc_dstcr_struct {
	uint8_t dstcr_DstMoFd;
	uint8_t dstcr_DstDwFd;
	uint8_t dstcr_DstDtFd;
	uint8_t dstcr_DstHrFd;
	uint8_t dstcr_DstMoRv;
	uint8_t dstcr_DstDwRv;
	uint8_t dstcr_DstDtRv;
	uint8_t dstcr_DstHrRv;
};

struct rtc_temp_struct {
	uint8_t temp_tk0l;
	uint8_t temp_tk0h;
};

struct rtc_nppm_struct {
	uint8_t nppm_nppml;
	uint8_t nppm_nppmh;
};
	
struct rtc_xt0_struct {
	uint8_t xt0_xt0;
};

struct rtc_alphah_struct {
	uint8_t alphah_alphah;
};

struct rtc_gpm_struct {
	uint8_t gpm_gpm1;
	uint8_t gpm_gpm2;
	};

struct rtc_data_struct {
	struct rtc_rtc_struct rtc;
	struct rtc_csr_struct csr;
	struct rtc_alarm_struct alarm;
	struct rtc_tsv2b_struct tsv2b;
	struct rtc_tsb2v_struct tsb2v;
	struct rtc_dstcr_struct dstcr;
	struct rtc_temp_struct temp;
	struct rtc_nppm_struct nppm;
	struct rtc_xt0_struct xt0;
	struct rtc_alphah_struct alphah;
	struct rtc_gpm_struct gpm;
};




void rtc_init(void);
bool rtc_write(uint8_t addr, uint8_t *data, uint8_t count);
bool rtc_read(uint8_t addr, uint8_t *data, uint8_t count);

void rtc_get_time(struct time_struct *ts);
void rtc_set_time(struct time_struct *ts);

#endif /* RTC_H_ */