/*
 * console.c
 *
 * Created: 6/9/2015 9:56:48 PM
 *  Author: Gary
 */ 

#include <stdint.h>
#include <stdbool.h>
#include <samd20.h>
#include "console.h"

// RX/TX buffers
#define UART_CONSOLE_TX_FIFO_SIZE 64
volatile uint16_t uart_console_tx_fifo[UART_CONSOLE_TX_FIFO_SIZE];
volatile uint16_t uart_console_tx_fifo_readptr;
volatile uint16_t uart_console_tx_fifo_writeptr;
#define UART_CONSOLE_TX_FIFO_MASK (UART_CONSOLE_TX_FIFO_SIZE-1)
#define UART_CONSOLE_TX_FIFO_EMPTY (uart_console_tx_fifo_readptr == uart_console_tx_fifo_writeptr)
#define UART_CONSOLE_TX_FIFO_FULL (((uart_console_tx_fifo_readptr - uart_console_tx_fifo_writeptr) & UART_CONSOLE_TX_FIFO_MASK) == 1)

#define UART_CONSOLE_RX_FIFO_SIZE 64
volatile uint16_t uart_console_rx_fifo[UART_CONSOLE_RX_FIFO_SIZE];
volatile uint16_t uart_console_rx_fifo_readptr;
volatile uint16_t uart_console_rx_fifo_writeptr;
#define UART_CONSOLE_RX_FIFO_MASK (UART_CONSOLE_RX_FIFO_SIZE-1)
#define UART_RX_FIFO_EMPTY (uart_console_rx_fifo_readptr == uart_console_rx_fifo_writeptr)
#define UART_RX_FIFO_FULL (((uart_console_rx_fifo_readptr - uart_console_rx_fifo_writeptr) & UART_CONSOLE_RX_FIFO_MASK) == 1)


// interrupt handler
void SERCOM2_Handler(void) {
	
	// transmit side - check DRE flag
	if (SERCOM2->USART.INTFLAG.bit.DRE) {
		
		// if there's a byte available in the transmit FIFO, fire it at the UART
		if (!UART_CONSOLE_TX_FIFO_EMPTY) {
			SERCOM2->USART.DATA.reg = uart_console_tx_fifo[uart_console_tx_fifo_readptr];
			uart_console_tx_fifo_readptr = (uart_console_tx_fifo_readptr + 1) & UART_CONSOLE_TX_FIFO_MASK;
		}
		// if the TX FIFO is empty, disable the DRE interrupt
		if (UART_CONSOLE_TX_FIFO_EMPTY) {
			SERCOM2->USART.INTENCLR.reg = SERCOM_USART_INTENCLR_DRE;
		}
		
	}
	
	// receive side - check RXRDY interrupt flag
	if (SERCOM2->USART.INTFLAG.bit.RXC) {
		uint8_t byte = SERCOM2->USART.DATA.reg;
		if (!UART_RX_FIFO_FULL) {
			uart_console_rx_fifo[uart_console_rx_fifo_writeptr] = byte;
			uart_console_rx_fifo_writeptr = (uart_console_rx_fifo_writeptr + 1) & UART_CONSOLE_RX_FIFO_MASK;
		}
	}
}

void console_init(void) {
	
	// initialize FIFO pointers
	uart_console_tx_fifo_readptr = 0;
	uart_console_tx_fifo_writeptr = 0;
	uart_console_rx_fifo_readptr = 0;
	uart_console_rx_fifo_writeptr = 0;
	
	// set GPIO muxing
	// transmit on PA14 / SERCOM2[2] (func C)
	// receive on PA15 / SERCOM2[3] (func C)
	PORT->Group[0].PMUX[15/2].bit.PMUXO = PORT_PMUX_PMUXO_C_Val;
	PORT->Group[0].PMUX[14/2].bit.PMUXE = PORT_PMUX_PMUXE_C_Val;
	PORT->Group[0].PINCFG[14].bit.PMUXEN = 1;
	PORT->Group[0].PINCFG[15].bit.PMUXEN = 1;
	
	// enable SERCOM3 clocks
	// GCLK0 is 8MHz clock, GCLK2 is 32768Hz clock
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM2_GCLK_ID_CORE) | GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN(0);
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM2_GCLK_ID_SLOW) | GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN(2);
	PM->APBCMASK.reg |= PM_APBCMASK_SERCOM2;
	
	// CTRLA
	SERCOM2->USART.CTRLA.bit.ENABLE = 0;
	SERCOM2->USART.CTRLA.reg = \
		(1<<SERCOM_USART_CTRLA_DORD_Pos) |	// LSB first data transmission
		(0<<SERCOM_USART_CTRLA_CPOL_Pos) |	// meaningless (not in asynchronous mode)
		(0<<SERCOM_USART_CTRLA_CMODE_Pos) |	// asynchronous communication mode
		SERCOM_USART_CTRLA_FORM_0 |			// USART framing
		SERCOM_USART_CTRLA_RXPO_PAD3 |		// RX on pad 0
		SERCOM_USART_CTRLA_TXPO_PAD2 |		// TX on pad 2
		SERCOM_USART_CTRLA_MODE_USART_INT_CLK;
		
	// 57600bps @ 8MHz core clock
	SERCOM2->USART.BAUD.reg = 57986;
	
	// enable RX/TX
	SERCOM2->USART.CTRLB.reg = SERCOM_USART_CTRLB_CHSIZE(0) |
		SERCOM_USART_CTRLB_RXEN |
		SERCOM_USART_CTRLB_TXEN;
		
	// enable receive interrupt
	SERCOM2->USART.INTENSET.bit.RXC = 1;
	
	// finally enable UART
	SERCOM2->USART.CTRLA.bit.ENABLE = 1;
	
	NVIC_SetPriority(SERCOM2_IRQn,1);
	NVIC_EnableIRQ(SERCOM2_IRQn);
}


bool console_getc(uint8_t *byte) {
	
	if (!UART_RX_FIFO_EMPTY) {
		*byte = uart_console_rx_fifo[uart_console_rx_fifo_readptr];
		uart_console_rx_fifo_readptr = (uart_console_rx_fifo_readptr + 1) & UART_CONSOLE_RX_FIFO_MASK;
		return true;
	}
	return false;
}


void console_putc(uint8_t byte) {
	
	// wait for room in TX FIFO
	while(UART_CONSOLE_TX_FIFO_FULL);
	
	// stuff byte in FIFO and enable transmit interrupt
	uart_console_tx_fifo[uart_console_tx_fifo_writeptr] = byte;
	uart_console_tx_fifo_writeptr = (uart_console_tx_fifo_writeptr + 1) & UART_CONSOLE_TX_FIFO_MASK;
	SERCOM2->USART.INTENSET.reg = SERCOM_USART_INTENSET_DRE;
}

void console_puts(uint8_t *str) {
	while(*str) {
		console_putc((uint8_t) *str);
		str++;
	}
}

uint16_t console_gets(uint8_t *str, uint16_t maxlen) {
	
	uint8_t byte;
	uint16_t length = 0;
	
	// leave space for terminating NULL
	if (maxlen == 0) return 0;
	maxlen--;
	
	while(1) {
		
		while(!console_getc(&byte));
		
		if ((byte == 13)||(byte == 10)) {
			
			*str = 0;
			return length;
			
			} else if ((byte==127)||(byte == 8)) {
			
				if (length) {
					length--;
					str--;
					maxlen++;
					console_putc(byte);
				}
			
			} else {
			
			if (maxlen) {
				maxlen--;
				*str = (char) byte;
				str++;
				length++;
				console_putc(byte);
			}
			
		}
		
	} // end of while(1)
	
}
