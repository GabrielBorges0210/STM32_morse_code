/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"

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
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

/* USER CODE BEGIN PV */
volatile uint8_t ir_signal_active = 0;
volatile uint32_t absence_timer = 0;
volatile uint32_t active_signal_duration = 0;
volatile uint32_t silenceTimer = 0;

volatile uint8_t packet_received_flag = 0;
volatile uint32_t final_packet_duration = 0;

volatile uint8_t pendingSpace = 0;
volatile uint8_t pendingWord = 0;
volatile uint8_t lineSilenceFlag = 0;
volatile uint8_t is_line_empty = 1;

typedef enum
{
  TX_IDLE = 0,
  TX_DOT,
  TX_DASH,
  TX_SYMBOL_SPACE,
  TX_LETTER_SPACE,
  TX_WORD_SPACE
} tx_state_t;

volatile tx_state_t txState = TX_IDLE;
volatile uint32_t txTimer = 0;
volatile uint32_t txDurationTarget = 0;

#define TX_BUFFER_SIZE 256
char txBuffer[TX_BUFFER_SIZE];
volatile uint16_t txReadIdx = 0;
volatile uint16_t txWriteIdx = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
void processTxSymbol(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void processTxSymbol(void)
{
  while (txReadIdx != txWriteIdx)
  {
    char c = txBuffer[txReadIdx];
    txReadIdx = (txReadIdx + 1) % TX_BUFFER_SIZE;
    txTimer = 0;

    switch (c)
    {
    case '.':
      txState = TX_DOT;
      txDurationTarget = 80;
      return;
    case '-':
      txState = TX_DASH;
      txDurationTarget = 240;
      return;
    case ' ':
      txState = TX_LETTER_SPACE;
      txDurationTarget = 600; 
      return;
    case '/':
      txState = TX_WORD_SPACE;
      txDurationTarget = 1400;
      return;
    default:
      break;
    }
  }
  txState = TX_IDLE;
}

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
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  TIM2->CCER &= ~TIM_CCER_CC2E;
  HAL_TIM_Base_Start_IT(&htim3);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (packet_received_flag)
    {
      packet_received_flag = 0;

      /* Buffer para unificar as mensagens */
      uint8_t usbBuffer[4];
      uint8_t usbLen = 0;

      if (pendingWord)
      {
        usbBuffer[usbLen++] = '/';
      }
      else if (pendingSpace)
      {
        usbBuffer[usbLen++] = ' ';
      }

      pendingSpace = 0;
      pendingWord = 0;
      is_line_empty = 0;

      if (final_packet_duration >= 40 && final_packet_duration <= 140)
      {
        usbBuffer[usbLen++] = '.';
      }
      else if (final_packet_duration > 140 && final_packet_duration <= 350)
      {
        usbBuffer[usbLen++] = '-';
      }

      /* Se temos algo para enviar, enviamos TUDO JUNTO e travamos
         o código até o USB confirmar o recebimento */
      if (usbLen > 0)
      {
        while (CDC_Transmit_FS(usbBuffer, usbLen) == USBD_BUSY)
        {
          // Trava de segurança: Espera o hardware USB esvaziar
        }
      }
    }

    if (lineSilenceFlag)
    {
      lineSilenceFlag = 0;
      pendingSpace = 0;
      pendingWord = 0;

      if (!is_line_empty)
      {
        uint8_t n[] = "\r\n";
        while (CDC_Transmit_FS(n, 2) == USBD_BUSY)
        {
          // Trava de segurança: Espera o hardware USB esvaziar
        }
        is_line_empty = 1;
      }
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
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 2525;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1263;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);
}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 95;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void morse_receive_byte(uint8_t rx_byte)
{
  uint16_t next_write = (txWriteIdx + 1) % TX_BUFFER_SIZE;

  if (next_write != txReadIdx)
  {
    txBuffer[txWriteIdx] = (char)rx_byte;
    txWriteIdx = next_write;

    if ((rx_byte == '\n' || rx_byte == '\r') && txState == TX_IDLE)
    {
      processTxSymbol();
    }
  }
  else
  {
    txReadIdx = 0;
    txWriteIdx = 0;
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM3)
  {
    // PROTEÇÃO HALF-DUPLEX (O MUDO DE SOFTWARE)
    if (txState != TX_IDLE)
    {
      ir_signal_active = 0;
      absence_timer = 0;
      active_signal_duration = 0;
      silenceTimer = 0;

      /* --- LÓGICA DE TRANSMISSÃO --- */
      txTimer++;

      if (txState == TX_DOT || txState == TX_DASH)
      {
        /* 2ms ON, 2ms OFF */
        if (txTimer % 4 < 2)
          TIM2->CCER |= TIM_CCER_CC2E;
        else
          TIM2->CCER &= ~TIM_CCER_CC2E;
      }
      else
      {
        TIM2->CCER &= ~TIM_CCER_CC2E;
      }

      if (txTimer >= txDurationTarget)
      {
        TIM2->CCER &= ~TIM_CCER_CC2E;

        if (txState == TX_DOT || txState == TX_DASH)
        {
          txState = TX_SYMBOL_SPACE;
          txDurationTarget = 80;
          txTimer = 0;
        }
        else
        {
          processTxSymbol();
        }
      }
    }
    else
    {
      /* LÓGICA DE RECEPÇÃO */
      if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET)
      {
        ir_signal_active = 1;
        active_signal_duration++;
        absence_timer = 0;
      }
      else
      {
        if (ir_signal_active)
        {
          absence_timer++;
          active_signal_duration++;

          /* TOLERÂNCIA DE 15ms: Salva os pontos estilhaçados */
          if (absence_timer >= 15)
          {
            ir_signal_active = 0;
            // Desconta os 15ms finais de silêncio para ter o pulso cravado
            final_packet_duration = active_signal_duration - 15;
            active_signal_duration = 0;
            packet_received_flag = 1;
            silenceTimer = 15;
          }
        }
        else
        {
          silenceTimer++;
          if (silenceTimer == 600)
          {
            pendingSpace = 1;
          }
          if (silenceTimer == 1400)
          {
            pendingSpace = 0;
            pendingWord = 1;
          }
          if (silenceTimer == 2000)
          {
            lineSilenceFlag = 1;
          }
        }
      }
    }
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  // Keeping EXTI only as an optional wake-up source if low-power mode is added later.
  // The duration tracking is now entirely handled by polling in the TIM3 callback.
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
