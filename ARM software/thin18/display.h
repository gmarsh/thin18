/*
 * display.h
 *
 * Created: 2016-01-06 9:24:28 PM
 *  Author: Gary
 */ 


#ifndef DISPLAY_H_
#define DISPLAY_H_

#define BUTTON_DEBOUNCE_COUNT	5
#define RTC_HANDOFF_DELAY		(256*5)


// time structure for reading/updating time
struct time_struct {
	uint8_t seconds;
	uint8_t minutes;
	uint8_t tenminutes;
	uint8_t hours;
	uint8_t tenhours;
};


struct display_struct {
	
	// button debouncing
	uint8_t minute_debounce_counter;
	bool minute_pushed;
	uint8_t hour_debounce_counter;
	bool hour_pushed;
	
	// current lit digit
	uint8_t active_digit;
	
	// previous INT sense from RTC chip
	bool prev_int_sense;
	
	// current displayed/counted time
	struct time_struct displaytime;
	
	// handoff counter - when this counts to zero, current time is written to RTC
	uint16_t rtc_handoff_counter;
	// time to write to RTC
	struct time_struct rtc_handoff_time;
	
};



void display_init(void);
void display_update(void);
void display_settime(struct time_struct *time_to_set);
void display_gettime(struct time_struct *time_to_get);

#endif /* DISPLAY_H_ */