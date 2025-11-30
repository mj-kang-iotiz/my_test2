#include "rs485_app.h"
#include "board_config.h"
#include "rs485.h"
#include "rs485_port.h"
#include <string.h>

#ifndef TAG
  #define TAG "RS485_APP"
#endif

#include "log.h"

#define RS485_UART_MAX_RECV_SIZE 1024

typedef struct {
  rs485_t rs485;
  QueueHandle_t rx_queue;
  TaskHandle_t rx_task;
  bool enabled;

  QueueHandle_t tx_queue;
  TaskHandle_t tx_task;
} rs485_instance_t;

static rs485_instance_t rs485_instance = {0};

static void rs485_tx_task(void *pvParameter) {
  rs485_instance_t *inst = (rs485_instance_t *)pvParameter;
  rs485_tx_request_t tx_req;

  LOG_INFO("RS485 TX Task started");

  while (1) {
    if (xQueueReceive(inst->tx_queue, &tx_req, portMAX_DELAY) == pdTRUE) {
      LOG_DEBUG("RS485 Sending %d bytes", tx_req.len);

      if (inst->rs485.ops && inst->rs485.ops->send) {
        if (inst->rs485.ops->tx_enable) {
          inst->rs485.ops->tx_enable();
          vTaskDelay(pdMS_TO_TICKS(1));
        }

        inst->rs485.ops->send(tx_req.data, tx_req.len);

        if (inst->rs485.ops->rx_enable) {
          inst->rs485.ops->rx_enable();
        }

        LOG_DEBUG("RS485 TX complete");
      } else {
        LOG_ERR("RS485 send ops not available");
      }
    }
  }

  vTaskDelete(NULL);
}

static void rs485_rx_task(void *pvParameter) {
  rs485_instance_t *inst = (rs485_instance_t *)pvParameter;

  size_t pos = 0;
  size_t old_pos = 0;
  uint8_t dummy = 0;

  LOG_INFO("RS485 RX Task started");

  while (1) {
    xQueueReceive(inst->rx_queue, &dummy, portMAX_DELAY);

    pos = rs485_port_get_rx_pos();
    char *rs485_recv = rs485_port_get_recv_buf();

    if (pos != old_pos) {
      if (pos > old_pos) {
        size_t len = pos - old_pos;
        LOG_DEBUG_RAW("RS485 RX: ", &rs485_recv[old_pos], len);
      } else {
        size_t len1 = RS485_UART_MAX_RECV_SIZE - old_pos;
        size_t len2 = pos;
        LOG_DEBUG_RAW("RS485 RX: ", &rs485_recv[old_pos], len1);
        if (pos > 0) {
          LOG_DEBUG_RAW("RS485 RX: ", rs485_recv, len2);
        }
      }
      old_pos = pos;
      if (old_pos == RS485_UART_MAX_RECV_SIZE) {
        old_pos = 0;
      }
    }
  }

  vTaskDelete(NULL);
}

void rs485_init_all(void) {
  const board_config_t *config = board_get_config();

  LOG_INFO("RS485 초기화 시작 - 보드: %d", config->board);

  if (!config->use_rs485) {
    LOG_INFO("RS485 사용 안함");
    return;
  }

  rs485_instance.enabled = true;

  if (rs485_port_init_instance(&rs485_instance.rs485) != 0) {
    LOG_ERR("RS485 포트 초기화 실패");
    rs485_instance.enabled = false;
    return;
  }

  rs485_instance.rx_queue = xQueueCreate(10, sizeof(uint8_t));
  if (rs485_instance.rx_queue == NULL) {
    LOG_ERR("RS485 RX 큐 생성 실패");
    rs485_instance.enabled = false;
    return;
  }

  rs485_port_set_queue(rs485_instance.rx_queue);

  rs485_instance.tx_queue = xQueueCreate(5, sizeof(rs485_tx_request_t));
  if (rs485_instance.tx_queue == NULL) {
    LOG_ERR("RS485 TX 큐 생성 실패");
    rs485_instance.enabled = false;
    return;
  }

  rs485_port_start(&rs485_instance.rs485);

  BaseType_t ret = xTaskCreate(rs485_rx_task, "rs485_rx", 512,
                                (void *)&rs485_instance,
                                tskIDLE_PRIORITY + 1, &rs485_instance.rx_task);

  if (ret != pdPASS) {
    LOG_ERR("RS485 RX 태스크 생성 실패");
    rs485_instance.enabled = false;
    return;
  }

  ret = xTaskCreate(rs485_tx_task, "rs485_tx", 512,
                    (void *)&rs485_instance,
                    tskIDLE_PRIORITY + 1, &rs485_instance.tx_task);

  if (ret != pdPASS) {
    LOG_ERR("RS485 TX 태스크 생성 실패");
    rs485_instance.enabled = false;
    return;
  }

  LOG_INFO("RS485 초기화 완료");
}

rs485_t *rs485_get_handle(void) {
  if (!rs485_instance.enabled) {
    return NULL;
  }

  return &rs485_instance.rs485;
}

bool rs485_send(const char *data, size_t len) {
  if (!rs485_instance.enabled) {
    LOG_ERR("RS485 not enabled");
    return false;
  }

  if (!data || len == 0 || len > 256) {
    LOG_ERR("RS485 invalid send parameters");
    return false;
  }

  rs485_tx_request_t tx_req;
  memcpy(tx_req.data, data, len);
  tx_req.len = len;

  if (xQueueSend(rs485_instance.tx_queue, &tx_req, pdMS_TO_TICKS(1000)) != pdTRUE) {
    LOG_ERR("RS485 TX queue full");
    return false;
  }

  return true;
}
