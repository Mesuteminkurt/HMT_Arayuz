/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
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
CAN_HandleTypeDef hcan1;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart4;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */
CAN_RxHeaderTypeDef RxHeader;
uint8_t RxData[8];
// Kesme icinden ana donguye veri tasimak icin global veri dizileri
volatile uint16_t g_pack_voltage = 0;
volatile int16_t  g_pack_current = 0;
volatile uint16_t g_pack_soc = 0;
volatile uint8_t  g_spi_comm_ok = 1; // 1: Haberlesme Var, 0: Koptu
volatile uint8_t  g_inverter_status = 0; // Inverter (Cikis) Durumu (1: Aktif, 0: Pasif)
volatile uint16_t g_cell_voltages[32] = {0};
volatile int16_t  g_temperatures[32] = {0};
// CAN Baglantisi Koptu mu Kontrolu Icin Zaman Damgasi
volatile uint32_t g_last_can_rx_ms_bms = 0;
volatile uint8_t  g_can_connected_bms = 0;


volatile uint32_t g_last_can_rx_ms_iso = 0;
volatile uint8_t  g_can_connected_iso = 0;
// --- Tank Sicakligi (sabit, ileride CAN'dan baglanacak) ---
// Birim: 0.1°C (ornek: 250 = 25.0°C)
volatile int16_t g_tank_temp = 250;


// İzolasyon Değerleri (0x500)
volatile uint32_t g_iso_n = 0;
volatile uint32_t g_iso_p = 0;

// --- Hiz Olcum Degiskenleri ---
int Is_First_Captured = 0, Is_First_Captured2 = 0;
volatile uint32_t IC_Val1 = 0, IC_Val2 = 0;
volatile uint32_t IC_Val12 = 0, IC_Val22 = 0;
volatile uint16_t tim1_overflow_flag = 0, tim1_overflow_flag2 = 0;
uint16_t speed = 0, speed1 = 0, speed2 = 0;
volatile float frequency = 0.0f, frequency2 = 0.0f;
volatile uint32_t tick_spd = 0, tick_spd2 = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_CAN1_Init(void);
static void MX_UART4_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART6_UART_Init(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Hucre Voltajlarini Nextion'daki x objelerine esleme dizisi (1'den 21'e kadar)
const char* cell_obj[21] = {
    "x28", "x29", "x30", "x31", "x32", "x33", "x34", "x35", "x36", "x37", // 1-10
    "x10", "x11", "x12", "x13", "x14",                                    // 11-15
    "x17", "x16", "x15",                                                  // 16-18
    "x20", "x19", "x18"                                                   // 19-21
};
// Batarya Sicakliklarini Nextion'daki x objelerine esleme dizisi (1'den 7'ye kadar)
const char* temp_obj[7] = {
    "x21", "x22", "x23", "x24", "x25", "x26", "x27"
};
/**
  * @brief Nextion ekrandaki bir bilesenin degerini (val) günceller
  */
void Nextion_SetVal(const char* obj, int value) {
    char buffer[32];
    uint8_t cmd_end[3] = {0xFF, 0xFF, 0xFF};
    sprintf(buffer, "%s.val=%d", obj, value);
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
    HAL_UART_Transmit(&huart4, cmd_end, 3, 100);
}
/**
  * @brief Nextion ekranda sayfa degistirir
  */
void Nextion_ChangePage(const char* page_name) {
    char buffer[32];
    uint8_t cmd_end[3] = {0xFF, 0xFF, 0xFF};
    sprintf(buffer, "page %s", page_name);
    HAL_UART_Transmit(&huart4, (uint8_t*)buffer, strlen(buffer), 100);
    HAL_UART_Transmit(&huart4, cmd_end, 3, 100);
}
/**
  * @brief Telemetri kartina USART6 uzerinden CSV formatinda tum verileri gonderir
  *
  * Format (38 alan, virgülle ayrılmış, '\n' ile biter):
  * speed,bat_v,bat_a,soc,energy,bms_spi,motor_contact,
  * temp1,temp2,temp3,temp4,temp5,temp6,temp7,
  * cell1,cell2,...,cell21,
  * tank_temp,iso_n,iso_p
  *
  * Tum degerler ham integer olarak gonderilir.
  * Telemetri karti (Cavli) bu degerleri alip gerekli bolmeleri yaparak
  * JSON formatina cevirir ve MQTT ile web arayuzune iletir.
  */
void Send_Telemetry_USART6(void) {
    char tx_buf[320];
    uint8_t t_idx[] = {0, 1, 2, 3, 4, 6, 7};
    uint16_t energy = g_pack_voltage * 21;
    int len = snprintf(tx_buf, sizeof(tx_buf),
        "%d,%d,%d,%d,%d,%d,%d,"
        "%d,%d,%d,%d,%d,%d,%d,"
        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
        "%d,%lu,%lu\n",
        speed, g_pack_voltage, g_pack_current, g_pack_soc, energy, g_spi_comm_ok, g_inverter_status,
        g_temperatures[t_idx[0]], g_temperatures[t_idx[1]], g_temperatures[t_idx[2]], g_temperatures[t_idx[3]],
        g_temperatures[t_idx[4]], g_temperatures[t_idx[5]], g_temperatures[t_idx[6]],
        g_cell_voltages[0],  g_cell_voltages[1],  g_cell_voltages[2],  g_cell_voltages[3],
        g_cell_voltages[4],  g_cell_voltages[5],  g_cell_voltages[6],  g_cell_voltages[7],
        g_cell_voltages[8],  g_cell_voltages[9],  g_cell_voltages[10], g_cell_voltages[11],
        g_cell_voltages[12], g_cell_voltages[13], g_cell_voltages[14], g_cell_voltages[15],
        g_cell_voltages[16], g_cell_voltages[17], g_cell_voltages[18], g_cell_voltages[19],
        g_cell_voltages[20],
        g_tank_temp,
        (unsigned long)g_iso_n,   // 37. alan: Izolasyon direnci N (ohm)
        (unsigned long)g_iso_p    // 38. alan: Izolasyon direnci P (ohm)
    );
    if (len > 0 && len < (int)sizeof(tx_buf)) {
        HAL_UART_Transmit(&huart6, (uint8_t*)tx_buf, len, 100);
    }
}
/**
  * @brief CAN RX0 Interrupt Callback Fonksiyonu
  */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == CAN1)
    {
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK)
        {
            uint32_t id = RxHeader.StdId;
            if (id == 0x100) {
            	 g_last_can_rx_ms_bms = HAL_GetTick();
            	            g_can_connected_bms = 1;
                g_pack_voltage = (RxData[0] << 8) | RxData[1];
                g_pack_current = (int16_t)((RxData[2] << 8) | RxData[3]);
                g_pack_soc = (RxData[4] << 8) | RxData[5];
            }
            else if (id == 0x101) {
            	 g_last_can_rx_ms_bms = HAL_GetTick();
            	            g_can_connected_bms = 1;
                g_spi_comm_ok = (RxData[0] == 1) ? 0 : 1;
                g_inverter_status = RxData[4]; // Vericiden gelen Inverter durumu (Byte 4)
            }
            else if (id >= 0x200 && id <= 0x20F) {
            	 g_last_can_rx_ms_bms = HAL_GetTick();
            	            g_can_connected_bms = 1;
                uint8_t index = id - 0x200;
                for (int i = 0; i < 4; i++) {
                    uint8_t cell_idx = (index * 4) + i;
                    if (cell_idx < 32) {
                        g_cell_voltages[cell_idx] = (RxData[i*2] << 8) | RxData[i*2 + 1];
                    }
                }
            }
            // SICAKLIK PAKETLERİ DÜZELTMESİ
            else if (id >= 0x300 && id <= 0x30F) {
            	 g_last_can_rx_ms_bms = HAL_GetTick();
            	            g_can_connected_bms = 1;
                uint8_t index = id - 0x300;
                for (int i = 0; i < 4; i++) {
                    uint8_t temp_idx = (index * 4) + i;
                    if (temp_idx < 32) {
                        int16_t val = (int16_t)((RxData[i*2] << 8) | RxData[i*2 + 1]);
                        g_temperatures[temp_idx] = val;
                    }
                }
            }
            else if(id == 0x500){
            	 g_last_can_rx_ms_iso = HAL_GetTick();
            	            g_can_connected_iso = 1;
                            g_iso_n = ((uint32_t)RxData[0] << 16) | ((uint32_t)RxData[1] << 8) | RxData[2];
                            g_iso_p = ((uint32_t)RxData[3] << 16) | ((uint32_t)RxData[4] << 8) | RxData[5];
            }
            // TODO: Tank sicakligi icin CAN ID'si eklenecek
            // else if (id == 0xXXX) {
            //     g_tank_temp = (int16_t)((RxData[0] << 8) | RxData[1]);
            // }
        }
    }
}
/**
  * @brief TIM1 Input Capture Callback — Hiz sensorlerinden kenar yakalama
  */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        // --- Kanal 1: Hiz Sensoru 1 ---
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
        {
            if (Is_First_Captured == 0)
            {
                IC_Val1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
                tim1_overflow_flag = 0;
                Is_First_Captured = 1;
            }
            else
            {
                IC_Val2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
                tick_spd = (IC_Val2 + (tim1_overflow_flag * 65535)) - IC_Val1;
                if (tick_spd > 0)
                    frequency = (16000000.0f / 160.0f) / tick_spd; // 100000 / tick
                Is_First_Captured = 0;
            }
            speed1 = (uint16_t)(frequency * 0.378f);
            if (speed1 > 600) speed1 = 600;
        }
        // --- Kanal 2: Hiz Sensoru 2 ---
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
        {
            if (Is_First_Captured2 == 0)
            {
                IC_Val12 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
                tim1_overflow_flag2 = 0;
                Is_First_Captured2 = 1;
            }
            else
            {
                IC_Val22 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
                tick_spd2 = (IC_Val22 + (tim1_overflow_flag2 * 65535)) - IC_Val12;
                if (tick_spd2 > 0)
                    frequency2 = (16000000.0f / 160.0f) / tick_spd2;
                Is_First_Captured2 = 0;
            }
            speed2 = (uint16_t)(frequency2 * 0.378f);
            if (speed2 > 600) speed2 = 600;
        }
    }
}
/**
  * @brief TIM1 Overflow Callback — Sinyal gelmezse hizi sifirla
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        tim1_overflow_flag++;
        tim1_overflow_flag2++;
        // 2 overflow (~1.3 saniye) boyunca kenar yakalanamazsa arac durmus demektir
        if (tim1_overflow_flag > 1)
        {
            frequency = 0;
            speed1 = 0;
        }
        if (tim1_overflow_flag2 > 1)
        {
            frequency2 = 0;
            speed2 = 0;
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
  MX_CAN1_Init();
  MX_UART4_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */
  // CAN Filtre Ayarlari (Her ID'yi kabul et)
  CAN_FilterTypeDef canfilterconfig;
  canfilterconfig.FilterActivation = CAN_FILTER_ENABLE;
  canfilterconfig.FilterBank = 0;
  canfilterconfig.FilterMode = CAN_FILTERMODE_IDMASK;
  canfilterconfig.FilterScale = CAN_FILTERSCALE_32BIT;
  canfilterconfig.FilterIdHigh = 0x0000;
  canfilterconfig.FilterIdLow = 0x0000;
  canfilterconfig.FilterMaskIdHigh = 0x0000;
  canfilterconfig.FilterMaskIdLow = 0x0000;
  canfilterconfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  HAL_CAN_ConfigFilter(&hcan1, &canfilterconfig);
  HAL_CAN_Start(&hcan1);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
  // --- Hiz Olcum Timer Baslat ---
  HAL_TIM_Base_Start_IT(&htim1);               // Overflow (period elapsed) kesmesi
  HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);  // Kanal 1 yakalama kesmesi
  HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_2);  // Kanal 2 yakalama kesmesi
  // Gonderim Zamanlayici Degiskenleri
  uint32_t last_fast_ms = 0;
  uint32_t last_slow_ms = 0;
  uint8_t slow_tx_state = 0; // Yavas veri gonderimi sirasi
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      uint32_t now = HAL_GetTick();
      // 1. HIZLI VERILER VE TELEMETRI - Her 200ms'de bir
      if (now - last_fast_ms >= 200) {
          last_fast_ms = now;
          // Hiz hesapla (iki sensorun ortalamasi)
          speed = speed1;
          Nextion_SetVal("n0", speed);
          // CAN bus koptu mu kontrolu (5000ms yani 5 saniye icinde yeni mesaj gelmediyse)
          if (now - g_last_can_rx_ms_bms > 5000) {
              g_can_connected_bms = 0;
          }

          if (now - g_last_can_rx_ms_iso > 5000) {
                        g_can_connected_iso = 0;
                    }
          // A. Hücre Gerilimi Kontrolü (2.6V = 2600mV)
          bool cell_undervoltage = false;
          for(int i = 0; i < 21; i++) {
              if (g_cell_voltages[i] < 2600 && g_cell_voltages[i] > 0) { // 0'ı atla (bağlı değilse)
                  cell_undervoltage = true;
                  break;
              }
          }
          Nextion_SetVal("bt14", cell_undervoltage ? 1 : 0);
          // B. Sıcaklık Kontrolü (30°C = 300)
          // valid_indices dizisi ile boş olan 5. indeksi atlıyoruz
          bool over_temp = false;
          uint8_t temp_check_indices[] = {0, 1, 2, 3, 4, 6, 7};
          for(int i = 0; i < 7; i++) {
              if (g_temperatures[temp_check_indices[i]] > 300) {
                  over_temp = true;
                  break;
              }
          }
          Nextion_SetVal("bt13", over_temp ? 1 : 0);
          Nextion_SetVal("bt6", g_can_connected_bms);               // CANbus aktiflik durumu
          Nextion_SetVal("bt9", g_can_connected_iso);               // CANbus aktiflik durumu
          Nextion_SetVal("x38", g_pack_soc);                    // SOC (%)
          Nextion_SetVal("x39", g_tank_temp);                    // Tank Temp
          Nextion_SetVal("x2", g_pack_voltage);                 // Batarya Toplam Voltaj
          Nextion_SetVal("x0", (g_pack_voltage * 21));          // Kalan Enerji (Wh)
          Nextion_SetVal("x1", g_pack_current);                 // Batarya Akimi
          Nextion_SetVal("bt12", g_spi_comm_ok);                // BMS SPI Durumu
          Nextion_SetVal("bt15", g_inverter_status);            // Inverter / GPIO Cikis Durumu
          Nextion_SetVal("n1", (int)g_iso_n);
          Nextion_SetVal("n2", (int)g_iso_p);
          // === TELEMETRI KARTI: USART6 uzerinden veri gonder ===
          Send_Telemetry_USART6();
      }
      // 2. YAVAS VERILER (Hucreler ve Sicakliklar)
      if (now - last_slow_ms >= 30) {
          last_slow_ms = now;
          if (slow_tx_state < 21) {
              // 0-20 arası hücreler
              Nextion_SetVal(cell_obj[slow_tx_state], g_cell_voltages[slow_tx_state]);
          }
          else if (slow_tx_state < 28) {
              // 21-27 arası sıcaklıklar
              uint8_t t_idx = slow_tx_state - 21; // 0, 1, 2, 3, 4, 5, 6
              // Veri çekilecek gerçek dizi indeksleri: 0, 1, 2, 3, 4, 6, 7
              // (İndeks 5'i yani boş olanı atlıyoruz)
              uint8_t source_idx;
              if (t_idx < 5) {
                  source_idx = t_idx;
              } else {
                  source_idx = t_idx + 1; // 5. indeksi atla, 6 ve 7'yi al
              }
              Nextion_SetVal(temp_obj[t_idx], g_temperatures[source_idx]);
          }
          else if (slow_tx_state == 28) {
              // En yüksek 3 sıcaklığı hesapla (sadece geçerli indeksleri kullanarak)
              int16_t t1 = -32768, t2 = -32768, t3 = -32768;
              uint8_t valid_indices[] = {0, 1, 2, 3, 4, 6, 7};
              for(int i = 0; i < 7; i++) {
                  int16_t val = g_temperatures[valid_indices[i]];
                  if(val > t1) { t3 = t2; t2 = t1; t1 = val; }
                  else if(val > t2) { t3 = t2; t2 = val; }
                  else if(val > t3) { t3 = val; }
              }
              if(t1 == -32768) t1 = 0;
              if(t2 == -32768) t2 = 0;
              if(t3 == -32768) t3 = 0;
              Nextion_SetVal("x3", t1);
              Nextion_SetVal("x4", t2);
              Nextion_SetVal("x5", t3);
          }
          slow_tx_state++;
          if (slow_tx_state > 28) { slow_tx_state = 0; }
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */
  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */
  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 2;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */
  /* USER CODE END CAN1_Init 2 */

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
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM1_Init 1 */
  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 159;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
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
  if (HAL_TIM_IC_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 10;
  if (HAL_TIM_IC_ConfigChannel(&htim1, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim1, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */
  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */
  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */
  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */
  /* USER CODE END UART4_Init 2 */

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
  huart2.Init.BaudRate = 9600;
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
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{

  /* USER CODE BEGIN USART6_Init 0 */
  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */
  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 921600;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */
  /* USER CODE END USART6_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
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
