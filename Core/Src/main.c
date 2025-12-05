/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
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
#include "gpio.h"
#include "usart.h"
#include "stm32f4xx_hal_tim.h"
#include "stm32f4xx_hal_tim_ex.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "SEGGER_SYSVIEW.h"
#include "gps.h"
#include "gps_app.h"
#include "gsm_app.h"
#include "lora_app.h"
#include "ble_app.h"
#include "led.h"
#include "rtcm.h"
#include "flash_params.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "softuart.h"
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

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void MX_TIM1_Init(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
TIM_HandleTypeDef htim1;

/* USER CODE BEGIN PV */
#define SOFT_UART_TX_PIN        GPIO_PIN_2
#define SOFT_UART_TX_PORT       GPIOD
#define SOFT_UART_RX_PIN        GPIO_PIN_12
#define SOFT_UART_RX_PORT       GPIOC

#define RS485_DE_PIN          GPIO_PIN_10     // DE
#define RS485_DE_PORT       	GPIOC
#define RS485_RE_PIN          GPIO_PIN_11     // DE와 RE 공통 핀
#define RS485_RE_PORT       	GPIOC

char* INIT_Notify = "+READY\r\n";
char* GPS_Notify = "+GPS,\r\n";

char* F_Version_Response = "+V0.0.1\r\n";
char* AT_Response = "+OK\r\n";
char* ATZ_Response = "+RESET\r\n";
char* ATnF_Response = "+CONFIGINIT\r\n";
char* GPSMANUF_Response = "+Unicore\r\n"; 					//"+Ublox\r\n"
char* CONFIG_Response = "+CONFIG=Hello1234567890abcdefghijklmnopqrstuvwyzABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n"; 						//need make CONFIG Variable
char* SETBASELINE_Response = "+SETBASELINE=\r\n"; 	//need make setbaseline variable
char* CASTER_Response = "+CASTER=\r\n"; 						//need make caster variable 
char* ID_Response = "+ID=\r\n";											//need make ID variable
char* MOUNTPOINT_Response = "+MOUNTPOINT=\r\n";			//need make MOUNTPOINT variable
char* PASSWORD_Response = "+PASSWORD=\r\n";					//need make PASSWORD variable
char* START_Response = "+GUGUSTART\r\n";
char* STOP_Response = "+GUGUSTOP\r\n";

char* ERROR_Response = "+ERROR\r\n";   	//ETC
char* ERROR1_Response = "+E01\r\n";			//DO NOT KNOW ERROR
char* ERROR2_Response = "+E02\r\n";			//Parameter ERROR
char* ERROR3_Response = "+E03\r\n";			//NO ready device ERROR

void RS485_SetTransmitMode(void)
{
    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_SET);  // DE=1, RE=1 (송신)
		HAL_GPIO_WritePin(RS485_RE_PORT, RS485_RE_PIN, GPIO_PIN_SET);  // DE=1, RE=1 (송신)
}

void RS485_SetReceiveMode(void)
{
    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_RESET); // DE=0, RE=0 (수신)
		HAL_GPIO_WritePin(RS485_RE_PORT, RS485_RE_PIN, GPIO_PIN_RESET); // DE=0, RE=0 (수신)
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if(htim->Instance==TIM1)
	{
		led_set_toggle(3);
		SoftUartHandler();
	}
}

uint8_t SoftUartGetchar(uint8_t SoftUartNumber)
{
    uint8_t ch;
    while(SoftUartRxAlavailable(SoftUartNumber)==0);
    SoftUartReadRxBuffer(SoftUartNumber,&ch,1);
    return ch;
}

void RS485_Transmit(char* data, uint8_t len) {
    RS485_SetTransmitMode();
    HAL_Delay(1);  // RS485 드라이버 Enable 대기 시간

    SoftUartPuts(0, (uint8_t*)data, len);              // 소프트 UART 전송
    SoftUartWaitUntilTxComplate(0);          // 전송 완료 대기

    HAL_Delay(1);  // 잔여 비트 출력 대기 (Stop Bit 등)
    RS485_SetReceiveMode();  // 다시 수신 모드 전환
}

int get_line(uint8_t SoftUartNumber, char *buffer, int maxlen)
{
    int index = 0;
    uint8_t ch;

    while (index < maxlen - 1)
    {
        while (SoftUartRxAlavailable(SoftUartNumber) == 0);  // 수신 대기
        SoftUartReadRxBuffer(SoftUartNumber, &ch, 1);

        buffer[index++] = ch;

        if (ch == '\n')  // 종료 문자
            break;
    }

    buffer[index] = '\0';  // 문자열 종료
    return index;
}



uint32_t uart_send(USART_TypeDef *handle, const char *buf, size_t len) {
  for (int i = 0; i < len; i++) {
    while (!LL_USART_IsActiveFlag_TXE(handle))
      ;
    LL_USART_TransmitData8(handle, *(buf + i));
  }

  while (!LL_USART_IsActiveFlag_TC(handle))
    ;

  return len;
}

// int _write(int file, char* p, int len)
//{
//    for(int i=0;i<len;i++)
//    {
//       while(!LL_USART_IsActiveFlag_TXE(USART6));
//       LL_USART_TransmitData8(USART6, *(p+i));
//    }
//
//    while(!LL_USART_IsActiveFlag_TC(USART6));
//
//    return len;
// }

static void rs485_task(void *pvParameter)
{
	char ch;
	char rx_buffer[64];
	uint8_t init_f = 0;

  while(1)
  {
    if(init_f == 0){
			init_f = 1;
			RS485_SetTransmitMode();
			vTaskDelay(pdMS_TO_TICKS(10));
			SoftUartPuts(0, (uint8_t*)INIT_Notify, strlen(INIT_Notify));  //dummy clear
			SoftUartWaitUntilTxComplate(0);
			SoftUartPuts(0, (uint8_t*)INIT_Notify, strlen(INIT_Notify));
			SoftUartWaitUntilTxComplate(0);
			vTaskDelay(pdMS_TO_TICKS(10));
		}
		
		RS485_SetReceiveMode();
    int len = get_line(0, rx_buffer, sizeof(rx_buffer));  // 문자열 수신

		RS485_SetTransmitMode();
		HAL_Delay(1);  // RS485 활성 대기
		// 문자열 비교: 대소문자 구분, 정확히 "TEST\r\n"
		
    if (strcmp(rx_buffer, "AT\n") == 0)
    {
				SoftUartPuts(0, (uint8_t*)AT_Response, strlen(AT_Response));
				SoftUartWaitUntilTxComplate(0);
    }
		else if (strcmp(rx_buffer, "ATZ\n") == 0)
    {
				SoftUartPuts(0, (uint8_t*)ATZ_Response, strlen(ATZ_Response));
				SoftUartWaitUntilTxComplate(0);
    }
		else if (strcmp(rx_buffer, "AT&F\n") == 0)
    {
				SoftUartPuts(0, (uint8_t*)ATnF_Response, strlen(ATnF_Response));
				SoftUartWaitUntilTxComplate(0);
    }
		else if (strcmp(rx_buffer, "AT+VER?\n") == 0)
    {
				SoftUartPuts(0, (uint8_t*)F_Version_Response, strlen(F_Version_Response));
				SoftUartWaitUntilTxComplate(0);
    }
		else if (strcmp(rx_buffer, "AT+GPSMANUF?\n") == 0)
    {
				SoftUartPuts(0, (uint8_t*)GPSMANUF_Response, strlen(GPSMANUF_Response));
				SoftUartWaitUntilTxComplate(0);
    }
		else if (strcmp(rx_buffer, "AT+CONFIG?\n") == 0)
    {
				SoftUartPuts(0, (uint8_t*)CONFIG_Response, strlen(CONFIG_Response));
				SoftUartWaitUntilTxComplate(0);
    }
		else if (strcmp(rx_buffer, "AT+SETBASELINE:xxx\n") == 0)
    {
				SoftUartPuts(0, (uint8_t*)SETBASELINE_Response, strlen(SETBASELINE_Response));
				SoftUartWaitUntilTxComplate(0);
    }
		else if (strcmp(rx_buffer, "AT+CASTER:xx.xx.xx.xxxx\n") == 0)
    {
				SoftUartPuts(0, (uint8_t*)CASTER_Response, strlen(CASTER_Response));
				SoftUartWaitUntilTxComplate(0);
    }
		else if (strcmp(rx_buffer, "AT+ID=xxxxx\n") == 0)
    {
				SoftUartPuts(0, (uint8_t*)ID_Response, strlen(ID_Response));
				SoftUartWaitUntilTxComplate(0);
    }
		else if (strcmp(rx_buffer, "AT+MOUNTPOINT=xxxx\n") == 0)
    {
				SoftUartPuts(0, (uint8_t*)MOUNTPOINT_Response, strlen(MOUNTPOINT_Response));
				SoftUartWaitUntilTxComplate(0);
    }
		else if (strcmp(rx_buffer, "AT+PASSWORD=xxxxx\n") == 0)
    {
				SoftUartPuts(0, (uint8_t*)PASSWORD_Response, strlen(PASSWORD_Response));
				SoftUartWaitUntilTxComplate(0);
    }
		else if (strcmp(rx_buffer, "AT+GUGUSTART\n") == 0)
    {
				SoftUartPuts(0, (uint8_t*)START_Response, strlen(START_Response));
				SoftUartWaitUntilTxComplate(0);
    }
		else if (strcmp(rx_buffer, "AT+GUGUSTOP\n") == 0)
    {
				SoftUartPuts(0, (uint8_t*)STOP_Response, strlen(STOP_Response));
				SoftUartWaitUntilTxComplate(0);
    }
		else{
				SoftUartPuts(0, (uint8_t*)ERROR1_Response, strlen(ERROR1_Response));
				SoftUartWaitUntilTxComplate(0);
		}
		
		RS485_SetReceiveMode();

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void initThread(void *pvParameter) {
	const board_config_t *config = board_get_config();

	flash_params_init();
  // flash_params_set_manual_position(true, "37.413421", "127.125791", "60");
//	flash_params_set_ntrip_url("www.gnssdata.or.kr");
//	flash_params_set_ntrip_port("2101");
//	flash_params_set_ntrip_mountpoint("SONP-RTCM32");
//	flash_params_set_ntrip_id("mj.kang@iotiz.kr");
//	flash_params_set_ntrip_pw("gnss");


  led_init();
  gps_init_all();
  // gsm_task_create(NULL);
  lora_instance_init();

  if(config->use_rs485)
  {
    xTaskCreate(rs485_task, "rs485", 1024, NULL, tskIDLE_PRIORITY + 1, NULL);
  }

  if(config->use_ble)
  {
	  ble_init_all();
  }

  vTaskDelete(NULL);
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
    HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  SEGGER_SYSVIEW_DisableEvents(SYSVIEW_EVTMASK_ISR_ENTER | SYSVIEW_EVTMASK_ISR_EXIT);

  traceSTART();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();

  GPIO_InitTypeDef GPIO_InitStruct = {0};
#if defined(BOARD_TYPE_BASE_UNICORE) || defined(BOARD_TYPE_BASE_UBLOX)
  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC10 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PC11 */
  GPIO_InitStruct.Pin = GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
#elif defined(BOARD_TYPE_ROVER_UNICORE) || defined(BOARD_TYPE_ROVER_UBLOX)
    /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10|GPIO_PIN_11, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC10 PC11 */
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10|GPIO_PIN_11, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SOFT_UART_TX_GPIO_Port, SOFT_UART_TX_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : SOFT_UART_RX_Pin */
  GPIO_InitStruct.Pin = SOFT_UART_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SOFT_UART_RX_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SOFT_UART_TX_Pin */
  GPIO_InitStruct.Pin = SOFT_UART_TX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SOFT_UART_TX_GPIO_Port, &GPIO_InitStruct);

  MX_TIM1_Init();
  HAL_TIM_Base_Start_IT(&htim1);

  SoftUartInit(0,SOFT_UART_TX_PORT,SOFT_UART_TX_PIN,SOFT_UART_RX_PORT,SOFT_UART_RX_PIN);
  SoftUartEnableRx(0);
#endif

  MX_DMA_Init();
//  MX_USART6_UART_Init();
  MX_ADC1_Init();

//	HAL_Delay(100);

  /* USER CODE BEGIN 2 */
  xTaskCreate(initThread, "init", 512, NULL, tskIDLE_PRIORITY + 1, NULL);

  vTaskStartScheduler();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    //	  uart6_rx_head = UART6_BUFFER_SIZE - LL_DMA_GetDataLength(DMA2,
    // LL_DMA_STREAM_1); // 이런식으로 사용
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
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
  RCC_OscInitStruct.PLL.PLLM = 13;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
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
  htim1.Init.Prescaler = 69;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 49;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
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
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1) {
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
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
