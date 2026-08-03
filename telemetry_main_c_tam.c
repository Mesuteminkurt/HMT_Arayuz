/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (Timer Interrupt Telemetry - Full Code)
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

/* USER CODE BEGIN PV */

#define VCU_BUF_SIZE 512
#define STATUS_BUF_SIZE 512
#define JSON_BUF_SIZE 1024

volatile uint8_t cavli_status[STATUS_BUF_SIZE];
volatile uint16_t cavli_status_len;
volatile uint8_t hb_counter = 0;

uint8_t  vcu_rx_byte;
char     vcu_rx_buf[VCU_BUF_SIZE];
volatile uint16_t vcu_rx_idx = 0;

// Gelen yeni 36 verili yapiya uygun degiskenler
uint16_t t_speed = 0;
uint16_t t_bat_v = 0;
int16_t  t_bat_a = 0;
uint16_t t_soc = 0;
int32_t  t_energy = 0;
uint8_t  t_bms_spi = 0;
uint8_t  t_motor_contact = 0;
int16_t  t_temps[7] = {0};    // 7 Adet Batarya Sicakligi
uint16_t t_cells[21] = {0};   // 21 Adet Hucre Voltajı (Verici 21 tane atiyor)
int16_t  t_tank_temp = 0;

volatile uint8_t  vcu_receiving = 0;
char json_buf[JSON_BUF_SIZE];
volatile uint8_t publish_flag = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

void Cavli_Send(char *msg);
void Cavli_Publish(char *topic, uint8_t *data, uint16_t len);
void Parse_VCU_CSV(char *line);

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

    // Orijinal ve dogru olan yapi:
    int hlen = snprintf(header, sizeof(header), "AT+MQTTPUB=3,\"%s\",\"", topic);
    HAL_UART_Transmit(&huart3, (uint8_t*)header, hlen, 1000);

    // Veriyi (data) gonderiyoruz
    HAL_UART_Transmit(&huart3, data, len, 2000);

    // Mesaji kapatip zorunlu parametreleri (0,0,0) ekliyoruz
    HAL_UART_Transmit(&huart3, (uint8_t*)"\",0,0,0\r\n", 9, 100);
}

void Parse_VCU_CSV(char *line) {
    char *tok;
    int field = 0;

    tok = strtok(line, ",");
    while (tok != NULL && field < 40) {
        int val = atoi(tok);

        // 0-6: Temel Veriler
        if (field == 0) t_speed = (uint16_t)val;
        else if (field == 1) t_bat_v = (uint16_t)val;
        else if (field == 2) t_bat_a = (int16_t)val;
        else if (field == 3) t_soc = (uint16_t)val;
        else if (field == 4) t_energy = (int32_t)val;
        else if (field == 5) t_bms_spi = (uint8_t)val;
        else if (field == 6) t_motor_contact = (uint8_t)val;

        // 7-13: 7 Adet Sicaklik
        else if (field >= 7 && field <= 13) {
            t_temps[field - 7] = (int16_t)val;
        }

        // 14-34: 21 Adet Hucre Voltaji
        else if (field >= 14 && field <= 34) {
            t_cells[field - 14] = (uint16_t)val;
        }

        // 35: Tank Sicakligi
        else if (field == 35) {
            t_tank_temp = (int16_t)val;
        }

        field++;
        tok = strtok(NULL, ",\n\r");
    }

    if (field >= 35) vcu_receiving = 1;
}

static int fmt_div10(char *buf, int16_t val) {
    int absval = (val < 0) ? -val : val;
    int integer = absval / 10;
    int frac = absval % 10;

    if (val < 0) {
        return sprintf(buf, "-%d.%d", integer, frac);
    } else {
        return sprintf(buf, "%d.%d", integer, frac);
    }
}

// 1. TEMEL ARAÇ VERİLERİ VE SICAKLIKLAR (AT komut buffer sınırı içinde)
int Build_JSON_Main(char *buf, int bufsize) {
    int pos = 0;
    char tmp[16];

    pos += snprintf(&buf[pos], bufsize - pos, "{");

    // Tek tırnak (') kullanımı: AT komut formatını korumak için
    pos += snprintf(&buf[pos], bufsize - pos, "'speed':%d", t_speed);

    fmt_div10(tmp, (int16_t)t_bat_v);
    pos += snprintf(&buf[pos], bufsize - pos, ",'bat_v':%s", tmp);

    fmt_div10(tmp, t_bat_a);
    pos += snprintf(&buf[pos], bufsize - pos, ",'bat_a':%s", tmp);

    pos += snprintf(&buf[pos], bufsize - pos, ",'soc':%d", t_soc);

    int e_int = (int)(t_energy / 10);
    int e_frac = abs((int)(t_energy % 10));
    pos += snprintf(&buf[pos], bufsize - pos, ",'energy':%d.%d", e_int, e_frac);

    pos += snprintf(&buf[pos], bufsize - pos, ",'bms_spi':%d", t_bms_spi);
    pos += snprintf(&buf[pos], bufsize - pos, ",'motor_contact':%d", t_motor_contact);

    // Max batarya sicakligini bul (Arayüz "bat_temp" degerini buradan alıyor)
    int16_t max_temp = t_temps[0];
    for (int i = 1; i < 7; i++) {
        if (t_temps[i] > max_temp) max_temp = t_temps[i];
    }
    fmt_div10(tmp, max_temp);
    pos += snprintf(&buf[pos], bufsize - pos, ",'bat_temp':%s", tmp);

    // 7 Adet Sicakligi pakete ekliyoruz (Arayüzün beklediği isimlerle: bat_temp_1, vb.)
    for (int i = 0; i < 7; i++) {
        fmt_div10(tmp, t_temps[i]);
        pos += snprintf(&buf[pos], bufsize - pos, ",'bat_temp_%d':%s", i + 1, tmp);
    }

    fmt_div10(tmp, t_tank_temp);
    pos += snprintf(&buf[pos], bufsize - pos, ",'tank_temp':%s", tmp);

    pos += snprintf(&buf[pos], bufsize - pos, "}");

    return pos;
}

// 2. SADECE HÜCRE VOLTAJLARI (Vericideki 21 hucre ile senkronize edildi)
int Build_JSON_Cells(char *buf, int bufsize) {
    int pos = 0;
    pos += snprintf(&buf[pos], bufsize - pos, "{");

    // 32 yerine vericiden gelen 21 hücre voltajini gonderiyoruz
    for (int i = 0; i < 21; i++) {
        if (i == 0) {
            pos += snprintf(&buf[pos], bufsize - pos, "'c%d':%d.%03d", i + 1, t_cells[i] / 1000, t_cells[i] % 1000);
        } else {
            pos += snprintf(&buf[pos], bufsize - pos, ",'c%d':%d.%03d", i + 1, t_cells[i] / 1000, t_cells[i] % 1000);
        }
    }

    pos += snprintf(&buf[pos], bufsize - pos, "}");
    return pos;
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        if (vcu_rx_byte == '\n') {
            vcu_rx_buf[vcu_rx_idx] = '\0';

            Parse_VCU_CSV(vcu_rx_buf);

            vcu_rx_idx = 0;
        }
        else if (vcu_rx_idx < VCU_BUF_SIZE - 2) {
            if (vcu_rx_byte != '\r') {
                vcu_rx_buf[vcu_rx_idx++] = vcu_rx_byte;
            }
        }
        HAL_UART_Receive_IT(&huart2, &vcu_rx_byte, 1);
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART3)
    {
        cavli_status_len = Size;
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
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */

  HAL_TIM_Base_Start_IT(&htim1);

  HAL_UART_Receive_IT(&huart2, &vcu_rx_byte, 1);
  HAL_UARTEx_ReceiveToIdle_IT(&huart3, (uint8_t*)cavli_status, STATUS_BUF_SIZE);

  HAL_GPIO_WritePin(GPIOA, PWR_Pin, 1);
  HAL_Delay(300);
  Cavli_Send("AT");                                HAL_Delay(500);
  Cavli_Send("ATE0");                              HAL_Delay(500);
  Cavli_Send("AT+CGDCONT=1,\"IP\",\"internet\"");  HAL_Delay(1000);
  Cavli_Send("AT+CGACT=1,1");                      HAL_Delay(3000);
  Cavli_Send("AT+MQTTCREATE=\"subutetrahmt.cloud.shiftr.io\",1883,\"hmt_telemetry\",250,0,\"subutetrahmt\",\"pPXOqugkEF24x0dH\",3,0");
  HAL_Delay(2000);
  Cavli_Send("AT+MQTTCONN=3,0");                   HAL_Delay(3000);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      if (publish_flag) {
          publish_flag = 0;

          // 1. Ana verileri "hmt_telemetry" topic'ine gönder
          int len_main = Build_JSON_Main(json_buf, JSON_BUF_SIZE);
          Cavli_Publish("hmt_telemetry", (uint8_t*)json_buf, len_main);

          // Modülün ilk komutu işleyebilmesi için bekleme süresi
          HAL_Delay(600);

          // 2. Hücre voltajlarını "hmt_cells" topic'ine gönder
          int len_cells = Build_JSON_Cells(json_buf, JSON_BUF_SIZE);
          Cavli_Publish("hmt_cells", (uint8_t*)json_buf, len_cells);
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
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

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
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
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
}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{
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
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

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
}

/* USER CODE BEGIN 4 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        hb_counter++;
        if (hb_counter >= 1)
        {
            hb_counter = 0;
            if (vcu_receiving == 1) {
                // Sadece bayrağı set ediyoruz, bekletici işlem yok.
                publish_flag = 1;
            }
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
  __disable_irq();
  while (1)
  {
  }
}
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
