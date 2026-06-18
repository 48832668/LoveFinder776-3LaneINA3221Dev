/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.cpp
  * @brief          : Main program body (C++17)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "adc.h"
#include "dma.h"
#include "spi.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "../../LoveFinderLib/ST7735/st7735.hpp"
#include "../../LoveFinderLib/BUTTON/BUTTON.hpp"
#include "tim.h"
#include <cstdio>
#include <cstdint>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

// BUTTON instance (PA1, EXTI falling edge)
BUTTON keyBtn;

// Button statistics
struct ButtonStats {
    uint16_t click;
    uint16_t doubleClick;
    uint16_t longPress;
};
static ButtonStats stats = {0, 0, 0};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_ADC1_Init();
  MX_TIM17_Init();
  /* USER CODE BEGIN 2 */

  // 启动定时器（1ms周期，用于长按检测）
  HAL_TIM_Base_Start_IT(&htim17);

  // 设置定时器用于按钮长按检测
  v_BUTTON_SetTimer(&htim17);

  // Enable LCD backlight
  HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, GPIO_PIN_SET);

  // Initialize LCD
  ST7735_Init();

  // Clear screen to black
  ST7735_FillScreen(ST7735_BLACK);

  // Initialize button (PA1, active low - pressed = low)
  BUTTON_Config btnConfig = BUTTON_Config::getDefault();
  btnConfig.activeLow = true;  // Button pressed = low level
  keyBtn.init(KEY1_GPIO_Port, KEY1_Pin, btnConfig);

  // Initial display
  ST7735_WriteString(10, 10, "Button Test", Font_7x10, ST7735_GREEN, ST7735_BLACK);
  ST7735_WriteString(10, 25, "Click:", Font_7x10, ST7735_WHITE, ST7735_BLACK);
  ST7735_WriteString(10, 38, "Double:", Font_7x10, ST7735_WHITE, ST7735_BLACK);
  ST7735_WriteString(10, 51, "Long:", Font_7x10, ST7735_WHITE, ST7735_BLACK);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // Update button state (polling mode)
    e_BUTTON_Event event = keyBtn.update();

    // Handle button events
    if (event != e_BUTTON_Event::NONE)
    {
        char buf[20];

        switch (event)
        {
            case e_BUTTON_Event::CLICK:
                stats.click++;
                ST7735_WriteString(60, 25, "        ", Font_7x10, ST7735_YELLOW, ST7735_BLACK);
                snprintf(buf, sizeof(buf), "%d", stats.click);
                ST7735_WriteString(60, 25, buf, Font_7x10, ST7735_YELLOW, ST7735_BLACK);
                break;

            case e_BUTTON_Event::DOUBLE_CLICK:
                stats.doubleClick++;
                ST7735_WriteString(70, 38, "        ", Font_7x10, ST7735_CYAN, ST7735_BLACK);
                snprintf(buf, sizeof(buf), "%d", stats.doubleClick);
                ST7735_WriteString(70, 38, buf, Font_7x10, ST7735_CYAN, ST7735_BLACK);
                break;

            case e_BUTTON_Event::LONG_PRESS:
                stats.longPress++;
                ST7735_WriteString(55, 51, "        ", Font_7x10, ST7735_MAGENTA, ST7735_BLACK);
                snprintf(buf, sizeof(buf), "%d", stats.longPress);
                ST7735_WriteString(55, 51, buf, Font_7x10, ST7735_MAGENTA, ST7735_BLACK);
                break;

            default:
                break;
        }
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
 * @brief GPIO EXTI Callback - not used in polling mode
 */
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    // 按键检测使用轮询模式，此回调留空
    (void)GPIO_Pin;
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
#ifdef USE_FULL_ASSERT
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