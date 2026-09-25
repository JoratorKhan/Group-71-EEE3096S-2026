/**
  ******************************************************************************
  * @file    task2_spi_config.c
  * @brief   TASK 2 : CONFIGURE THE HARDWARE SPI PERIPHERAL
  *
  * Configure the SPI peripheral entirely through its registers. Not allowed:
  * HAL_SPI_*, LL SPI transfer functions, CubeMX-generated SPI configuration,
  * GPIO bit-banging. Every register write you add should cite the RM0091
  * register it comes from.
  ******************************************************************************
  */

#include "prac2a.h"

/* ---- Register bit positions (RM0091) ----------------------------------- */
/* 7.4.6 RCC_AHBENR */
#define RCC_AHBENR_IOPBEN_BIT   18u
/* 7.4.8 RCC_APB1ENR */
#define RCC_APB1ENR_SPI2EN_BIT  14u

/* 9.4.1 GPIOx_MODER field values */
#define GPIO_MODE_OUT           1u      /* 01: general purpose output       */
#define GPIO_MODE_AF            2u      /* 10: alternate function           */
/* 9.4.3 GPIOx_OSPEEDR field value */
#define GPIO_SPEED_HIGH         3u      /* x1: high speed                   */
/* 9.4.4 GPIOx_PUPDR field value */
#define GPIO_PULL_UP            1u      /* 01: pull-up                      */

/* 27.7.1 SPIx_CR1 */
#define SPI_CR1_CPHA_BIT        0u
#define SPI_CR1_CPOL_BIT        1u
#define SPI_CR1_MSTR_BIT        2u
#define SPI_CR1_BR_POS          3u      /* BR[2:0] = bits 5:3               */
#define SPI_CR1_SPE_BIT         6u
#define SPI_CR1_LSBFIRST_BIT    7u
#define SPI_CR1_SSI_BIT         8u
#define SPI_CR1_SSM_BIT         9u

/* 27.7.2 SPIx_CR2 */
#define SPI_CR2_DS_POS          8u      /* DS[3:0] = bits 11:8              */
#define SPI_CR2_DS_8BIT         7u      /* 0111: 8-bit                      */
#define SPI_CR2_FRXTH_BIT       12u

volatile uint32_t dbg_gpiob_moder      = 0u;
volatile uint32_t dbg_gpiob_afrh       = 0u;
volatile uint32_t dbg_spi_cr1          = 0u;
volatile uint32_t dbg_spi_cr2          = 0u;
volatile uint32_t dbg_spi_sr           = 0u;
volatile uint32_t dbg_sck_hz_predicted = 0u;

void eeprom_spi_init(void)
{
    /* TODO 2.4  Clocks.
     * RM0091 7.4.6 RCC_AHBENR  bit 18 IOPBEN: GPIOB clock enable.
     * RM0091 7.4.8 RCC_APB1ENR bit 14 SPI2EN: SPI2 clock enable. */
    RCC->AHBENR  |= (1UL << RCC_AHBENR_IOPBEN_BIT);
    RCC->APB1ENR |= (1UL << RCC_APB1ENR_SPI2EN_BIT);
    (void)RCC->APB1ENR;     /* read back: clock is on before SPI2 is touched */

    /* TODO 2.5  Chip select PB12 as GPIO output, driven HIGH.
     * Order: set the output latch high FIRST, then switch to output mode, so
     * the pin goes straight from input to driving high and never drives low.
     * CS must start high because a high-to-low edge on CS starts an EEPROM
     * instruction (CAT25040 datasheet, Pin Description, CS).
     * RM0091 9.4.7 GPIOx_BSRR: BS12 = 1 sets ODR12.
     * RM0091 9.4.1 GPIOx_MODER: MODER12 = 01. */
    EE_SPI_GPIO->BSRR   = EE_CS_MASK;
    EE_SPI_GPIO->MODER  = (EE_SPI_GPIO->MODER & ~MODER2_MASK(EE_PIN_CS))
                        | MODER2(EE_PIN_CS, GPIO_MODE_OUT);

    /* TODO 2.6  SCK, MISO, MOSI to SPI2 alternate function.
     * Datasheet Table 15: PB13/PB14/PB15 = SPI2_SCK/MISO/MOSI on AF0.
     * RM0091 9.4.10 GPIOx_AFRH: AFSELy[3:0] for pins 8..15.
     * RM0091 9.4.1  GPIOx_MODER: MODERy = 10 (alternate function).
     * AFRH is written before MODER so each pin enters AF mode already
     * routed to SPI2. */
    EE_SPI_GPIO->AFR[1] = (EE_SPI_GPIO->AFR[1]
                           & ~(AFRH4_MASK(EE_PIN_SCK)
                             | AFRH4_MASK(EE_PIN_MISO)
                             | AFRH4_MASK(EE_PIN_MOSI)))
                        | AFRH4(EE_PIN_SCK,  EE_SPI_AF)
                        | AFRH4(EE_PIN_MISO, EE_SPI_AF)
                        | AFRH4(EE_PIN_MOSI, EE_SPI_AF);

    EE_SPI_GPIO->MODER  = (EE_SPI_GPIO->MODER
                           & ~(MODER2_MASK(EE_PIN_SCK)
                             | MODER2_MASK(EE_PIN_MISO)
                             | MODER2_MASK(EE_PIN_MOSI)))
                        | MODER2(EE_PIN_SCK,  GPIO_MODE_AF)
                        | MODER2(EE_PIN_MISO, GPIO_MODE_AF)
                        | MODER2(EE_PIN_MOSI, GPIO_MODE_AF);

    /* TODO 2.7  High speed on SCK and MOSI; pull-up on MISO.
     * CAT25040 datasheet, Pin Description (CS): when CS is high, SO is
     * tri-stated (high impedance). The pull-up stops PB14 floating then.
     * RM0091 9.4.3 GPIOx_OSPEEDR: OSPEEDRy = 11 (high speed).
     * RM0091 9.4.4 GPIOx_PUPDR:   PUPDRy   = 01 (pull-up). */
    EE_SPI_GPIO->OSPEEDR = (EE_SPI_GPIO->OSPEEDR
                            & ~(MODER2_MASK(EE_PIN_SCK) | MODER2_MASK(EE_PIN_MOSI)))
                         | MODER2(EE_PIN_SCK,  GPIO_SPEED_HIGH)
                         | MODER2(EE_PIN_MOSI, GPIO_SPEED_HIGH);

    EE_SPI_GPIO->PUPDR   = (EE_SPI_GPIO->PUPDR & ~MODER2_MASK(EE_PIN_MISO))
                         | MODER2(EE_PIN_MISO, GPIO_PULL_UP);

    /* SPI must be disabled while it is configured.
     * RM0091 27.7.1 SPIx_CR1 bit 6 SPE = 0; RM0091 27.3.7. */
    EE_SPI->CR1 &= ~(1UL << SPI_CR1_SPE_BIT);

    /* TODO 2.8  SPI_CR2 (RM0091 27.7.2, reset value 0x0700), before SPE.
     * DS[3:0] (bits 11:8) = 0111 : 8-bit data frame.
     * FRXTH   (bit 12)    = 1    : RXNE when RXFIFO >= 1/4 (8 bits).
     *                              Reset value 0 waits for 16 bits, so a
     *                              single-byte transfer would never set RXNE.
     * All other bits 0: no interrupts, no DMA, Motorola format (FRF = 0),
     * NSSP = 0, SSOE = 0 (NSS pin not used; CS is a plain GPIO). */
    EE_SPI->CR2 = (SPI_CR2_DS_8BIT << SPI_CR2_DS_POS)
                | (1UL << SPI_CR2_FRXTH_BIT);

    /* TODO 2.9  SPI_CR1 (RM0091 27.7.1).
     * CPHA     bit 0   = 0 : } SPI mode (0,0). CAT25040 datasheet: supports
     * CPOL     bit 1   = 0 : } modes (0,0) and (1,1); SI latched on rising
     *                          SCK edge, SO shifted out on falling edge.
     * MSTR     bit 2   = 1 : master.
     * BR[2:0]  bits 5:3 = EE_SPI_BR (100) : fPCLK/32 = 250 kHz.
     * LSBFIRST bit 7   = 0 : MSB first (CAT25040 timing figures, MSB first).
     * SSI      bit 8   = 1 : } Software NSS management with internal NSS held
     * SSM      bit 9   = 1 : } high. RM0091 27.3.4: in master mode a low NSS
     *                          causes a mode fault and the SPI leaves master
     *                          mode.
     * BIDIMODE, BIDIOE, CRCEN, CRCNEXT, CRCL, RXONLY = 0: full duplex, no CRC.
     * SPE left 0 here. */
    EE_SPI->CR1 = (0UL << SPI_CR1_CPHA_BIT)
                | (0UL << SPI_CR1_CPOL_BIT)
                | (1UL << SPI_CR1_MSTR_BIT)
                | (EE_SPI_BR << SPI_CR1_BR_POS)
                | (0UL << SPI_CR1_LSBFIRST_BIT)
                | (1UL << SPI_CR1_SSI_BIT)
                | (1UL << SPI_CR1_SSM_BIT);

    /* Task 5 fault case. Leave this call exactly here: after your CR1 and CR2
     * configuration, before the peripheral is enabled. It does nothing unless
     * RUN_TASK is 5. */
    task5_fault_hook();

    /* TODO 2.10  Enable the peripheral.
     * RM0091 27.7.1 SPIx_CR1 bit 6 SPE = 1 (RM0091 27.3.7). */
    EE_SPI->CR1 |= (1UL << SPI_CR1_SPE_BIT);

    dbg_gpiob_moder      = EE_SPI_GPIO->MODER;
    dbg_gpiob_afrh       = EE_SPI_GPIO->AFR[1];
    dbg_spi_cr1          = EE_SPI->CR1;
    dbg_spi_cr2          = EE_SPI->CR2;
    dbg_spi_sr           = EE_SPI->SR;
    dbg_sck_hz_predicted = EE_SCK_HZ_PREDICTED;
}

/* ==========================================================================
 * RUN_TASK 2 - given
 * Configures SPI and then does nothing else, so you can inspect the selected
 * SPI peripheral and GPIOB in the SFR view (or the dbg_* variables) while the
 * program runs. PC13 keeps toggling so you can see the program has not hung.
 * ========================================================================== */

void task2_setup(void)
{
    task1_gpio_init();
    eeprom_spi_init();
}

void task2_loop(uint32_t now)
{
    task1_gpio_update(now);
}
