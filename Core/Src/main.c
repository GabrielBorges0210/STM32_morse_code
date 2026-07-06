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
/* --- VARIAVEIS DO RECEPTOR OTICO (ENVELOPE DETECTOR) --- */
volatile uint8_t sinal_ir_ativo = 0;
volatile uint32_t timer_de_ausencia = 0;
volatile uint32_t duracao_sinal_ativo = 0;
volatile uint32_t silence_timer = 0;

volatile uint8_t pacote_recebido_flag = 0;
volatile uint32_t pacote_duracao_final = 0;

volatile uint8_t pending_space = 0;
volatile uint8_t pending_word = 0;
volatile uint8_t flag_silencio_linha = 0;
volatile uint8_t linha_vazia = 1;

/* --- VARIAVEIS DO TRANSMISSOR OTICO (FSM) --- */
typedef enum
{
  TX_IDLE = 0,
  TX_PONTO,
  TX_TRACO,
  TX_ESPACO_SIMBOLO,
  TX_ESPACO_LETRA,
  TX_ESPACO_PALAVRA
} TX_State_t;

volatile TX_State_t tx_state = TX_IDLE;
volatile uint32_t tx_timer = 0;
volatile uint32_t tx_duration_target = 0;

#define TX_BUFFER_SIZE 256
char tx_buffer[TX_BUFFER_SIZE];
volatile uint16_t tx_read_idx = 0;
volatile uint16_t tx_write_idx = 0;

/* --- VARIAVEIS DA UART --- */
uint8_t uart_rx_byte;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
/* USER CODE BEGIN PFP */
void Process_TX_Symbol(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void Process_TX_Symbol(void)
{
  while (tx_read_idx != tx_write_idx)
  {
    char c = tx_buffer[tx_read_idx];
    tx_read_idx = (tx_read_idx + 1) % TX_BUFFER_SIZE;
    tx_timer = 0;

    switch (c)
    {
    case '.':
      tx_state = TX_PONTO;
      tx_duration_target = 80;
      return;
    case '-':
      tx_state = TX_TRACO;
      tx_duration_target = 240;
      return;
    case ' ':
      tx_state = TX_ESPACO_LETRA;
      tx_duration_target = 240;
      return;
    case '/':
      tx_state = TX_ESPACO_PALAVRA;
      tx_duration_target = 560;
      return;
    default:
      break;
    }
  }
  tx_state = TX_IDLE;
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
    if (pacote_recebido_flag)
    {
      pacote_recebido_flag = 0;

      if (pending_word)
      {
        uint8_t b = '/';
        CDC_Transmit_FS(&b, 1);
      }
      else if (pending_space)
      {
        uint8_t e = ' ';
        CDC_Transmit_FS(&e, 1);
      }

      pending_space = 0;
      pending_word = 0;
      linha_vazia = 0;

      if (pacote_duracao_final >= 40 && pacote_duracao_final <= 140)
      {
        uint8_t p = '.';
        CDC_Transmit_FS(&p, 1);
      }
      else if (pacote_duracao_final > 140 && pacote_duracao_final <= 350)
      {
        uint8_t t = '-';
        CDC_Transmit_FS(&t, 1);
      }
    }

    if (flag_silencio_linha)
    {
      flag_silencio_linha = 0;
      pending_space = 0;
      pending_word = 0;

      if (!linha_vazia)
      {
        uint8_t n[] = "\r\n";
        CDC_Transmit_FS(n, 2);
        linha_vazia = 1;
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
  /* --- RECEPÇÃO DE COMANDOS DO PC (VIA USB CDC) --- */
  void Morse_Receive_Byte(uint8_t rx_byte)
  {
    uint16_t next_write = (tx_write_idx + 1) % TX_BUFFER_SIZE;

    if (next_write != tx_read_idx)
    {
      tx_buffer[tx_write_idx] = (char)rx_byte;
      tx_write_idx = next_write;

      if ((rx_byte == '\n' || rx_byte == '\r') && tx_state == TX_IDLE)
      {
        Process_TX_Symbol();
      }
    }
    else
    {
      tx_read_idx = 0;
      tx_write_idx = 0; // Trava de segurança (Overflow)
    }
  }

  /* --- INTERRUPCAO DA BASE DE TEMPO (1kHz / 1ms) --- */
  void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef * htim)
  {
    if (htim->Instance == TIM3)
    {
      if (sinal_ir_ativo)
      {
        timer_de_ausencia++;
        duracao_sinal_ativo++;

        if (timer_de_ausencia >= 4)
        {
          sinal_ir_ativo = 0;
          pacote_duracao_final = duracao_sinal_ativo;
          duracao_sinal_ativo = 0;
          pacote_recebido_flag = 1;
          silence_timer = 0;
        }
      }
      else
      {
        silence_timer++;
        if (silence_timer == 240)
          pending_space = 1;
        if (silence_timer == 560)
        {
          pending_space = 0;
          pending_word = 1;
        }
        if (silence_timer == 1000)
          flag_silencio_linha = 1;
      }

      if (tx_state != TX_IDLE)
      {
        tx_timer++;
        if (tx_state == TX_PONTO || tx_state == TX_TRACO)
        {
          if (tx_timer % 2 != 0)
            TIM2->CCER |= TIM_CCER_CC2E;
          else
            TIM2->CCER &= ~TIM_CCER_CC2E;
        }
        else
        {
          TIM2->CCER &= ~TIM_CCER_CC2E;
        }

        if (tx_timer >= tx_duration_target)
        {
          TIM2->CCER &= ~TIM_CCER_CC2E;
          if (tx_state == TX_PONTO || tx_state == TX_TRACO)
          {
            tx_state = TX_ESPACO_SIMBOLO;
            tx_duration_target = 80;
            tx_timer = 0;
          }
          else
          {
            Process_TX_Symbol();
          }
        }
      }
    }
  }

  /* --- INTERRUPCAO EXTERNA (Pino PA0 / TSOP) --- */
  void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
  {
    if (GPIO_Pin == GPIO_PIN_0)
    {
      if (tx_state == TX_IDLE)
      {
        sinal_ir_ativo = 1;
        timer_de_ausencia = 0;
      }
    }
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
