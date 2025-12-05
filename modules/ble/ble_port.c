#include "ble_port.h"
#include "ble_app.h"
#include "board_config.h"
#include "board_type.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_cortex.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_usart.h"
#include "FreeRTOS.h"
#include "task.h"  // vTaskDelay 사용을 위해 추가
#include "queue.h"
#include <string.h>

#ifndef TAG
    #define TAG "BLE_PORT"
#endif

#include "log.h"

static int ble_uart5_change_baudrate(uint32_t baudrate);
static int ble_send_at_command_sync(const char *at_cmd, const char *expected_response, uint32_t timeout_ms);
static int ble_uart5_recv_line_poll(char *buf, size_t buf_size, uint32_t timeout_ms);


#define BLE_PORT_UART USART5
#define BLE_PORT_UART_DMA DMA1
#define BLE_PORT_UART_DMA_STREAM LL_DMA_STREAM_0

static char ble_recv_buf[1][1024];
static QueueHandle_t ble_queues[1] = {NULL};

int ble_set_at_cmd_mode(void);

static void ble_uart5_dma_init(void)
{
  /* Init with LL driver */
  /* DMA controller clock enable */
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration */
  NVIC_SetPriority(DMA1_Stream0_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),5, 0));
  NVIC_EnableIRQ(DMA1_Stream0_IRQn);

}

static void ble_uart5_init(void)
{
 
  /* USER CODE BEGIN UART5_Init 0 */

  /* USER CODE END UART5_Init 0 */

  LL_USART_InitTypeDef USART_InitStruct = {0};

  LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Peripheral clock enable */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART5);

  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
  LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOD);
  /**UART5 GPIO Configuration
  PC12   ------> UART5_TX
  PD2   ------> UART5_RX
  */
  GPIO_InitStruct.Pin = LL_GPIO_PIN_12;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_8;
  LL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LL_GPIO_PIN_2;
  GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
  GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
  GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
  GPIO_InitStruct.Alternate = LL_GPIO_AF_8;
  LL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* UART5 DMA Init */

  /* UART5_RX Init */
  LL_DMA_SetChannelSelection(DMA1, LL_DMA_STREAM_0, LL_DMA_CHANNEL_4);

  LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_STREAM_0, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);

  LL_DMA_SetStreamPriorityLevel(DMA1, LL_DMA_STREAM_0, LL_DMA_PRIORITY_LOW);

  LL_DMA_SetMode(DMA1, LL_DMA_STREAM_0, LL_DMA_MODE_CIRCULAR);

  LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_STREAM_0, LL_DMA_PERIPH_NOINCREMENT);

  LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_STREAM_0, LL_DMA_MEMORY_INCREMENT);

  LL_DMA_SetPeriphSize(DMA1, LL_DMA_STREAM_0, LL_DMA_PDATAALIGN_BYTE);

  LL_DMA_SetMemorySize(DMA1, LL_DMA_STREAM_0, LL_DMA_MDATAALIGN_BYTE);

  LL_DMA_DisableFifoMode(DMA1, LL_DMA_STREAM_0);

  /* UART5 interrupt Init */
  NVIC_SetPriority(UART5_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),5, 0));
  NVIC_EnableIRQ(UART5_IRQn);

  /* USER CODE BEGIN UART5_Init 1 */

  /* USER CODE END UART5_Init 1 */
  USART_InitStruct.BaudRate = 9600;
  USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
  USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
  USART_InitStruct.Parity = LL_USART_PARITY_NONE;
  USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
  USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
  USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
  LL_USART_Init(UART5, &USART_InitStruct);
  LL_USART_ConfigAsyncMode(UART5);
  /* USER CODE BEGIN UART5_Init 2 */

  /* USER CODE END UART5_Init 2 */
}

int ble_uart5_comm_start(void) {
  // 1. UART 먼저 활성화 (인터럽트 설정 전에 필수!)
  LL_USART_Enable(UART5);

  // 2. DMA 설정
  LL_DMA_SetPeriphAddress(DMA1, LL_DMA_STREAM_0, (uint32_t)&UART5->DR);
  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_STREAM_0,
                          (uint32_t)&ble_recv_buf[0]);
  LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_0,
                       sizeof(ble_recv_buf[0]));
  LL_DMA_EnableIT_TE(DMA1, LL_DMA_STREAM_0);
  LL_DMA_EnableIT_FE(DMA1, LL_DMA_STREAM_0);
  LL_DMA_EnableIT_DME(DMA1, LL_DMA_STREAM_0);

  // 3. UART 인터럽트 활성화 (UART가 이미 활성화된 상태에서!)
  LL_USART_EnableIT_IDLE(UART5);
  LL_USART_EnableIT_PE(UART5);
  LL_USART_EnableIT_ERROR(UART5);
  LL_USART_EnableDMAReq_RX(UART5);

  // 4. DMA 스트림 시작
  LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_0);

  return 0;
}

int ble_uart5_hw_init(void) {
  int ret;
  uint32_t current_baudrate = 9600;  // 최종 확정된 속도

  // 1. DMA 초기화 (DMA는 나중에 comm_start에서 시작)
  ble_uart5_dma_init();

  // 2. UART 9600bps로 초기화 및 활성화
  ble_uart5_init();
  LL_USART_Enable(UART5);
  LOG_INFO("BLE UART initialized at 9600 bps");

  // 3. AT 커맨드 모드로 전환
  //    - UART를 먼저 활성화한 후 모드 전환 (부팅 메시지 수신 위해)
  //    - 에지 트리거를 위해 Low → High 순서로 토글
  ble_set_bypass_mode();  // Low
  vTaskDelay(pdMS_TO_TICKS(100));  // vTaskDelay로 변경 (와치독 feed 가능)
  ble_set_at_cmd_mode();  // High (AT 모드)
  vTaskDelay(pdMS_TO_TICKS(500));  // vTaskDelay로 변경 (와치독 feed 가능)

  // 부팅 메시지 버퍼 클리어 (예: +READY 등)
  while (LL_USART_IsActiveFlag_RXNE(UART5)) {
    (void)LL_USART_ReceiveData8(UART5);  // 버퍼 비우기
  }
  LOG_INFO("UART RX buffer cleared");

  // 4. BLE 모듈을 115200bps로 직접 설정
  //    대부분의 BLE 모듈은 공장 초기값이 115200bps
  LOG_INFO("Setting BLE module to 115200 bps directly...");
  ble_uart5_change_baudrate(115200);
  current_baudrate = 115200;
  vTaskDelay(pdMS_TO_TICKS(100));  // UART 안정화 대기

  // 115200bps에서 통신 테스트
  LOG_INFO("Testing communication at 115200 bps...");
  ret = ble_send_at_command_sync("AT\r\n", "+OK", 2000);

  if (ret != 0) {
    LOG_ERR("BLE module not responding at 115200 bps!");
    LOG_ERR("Please check: 1) BLE module power, 2) UART connections, 3) GPIO pins");
    // 초기화 실패, 하지만 계속 진행 (디버깅 위해)
  } else {
    LOG_INFO("BLE module responding at 115200 bps");
  }

  // 최종 속도 로그
  LOG_INFO("BLE initialization complete at %lu bps", current_baudrate);

  // 5. AT+ADVON 전송 (advertising 시작)
  //    주의: 일부 BLE 모듈은 부팅 시 자동으로 advertising 시작
  //    이 경우 +ERROR 응답 가능 (이미 advertising 중)
  LOG_INFO("Attempting to start BLE advertising with AT+ADVON...");
  ret = ble_send_at_command_sync("AT+ADVON\r\n", "+OK", 2000);

  if (ret == 0) {
    LOG_INFO("ADVON: Advertising started successfully");
  } else {
    LOG_WARN("ADVON: Command failed (+ERROR or timeout)");
    LOG_WARN("ADVON: This may be normal if advertising auto-starts on boot");
    LOG_WARN("ADVON: Check if BLE device is visible on phone/scanner");
    // 에러가 나도 계속 진행 (자동 advertising 가능)
  }

  vTaskDelay(pdMS_TO_TICKS(100));

  // UART 종료 전 모든 플래그 클리어 (중요!)
  LL_USART_ClearFlag_IDLE(UART5);
  LL_USART_ClearFlag_PE(UART5);
  LL_USART_ClearFlag_FE(UART5);
  LL_USART_ClearFlag_ORE(UART5);
  LL_USART_ClearFlag_NE(UART5);
  LL_USART_ClearFlag_TC(UART5);
  LL_USART_ClearFlag_RXNE(UART5);

  LL_USART_Disable(UART5);
  // 6. Bypass 모드로 전환 (정상 동작 준비)
  ble_set_bypass_mode();

  return 0;
}

int ble_uart5_send(const char *data, size_t len) {
  for (int i = 0; i < len; i++) {
    while (!LL_USART_IsActiveFlag_TXE(UART5))
      ;
    LL_USART_TransmitData8(UART5, *(data + i));
  }

  while (!LL_USART_IsActiveFlag_TC(UART5))
    ;

  return 0;
}

// 폴링 방식으로 1바이트 수신 (타임아웃 포함)
static int ble_uart5_recv_poll(uint8_t *byte, uint32_t timeout_ms) {
  uint32_t start = HAL_GetTick();

  while (!LL_USART_IsActiveFlag_RXNE(UART5)) {
    if ((HAL_GetTick() - start) > timeout_ms) {
      return -1;  // 타임아웃
    }
  }

  *byte = LL_USART_ReceiveData8(UART5);
  return 0;
}

// 폴링 방식으로 문자열 수신 (라인 단위, 타임아웃 포함)
static int ble_uart5_recv_line_poll(char *buf, size_t buf_size, uint32_t timeout_ms) {
  size_t pos = 0;
  uint32_t start = HAL_GetTick();

  while (pos < buf_size - 1) {
    uint8_t byte;

    // 남은 시간 계산
    uint32_t elapsed = HAL_GetTick() - start;
    if (elapsed > timeout_ms) {
      buf[pos] = '\0';
      return -1;  // 타임아웃
    }

    // 1바이트 수신
    if (ble_uart5_recv_poll(&byte, timeout_ms - elapsed) != 0) {
      buf[pos] = '\0';
      return -1;  // 타임아웃
    }

    buf[pos++] = (char)byte;

    // \r 수신 시 종료 (다음 바이트 \n은 무시됨)
    if (byte == '\r') {
      buf[pos] = '\0';
      return pos;
    }
  }

  buf[buf_size - 1] = '\0';
  return pos;
}

// UART 속도 재설정
static int ble_uart5_change_baudrate(uint32_t baudrate) {
  // UART 비활성화
  LL_USART_Disable(UART5);

  // 보드레이트 변경
  LL_USART_SetBaudRate(UART5, HAL_RCC_GetPCLK1Freq(), LL_USART_OVERSAMPLING_16, baudrate);

  // UART 재활성화
  LL_USART_Enable(UART5);

  LOG_INFO("UART5 baudrate changed to %lu", baudrate);
  return 0;
}

// 동기 AT 커맨드 전송 및 응답 대기 (초기화용, 폴링 방식)
static int ble_send_at_command_sync(const char *at_cmd, const char *expected_response, uint32_t timeout_ms) {
  char response[128];

  LOG_INFO("Sending AT command (sync): %s", at_cmd);

  // AT 커맨드 전송
  ble_uart5_send(at_cmd, strlen(at_cmd));

  // 응답 대기 (폴링 방식)
  uint32_t start = HAL_GetTick();

  while ((HAL_GetTick() - start) < timeout_ms) {
    int len = ble_uart5_recv_line_poll(response, sizeof(response), 100);

    if (len > 0) {
      LOG_INFO("Received response: %s", response);

      // 기대 응답과 비교
      if (strstr(response, expected_response) != NULL) {
        LOG_INFO("AT command succeeded");
        return 0;  // 성공
      }

      // 에러 응답 확인
      if (strstr(response, "+ERROR") != NULL) {
        LOG_ERR("AT command failed: %s", response);
        return -1;
      }
    }
  }

  LOG_ERR("AT command timeout");
  return -1;  // 타임아웃
}

// UART 변경 전용: AT+UART 커맨드 전송 후 +OK와 +READY 순차 대기
static int ble_send_uart_change_command(uint32_t baudrate, uint32_t timeout_ms) {
  char at_cmd[32];
  char response[128];

  snprintf(at_cmd, sizeof(at_cmd), "AT+UART=%lu\r\n", baudrate);
  LOG_INFO("Sending UART change command: %s", at_cmd);

  // AT 커맨드 전송 (현재 속도로)
  ble_uart5_send(at_cmd, strlen(at_cmd));

  // 1. +OK 응답 대기 (현재 속도로)
  uint32_t start = HAL_GetTick();
  bool ok_received = false;

  while ((HAL_GetTick() - start) < timeout_ms) {
    int len = ble_uart5_recv_line_poll(response, sizeof(response), 100);

    if (len > 0) {
      LOG_INFO("Received response: %s", response);

      if (strstr(response, "+OK") != NULL) {
        LOG_INFO("UART change: +OK received at current baudrate");
        ok_received = true;
        break;
      }

      if (strstr(response, "+ERROR") != NULL) {
        LOG_ERR("UART change failed: %s", response);
        return -1;
      }
    }
  }

  if (!ok_received) {
    LOG_ERR("UART change: +OK timeout");
    return -1;
  }

  // 2. 즉시 MCU UART를 새로운 속도로 변경
  //    BLE 모듈도 이 시점에 속도를 변경하기 시작함
  LOG_INFO("Changing MCU UART to %lu bps...", baudrate);
  ble_uart5_change_baudrate(baudrate);

  // 버퍼 클리어 (속도 변경 중 잘못된 데이터가 있을 수 있음)
  vTaskDelay(pdMS_TO_TICKS(10));  // vTaskDelay로 변경 (UART 안정화 대기)
  while (LL_USART_IsActiveFlag_RXNE(UART5)) {
    (void)LL_USART_ReceiveData8(UART5);
  }

  // 3. 2초 대기 (매뉴얼 명시 - BLE 모듈이 baudrate 변경 완료하는 시간)
  LOG_INFO("Waiting 2 seconds for BLE module baudrate change...");
  vTaskDelay(pdMS_TO_TICKS(2000));  // vTaskDelay로 변경 (와치독 feed 가능!)

  // 4. +READY 응답 대기 (새로운 속도로)
  LOG_INFO("Waiting for +READY at new baudrate...");
  start = HAL_GetTick();

  while ((HAL_GetTick() - start) < timeout_ms) {
    int len = ble_uart5_recv_line_poll(response, sizeof(response), 100);

    if (len > 0) {
      LOG_INFO("Received response: %s", response);

      if (strstr(response, "+READY") != NULL) {
        LOG_INFO("UART change: +READY received - baudrate change complete");
        return 0;  // 성공
      }
    }
  }

  LOG_WARN("UART change: +READY timeout (but +OK was received, proceeding)");
  return 0;  // +OK를 받았으므로 성공으로 간주
}

int ble_set_at_cmd_mode(void)
{
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_SET);
}

int ble_set_bypass_mode(void)
{
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_RESET);
}

static const ble_hal_ops_t ble_uart5_ops = {
    .init = ble_uart5_hw_init,
    .reset = NULL,
    .start = ble_uart5_comm_start,
    .stop = NULL,
    .send = ble_uart5_send,
    .recv = NULL,
	.at_mode = ble_set_at_cmd_mode,
	.bypass_mode = ble_set_bypass_mode,
};

#if defined(BOARD_TYPE_BASE_UNICORE) || defined(BOARD_TYPE_BASE_UBLOX)
/**
 * @brief This function handles USART3 global interrupt.
 */
void USART5_IRQHandler(void) {
    /* USER CODE BEGIN USART3_IRQn 0 */
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (LL_USART_IsActiveFlag_IDLE(UART5)) {
    if (ble_queues[0] != NULL) {
      uint8_t dummy = 0;
      xQueueSendFromISR(ble_queues[0], &dummy, &xHigherPriorityTaskWoken);
    }
    LL_USART_ClearFlag_IDLE(UART5);
  }


  if (LL_USART_IsActiveFlag_PE(UART5)) {
    LL_USART_ClearFlag_PE(UART5);
  }
  if (LL_USART_IsActiveFlag_FE(UART5)) {
    LL_USART_ClearFlag_FE(UART5);
  }
  if (LL_USART_IsActiveFlag_ORE(UART5)) {
    LL_USART_ClearFlag_ORE(UART5);
  }
  if (LL_USART_IsActiveFlag_NE(UART5)) {
    LL_USART_ClearFlag_NE(UART5);
  }

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  /* USER CODE END USART3_IRQn 0 */
  /* USER CODE BEGIN USART3_IRQn 1 */

  /* USER CODE END USART3_IRQn 1 */
}

void DMA1_Stream0_IRQHandler(void)
{

}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == GPIO_PIN_11)  // PC11: BLE 연결 상태 감지 핀
    {
        GPIO_PinState pin_state = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_11);

        if(pin_state == GPIO_PIN_RESET)
        {
            // DISCONNECT (LOW)
            ble_set_connection_state(BLE_CONN_DISCONNECTED);
            LOG_INFO("BLE GPIO: Disconnected (PC11 LOW)");
        }
        else
        {
            // CONNECT (HIGH)
            ble_set_connection_state(BLE_CONN_CONNECTED);
            LOG_INFO("BLE GPIO: Connected (PC11 HIGH)");
        }
    }
}


void EXTI15_10_IRQHandler(void)
{
     HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_11);
}

#endif

int ble_port_init_instance(ble_t *ble_handle) {
  const board_config_t *config = board_get_config();

  LOG_INFO("BLE Port 초기화 시작 (보드: %d)", config->board);

    if(config->use_ble == true)
    {
        ble_handle->ops = &ble_uart5_ops;
        if (ble_handle->ops->init) {
            ble_handle->ops->init();
        }
    }
    else
    {
        LOG_ERR("BLE 포트 초기화 실패: 잘못된 BLE 모드");
        return -1;
    }

    LOG_INFO("BLE 초기화 완료");

    return 0;
}


void ble_port_start(ble_t *ble_handle) {
  if (!ble_handle || !ble_handle->ops || !ble_handle->ops->start) {
    LOG_ERR("BLE start failed: invalid handle or ops");
    return;
  }

  ble_handle->ops->start();
}

void ble_port_stop(ble_t *ble_handle) {
  if (!ble_handle || !ble_handle->ops || !ble_handle->ops->stop) {
    LOG_ERR("BLE start failed: invalid handle or ops");
    return;
  }

  ble_handle->ops->stop();
}

uint32_t ble_port_get_rx_pos() {
  uint32_t pos = sizeof(ble_recv_buf[0]) - LL_DMA_GetDataLength(BLE_PORT_UART_DMA, BLE_PORT_UART_DMA_STREAM);
  return pos;
}

char *ble_port_get_recv_buf() 
{
  return ble_recv_buf[0];
}

void ble_port_set_queue(QueueHandle_t queue) {
    ble_queues[0] = queue;
}
