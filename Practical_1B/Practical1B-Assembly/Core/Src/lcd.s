/*
 * lcd.s
 * EEE3096S 2026 - Practical 1B, Task 5
 * 4-bit bit-banged HD44780 driver, and the level shifter timing fault
 *
 * Student 1 : Joshua Handyside  HNDJOS012
 * Student 2 : Tebogo Teffo  TFFTEB003
 */

    .syntax unified
    .thumb
    .cpu    cortex-m0
    .fpu    softvfp

    .global LCD_Run
    .type   LCD_Run, %function

@ ---------------------------------------------------------------------------
@ Register addresses. BSRR is at offset 0x18 from each port base.
@ ---------------------------------------------------------------------------
    .equ GPIOA_BSRR, 0x48000018
    .equ GPIOB_BSRR, 0x48000418
    .equ GPIOC_BSRR, 0x48000818

@ ------------------------------------------------------------------------a---
@ PIN MAP
@   PC15  Enable (E)     -> PC15_S on the 5 V side
@   PC14  Register Select (RS)
@   PB8   D4      PB9   D5      PA12  D6      PA15  D7
@   R/W is tied to ground. The LCD is write only. 
@ ---------------------------------------------------------------------------

    .section .text.LCD_Run, "ax", %progbits

@ ===========================================================================
@ ENTRY POINT
@ ===========================================================================
LCD_Run:
    PUSH {LR}

    @ wait for the LCD power rail to settle
    MOVS R0, #50
    BL LCD_DelayLong
    
    @ call the 4-bit initialization sequence.
    BL LCD_Init
    
    @ write the character 'A' (0x41) to the display.
    MOVS R0, #0x41
    BL LCD_WriteData

hang:
    B    hang

    .size LCD_Run, .-LCD_Run

@ ===========================================================================
@ LCD_Init
@ Puts the controller into 4-bit mode and readies the display.
@ ===========================================================================
    .type LCD_Init, %function
LCD_Init:
    PUSH {LR}

    @ send the 4-bit initialization sequence.
    @ referenced from the HD44780 datasheet flowchart
    @ send commands with RS low using LCD_WriteCmd

	LDR R0 =GPIOC_BSRR
	LDR R1, = 0x40000000 @ reset RS
	STR R1, [R0]

	MOVS R0, 0x03
	BL LCD_SendNibble @ first wakeup command
	MOVS R0, #5
	BL LCD_DelayLong @ wait for more than 4.1ms

	MOVS R0, #0x03
	BL LCD_SendNubble @ second wakeup command
	BL LCD_Pulse
	MOVS R0, #150
	BL LCD_DelayShort @ wait for more than 100us

	MOVS R0, #0x03
	BL LCD_SendNubble @ third wakeup command
	BL LCD_Pulse
	MOVS R0, #100
	BL LCD_DelayShort

	MOVS R0, #0x02
	BL LCD_SendNibble @ switching to 4 bit mode
	BL LCD_Pulse
	MOVS R0, #100
	BL LCD_DelayShort

	MOVS R0, #0x28
	BL LCD_WriteCmd
	MOVS R0, #0x08 @ turn off display
	BL LCD_WriteCmd
	MOVS R0, #0x01 @ clear display
	BL LCD_WriteCmd
	MOVS R0, #2 @ wait more than 1.52ms for clear
	BL LCD_DelayLong
	MOVS R0, #0x06 @ set entry mode to increment cursor
	BL LCD_WriteCmd
	MOVS R0, #0x0C @ turn on display and turn off cursor
	BL LCD_WriteCmd

    POP {PC}

@ ===========================================================================
@ LCD_WriteCmd   R0 = command byte, RS low
@ LCD_WriteData  R0 = data byte,    RS high
@ Both send the high nibble first, then the low nibble.
@ ===========================================================================
    .type LCD_WriteCmd, %function
LCD_WriteCmd:
    PUSH {R0, LR}
    @ drive RS (PC14) LOW, then fall through to the shared sender
    LDR R1, =GPIOC_BSRR
    LDR R2, =0x44000000 @ set RS low
    STR R2, [R1]
    B LCD_Send8

    .type LCD_WriteData, %function
LCD_WriteData:
    PUSH {R0, LR}
    @ drive RS (PC14) HIGH, then fall through.
    LDR R1, =GPIOC_BSRR
    LDR R2, [R1] @ set RS high
    STR R2, [R1]

LCD_Send8:
    @ send the upper nibble of R0, pulse Enable,
    @ then the lower nibble of R0, pulse Enable again
    MOV R4, R0 @ save the original byte

	LSRS R0, R4, #4 @shift bits down
	BL LCD_SendNibble
	BL LCD_Pulse

	MOV R0, R4 @ put back original byte
	BL LCD_SendNibble
	BL LCD_Pulse

	MOVS R0, #50 @ standard time delay
	BL LCD_DelayShort

    POP {R0, PC}

@ ===========================================================================
@ LCD_SendNibble   R0 bits 3:0 -> the four data lines
@ ===========================================================================
    .type LCD_SendNibble, %function
LCD_SendNibble:
    PUSH {R1, R2, R3, LR}

    @ map the four bits of R0 onto the four data pins (across 3 ports)
    LDR R2, =GPIOA_BSRR
    LDR R3, =GPIOB_BSRR

    @   R0 bit 0 -> PB8   (D4)
    LSRS R4, R0, #1 @ send bit 0 to carry flag
    BCC clr_d4
    LDR R1, =0x00000100 @ set PB8
    B wr_d4
clr_d4:
	LDR R1, =0x01000000 @ restet PB8
wr_d4:
	STR R1, [R3]

    @   R0 bit 1 -> PB9   (D5)
    LSRS R4, R0, #2 @ send bit 1 to carry flag
    BCC clr_d5
    LDR R1, =0x00000200 @ set PB9
    B wr_d5
clr_d5:
	LDR R1, =0x02000000 @ reset PB9
wr_d5:
	STR R1, [R3]

    @   R0 bit 2 -> PA12  (D6)
    LSRS R4, R0, #3 @ send bit 2 to carry flag
    BCC clr_d6
    LDR R1, =0x00001000 @ set PA12
    B wr_d6
clr_d6:
	LDR R1, =0x10000000 @ reset PA12
wr_d6:
	STR R1, [R2]

    @   R0 bit 3 -> PA15  (D7)
    LSRS R4, R0, #4 @ send bit 3 to carry flag
    BCC clr_d7
    LDR R1, =0x00008000 @ set PA15
    B wr_d7
clr_d7:
	LDR R1, =0x80000000 @ reset PA15
wr_d7:
	STR R1, [R2]

    POP {R1, R2, R3, PC}

@ ===========================================================================
@ LCD_Pulse
@ ===========================================================================
    .type LCD_Pulse, %function
LCD_Pulse:
    PUSH {R0, R1, R2, LR}

    LDR  R0, =GPIOC_BSRR

    @ set PC15 HIGH.
    LDR R1, =0x00008000
    STR R1, [R0] @ 2 cycles

    @ -----------------------------------------------------------------
    @ TIMING FIX:
    @ implement a calculated pad delay here to overcome the RC time
    @ constant of the level shifter and meet the HD44780 hold time requirements
    @ Show your cycle arithmetic in the comments.
    @
    @ Currently has 20 NOP commands for testing, will replace with calculated value
    @ -----------------------------------------------------------------
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP
    NOP


    @ set PC15 LOW
    LDR R1, =0x80000000
    STR R1, [R0] @ 2 cycles

    @ hold Enable low long enough to meet the LCD cycle time
	MOVS R0, #10
	BL LCD_DelayShort

    POP {R0, R1, R2, PC}

@ ===========================================================================
@ Delay helpers
@ ===========================================================================
    .type LCD_DelayLong, %function
LCD_DelayLong:
	@ 1ms = 8000 cycles
	@ each lööp takes 4 cycles, therefore 2000 lööps per ms
	LDR R1, =2000
	MULS R1, R0, R1
DelayLongLoop:
	SUBS R1, #1 @ 1 cycle
	BNE DelayLongLoop @ 3 cycles
    BX   LR

    .type LCD_DelayShort, %function
LCD_DelayShort:
	@ 1us = 8 cycles
    @ each lööp takes 4 cycles, therefore 2 lööps per us
    LDR R1, =2
    MULS R1, R0, R1
DelayShortLoop:
	SUBS R1, #1 @ 1 cycle
	BNE DelayShortLoop @ 3 cycles
    BX   LR
