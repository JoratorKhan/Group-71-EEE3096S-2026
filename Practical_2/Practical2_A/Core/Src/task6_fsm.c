/**
  ******************************************************************************
  * @file    task6_fsm.c
  * @brief   TASK 6 : NON-BLOCKING EEPROM TRANSACTION STATE MACHINE
  *
  * Restructure the Task 4 transaction so the main loop never stops:
  *
  *   request -> write-enable -> write -> wait for EEPROM
  *           -> read-back -> verify -> result
  *
  * Rules (handout, Task 6):
  *   - No HAL_Delay(), and no software busy-wait for the EEPROM's internal
  *     write cycle. You may use HAL_GetTick() to decide when the next status
  *     check is due.
  *   - Each call does a small amount of work, updates the state, and returns.
  *   - Do not put the whole transaction inside one blocking function called
  *     from the loop.
  *   - While a transaction is in progress the loop must still respond to PA3.
  *
  * Controls: PA0 starts, PA3 aborts. PB0..PB7 show the last byte read, PB11
  * (green) a successful verification, PB10 (red) a failed one.
  ******************************************************************************
  */

#include "prac2a.h"

/* TODO 6.1  Design your states. You need enough of them to tell apart at
 *           least: idle, write preparation, write transaction, EEPROM busy /
 *           status checking, read transaction, verification, and success or
 *           failure.
 *
 *           Draw the diagram first. It must show the initial state, the
 *           condition on every transition, and the success, failure and
 *           abort paths.
 *
 *           typedef enum { ... } ee_state_t;
 */
typedef enum
{
    EE_IDLE       = 0,  /* waiting for PA0                                */
    EE_WRITE_PREP = 1,  /* device ready (RDY=0)? then WREN                */
    EE_WRITE      = 2,  /* WEL set? then WRITE + address + data           */
    EE_BUSY       = 3,  /* internal write cycle: RDSR only when due       */
    EE_READ       = 4,  /* READ + address, clock one byte back            */
    EE_VERIFY     = 5,  /* compare read-back with byte written            */
    EE_PASS       = 6,  /* verified: green                                */
    EE_FAIL       = 7   /* mismatch, WEL not set, or timeout: red         */
} ee_state_t;

#define EE_POLL_MS   1u   /* interval between status checks while waiting */

volatile uint8_t ee_state       = 0u;
volatile uint8_t ee_last_read   = 0u;
volatile uint8_t ee_use_fsm     = 1u;

volatile uint32_t ee_demo_step_ms = 0u;           /* demo only: hold each active state this long (0 = off) */

static uint16_t     ee_addr        = 0u;          /* latched at start         */
static uint8_t      ee_data        = 0u;          /* latched at start         */
static uint32_t     ee_t0          = 0u;          /* start of current wait    */
static uint32_t     ee_poll_last   = 0u;          /* last status check        */
static uint32_t     ee_step_last   = 0u;          /* tick of last state change */
static status_led_t ee_idle_status = STATUS_OFF;  /* shown while in IDLE      */

void update_eeprom_state_machine(uint32_t now)
{
    /* TODO 6.2  One step of your state machine.
     *
     *   - btn_start_edge (PA0) starts a transaction from idle.
     *   - btn_abort_edge (PA3) abandons the transaction in progress and returns
     *     to idle. Leave the SPI bus in a state the next transaction can use.
     *   - While the EEPROM is busy writing, do NOT wait in here. Work out when
     *     the next status check is due, remember it, and return.
     *   - Decide that the write has finished from the status register, never
     *     from elapsed time alone. Also decide what happens if the EEPROM never
     *     reports ready.
     *   - Store the byte read back in ee_last_read.
     *   - Clear each button edge once you have acted on it. */
    ee_state_t s = (ee_state_t)ee_state;
    uint8_t active = ((s != EE_IDLE) && (s != EE_PASS) && (s != EE_FAIL)) ? 1u : 0u;

    /* PA3: abort an active transaction */
    if (btn_abort_edge)
    {
        btn_abort_edge = 0u;
        if (active)
        {
            if (s == EE_WRITE)              /* WEL is set but no write sent: clear it */
            {
                eeprom_cs_low();
                spi_transfer(EEPROM_CMD_WRDI);
                eeprom_cs_high();
            }
            eeprom_cs_high();               /* bus released, CS high */
            ee_idle_status = STATUS_OFF;
            ee_state = (uint8_t)EE_IDLE;
            ee_step_last = now;
            return;
        }
    }

    /* PA0 is ignored while a transaction is active */
    if (active)
    {
        btn_start_edge = 0u;
    }

    /* Demo hold: return without advancing until the hold time has passed */
    if (active && (ee_demo_step_ms != 0u) &&
        ((uint32_t)(now - ee_step_last) < ee_demo_step_ms))
    {
        return;
    }

    switch (s)
    {
    case EE_IDLE:
    case EE_PASS:
    case EE_FAIL:
        if (btn_start_edge)
        {
            btn_start_edge = 0u;
            ee_addr        = eeprom_test_addr;   /* latch test values */
            ee_data        = eeprom_test_byte;
            ee_t0          = now;
            ee_poll_last   = now - EE_POLL_MS;   /* first check due now */
            ee_idle_status = STATUS_OFF;
            ee_state = (ee_addr < EEPROM_SIZE_BYTES) ? (uint8_t)EE_WRITE_PREP
                                                     : (uint8_t)EE_FAIL;
        }
        break;

    case EE_WRITE_PREP:
        if ((uint32_t)(now - ee_poll_last) < EE_POLL_MS)
        {
            break;                              /* check not due: return */
        }
        ee_poll_last = now;
        if ((eeprom_read_status() & EEPROM_SR_RDY) == 0u)
        {
            eeprom_write_enable();              /* WEL set on CS rising edge */
            ee_state = (uint8_t)EE_WRITE;
        }
        else if ((uint32_t)(now - ee_t0) >= EEPROM_WRITE_TIMEOUT_MS)
        {
            eeprom_timeout_count++;
            ee_state = (uint8_t)EE_FAIL;
        }
        break;

    case EE_WRITE:
        if ((eeprom_read_status() & EEPROM_SR_WEL) == 0u)
        {
            ee_state = (uint8_t)EE_FAIL;        /* write would be ignored */
            break;
        }
        eeprom_cs_low();
        spi_transfer(EEPROM_CMD_WRITE);
        spi_transfer((uint8_t)(ee_addr >> 8));
        spi_transfer((uint8_t)(ee_addr & 0xFFu));
        spi_transfer(ee_data);
        eeprom_cs_high();                       /* internal write starts here */
        ee_t0        = now;
        ee_poll_last = now;                     /* first check 1 ms later */
        ee_state = (uint8_t)EE_BUSY;
        break;

    case EE_BUSY:
        if ((uint32_t)(now - ee_poll_last) < EE_POLL_MS)
        {
            break;                              /* check not due: return */
        }
        ee_poll_last = now;
        if ((eeprom_read_status() & EEPROM_SR_RDY) == 0u)
        {
            eeprom_write_wait_ms = (uint32_t)(now - ee_t0);
            ee_state = (uint8_t)EE_READ;
        }
        else if ((uint32_t)(now - ee_t0) >= EEPROM_WRITE_TIMEOUT_MS)
        {
            eeprom_timeout_count++;
            ee_state = (uint8_t)EE_FAIL;
        }
        break;

    case EE_READ:
        ee_last_read = eeprom_read_byte(ee_addr);
        ee_state = (uint8_t)EE_VERIFY;
        break;

    case EE_VERIFY:
        eeprom_verify_ok = (ee_last_read == ee_data) ? 1u : 0u;
        ee_state = eeprom_verify_ok ? (uint8_t)EE_PASS : (uint8_t)EE_FAIL;
        break;

    default:
        eeprom_cs_high();
        ee_state = (uint8_t)EE_IDLE;
        break;
    }

    if ((ee_state_t)ee_state != s)
    {
        ee_step_last = now;                     /* restart the hold on every state change */
    }
}

void update_outputs(void)
{
    /* TODO 6.3  PB0..PB7 show ee_last_read (leds_write_byte). Green after a
     *           successful verification, red after a failed one
     *           (status_leds_show). Decide what the status LEDs should show
     *           while a transaction is in progress, and after an abort. */
    leds_write_byte(ee_last_read);

    switch ((ee_state_t)ee_state)
    {
    case EE_PASS:
        status_leds_show(STATUS_PASS);
        break;
    case EE_FAIL:
        status_leds_show(STATUS_FAIL);
        break;
    case EE_IDLE:
        status_leds_show(ee_idle_status);   /* boot result, or off after abort */
        break;
    default:
        status_leds_show(STATUS_OFF);       /* transaction in progress */
        break;
    }
}

/* ==========================================================================
 * RUN_TASK 6 - given
 *
 * The main loop has exactly the shape the handout asks for:
 *     read_inputs();  update_eeprom_state_machine();  update_outputs();
 *
 * Set ee_use_fsm = 0 in Live Expressions to run the Task 4 blocking path on
 * PA0 instead - useful when you explain why yours is non-blocking. PC13 keeps
 * toggling as a heartbeat; watch it on the scope in both modes.
 * ========================================================================== */

void task6_setup(void)
{
    task1_gpio_init();
    eeprom_spi_init();
    board_io_init();

    eeprom_read_only_path();
    ee_last_read = eeprom_read_value;

    /* TODO 6.4  Your state machine starts in its idle state. Make sure the
     *           boot-time result (eeprom_verify_ok) still shows on the status
     *           LEDs, so a reset still shows green for the persistence test. */
    ee_state       = (uint8_t)EE_IDLE;
    ee_idle_status = eeprom_verify_ok ? STATUS_PASS : STATUS_FAIL;
}

void task6_loop(uint32_t now)
{
    task1_gpio_update(now);
    read_inputs(now);

    if (ee_use_fsm)
    {
        update_eeprom_state_machine(now);
        update_outputs();
    }
    else
    {
        if (btn_start_edge)
        {
            btn_start_edge = 0u;
            eeprom_write_verify_path();     /* Task 4: blocks for the write */
            ee_last_read = eeprom_read_value;
        }
        btn_abort_edge = 0u;
    }
}
