/*
 * console.h
 *
 * Created: 2016-01-05 1:51:12 AM
 *  Author: Gary
 */ 


#ifndef CONSOLE_H_
#define CONSOLE_H_

#include <stdint.h>
#include <stdbool.h>


void console_init(void);
bool console_getc(uint8_t *byte);
void console_putc(uint8_t byte);
void console_puts(uint8_t *str);
uint16_t console_gets(uint8_t *str, uint16_t maxlen);

//void SERCOM2_Handler(void);


#endif /* CONSOLE_H_ */