#include "ble_app.h"
#include "board_config.h"
#include "ble.h"
#include "ble_port.h"
#include <string.h>

#ifndef TAG
  #define TAG "BLE_APP"
#endif

#include "log.h"

void ble_cmd_parse_process(ble_instance_t *inst, const void *data, size_t len)
{
  const uint8_t *d = data;

  for (; len > 0; ++d, --len)
  {
    if(inst->parse_stae == BLE_CMD_PARSE_STATE_NONE)
    {
        if(*d == '+')
        {
            inst->parser.pos = 0;
            inst->parser.data[inst->parser.pos++] = (char)(*d);
            inst->parser.data[inst->parser.pos] = '\0';
            inst->parse_stae = BLE_CMD_PARSE_STATE_DATA;
        }
      else if(*d == 'A')
      {
        inst->parser.pos = 0;
        inst->parser.data[inst->parser.pos++] = (char)(*d);
        inst->parser.data[inst->parser.pos] = '\0';
        inst->parse_stae = BLE_CMD_PARSE_STATE_GOT_A;
      }
    }
    else if(inst->parse_stae == BLE_CMD_PARSE_STATE_GOT_A)
    {
      if(*d == 'T')
      {
        inst->parser.data[inst->parser.pos++] = (char)(*d);
        inst->parser.data[inst->parser.pos] = '\0';
        inst->parse_stae = BLE_CMD_PARSE_STATE_DATA;
      }
      else
      {
        inst->parser.pos = 0;
        inst->parser.data[inst->parser.pos] = '\0';
        inst->parse_stae = BLE_CMD_PARSE_STATE_NONE;

        if(*d == '+')
        {
            inst->parser.data[inst->parser.pos++] = (char)(*d);
            inst->parse_stae = BLE_CMD_PARSE_STATE_DATA;
        }
      }
    }
    else if(inst->parse_stae == BLE_CMD_PARSE_STATE_DATA)
    {
        if(inst->parser.pos < sizeof(inst->parser.data) - 1)
        {
          inst->parser.data[inst->parser.pos++] = (char)(*d);
          inst->parser.data[inst->parser.pos] = '\0';
        }
        else
        {
          inst->parser.pos = 0;
          inst->parser.data[inst->parser.pos] = '\0';
          inst->parser.prev_char = '\0';
          inst->parse_stae = BLE_CMD_PARSE_STATE_NONE;
        }

        if(*d == '\r')
        {
          inst->parser.data[inst->parser.pos - 1] = '\0'; // \r 제거
          LOG_INFO("BLE AT Command received: %s", inst->parser.data);

          ble_at_cmd_handler(inst);

          inst->parser.prev_char = '\0';
          inst->parser.pos = 0;
          inst->parse_stae = BLE_CMD_PARSE_STATE_NONE;
        }
        inst->parser.prev_char = (char)(*d);
    }
  }
}

static ble_instance_t ble_instance = {0};

static void ble_tx_task(void *pvParameter) {
  ble_instance_t *inst = (ble_instance_t *)pvParameter;
  ble_tx_request_t tx_req;

  LOG_INFO("BLE TX Task started");

  while (1) {
    if (xQueueReceive(inst->tx_queue, &tx_req, portMAX_DELAY) == pdTRUE) {
      LOG_DEBUG("BLE Sending %d bytes", tx_req.len);

      xSemaphoreTake(inst->mutex, portMAX_DELAY);

      if (inst->ble.ops && inst->ble.ops->send) {
        inst->ble.ops->send(tx_req.data, tx_req.len);
        LOG_DEBUG("BLE TX complete");
      } else {
        LOG_ERR("BLE send ops not available");
      }

      xSemaphoreGive(inst->mutex);
    }
  }

  vTaskDelete(NULL);
}

static void ble_rx_task(void *pvParameter) {
  ble_instance_t *inst = (ble_instance_t *)pvParameter;

  size_t pos = 0;
  size_t old_pos = 0;
  uint8_t dummy = 0;
  size_t total_received = 0;

  LOG_INFO("BLE RX Task started");

  while (1) {
    xQueueReceive(inst->rx_queue, &dummy, portMAX_DELAY);

    xSemaphoreTake(inst->mutex, portMAX_DELAY);

    pos = ble_port_get_rx_pos();
    char *ble_recv = ble_port_get_recv_buf();

    if (pos != old_pos) {
      if (pos > old_pos) {
        size_t len = pos - old_pos;
        total_received = len;
        LOG_DEBUG_RAW("BLE RX: ", &ble_recv[old_pos], len);
       ble_cmd_parse_process(inst, &ble_recv[old_pos], pos - old_pos);
      } else {
        size_t len1 = BLE_UART_MAX_RECV_SIZE - old_pos;
        size_t len2 = pos;
        total_received = len1 + len2;
        LOG_DEBUG_RAW("BLE RX: ", &ble_recv[old_pos], len1);
       ble_cmd_parse_process(inst, &ble_recv[old_pos],
                               BLE_UART_MAX_RECV_SIZE - old_pos);
        if (pos > 0) {
          LOG_DEBUG_RAW("BLE RX: ", ble_recv, len2);
         ble_cmd_parse_process(inst, ble_recv, pos);
        }
      }
      old_pos = pos;
      if (old_pos == BLE_UART_MAX_RECV_SIZE) {
        old_pos = 0;
      }
    }

    xSemaphoreGive(inst->mutex);
  }

  vTaskDelete(NULL);
}

void ble_init_all(void) {
  const board_config_t *config = board_get_config();

  LOG_INFO("BLE 초기화 시작 - 보드: %d", config->board);

  if (!config->use_ble) {
    LOG_INFO("BLE 사용 안함");
    return;
  }

  ble_instance.enabled = true;

  if (ble_port_init_instance(&ble_instance.ble) != 0) {
    LOG_ERR("BLE 포트 초기화 실패");
    ble_instance.enabled = false;
    return;
  }

  ble_instance.rx_queue = xQueueCreate(10, sizeof(uint8_t));
  if (ble_instance.rx_queue == NULL) {
    LOG_ERR("BLE RX 큐 생성 실패");
    ble_instance.enabled = false;
    return;
  }

  ble_port_set_queue(ble_instance.rx_queue);

  ble_instance.tx_queue = xQueueCreate(5, sizeof(ble_tx_request_t));
  if (ble_instance.tx_queue == NULL) {
    LOG_ERR("BLE TX 큐 생성 실패");
    ble_instance.enabled = false;
    return;
  }

  ble_instance.mutex = xSemaphoreCreateMutex();
  if (ble_instance.mutex == NULL) {
    LOG_ERR("BLE mutex 생성 실패");
    ble_instance.enabled = false;
    return;
  }

  ble_port_start(&ble_instance.ble);

  BaseType_t ret = xTaskCreate(ble_rx_task, "ble_rx", 512,
                                (void *)&ble_instance,
                                tskIDLE_PRIORITY + 1, &ble_instance.rx_task);

  if (ret != pdPASS) {
    LOG_ERR("BLE RX 태스크 생성 실패");
    ble_instance.enabled = false;
    return;
  }

  ret = xTaskCreate(ble_tx_task, "ble_tx", 512,
                    (void *)&ble_instance,
                    tskIDLE_PRIORITY + 1, &ble_instance.tx_task);

  if (ret != pdPASS) {
    LOG_ERR("BLE TX 태스크 생성 실패");
    ble_instance.enabled = false;
    return;
  }

  LOG_INFO("BLE 초기화 완료");
}

ble_t *ble_get_handle(void)
{
  if (!ble_instance.enabled) {
    return NULL;
  }

  return &ble_instance.ble;
}

ble_instance_t* ble_get_instance(void)
{
    return &ble_instance;
}

bool ble_send(const char *data, size_t len, bool is_at) {
  if (!ble_instance.enabled) {
    LOG_ERR("BLE not enabled");
    return false;
  }

  if (!data || len == 0 || len > 512) {
    LOG_ERR("BLE invalid send parameters");
    return false;
  }

  ble_tx_request_t tx_req;
  memcpy(tx_req.data, data, len);
  tx_req.len = len;
  tx_req.is_at = is_at;

  if (xQueueSend(ble_instance.tx_queue, &tx_req, pdMS_TO_TICKS(1000)) != pdTRUE) {
    LOG_ERR("BLE TX queue full");
    return false;
  }

  return true;
}
