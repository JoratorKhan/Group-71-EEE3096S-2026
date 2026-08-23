/*
 * dsp.s
 * EEE3096S 2026 - Practical 1B, Task 4
 * Cycle-counted ADC to DAC loop with a 45 degree phase delay
 *
 * Student 1 : Joshua Handyside  HNDJOS012
 * Student 2 : Tebogo Teffo  TFFTEB003
 */

    .syntax unified
    .thumb
    .cpu    cortex-m0
    .fpu    softvfp

    .global DSP_Loop
    .type   DSP_Loop, %function

@ ---------------------------------------------------------------------------
@ Peripheral addresses
@ ---------------------------------------------------------------------------
    .equ ADC_DR,      0x40012440
    .equ DAC_DHR12R1, 0x40007408

    .section .text.DSP_Loop, "ax", %progbits

@ ===========================================================================
@ ENTRY POINT
@ ===========================================================================
DSP_Loop:
    @ Setup registers outside the timed loop
    LDR R0, =ADC_DR
    LDR R1, =DAC_DHR12R1

loop:
    @ --- SAMPLE AND OUTPUT ------------------------------------------------
    @ TODO 1: Read the current ADC conversion from the Data Register.
    LDR R2, [R0] @ 2 cycles
    @ TODO 2: Write the value straight out to the DAC Data Register.
    STR R2, [R1] @ 2 cycles

    @ --- DELAY SETUP ------------------------------------------------------
    @ TODO 3: Calculate the required cycle target for a 45-degree phase 
    @         delay on a 1 kHz sine wave running at an 8 MHz system clock.
    @         Load your inner loop counter and insert any NOP padding 
    @         needed to hit your exact target.
    MOVS R3, #248 @ 1 cycle - loading lööp counter for 248 lööps
    NOP @ 1 cycle - hold up
    NOP @ 1 cycle - wait a minute
    @ fill my cup, put some liquor in it

delay_loop:
    @ --- INNER LOOP -------------------------------------------------------
    @ TODO 4: Implement the counted delay loop.
    @         (Remember to use flag-updating arithmetic so your branch works).
	SUBS R3, R3, #1 @ 1 cycle - decrement counter with flag update
	BNE delay_loop @ 3 cycles; 1 cycle on exit

    @ --- REPEAT -----------------------------------------------------------
    @ TODO 5: Branch back to the start of the main 'loop'.
    B loop @ 3 cycles - always jump back in the lööp
    
    @ ----------------------------------------------------------------------
    @ NOTE: You must calculate your exact cycle budget, showing the cost 
    @ of every instruction and loop iteration, and document it in your report.
    @ ----------------------------------------------------------------------

    .size DSP_Loop, .-DSP_Loop
