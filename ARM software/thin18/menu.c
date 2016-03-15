/*
 * menu.c
 *
 * Created: 2/4/2016 12:13:39 PM
 *  Author: gmarsh
 */ 

 #include <stdio.h>
 #include <stdbool.h>
 #include <string.h>
 #include <stdlib.h>
 #include <samd20.h>
 #include "cal.h"
 #include "console.h"
 #include "display.h"
 #include "rtc.h"
 #include "menu.h"
 
extern struct display_struct display;



 void menu(void) {
 
	uint8_t str[64];
	uint8_t rtc_address;
	uint8_t rtc_data;

	uint32_t cal_value;


	while(1) {
		
		console_puts((uint8_t *) "\r\n\r\n== thIN-18 debug menu ==\r\n\r\n");

		console_puts((uint8_t *) "C) Test calibration\r\n");
		console_puts((uint8_t* ) "S) Select calibration source\r\n");
		console_puts((uint8_t *) "E) Enable/disable HV DC/DC converter\r\n");
		console_puts((uint8_t *) "R) Read RTC register\r\n");
		console_puts((uint8_t *) "W) Write RTC register\r\n");
		
		console_puts((uint8_t *) "1) Set 10H tube\r\n");
		console_puts((uint8_t *) "2) Set 1H tube\r\n");
		console_puts((uint8_t *) "3) Set 10M tube\r\n");
		console_puts((uint8_t *) "4) Set 1M tube\r\n");
	
		console_puts((uint8_t *) "\r\n>> ");

		if (console_gets(str,63) == 0) continue;

		console_puts((uint8_t *) "\r\n");

		switch(str[0]) {
			
			case '1':
				console_puts((uint8_t *) "Digit: ");
				if (console_gets(str,2)) {
					display.displaytime.tenhours = (uint8_t) strtoul((char *) str,NULL,10);
				}
			break;
			
			case '2':
				console_puts((uint8_t *) "Digit: ");
				if (console_gets(str,2)) {
					display.displaytime.hours = (uint8_t) strtoul((char *) str,NULL,10);
				}
			break;
			
			case '3':
				console_puts((uint8_t *) "Digit: ");
				if (console_gets(str,2)) {
					display.displaytime.tenminutes = (uint8_t) strtoul((char *) str,NULL,10);
				}
			break;
			
			case '4':
				console_puts((uint8_t *) "Digit: ");
				if (console_gets(str,2)) {
					display.displaytime.minutes = (uint8_t) strtoul((char *) str,NULL,10);
				}
			break;
			
			case 's':
			case 'S':
				console_puts((uint8_t *) "Source? 1=ext, 0=OSC8M: ");
				if (console_gets(str,2)) {
					if (str[0] == '0') {
						cal_input_select(false);
					}
					if (str[0] == '1') {
						cal_input_select(true);
					}
				}
			break;
			
			case 'c':
			case 'C':

				if (!cal_ready()) {
					console_puts((uint8_t *) "Cal not ready.");
					break;
				}
				
				while(1) {
					if (cal_fetch(&cal_value)) {
						itoa(cal_value,(char *) str,10);
						console_puts(str);
						console_puts((uint8_t *) "\r\n");
					}
					if (console_getc(&str[0])) break;
				}

			break;


			case 'e':
			case 'E':
				while(1) {
					console_puts((uint8_t *) "\r\nEnable? [1,0]: ");
					if (console_gets(str,2) == 0) break;
					if (str[0] == '1') PORT->Group[0].OUTSET.reg = (1<<6);
					else if (str[0] == '0') PORT->Group[0].OUTCLR.reg = (1<<6);
				}
			break;

			case 'r':
			case 'R':
				console_puts((uint8_t *) "Address: 0x");
				if (console_gets(str,63) == 0) break;
				rtc_address = (uint8_t) strtol((char *) str, NULL, 16);
				rtc_read(rtc_address,&rtc_data,1);
				console_puts((uint8_t *) "\r\nData: 0x");
				itoa(rtc_data,(char *) str,16); console_puts(str);
			break;

			case 'w':
			case 'W':
				console_puts((uint8_t *) "Address: 0x");
				if (console_gets(str,63) == 0) break;
				rtc_address = (uint8_t) strtol((char *) str, NULL, 16);
				console_puts((uint8_t *) "\r\nData: 0x");
				if (console_gets(str,63) == 0) break;
				rtc_data = (uint8_t) strtol((char *) str, NULL, 16);
				rtc_write(rtc_address,&rtc_data,1);
			break;

		}

	}

 }
