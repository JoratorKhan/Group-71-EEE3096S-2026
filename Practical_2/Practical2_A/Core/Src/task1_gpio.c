/**
  ******************************************************************************
  * @file    task1_gpio.c
  * @brief   TASK 1 : MEMORY-MAPPED GPIO ACCESS
  *
  * Drive PC13 as a digital output using EXPLICIT volatile register pointers,
  * built from addresses you find in RM0091:
  *
  *     register address = peripheral base address + register offset
  *
  * Do not use HAL_GPIO_Init(), HAL_GPIO_WritePin() or HAL_GPIO_TogglePin().
  * You must be able to show where every base and offset came from.
  ******************************************************************************
  */

#include "prac2a.h"

/* TODO 1.1  Peripheral base addresses: RM0091 section 2.2.2, Table 1. */
#define RCC_BASE_ADDR       0x40021000UL    /* RCC   : 0x4002 1000 - 0x4002 13FF */
#define GPIOC_BASE_ADDR     0x48000800UL    /* GPIOC : 0x4800 0800 - 0x4800 0BFF */

/* TODO 1.2  Register offsets: RM0091 sections 7.4.6 and 9.4. */
#define RCC_AHBENR_OFFSET   0x14UL          /* RM0091 7.4.6  */
#define GPIO_MODER_OFFSET   0x00UL          /* RM0091 9.4.1  */
#define GPIO_ODR_OFFSET     0x14UL          /* RM0091 9.4.6  */
#define GPIO_BSRR_OFFSET    0x18UL          /* RM0091 9.4.7  */
#define GPIO_BRR_OFFSET     0x28UL          /* RM0091 9.4.11 */

/* Bit positions */
#define RCC_AHBENR_IOPCEN   (1UL << 19)     /* RM0091 7.4.6: bit 19 IOPCEN */
#define PC13_PIN            13u
#define PC13_MASK           (1UL << PC13_PIN)

/* volatile: every access is a real load/store to the hardware register.
 * The compiler may not cache, merge, reorder or delete these accesses.  */
static volatile uint32_t * const pRCC_AHBENR =
        (volatile uint32_t *)(RCC_BASE_ADDR + RCC_AHBENR_OFFSET);

/* TODO 1.3  GPIOC register pointers */
static volatile uint32_t * const pGPIOC_MODER =
        (volatile uint32_t *)(GPIOC_BASE_ADDR + GPIO_MODER_OFFSET);  /* 0x48000800 */
static volatile uint32_t * const pGPIOC_ODR =
        (volatile uint32_t *)(GPIOC_BASE_ADDR + GPIO_ODR_OFFSET);    /* 0x48000814 */
static volatile uint32_t * const pGPIOC_BSRR =
        (volatile uint32_t *)(GPIOC_BASE_ADDR + GPIO_BSRR_OFFSET);   /* 0x48000818 */
static volatile uint32_t * const pGPIOC_BRR =
        (volatile uint32_t *)(GPIOC_BASE_ADDR + GPIO_BRR_OFFSET);    /* 0x48000828 */

/* TODO 1.4  Compile-time checks against the CMSIS device header. */
_Static_assert(RCC_BASE_ADDR   == RCC_BASE,   "RCC base mismatch");
_Static_assert(GPIOC_BASE_ADDR == GPIOC_BASE, "GPIOC base mismatch");
_Static_assert(RCC_BASE_ADDR + RCC_AHBENR_OFFSET
               == (uint32_t)(uintptr_t)&RCC->AHBENR,  "AHBENR offset");
_Static_assert(GPIOC_BASE_ADDR + GPIO_MODER_OFFSET
               == (uint32_t)(uintptr_t)&GPIOC->MODER, "MODER offset");
_Static_assert(GPIOC_BASE_ADDR + GPIO_ODR_OFFSET
               == (uint32_t)(uintptr_t)&GPIOC->ODR,   "ODR offset");
_Static_assert(GPIOC_BASE_ADDR + GPIO_BSRR_OFFSET
               == (uint32_t)(uintptr_t)&GPIOC->BSRR,  "BSRR offset");
_Static_assert(GPIOC_BASE_ADDR + GPIO_BRR_OFFSET
               == (uint32_t)(uintptr_t)&GPIOC->BRR,   "BRR offset");

volatile uint32_t task1_half_period_ms = 5u;
volatile uint32_t task1_toggle_count   = 0u;

static uint32_t t1_last = 0u;

void task1_gpio_init(void)
{
    /* TODO 1.5  Enable GPIOC clock (read-modify-write keeps other clocks on). */
    *pRCC_AHBENR |= RCC_AHBENR_IOPCEN;

    /* TODO 1.6  PC13 -> general purpose output (MODER13[1:0] = 01).
     *           Bits 27:26. Clear both, then set 01. Other pins untouched. */
    uint32_t moder = *pGPIOC_MODER;
    moder &= ~(3UL << (PC13_PIN * 2u));
    moder |=  (1UL << (PC13_PIN * 2u));
    *pGPIOC_MODER = moder;

    /* TODO 1.7  Known starting level: LOW. */
    *pGPIOC_BRR = PC13_MASK;
}

void task1_gpio_update(uint32_t now)
{
    if ((uint32_t)(now - t1_last) < task1_half_period_ms)
    {
        return;
    }
    t1_last = now;

    /* TODO 1.8  Read present level from ODR, drive the opposite level.
     *           BSRR/BRR used for the write: a single store that changes
     *           only PC13, so no read-modify-write of the whole port and no
     *           risk of overwriting other pins changed in between.        */
    if (*pGPIOC_ODR & PC13_MASK)
    {
        *pGPIOC_BRR = PC13_MASK;     /* currently HIGH -> drive LOW  */
    }
    else
    {
        *pGPIOC_BSRR = PC13_MASK;    /* currently LOW  -> drive HIGH */
    }

    task1_toggle_count++;
}

/* ==========================================================================
 * RUN_TASK 1 - given
 * ========================================================================== */

void task1_setup(void)
{
    task1_gpio_init();
}

void task1_loop(uint32_t now)
{
    task1_gpio_update(now);
}
