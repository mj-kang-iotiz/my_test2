#include "ble_port.h"
#include "board_config.h"
#include "board_type.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_cortex.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_usart.h"
#include "FreeRTOS.h"
#include "queue.h"

#ifndef TAG
    #define TAG "BLE_PORT"
#endif

#include "log.h"

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
  LL_DMA_SetPeriphAddress(DMA1, LL_DMA_STREAM_0, (uint32_t)&UART5->DR);
  LL_DMA_SetMemoryAddress(DMA1, LL_DMA_STREAM_0,
                          (uint32_t)&ble_recv_buf[0]);
  LL_DMA_SetDataLength(DMA1, LL_DMA_STREAM_0,
                       sizeof(ble_recv_buf[0]));
  LL_DMA_EnableIT_TE(DMA1, LL_DMA_STREAM_0);
  LL_DMA_EnableIT_FE(DMA1, LL_DMA_STREAM_0);
  LL_DMA_EnableIT_DME(DMA1, LL_DMA_STREAM_0);

  LL_USART_EnableIT_IDLE(UART5);
  LL_USART_EnableIT_PE(UART5);
  LL_USART_EnableIT_ERROR(UART5);
  LL_USART_EnableDMAReq_RX(UART5);

  LL_DMA_EnableStream(DMA1, LL_DMA_STREAM_0);
  LL_USART_Enable(UART5);

  return 0;
}

int ble_uart5_hw_init(void) {
  ble_uart5_dma_init();
  ble_uart5_init();
  // 초기화 시 Bypass 모드로 시작 (설정 필요 시 AT 모드로 전환)
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
    if(GPIO_Pin == GPIO_PIN_11)
    {
        uint32_t read = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_11);
        if(read == GPIO_PIN_RESET)
        {
            // DISCONNECT
        }
        else
        {
            // CONNECT
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
