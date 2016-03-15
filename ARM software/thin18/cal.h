/*
 * cal.h
 *
 * Created: 2/4/2016 10:08:44 AM
 *  Author: gmarsh
 */ 


#ifndef CAL_H_
#define CAL_H_

void cal_update(void);
void cal_init(void);
bool cal_fetch(uint32_t *value);
bool cal_ready(void);
void cal_input_select(bool use_cal_input);

#endif /* CAL_H_ */