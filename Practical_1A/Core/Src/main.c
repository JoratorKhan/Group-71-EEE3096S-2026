/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdlib.h>  // Added for rand() function
#include "stm32f0xx.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LED_SWITCH_DELAY1 1000 // 1s
#define LED_SWITCH_DELAY2 500 //0.5s
#define DEBOUNCE_TIME 50 //ms
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim16;

/* USER CODE BEGIN PV */
// TODO: Define input variables
//Arrays for easy indexing of the LEDs
GPIO_TypeDef* LED_GPIO_Port[8] = {
  LED0_GPIO_Port,
  LED1_GPIO_Port,
  LED2_GPIO_Port,
  LED3_GPIO_Port,
  LED4_GPIO_Port,
  LED5_GPIO_Port,
  LED6_GPIO_Port,
  LED7_GPIO_Port
};


uint16_t led_pins[8] = {
  LED0_Pin, LED1_Pin, LED2_Pin, LED3_Pin,
  LED4_Pin, LED5_Pin, LED6_Pin, LED7_Pin
};
//LED_Modes
typedef enum {
  MODE_1 = 0, // Forward and backward LED pattern
  MODE_2,     // Reverse of Mode 1
  MODE_3,  // Random LED pattern
  MODE_OFF
} LED_Mode;

// Add global variables for mode, LED index, direction, delay, and debounce
volatile LED_Mode current_mode = MODE_OFF;
volatile uint8_t current_led = 0;
volatile uint8_t direction = 1;
volatile uint32_t led_delay = LED_SWITCH_DELAY1;

// Debounce timers for each button
uint32_t debounce_time_pa0 = 0;
uint32_t debounce_time_pa1 = 0;
uint32_t debounce_time_pa2 = 0;
uint32_t debounce_time_pa3 = 0;
#define DEBOUNCE_MS 50



//LED delay
uint32_t prev_millis = 0;
uint32_t curr_millis = 0;
//LED_Mode mode;

// Add global variables for state machine and timer event
volatile uint8_t timer_event = 0;

// For mode 3 state machine - FIXED VERSION
typedef enum {
    SPARKLE_IDLE = 0,
    SPARKLE_DISPLAY,
    SPARKLE_TURN_OFF_SEQUENCE
} SparkleState;

volatile SparkleState sparkle_state = SPARKLE_IDLE;
volatile uint8_t sparkle_pattern = 0;
volatile uint32_t sparkle_display_until = 0;
volatile uint32_t sparkle_next_off_time = 0;
volatile uint8_t sparkle_off_index = 0;
volatile uint8_t sparkle_leds_to_turn_off[8];  // Array to store which LEDs need to be turned off
volatile uint8_t sparkle_num_leds_on = 0;      // Number of LEDs that are currently on

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM16_Init(void);
/* USER CODE BEGIN PFP */
void TIM16_IRQHandler(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Function Prototypes
void turn_on_all_leds(void);
void turn_off_all_leds(void);
void turn_on_led(uint8_t led_index);
void turn_off_led(uint8_t led_index);
uint8_t generate_random_number(void);
void change_led_delay(uint32_t new_delay);
void mode1_function(void);
void mode2_function(void);
void mode3_function(void);
LED_Mode select_mode(void);
void change_pattern(void);


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM16_Init();
  /* USER CODE BEGIN 2 */


  // TODO: Start timer TIM16
  HAL_TIM_Base_Start_IT(&htim16);

  // Ensure all LEDs are off at startup
  turn_off_all_leds();

  // Initialize random seed (you might want to use a better seed source)
  srand(42);  // Simple seed - in real application, use ADC noise or other entropy source
 
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now = HAL_GetTick();
    // --- BUTTON HANDLING (debounced, edge-detect) ---
    static uint8_t prev_btn0 = 1, prev_btn1 = 1, prev_btn2 = 1, prev_btn3 = 1;
    uint8_t curr_btn0 = HAL_GPIO_ReadPin(Button0_GPIO_Port, Button0_Pin);
    uint8_t curr_btn1 = HAL_GPIO_ReadPin(Button1_GPIO_Port, Button1_Pin);
    uint8_t curr_btn2 = HAL_GPIO_ReadPin(Button2_GPIO_Port, Button2_Pin);
    uint8_t curr_btn3 = HAL_GPIO_ReadPin(Button3_GPIO_Port, Button3_Pin);
    static uint32_t debounce0 = 0, debounce1 = 0, debounce2 = 0, debounce3 = 0;
    
    // PA0: Toggle delay
    if (curr_btn0 == GPIO_PIN_RESET && prev_btn0 == GPIO_PIN_SET && (now - debounce0 > DEBOUNCE_MS)) {
      debounce0 = now;
      if (led_delay == LED_SWITCH_DELAY1) {
        led_delay = LED_SWITCH_DELAY2;
      } else {
        led_delay = LED_SWITCH_DELAY1;
      }
      change_led_delay(led_delay);
    }
    // PA1: Mode 1
    if (curr_btn1 == GPIO_PIN_RESET && prev_btn1 == GPIO_PIN_SET && (now - debounce1 > DEBOUNCE_MS)) {
      debounce1 = now;
      current_mode = MODE_1;
      current_led = 0;
      direction = 1;
      sparkle_state = SPARKLE_IDLE;
      turn_off_all_leds();  // Clear LEDs when switching modes
    }
    // PA2: Mode 2
    if (curr_btn2 == GPIO_PIN_RESET && prev_btn2 == GPIO_PIN_SET && (now - debounce2 > DEBOUNCE_MS)) {
      debounce2 = now;
      current_mode = MODE_2;
      current_led = 0;
      direction = 1;
      sparkle_state = SPARKLE_IDLE;
      turn_off_all_leds();  // Clear LEDs when switching modes
    }
    // PA3: Mode 3
    if (curr_btn3 == GPIO_PIN_RESET && prev_btn3 == GPIO_PIN_SET && (now - debounce3 > DEBOUNCE_MS)) {
      debounce3 = now;
      current_mode = MODE_3;
      sparkle_state = SPARKLE_IDLE;
      turn_off_all_leds();  // Clear LEDs when switching modes
    }
    prev_btn0 = curr_btn0;
    prev_btn1 = curr_btn1;
    prev_btn2 = curr_btn2;
    prev_btn3 = curr_btn3;

    // --- LED PATTERN STATE MACHINE ---
    if (current_mode == MODE_1 && timer_event) {
      mode1_function();
      timer_event = 0;
    } else if (current_mode == MODE_2 && timer_event) {
      mode2_function();
      timer_event = 0;
    } else if (current_mode == MODE_3) {
      // FIXED Mode 3 state machine
      switch (sparkle_state) {
        case SPARKLE_IDLE:
          if (timer_event) {
            // Generate random pattern (0-255)
            sparkle_pattern = (uint8_t)(rand() % 256);

            // Display the pattern and count how many LEDs are on
            sparkle_num_leds_on = 0;
            for (uint8_t i = 0; i < 8; i++) {
              if (sparkle_pattern & (1 << i)) {
                HAL_GPIO_WritePin(LED_GPIO_Port[i], led_pins[i], GPIO_PIN_SET);
                sparkle_leds_to_turn_off[sparkle_num_leds_on] = i;  // Store which LED to turn off later
                sparkle_num_leds_on++;
              } else {
                HAL_GPIO_WritePin(LED_GPIO_Port[i], led_pins[i], GPIO_PIN_RESET);
              }
            }

            // Set display duration (100-1500ms)
            uint32_t display_duration = (rand() % 1401) + 100;  // 100 to 1500ms
            sparkle_display_until = now + display_duration;

            sparkle_state = SPARKLE_DISPLAY;
            timer_event = 0;  // Clear the timer event
          }
          break;

        case SPARKLE_DISPLAY:
          // Wait for display time to elapse
          if (now >= sparkle_display_until) {
            // Start turning off LEDs one by one
            sparkle_off_index = 0;
            sparkle_next_off_time = now;  // Start immediately
            sparkle_state = SPARKLE_TURN_OFF_SEQUENCE;
          }
          // Ignore timer events during display phase
          timer_event = 0;
          break;

        case SPARKLE_TURN_OFF_SEQUENCE:
          // Turn off LEDs one by one with random delays
          if (sparkle_off_index < sparkle_num_leds_on && now >= sparkle_next_off_time) {
            // Turn off the current LED
            uint8_t led_to_turn_off = sparkle_leds_to_turn_off[sparkle_off_index];
            HAL_GPIO_WritePin(LED_GPIO_Port[led_to_turn_off], led_pins[led_to_turn_off], GPIO_PIN_RESET);

            sparkle_off_index++;

            // Set next turn-off time with random delay (100-150ms)
            if (sparkle_off_index < sparkle_num_leds_on) {
              uint32_t off_delay = (rand() % 51) + 100;  // 100 to 150ms
              sparkle_next_off_time = now + off_delay;
            } else {
              // All LEDs are off, go back to idle state
              sparkle_state = SPARKLE_IDLE;
            }
          }
          // Ignore timer events during turn-off sequence
          timer_event = 0;
          break;

        default:
          sparkle_state = SPARKLE_IDLE;
          timer_event = 0;
          break;
      }
    } else if (current_mode == MODE_OFF) {
      turn_off_all_leds();
      timer_event = 0;
    }
  }
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);
  while(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_0)
  {
  }
  LL_RCC_HSI_Enable();

   /* Wait till HSI is ready */
  while(LL_RCC_HSI_IsReady() != 1)
  {

  }
  LL_RCC_HSI_SetCalibTrimming(16);
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSI);

   /* Wait till System clock is ready */
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSI)
  {

  }
  LL_SetSystemCoreClock(8000000);

   /* Update the time base */
  if (HAL_InitTick (TICK_INT_PRIORITY) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM16 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM16_Init(void)
{

  /* USER CODE BEGIN TIM16_Init 0 */

  /* USER CODE END TIM16_Init 0 */

  /* USER CODE BEGIN TIM16_Init 1 */

  /* USER CODE END TIM16_Init 1 */
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 8000-1;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 1000-1;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM16_Init 2 */
  NVIC_EnableIRQ(TIM16_IRQn);
  /* USER CODE END TIM16_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOF);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);

  /**/
  LL_GPIO_ResetOutputPin(LED0_GPIO_Port, LED0_Pin);

  /**/
  LL_GPIO_ResetOutputPin(LED1_GPIO_Port, LED1_Pin);

  /**/
  LL_GPIO_ResetOutputPin(LED2_GPIO_Port, LED2_Pin);

  /**/
  LL_GPIO_ResetOutputPin(LED3_GPIO_Port, LED3_Pin);

  /**/
  LL_GPIO_ResetOutputPin(LED4_GPIO_Port, LED4_Pin);

  /**/
  LL_GPIO_ResetOutputPin(LED5_GPIO_Port, LED5_Pin);

  /**/
  LL_GPIO_ResetOutputPin(LED6_GPIO_Port, LED6_Pin);

  /**/
  LL_GPIO_ResetOutputPin(LED7_GPIO_Port, LED7_Pin);

  /**/
  GPIO_InitStruct.Pin = Button0_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(Button0_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = Button1_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(Button1_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = Button2_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(Button2_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = Button3_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;
  LL_GPIO_Init(Button3_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LED0_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(LED0_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LED1_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(LED1_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LED2_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(LED2_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LED3_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(LED3_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LED4_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(LED4_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LED5_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(LED5_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LED6_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(LED6_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LED7_Pin;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(LED7_GPIO_Port, &GPIO_InitStruct);

  /**/
  GPIO_InitStruct.Pin = LL_GPIO_PIN_9;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void TIM16_IRQHandler(void)
{
	HAL_TIM_IRQHandler(&htim16);
	timer_event = 1; // Just set the flag
}

//Utility function to turn on all LEDs off
void turn_on_all_leds(void) {
  for (uint8_t i = 0; i < 8; i++) {
    HAL_GPIO_WritePin(LED_GPIO_Port[i], led_pins[i], GPIO_PIN_SET);
  }
}

//Utility function to turn off all LEDs off
void turn_off_all_leds(void) {
  for (uint8_t i = 0; i < 8; i++) {
    HAL_GPIO_WritePin(LED_GPIO_Port[i], led_pins[i], GPIO_PIN_RESET);
  }
}

//Utility fucntion to turn a specific LED on
void turn_on_led(uint8_t led_index) {
  // Switch off all LEDs first
  turn_off_all_leds();

  // Check if the index is within bounds
  if (led_index < 8) {
    HAL_GPIO_WritePin(LED_GPIO_Port[led_index], led_pins[led_index], GPIO_PIN_RESET);
  }
}

//Utility function to turn off a specific LED
void turn_off_led(uint8_t led_index) {
  // Switch on all LEDs first
  turn_on_all_leds();

  // Check if the index is within bounds
  if (led_index < 8) {
    HAL_GPIO_WritePin(LED_GPIO_Port[led_index], led_pins[led_index], GPIO_PIN_SET);
  }
}




// Utility fcuntion to change the lED delay
void change_led_delay(uint32_t new_delay) {
  // Stop timer
  HAL_TIM_Base_Stop_IT(&htim16);

  // Update the timer period
  //__HAL_TIM_SET_AUTORELOAD(&htim16, new_delay - 1);
  htim16.Instance->ARR = new_delay - 1;

  // REset the timer counter
  __HAL_TIM_SET_COUNTER(&htim16, 0);

  // Start timer again
  HAL_TIM_Base_Start_IT(&htim16);
}

//Mode 1 Function
void mode1_function(void) {
  turn_on_led(current_led);
  if (direction == 1) {
    if (current_led < 7) {
      current_led++;
    } else {
      direction = 0;
      current_led = 6;
    }
  } else {
    if (current_led > 0) {
      current_led--;
    } else {
      direction = 1;
      current_led = 1;
    }
  }
}

//Mode 2 Function ime reverse of Mode 1 function
void mode2_function(void) {
  // All on except one
  for (uint8_t i = 0; i < 8; i++) {
    if (i == current_led) {
      HAL_GPIO_WritePin(LED_GPIO_Port[i], led_pins[i], GPIO_PIN_RESET);
    } else {
      HAL_GPIO_WritePin(LED_GPIO_Port[i], led_pins[i], GPIO_PIN_SET);
    }
  }
  if (direction == 1) {
    if (current_led < 7) {
      current_led++;
    } else {
      direction = 0;
      current_led = 6;
    }
  } else {
    if (current_led > 0) {
      current_led--;
    } else {
      direction = 1;
      current_led = 1;
    }
  }
}

//Mode 3 Function
void mode3_function(void) {
    // Not used in new state machine - functionality moved to main loop
}

//Select the mode based on the button pressed
LED_Mode select_mode(void){
  curr_millis = HAL_GetTick();
  if (!(HAL_GPIO_ReadPin(Button1_GPIO_Port, Button1_Pin)==GPIO_PIN_SET)  && (curr_millis - prev_millis > DEBOUNCE_TIME)) {
    prev_millis = curr_millis; // Update the previous millis for debounce
    return MODE_1; // Forward and backward LED pattern
  }
  else if (!(HAL_GPIO_ReadPin(Button2_GPIO_Port, Button2_Pin)==GPIO_PIN_SET) && (curr_millis - prev_millis > DEBOUNCE_TIME)) {
    prev_millis = curr_millis; // Update the previous millis for debounce
    return MODE_2; // Reverse of Mode 1
  }
  else if (!(HAL_GPIO_ReadPin(Button3_GPIO_Port, Button3_Pin)==GPIO_PIN_SET) && (curr_millis - prev_millis > DEBOUNCE_TIME)) {
    prev_millis = curr_millis; // Update the previous millis for debounce
    return MODE_3; // Random LED pattern
  }
  prev_millis = curr_millis; // Update the previous millis for debounce
  return MODE_OFF; // Default to off if no button is pressed
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */