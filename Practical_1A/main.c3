/* USER CODE BEGIN Header */
/**
  * @file           : main.c
  * @brief          : Task 3 - Running light with debounced speed toggle
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f0xx.h"
#include <stdint.h>

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim16;

/* USER CODE BEGIN PV */
volatile uint8_t timer_event = 0;
volatile uint8_t current_led = 0;
volatile int8_t  direction   = 1;

// LED arrays
GPIO_TypeDef* led_ports[8] = {GPIOB, GPIOB, GPIOB, GPIOB,
                              GPIOB, GPIOB, GPIOB, GPIOB};
uint16_t led_pins[8] = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3,
                        GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7};

// Debounce window. Replace with (measured max bounce + margin) after the lab.
#define DEBOUNCE_MS 2

// Button state tracking
GPIO_PinState button_idle_state  = GPIO_PIN_SET;  // sampled at startup
GPIO_PinState button_stable      = GPIO_PIN_SET;  // last confirmed level
GPIO_PinState button_last_raw    = GPIO_PIN_SET;  // last raw sample
uint32_t      last_button_press_time = 0;         // when raw level last changed

// Speed state: 0 = slow (1s), 1 = fast (0.5s)
uint8_t speed_state = 0;

// Current ARR value (stored for reference)
uint32_t current_arr = 999;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM16_Init(void);
void TIM16_IRQHandler(void);
void turn_off_all_leds(void);
void update_led_pattern(void);
void handle_button_press(void);
void change_timer_period(uint32_t new_period_ms);

/* USER CODE BEGIN 0 */
void turn_off_all_leds(void)
{
    for (uint8_t i = 0; i < 8; i++) {
        HAL_GPIO_WritePin(led_ports[i], led_pins[i], GPIO_PIN_RESET);
    }
}

void update_led_pattern(void)
{
    turn_off_all_leds();
    HAL_GPIO_WritePin(led_ports[current_led], led_pins[current_led], GPIO_PIN_SET);

    if (direction == 1) {
        if (current_led == 7) { direction = -1; current_led = 6; }
        else                  { current_led++; }
    } else {
        if (current_led == 0) { direction =  1; current_led = 1; }
        else                  { current_led--; }
    }
}

void change_timer_period(uint32_t new_period_ms)
{
    // Timer clock is 1000 Hz (PSC = 7999), so 1 tick = 1 ms.
    uint32_t new_arr = new_period_ms - 1;

    // Update the auto-reload register directly (no full re-init).
    TIM16->ARR = new_arr;

    // Restart the counter so the change applies from a clean cycle.
    TIM16->CNT = 0;

    // Store the new ARR value for reference
    current_arr = new_arr;
}

void handle_button_press(void)
{
    uint32_t now = HAL_GetTick();
    GPIO_PinState raw = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);

    // Any change in the raw level restarts the settling window.
    if (raw != button_last_raw) {
        button_last_raw = raw;
        last_button_press_time = now;
        return;
    }

    // Level has not been steady long enough yet: still bouncing.
    if ((now - last_button_press_time) < DEBOUNCE_MS) {
        return;
    }

    // Level is settled. Act only if it differs from the last confirmed level.
    if (raw != button_stable) {
        button_stable = raw;

        // Toggle only on the transition into the pressed level.
        if (raw != button_idle_state) {
            speed_state = !speed_state;
            change_timer_period(speed_state ? 500 : 1000);
        }
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
    turn_off_all_leds();

    // Sample the resting level of PA0 so polarity is detected automatically.
    // Do not hold the button while the board powers up.
    button_idle_state      = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
    button_stable          = button_idle_state;
    button_last_raw        = button_idle_state;
    last_button_press_time = HAL_GetTick();

    // Start timer at 1-second period
    change_timer_period(1000);
    HAL_TIM_Base_Start_IT(&htim16);
    /* USER CODE END 2 */

    while (1)
    {
        /* USER CODE BEGIN WHILE */
        handle_button_press();

        if (timer_event) {
            timer_event = 0;
            update_led_pattern();
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
  * @brief TIM16 Initialization - Prescaler fixed at 7999
  */
static void MX_TIM16_Init(void)
{
    htim16.Instance = TIM16;
    htim16.Init.Prescaler = 7999;
    htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim16.Init.Period = 999;
    htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim16.Init.RepetitionCounter = 0;
    htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    if (HAL_TIM_Base_Init(&htim16) != HAL_OK) {
        Error_Handler();
    }

    NVIC_EnableIRQ(TIM16_IRQn);
}

/**
  * @brief GPIO Initialization - PB0..PB7 outputs, PA0 input with pull-up
  */
static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Configure LEDs (PB0..PB7) as outputs
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    for (uint8_t i = 0; i < 8; i++) {
        GPIO_InitStruct.Pin = led_pins[i];
        HAL_GPIO_Init(led_ports[i], &GPIO_InitStruct);
    }

    // Configure PA0 as input with pull-up
    GPIO_InitStruct.Pin  = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/**
  * @brief TIM16 interrupt handler - sets flag only
  */
void TIM16_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim16);
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
