/**
  ******************************************************************************
  * @file    board_config.h
  * @brief   Board facts, and the values you must look up.
  *
  * TODO 0 - START HERE.
  *
  * Values marked TODO have to be FOUND in the document named next to them.
  * The handout is explicit: "Where a task asks for a register field,
  * peripheral address, alternate function, command value, timing requirement,
  * or status bit, do not guess it."
  *
  * The project compiles with the placeholders, but the board will not work
  * until they are correct. That is deliberate.
  *
  * Two things below are GIVEN rather than left as TODOs: the SPI pin mapping
  * and the EEPROM address format. The handout is wrong about both on this
  * board, so you could not find the right answer in your documents. README
  * section 5 explains how to confirm them on the bench - do that.
  ******************************************************************************
  */

#ifndef __BOARD_CONFIG_H
#define __BOARD_CONFIG_H

#include "stm32f0xx.h"

/* ==========================================================================
 * 1. CLOCK
 * ========================================================================== */

/* TODO 2.1  DONE.
 *   main.c SystemClock_Config(): SYSCLK = HSI, no PLL, AHB /1, APB1 /1.
 *   RM0091 7.2.2 (HSI clock): HSI is an internal 8 MHz RC oscillator.
 *   RM0091 7.2, Figure 11 (clock tree): SPI2 is clocked by PCLK.
 *   PCLK = 8 MHz / 1 / 1 = 8 MHz. */
#define PCLK1_HZ                8000000UL

/* ==========================================================================
 * 2. SPI PINS - GIVEN (see README section 5, and verify by continuity)
 * ========================================================================== */
#define EE_SPI_GPIO             GPIOB
#define EE_PIN_CS               12u         /* EEPROM pin 1 (CS#)           */
#define EE_PIN_SCK              13u         /* EEPROM pin 6 (SCK)           */
#define EE_PIN_MISO             14u         /* EEPROM pin 2 (SO)            */
#define EE_PIN_MOSI             15u         /* EEPROM pin 5 (SI)            */
#define EE_CS_MASK              (1UL << EE_PIN_CS)

/* TODO 2.2  DONE.
 *   STM32F051 datasheet Table 15 (Alternate functions selected through
 *   GPIOB_AFR registers for port B):
 *     PB13 AF0 = SPI2_SCK, PB14 AF0 = SPI2_MISO, PB15 AF0 = SPI2_MOSI.
 *   RM0091 Table 1 (peripheral register boundary addresses):
 *     SPI2 = 0x4000 3800 - 0x4000 3BFF. */
#define EE_SPI                  ((SPI_TypeDef *)0x40003800UL) /* SPI2       */
#define EE_SPI_AF               0u          /* AF0                          */

/* ==========================================================================
 * 3. SPI BAUD RATE
 * ========================================================================== */

/* TODO 2.3  DONE.
 *   RM0091 27.7.1 SPIx_CR1 BR[2:0]: fSCK = fPCLK / 2^(BR+1).
 *   8 000 000 / 250 000 = 32 = 2^5  ->  BR = 4 (100b).
 *   Predicted fSCK = 8 000 000 >> 5 = 250 000 Hz. */
#define EE_SPI_BR               4UL         /* BR[2:0] = 100 -> fPCLK/32    */
#define EE_SCK_HZ_PREDICTED     (PCLK1_HZ >> (EE_SPI_BR + 1U))

/* ==========================================================================
 * 4. EEPROM
 * ========================================================================== */

/* GIVEN (see README section 5): the fitted part is addressed differently
 * from the CAT25040 named in the handout. Confirm both values on the bench. */
#define EEPROM_ADDR_BYTES       2u          /* address bytes, MSB first     */
#define EEPROM_SIZE_BYTES       8192u

/* TODO 4.1  Instruction opcodes, from the EEPROM datasheet's instruction set
 *           table. */
#define EEPROM_CMD_WREN         0x06u       /* <- TODO */
#define EEPROM_CMD_WRDI         0x04u       /* <- TODO */
#define EEPROM_CMD_RDSR         0x05u       /* <- TODO */
#define EEPROM_CMD_WRSR         0x01u       /* <- TODO */
#define EEPROM_CMD_READ         0x03u       /* <- TODO */
#define EEPROM_CMD_WRITE        0x02u       /* <- TODO */

/* TODO 4.2  Status register bit MASKS, from the datasheet's status register
 *           table. Which bit says a write is in progress - and is it 1 or 0
 *           while the device is busy? Which bit is the write enable latch? */
#define EEPROM_SR_RDY           0x01u       /* <- TODO: busy bit mask       */
#define EEPROM_SR_WEL           0x02u       /* <- TODO                      */

/* Upper bound on waiting for a write. A hang guard only: completion must be
 * decided from the status register, never from elapsed time. */
#define EEPROM_WRITE_TIMEOUT_MS 50u

/* ==========================================================================
 * 5. BOARD I/O - GIVEN (UCT board: Board.md)
 * ========================================================================== */
#define LED_BYTE_GPIO           GPIOB
#define LED_BYTE_MASK           0x00FFu     /* PB0..PB7                     */
#define LED_RED_PIN             10u         /* PB10                         */
#define LED_GREEN_PIN           11u         /* PB11                         */

#define BTN_GPIO                GPIOA
#define BTN_START_PIN           0u          /* PA0 / SW0, active low        */
#define BTN_ABORT_PIN           3u          /* PA3 / SW3, active low        */

#define SCOPE_GPIO              GPIOC
#define SCOPE_PIN               13u         /* PC13, header P1              */

/* ==========================================================================
 * 6. GROUP VALUES
 * ========================================================================== */

/* TODO 3.1  The last three decimal digits of each student number.
 *           Write them WITHOUT leading zeros: in C, 010 is octal, i.e. 8.
 *           Then work out B and A by hand for your report, and check them
 *           against the build (TODO 3.2 in main.c). */
#define STUDENT_N1              12u         /* HNDJOS012 */
#define STUDENT_N2              3u          /* TFFTEB003 */

/* The formulas from the handout. */
#define TEST_BYTE_B_RAW         ((((STUDENT_N1 ^ STUDENT_N2) + 0x3Du)) % 256u)
#define TEST_BYTE_B             (((TEST_BYTE_B_RAW == 0x00u) ||   \
                                  (TEST_BYTE_B_RAW == 0xFFu))     \
                                 ? (TEST_BYTE_B_RAW ^ 0x5Au)      \
                                 :  TEST_BYTE_B_RAW)
#define EEPROM_ADDR_A           ((STUDENT_N1 + 3u * STUDENT_N2) % 256u)

/* ==========================================================================
 * Helpers for fields that are 2 bits per pin (MODER, OSPEEDR, PUPDR) and
 * 4 bits per pin (AFRH, pins 8..15). They only do the bit positions - you
 * still have to know what value goes in each field.
 * ========================================================================== */
#define MODER2(pin, val)        ((uint32_t)(val) << ((pin) * 2u))
#define MODER2_MASK(pin)        (3UL << ((pin) * 2u))
#define AFRH4(pin, af)          ((uint32_t)(af) << (((pin) - 8u) * 4u))
#define AFRH4_MASK(pin)         (0xFUL << (((pin) - 8u) * 4u))

#endif /* __BOARD_CONFIG_H */
