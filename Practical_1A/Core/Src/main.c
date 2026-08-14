/* USER CODE BEGIN Header */
/**
  * @file           : main.c
  * @brief          : Task 4 - Multi-mode LED control (running, inverse, sparkle)
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f0xx.h"
#include <stdint.h>
#include <stdlib.h>      // for rand()

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim16;

/* USER CODE BEGIN PV */
// LED arrays
GPIO_TypeDef* led_ports[8] = {GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB, GPIOB};
uint16_t led_pins[8] = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3,
                        GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7};

// Timer event flag (set by ISR only)
volatile uint8_t timer_event = 0;

// Mode enumeration
typedef enum {
    MODE_1 = 0,
    MODE_2,
    MODE_3,
    MODE_OFF
} LED_Mode;
volatile LED_Mode current_mode = MODE_OFF;

// Mode 1 & 2 shared variables
volatile uint8_t current_led = 0;
volatile int8_t direction = 1;

// ---- Button handling -------------------------------------------------------
// Set this to (measured max bounce + margin) from the oscilloscope capture.
#define DEBOUNCE_MS 2
#define NUM_BUTTONS 4

uint16_t      button_pins[NUM_BUTTONS]       = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3};
GPIO_PinState button_idle_state[NUM_BUTTONS] = {GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET};
GPIO_PinState button_stable[NUM_BUTTONS]     = {GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET};
GPIO_PinState button_last_raw[NUM_BUTTONS]   = {GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET};
uint32_t      last_button_time[NUM_BUTTONS]  = {0, 0, 0, 0};

uint8_t speed_state = 0;        // 0 = slow (1s), 1 = fast (0.5s)

// ---- Mode 3 state machine --------------------------------------------------
typedef enum {
    SPARKLE_IDLE = 0,
    SPARKLE_DISPLAY,
    SPARKLE_TURN_OFF
} SparkleState;

volatile SparkleState sparkle_state         = SPARKLE_IDLE;
volatile uint8_t      sparkle_pattern       = 0;
volatile uint32_t     sparkle_display_until = 0;
volatile uint32_t     sparkle_next_off_time = 0;
volatile uint8_t      sparkle_off_index     = 0;
uint8_t               sparkle_leds_on[8];   // indices of LEDs currently lit
volatile uint8_t      sparkle_num_leds_on   = 0;

#define SPARKLE_HOLD_MIN_MS   100
#define SPARKLE_HOLD_MAX_MS   1500
#define SPARKLE_OFF_MIN_MS    100
#define SPARKLE_OFF_MAX_MS    150

// Current period reference
uint32_t current_period_ms = 1000;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM16_Init(void);
void TIM16_IRQHandler(void);

/* USER CODE BEGIN PFP */
void clear_all_leds(void);
void turn_on_led(uint8_t index);
void turn_off_led(uint8_t index);
void change_timer_period(uint32_t new_period_ms);
void handle_buttons(void);
void set_mode(LED_Mode new_mode);
void advance_index(void);
void mode1_update(void);
void mode2_update(void);
void mode3_update(void);
static uint32_t rand_range(uint32_t lo, uint32_t hi);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
void clear_all_leds(void)
{
    for (uint8_t i = 0; i < 8; i++) {
        HAL_GPIO_WritePin(led_ports[i], led_pins[i], GPIO_PIN_RESET);
    }
}

void turn_on_led(uint8_t index)
{
    if (index < 8) {
        HAL_GPIO_WritePin(led_ports[index], led_pins[index], GPIO_PIN_SET);
    }
}

void turn_off_led(uint8_t index)
{
    if (index < 8) {
        HAL_GPIO_WritePin(led_ports[index], led_pins[index], GPIO_PIN_RESET);
    }
}

// Inclusive random value in [lo, hi]
static uint32_t rand_range(uint32_t lo, uint32_t hi)
{
    return lo + (uint32_t)(rand() % (int32_t)(hi - lo + 1));
}

void change_timer_period(uint32_t new_period_ms)
{
    // Timer clock = 8 MHz / (7999 + 1) = 1000 Hz, so 1 tick = 1 ms.
    // ARR = desired_period_ms - 1
    uint32_t new_arr = new_period_ms - 1;

    TIM16->ARR = new_arr;   // write ARR directly, no re-init
    TIM16->CNT = 0;         // restart the count from a clean cycle

    current_period_ms = new_period_ms;
}

void handle_buttons(void)
{
    uint32_t now = HAL_GetTick();

    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        GPIO_PinState raw = HAL_GPIO_ReadPin(GPIOA, button_pins[i]);

        // Any change in the raw level restarts the settling window.
        if (raw != button_last_raw[i]) {
            button_last_raw[i]  = raw;
            last_button_time[i] = now;
            continue;
        }

        // Not steady for long enough yet: still bouncing.
        if ((now - last_button_time[i]) < DEBOUNCE_MS) {
            continue;
        }

        // Level is settled. Act only if it differs from the last confirmed level.
        if (raw != button_stable[i]) {
            button_stable[i] = raw;

            // Act only on the transition into the pressed level.
            if (raw != button_idle_state[i]) {
                switch (i) {
                    case 0:     // PA0 - speed toggle, works in every mode
                        speed_state = !speed_state;
                        change_timer_period(speed_state ? 500 : 1000);
                        break;
                    case 1:     // PA1 - running light
                        set_mode(MODE_1);
                        break;
                    case 2:     // PA2 - inverse running light
                        set_mode(MODE_2);
                        break;
                    case 3:     // PA3 - sparkle
                        set_mode(MODE_3);
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

void set_mode(LED_Mode new_mode)
{
    current_mode = new_mode;

    clear_all_leds();

    // Reset mode-specific tracking variables
    current_led  = 0;
    direction    = 1;

    sparkle_state         = SPARKLE_IDLE;
    sparkle_off_index     = 0;
    sparkle_num_leds_on   = 0;
    sparkle_pattern       = 0;
    sparkle_display_until = HAL_GetTick();
    sparkle_next_off_time = HAL_GetTick();
}

// Shared 0..7..0 bounce logic, no duplicated turn-around LEDs
void advance_index(void)
{
    if (direction == 1) {
        if (current_led == 7) { direction = -1; current_led = 6; }
        else                  { current_led++; }
    } else {
        if (current_led == 0) { direction =  1; current_led = 1; }
        else                  { current_led--; }
    }
}

void mode1_update(void)
{
    clear_all_leds();
    turn_on_led(current_led);
    advance_index();
}

void mode2_update(void)
{
    for (uint8_t i = 0; i < 8; i++) {
        turn_on_led(i);
    }
    turn_off_led(current_led);
    advance_index();
}

void mode3_update(void)
{
    uint32_t now = HAL_GetTick();

    switch (sparkle_state) {
        case SPARKLE_IDLE:
            // Random 8-bit pattern, 0 to 255
            sparkle_pattern     = (uint8_t)(rand() & 0xFF);
            sparkle_num_leds_on = 0;

            for (uint8_t i = 0; i < 8; i++) {
                if (sparkle_pattern & (1u << i)) {
                    turn_on_led(i);
                    sparkle_leds_on[sparkle_num_leds_on++] = i;
                }
            }

            // Shuffle so the LEDs switch off in a random order
            for (uint8_t i = sparkle_num_leds_on; i > 1; i--) {
                uint8_t j = (uint8_t)(rand() % i);
                uint8_t t = sparkle_leds_on[i - 1];
                sparkle_leds_on[i - 1] = sparkle_leds_on[j];
                sparkle_leds_on[j] = t;
            }

            sparkle_off_index     = 0;
            sparkle_display_until = now + rand_range(SPARKLE_HOLD_MIN_MS, SPARKLE_HOLD_MAX_MS);
            sparkle_state         = SPARKLE_DISPLAY;
            break;

        case SPARKLE_DISPLAY:
            if ((int32_t)(now - sparkle_display_until) >= 0) {
                sparkle_next_off_time = now;    // first LED goes off straight away
                sparkle_state         = SPARKLE_TURN_OFF;
            }
            break;

        case SPARKLE_TURN_OFF:
            if (sparkle_off_index >= sparkle_num_leds_on) {
                sparkle_state = SPARKLE_IDLE;   // all off, start a new pattern
                break;
            }
            if ((int32_t)(now - sparkle_next_off_time) >= 0) {
                turn_off_led(sparkle_leds_on[sparkle_off_index]);
                sparkle_off_index++;
                sparkle_next_off_time = now + rand_range(SPARKLE_OFF_MIN_MS, SPARKLE_OFF_MAX_MS);
            }
            break;

        default:
            sparkle_state = SPARKLE_IDLE;
            break;
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM16_Init();

    /* USER CODE BEGIN 2 */
    srand(HAL_GetTick());

    clear_all_leds();

    // Sample the resting level of each button so polarity is detected
    // automatically. Do not hold a button while the board powers up.
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        button_idle_state[i] = HAL_GPIO_ReadPin(GPIOA, button_pins[i]);
        button_stable[i]     = button_idle_state[i];
        button_last_raw[i]   = button_idle_state[i];
        last_button_time[i]  = HAL_GetTick();
    }

    change_timer_period(1000);
    HAL_TIM_Base_Start_IT(&htim16);
    /* USER CODE END 2 */

    while (1)
    {
        /* USER CODE BEGIN WHILE */
        handle_buttons();

        // Timer-driven updates for Mode 1 and Mode 2
        if (timer_event) {
            timer_event = 0;

            switch (current_mode) {
                case MODE_1:
                    mode1_update();
                    break;
                case MODE_2:
                    mode2_update();
                    break;
                case MODE_3:
                    // Mode 3 is driven by HAL_GetTick, not the timer
                    break;
                case MODE_OFF:
                default:
                    clear_all_leds();
                    break;
            }
        }

        // Mode 3 needs continuous polling for its own timing
        if (current_mode == MODE_3) {
            mode3_update();
        }
        /* USER CODE END WHILE */
    }
}

/**
  * @brief System Clock Configuration (HSI 8 MHz)
  */
void SystemClock_Config(void)
{
    LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);
    while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0) {}

    LL_RCC_HSI_Enable();
    while (LL_RCC_HSI_IsReady() != 1) {}

    LL_RCC_HSI_SetCalibTrimming(16);
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI) {}

    LL_SetSystemCoreClock(8000000);

    if (HAL_InitTick(TICK_INT_PRIORITY) != HAL_OK) {
        Error_Handler();
    }
}

/**
  * @brief TIM16 Initialization - prescaler fixed at 7999
  */
static void MX_TIM16_Init(void)
{
    htim16.Instance = TIM16;
    htim16.Init.Prescaler = 7999;      // 8000 - 1
    htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim16.Init.Period = 999;          // changed dynamically at runtime
    htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim16.Init.RepetitionCounter = 0;
    htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(&htim16) != HAL_OK) {
        Error_Handler();
    }

    NVIC_EnableIRQ(TIM16_IRQn);
}

/**
  * @brief GPIO Initialization - PB0..PB7 outputs, PA0..PA3 inputs with pull-up
  */
static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // LEDs PB0..PB7 as outputs
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    for (uint8_t i = 0; i < 8; i++) {
        GPIO_InitStruct.Pin = led_pins[i];
        HAL_GPIO_Init(led_ports[i], &GPIO_InitStruct);
    }

    // Buttons PA0..PA3 as inputs with pull-up (active low)
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
  * @brief TIM16 interrupt handler - sets flag only
  */
void TIM16_IRQHandler(void)
{
    __HAL_TIM_CLEAR_IT(&htim16, TIM_IT_UPDATE);
    timer_event = 1;
}

/**
  * @brief Error handler
  */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
