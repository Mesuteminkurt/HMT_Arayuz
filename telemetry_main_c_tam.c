/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : UART DMA Alımı + JSON Gönderimi (Eksiksiz Tam Kod)
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "string.h"
#include "math.h"
#include "stdlib.h"
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
TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart2_rx;

/* USER CODE BEGIN PV */

#define VCU_BUF_SIZE 512
#define STATUS_BUF_SIZE 512

volatile uint8_t cavli_status[STATUS_BUF_SIZE];
volatile uint16_t cavli_status_len;

// DMA için tamponlar
char     vcu_rx_buf[VCU_BUF_SIZE];
char     vcu_rx_buf_process[VCU_BUF_SIZE];
volatile uint8_t  vcu_line_ready = 0;

// --- VCU değişkenleri ---
uint16_t t_speed = 0;
uint16_t t_bat_v = 0;
int16_t  t_bat_a = 0;
uint16_t t_soc = 0;
int32_t  t_energy = 0;

uint8_t  t_bms_spi = 0;
uint8_t  t_motor_contact = 0;
int16_t  t_temps[7] = {0};
uint16_t t_cells[21] = {0};
int16_t  t_tank_temp = 0;

// İzolasyon Değerleri
uint32_t t_iso_n = 0;
uint32_t t_iso_p = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */
void Cavli_Send(char *msg);
void Cavli_Publish(char *topic, uint8_t *data, uint16_t len);
void Parse_VCU_CSV(char *line);
static int fmt_div10(char *buf, int16_t val);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void Cavli_Send(char *msg)
{
    HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 1000);
    HAL_UART_Transmit(&huart3, (uint8_t*)"\r\n", 2, 100);
}

void Cavli_Publish(char *topic, uint8_t *data, uint16_t len)
{
    char header[96];
    int hlen = snprintf(header, sizeof(header), "AT+MQTTPUB=3,\"%s\",\"", topic);

    HAL_UART_Transmit(&huart3, (uint8_t*)header, hlen, 1000);
    HAL_UART_Transmit(&huart3, data, len, 2000);
    HAL_UART_Transmit(&huart3, (uint8_t*)"\",0,0,0\r\n", 9, 100);
}

static int fmt_div10(char *buf, int16_t val) {
    int absval = (val < 0) ? -val : val;
    int integer = absval / 10;
    int frac = absval % 10;

    if (val < 0) return sprintf(buf, "-%d.%d", integer, frac);
    else return sprintf(buf, "%d.%d", integer, frac);
}

void Parse_VCU_CSV(char *line) {
    int field = 0;
    char *start = line;
    char *end;

    while (start != NULL && field < 40) {
        end = strchr(start, ',');
        if (end != NULL) {
            *end = '\0';
        }

        long val = atol(start);

        if (field == 0) t_speed = (uint16_t)val;
        else if (field == 1) t_bat_v = (uint16_t)val;
        else if (field == 2) t_bat_a = (int16_t)val;
        else if (field == 3) t_soc = (uint16_t)val;
        else if (field == 4) t_energy = (int32_t)val;
        else if (field == 5) t_bms_spi = (uint8_t)val;
        else if (field == 6) t_motor_contact = (uint8_t)val;
        else if (field >= 7 && field <= 13) t_temps[field - 7] = (int16_t)val;
        else if (field >= 14 && field <= 34) t_cells[field - 14] = (uint16_t)val;
        else if (field == 35) t_tank_temp = (int16_t)val;
        else if (field == 36) t_iso_n = (uint32_t)val;
        else if (field == 37) t_iso_p = (uint32_t)val;

        if (end != NULL) {
            start = end + 1;
        } else {
            start = NULL;
        }
        field++;
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    // --- VCU DMA Kesmesi ---
    if (huart->Instance == USART2)
    {
        // String güvenliği için null ekle
        if (Size >= VCU_BUF_SIZE) Size = VCU_BUF_SIZE - 1;
        vcu_rx_buf[Size] = '\0';

        // --- PAKET DOĞRULAMA (KAYMAYI ÖNLEYEN KISIM) ---
        int comma_count = 0;
        for (int i = 0; i < Size; i++) {
            if (vcu_rx_buf[i] == ',') {
                comma_count++;
            }
        }

        // 37 Virgül ve \n şartını sağlamayan bozuk paketler çöpe atılır
        if (Size > 0 && vcu_rx_buf[Size - 1] == '\n' && comma_count == 37) {
            if (vcu_line_ready == 0) {
                strcpy(vcu_rx_buf_process, (char*)vcu_rx_buf);
                vcu_line_ready = 1;
            }
        }

        // DMA'yı bir sonraki paket için yeniden kur
        HAL_UARTEx_ReceiveToIdle_DMA(&huart2, (uint8_t*)vcu_rx_buf, VCU_BUF_SIZE);
    }
    // --- Modem IT Kesmesi ---
    else if (huart->Instance == USART3)
    {
        cavli_status_len = Size;
        if (Size >= STATUS_BUF_SIZE) Size = STATUS_BUF_SIZE - 1;
        cavli_status[Size] = '\0';

        HAL_UARTEx_ReceiveToIdle_IT(&huart3, (uint8_t*)cavli_status, STATUS_BUF_SIZE);
    }
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
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

  // 1. VCU UART (USART2) DMA BAŞLATMASI
  HAL_UARTEx_ReceiveToIdle_DMA(&huart2, (uint8_t*)vcu_rx_buf, VCU_BUF_SIZE);

  // 2. Modem UART (USART3) Kesme Başlatması
  HAL_UARTEx_ReceiveToIdle_IT(&huart3, (uint8_t*)cavli_status, STATUS_BUF_SIZE);

  // --- CAVLI UYANDIRMA VE BOOT SÜRECİ ---
  // Modemin ilk güç aldıktan sonra elektriksel olarak stabil olması için bekle
  HAL_Delay(1000);

  // Modemin RST pini ile donanımsal reset atılması (Soğuk açılışlarda kilitlenmeleri çözer)
  HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(300);
  HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_SET);
  HAL_Delay(500);

  // Power Key (PWRKEY) ile modemi tetikleme
  // Hücresel modüller (Cavli, Quectel vb.) uyandırmak için PWR pinine 1-2 sn'lik bir pals (pulse) ister.
  // Eski kodda pin 1 yapılıp bırakılıyordu, bu da tuşa sürekli basılı tutmak anlamına gelir.
  HAL_GPIO_WritePin(GPIOA, PWR_Pin, GPIO_PIN_SET);
  HAL_Delay(1500); // 1.5 saniye basılı tut
  HAL_GPIO_WritePin(GPIOA, PWR_Pin, GPIO_PIN_RESET); // Bırak

  // Modemin işletim sistemini yüklemesi ve AT komutlarına yanıt vermeye hazır hale gelmesi için bekle
  HAL_Delay(4000);

  Cavli_Send("AT");                                HAL_Delay(500);
  Cavli_Send("AT");                                HAL_Delay(500);

  // Şebekeye (baz istasyonuna) tam bağlanması için ekstra süre
  HAL_Delay(10000);

  Cavli_Send("ATE0");                              HAL_Delay(500);
  Cavli_Send("AT+CGDCONT=1,\"IP\",\"internet\"");  HAL_Delay(1000);
  Cavli_Send("AT+CGACT=1,1");                      HAL_Delay(3000);
  Cavli_Send("AT+MQTTCREATE=\"subutetrahmt2.cloud.shiftr.io\",1883,\"hmt_telemetry\",250,0,\"subutetrahmt2\",\"pPXOqugkEF24x0dH\",3,0");
  HAL_Delay(2000);
  Cavli_Send("AT+MQTTCONN=3,0");

  // Shiftr.io'ya bağlandıktan sonra publish atmadan önce bekleme
  HAL_Delay(5000);

  uint32_t last_send_time = HAL_GetTick();
  uint8_t send_counter = 0; // 0-4: ana paket, 5-6: hucre paketleri

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      // 1. ADIM: VCU'dan yeni veri geldi mi?
      if (vcu_line_ready) {
          Parse_VCU_CSV(vcu_rx_buf_process);
          vcu_line_ready = 0;
      }

      // 2. ADIM: 500ms arayla gonder
      if (HAL_GetTick() - last_send_time >= 500) {
          last_send_time = HAL_GetTick();

          if (send_counter < 5) {
              // === ANA PAKET: (Karakter Limiti İçin Kısaltıldı) ===
              char json_buf[384];
              char tmp_v[16], tmp_a[16], tmp_tk[16];
              char tmp_t[7][16];

              fmt_div10(tmp_v, (int16_t)t_bat_v);
              fmt_div10(tmp_a, (int16_t)t_bat_a);
              fmt_div10(tmp_tk, t_tank_temp);

              for (int i = 0; i < 7; i++) {
                  fmt_div10(tmp_t[i], t_temps[i]);
              }

              int e_int = (int)(t_energy / 10);
              int e_frac = abs((int)(t_energy % 10));

              int len = snprintf(json_buf, sizeof(json_buf),
                  "{'spd':%d,'v':%s,'a':%s,'soc':%d,'e':%d.%d,"
                  "'spi':%d,'mc':%d,'tt':%s,"
                  "'in':%lu,'ip':%lu,"
                  "'t1':%s,'t2':%s,'t3':%s,'t4':%s,'t5':%s,'t6':%s,'t7':%s}",
                  t_speed, tmp_v, tmp_a, t_soc, e_int, e_frac,
                  t_bms_spi, t_motor_contact, tmp_tk,
                  (unsigned long)t_iso_n, (unsigned long)t_iso_p,
                  tmp_t[0], tmp_t[1], tmp_t[2], tmp_t[3],
                  tmp_t[4], tmp_t[5], tmp_t[6]);

              Cavli_Publish("hmt_telemetry", (uint8_t*)json_buf, len);

          } else if (send_counter == 5) {
              // === HUCRE PAKETI 1 (1'den 11'e) (Karakter Limiti İçin Bölündü) ===
              char json_buf[384];

              int len = snprintf(json_buf, sizeof(json_buf),
                  "{'c1':%d,'c2':%d,'c3':%d,'c4':%d,'c5':%d,'c6':%d,'c7':%d,'c8':%d,'c9':%d,'c10':%d,'c11':%d}",
                  t_cells[0],  t_cells[1],  t_cells[2],  t_cells[3],
                  t_cells[4],  t_cells[5],  t_cells[6],  t_cells[7],
                  t_cells[8],  t_cells[9],  t_cells[10]);

              Cavli_Publish("hmt_telemetry", (uint8_t*)json_buf, len);

          } else if (send_counter == 6) {
              // === HUCRE PAKETI 2 (12'den 21'e) ===
              char json_buf[384];

              int len = snprintf(json_buf, sizeof(json_buf),
                  "{'c12':%d,'c13':%d,'c14':%d,'c15':%d,'c16':%d,'c17':%d,'c18':%d,'c19':%d,'c20':%d,'c21':%d}",
                  t_cells[11], t_cells[12], t_cells[13], t_cells[14], t_cells[15],
                  t_cells[16], t_cells[17], t_cells[18], t_cells[19], t_cells[20]);

              Cavli_Publish("hmt_telemetry", (uint8_t*)json_buf, len);
          }

          send_counter++;
          if (send_counter > 6) send_counter = 0;
      }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.Prediv1Source = RCC_PREDIV1_SOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL2.PLL2State = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the Systick interrupt time
  */
  __HAL_RCC_PLLI2S_ENABLE();
}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 62499;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 1151;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);

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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(PWR_GPIO_Port, PWR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(RST_GPIO_Port, RST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : PWR_Pin */
  GPIO_InitStruct.Pin = PWR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PWR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : RST_Pin */
  GPIO_InitStruct.Pin = RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RST_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    // Donanım bayraklarını zorla silerek sonsuz döngüleri engelle
    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    __HAL_UART_CLEAR_PEFLAG(huart);

    HAL_UART_AbortReceive(huart);

    // Kilitlenmiş işlemi yeniden başlat
    if (huart->Instance == USART2) {
        HAL_UARTEx_ReceiveToIdle_DMA(&huart2, (uint8_t*)vcu_rx_buf, VCU_BUF_SIZE);
    }
    else if (huart->Instance == USART3) {
        HAL_UARTEx_ReceiveToIdle_IT(&huart3, (uint8_t*)cavli_status, STATUS_BUF_SIZE);
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
