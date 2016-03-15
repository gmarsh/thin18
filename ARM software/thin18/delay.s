
/*
 * delay.s
 *
 * Created: 2016-02-06 10:04:36 PM
 *  Author: Gary
 */ 

	.syntax unified
	.section .text
	.align 2
	.thumb
	.thumb_func

	.global delay_ms
	.type delay_ms function
	.global delay_us
	.type delay_us function

delay_ms:
	
	ldr r1,=1000
	muls r0,r0,r1

delay_us:
	ldr r1,=2		// CPU frequency / 4
	muls r0,r0,r1

	// test for zero and exit
	subs r0,#0
	beq _delay_done

_delay_loop:
	subs r0,#1			// 1 clock
	bne _delay_loop		// 3 clocks

_delay_done:   
   bx lr  
