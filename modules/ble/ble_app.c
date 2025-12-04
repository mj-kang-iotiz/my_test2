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
      else
      {
        inst->parser.pos = 0;
        inst->parser.data[inst->parser.pos++] = (char)(*d);
        inst->parser.data[inst->parser.pos] = '\0';
        inst->parse_stae = BLE_CMD_PARSE_STATE_APP;
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
    else if(inst->parse_stae == BLE_CMD_PARSE_STATE_DATA || inst->parse_stae == BLE_CMD_PARSE_STATE_APP)
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

        if(*d == '\r' || *d == '\n')
        {
          inst->parser.data[inst->parser.pos - 1] = '\0'; // \r 제거
          LOG_INFO("BLE AT Command received: %s", inst->parser.data);

          if(inst->parse_stae == BLE_CMD_PARSE_STATE_DATA)
          {
            ble_at_cmd_handler(inst);
          }
          else
          {
            ble_app_cmd_handler(inst);
          }

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

// 비동기 AT 커맨드 전송 및 응답 대기
ble_at_status_t ble_send_at_command_async(const char *at_cmd, const char *expected_response,
                                           char *response_buf, size_t response_buf_size,
                                           uint32_t timeout_ms) {
  if (!ble_instance.enabled) {
    LOG_ERR("BLE not enabled");
    return BLE_AT_STATUS_ERROR;
  }

  if (!at_cmd || !expected_response) {
    LOG_ERR("Invalid parameters");
    return BLE_AT_STATUS_ERROR;
  }

  // 이미 진행 중인 비동기 요청이 있는지 확인
  xSemaphoreTake(ble_instance.mutex, portMAX_DELAY);
  if (ble_instance.async_request != NULL) {
    xSemaphoreGive(ble_instance.mutex);
    LOG_ERR("Another async AT command is pending");
    return BLE_AT_STATUS_ERROR;
  }

  // 비동기 요청 구조체 생성 및 초기화
  ble_async_at_request_t request = {0};
  strncpy(request.expected_response, expected_response, sizeof(request.expected_response) - 1);
  request.response_len = 0;
  request.status = BLE_AT_STATUS_PENDING;
  request.timeout_ticks = pdMS_TO_TICKS(timeout_ms);
  request.wait_sem = xSemaphoreCreateBinary();

  if (request.wait_sem == NULL) {
    xSemaphoreGive(ble_instance.mutex);
    LOG_ERR("Failed to create semaphore");
    return BLE_AT_STATUS_ERROR;
  }

  // 전역 async_request 포인터 설정
  ble_instance.async_request = &request;
  xSemaphoreGive(ble_instance.mutex);

  // AT 모드로 전환
  LOG_INFO("Switching to AT command mode");
  if (ble_instance.ble.ops && ble_instance.ble.ops->at_mode) {
    ble_instance.ble.ops->at_mode();
  }

  // 모드 전환 대기 (BLE 모듈이 안정화될 시간)
  vTaskDelay(pdMS_TO_TICKS(100));

  // AT 커맨드 전송
  LOG_INFO("Sending AT command: %s", at_cmd);
  if (!ble_send(at_cmd, strlen(at_cmd), true)) {
    xSemaphoreTake(ble_instance.mutex, portMAX_DELAY);
    ble_instance.async_request = NULL;
    xSemaphoreGive(ble_instance.mutex);
    vSemaphoreDelete(request.wait_sem);
    LOG_ERR("Failed to send AT command");
    return BLE_AT_STATUS_ERROR;
  }

  // 응답 대기 (타임아웃 포함)
  LOG_INFO("Waiting for response: %s (timeout: %lu ms)", expected_response, timeout_ms);
  BaseType_t result = xSemaphoreTake(request.wait_sem, request.timeout_ticks);

  // 세마포어 해제 및 정리
  vSemaphoreDelete(request.wait_sem);

  xSemaphoreTake(ble_instance.mutex, portMAX_DELAY);
  ble_at_status_t status = request.status;

  // 응답 버퍼 복사 (사용자가 제공한 경우)
  if (response_buf && response_buf_size > 0 && request.response_len > 0) {
    size_t copy_len = (request.response_len < response_buf_size - 1) ?
                      request.response_len : (response_buf_size - 1);
    memcpy(response_buf, request.response_buf, copy_len);
    response_buf[copy_len] = '\0';
  }

  ble_instance.async_request = NULL;
  xSemaphoreGive(ble_instance.mutex);

  // Bypass 모드로 전환 (응답 수신 후)
  LOG_INFO("Switching to bypass mode");
  if (ble_instance.ble.ops && ble_instance.ble.ops->bypass_mode) {
    ble_instance.ble.ops->bypass_mode();
  }

  if (result == pdFALSE) {
    LOG_ERR("AT command timeout");
    return BLE_AT_STATUS_TIMEOUT;
  }

  LOG_INFO("AT command completed with status: %d", status);
  return status;
}

// BLE 디바이스 이름 설정 (AT+MANUF=<name>)
bool ble_set_device_name_async(const char *device_name, uint32_t timeout_ms) {
  if (!device_name) {
    LOG_ERR("Device name is NULL");
    return false;
  }

  // AT+MANUF=<name>\r\n 커맨드 생성
  char at_cmd[64];
  snprintf(at_cmd, sizeof(at_cmd), "AT+MANUF=%s\r\n", device_name);

  // 응답 버퍼
  char response[BLE_AT_RESPONSE_MAX_SIZE];

  // 비동기 AT 커맨드 전송 (+OK 응답 기대)
  ble_at_status_t status = ble_send_at_command_async(at_cmd, "+OK", response, sizeof(response), timeout_ms);

  if (status == BLE_AT_STATUS_COMPLETED) {
    LOG_INFO("Device name set successfully: %s", device_name);
    return true;
  } else if (status == BLE_AT_STATUS_TIMEOUT) {
    LOG_ERR("Device name setting timeout");
    return false;
  } else if (status == BLE_AT_STATUS_ERROR) {
    LOG_ERR("Device name setting error: %s", response);
    return false;
  }

  LOG_ERR("Device name setting failed with status: %d", status);
  return false;
}

// BLE UART 통신 속도 설정 (AT+UART=<baudrate>)
bool ble_set_uart_baudrate_async(uint32_t baudrate, uint32_t timeout_ms) {
  // AT+UART=<baudrate>\r\n 커맨드 생성
  char at_cmd[64];
  snprintf(at_cmd, sizeof(at_cmd), "AT+UART=%lu\r\n", baudrate);

  // 응답 버퍼
  char response[BLE_AT_RESPONSE_MAX_SIZE];

  // 비동기 AT 커맨드 전송 (+OK 응답 기대)
  ble_at_status_t status = ble_send_at_command_async(at_cmd, "+OK", response, sizeof(response), timeout_ms);

  if (status == BLE_AT_STATUS_COMPLETED) {
    LOG_INFO("UART baudrate set successfully: %lu", baudrate);
    return true;
  } else if (status == BLE_AT_STATUS_TIMEOUT) {
    LOG_ERR("UART baudrate setting timeout");
    return false;
  } else if (status == BLE_AT_STATUS_ERROR) {
    LOG_ERR("UART baudrate setting error: %s", response);
    return false;
  }

  LOG_ERR("UART baudrate setting failed with status: %d", status);
  return false;
}

// BLE 초기 설정 시퀀스 (디바이스 이름 + UART 속도 설정)
bool ble_configure_async(const char *device_name, uint32_t baudrate) {
  const uint32_t timeout_ms = 5000;  // 5초 타임아웃

  LOG_INFO("BLE configuration started");

  // 1. 디바이스 이름 설정
  if (device_name != NULL) {
    LOG_INFO("Setting device name: %s", device_name);
    if (!ble_set_device_name_async(device_name, timeout_ms)) {
      LOG_ERR("Failed to set device name");
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));  // 커맨드 간 대기
  }

  // 2. UART 속도 설정
  if (baudrate > 0) {
    LOG_INFO("Setting UART baudrate: %lu", baudrate);
    if (!ble_set_uart_baudrate_async(baudrate, timeout_ms)) {
      LOG_ERR("Failed to set UART baudrate");
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));  // 커맨드 간 대기
  }

  LOG_INFO("BLE configuration completed successfully");
  return true;
}
