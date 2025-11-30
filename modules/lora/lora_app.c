#include "lora.h"
#include "lora_app.h"
#include "lora_port.h"
#include "board_config.h"
#include "gps_app.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef TAG
#define TAG "LORA_APP"
#endif

#include "log.h"

#define LORA_CMD_QUEUE_SIZE 10
#define LORA_AT_CMD_TIMEOUT_MS 2000
#define LORA_INIT_MAX_RETRY 3
#define LORA_INIT_TIMEOUT_MS 2000 // work_mode AT command timeout

#define LORA_RECV_BUF_SIZE 1024

static void lora_process_task(void *pvParameter);
static void lora_tx_task(void *pvParameter);
static void lora_tx_test_task(void *pvParameter);

/**
 * @brief LoRa P2P BASE 모드 초기화 명령어
 */
static const char *lora_p2p_base_cmds[] = {
    "at+set_config=lora:work_mode:1\r\n",             // P2P 모드 (1=P2P, 0=LoRaWAN)
    "at+set_config=lorap2p:920900000:7:0:1:8:14\r\n", // 920.9MHz, SF7, BW125kHz, CR4/5, Preamble8, 14dBm
    "at+set_config=lorap2p:transfer_mode:2\r\n",      // Transfer mode 2 (BASE)
};

/**
 * @brief LoRa P2P ROVER 모드 초기화 명령어
 */
static const char *lora_p2p_rover_cmds[] = {
    "at+set_config=lora:work_mode:1\r\n",             // P2P 모드 (1=P2P, 0=LoRaWAN)
    "at+set_config=lorap2p:920900000:7:0:1:8:14\r\n", // 920.9MHz, SF7, BW125kHz, CR4/5, Preamble8, 14dBm
    "at+set_config=lorap2p:transfer_mode:1\r\n",      // Transfer mode 1 (ROVER)
};

#define LORA_P2P_BASE_CMD_COUNT (sizeof(lora_p2p_base_cmds) / sizeof(lora_p2p_base_cmds[0]))
#define LORA_P2P_ROVER_CMD_COUNT (sizeof(lora_p2p_rover_cmds) / sizeof(lora_p2p_rover_cmds[0]))

static void lora_tx_test_task(void *pvParameter)
{

  // "hello world\r\n"의 HEX 표현
  const char *test_data = "68656C6C6F20776F726C640D0A68656C6C6F20776F726C640D0A68656C6C6F20776F726C640D0A68656C6C6F20776F726C640D0A68656C6C6F20776F726C640D0A68656C6C6F20776F726C640D0A68656C6C6F20776F726C640D0A68656C6C6F20776F726C640D0A68656C6C6F20776F726C640D0D0D";

  LOG_INFO("LoRa TX Test Task started - sending 'hello world\\r\\n' every 10 seconds");

  // 초기화 완료 대기 (5초)
  vTaskDelay(pdMS_TO_TICKS(5000));

  while (1)
  {
    LOG_INFO("Sending test data: hello world\\r\\n");

    if (lora_send_p2p_data(test_data, LORA_AT_CMD_TIMEOUT_MS))
    {
      LOG_INFO("Test data sent successfully");
    }
    else
    {
      LOG_ERR("Failed to send test data");
    }

    // 10초 대기
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  vTaskDelete(NULL);
}

/**

 * @brief LoRa 송신 테스트 시작

 */

void lora_start_tx_test(void)
{

  BaseType_t ret = xTaskCreate(lora_tx_test_task, "lora_tx_test", 512,

                               NULL, tskIDLE_PRIORITY + 2, NULL);

  if (ret != pdPASS)
  {

    LOG_ERR("Failed to create LoRa TX Test Task");
  }
  else
  {

    LOG_INFO("LoRa TX Test Task created successfully");
  }
}

typedef void (*lora_init_callback_t)(bool success, void *user_data);

typedef struct
{
  uint8_t current_step;          // 현재 단계 (0 ~ cmd_count-1)
  uint8_t retry_count;           // 현재 단계 재시도 횟수
  const char **cmd_list;         // 명령어 리스트
  uint8_t cmd_count;             // 명령어 개수
  lora_init_callback_t callback; // 완료 콜백
} lora_init_context_t;

typedef struct
{
  lora_t lora;
  QueueHandle_t queue;     // RX 이벤트 큐
  QueueHandle_t cmd_queue; // TX 명령어 큐
  TaskHandle_t rx_task;    // RX Task
  TaskHandle_t tx_task;    // TX Task
  SemaphoreHandle_t mutex; // UART 송신 보호용 mutex
  bool initialized;
  bool init_complete;
  bool tx_task_ready;      // TX Task 준비 완료 플래그
  bool rx_task_ready;      // RX Task 준비 완료 플래그

  lora_cmd_request_t *current_cmd_req;        // 현재 처리 중인 명령어
  lora_p2p_recv_callback_t p2p_recv_callback; // P2P 수신 콜백
  void *p2p_recv_user_data;                   // P2P 수신 콜백 사용자 데이터
} lora_app_instance_t;

static lora_app_instance_t instance;

/**
 * @brief LoRa 초기화 완료 콜백
 */
static void lora_overall_init_complete(bool success, void *user_data)
{

  const board_config_t *config = board_get_config();
  if (config->lora_mode == LORA_MODE_BASE)
  {
    LOG_INFO("LoRa BASE init %s", success ? "succeeded" : "failed");
  }
  else if (config->lora_mode == LORA_MODE_ROVER)
  {
    LOG_INFO("LoRa ROVER init %s", success ? "succeeded" : "failed");
  }

  if (success)
  {
    instance.init_complete = true;
    LOG_INFO("LoRa init complete - now accepting P2P data");
  }
}

/**
 * @brief LoRa 초기화 명령어 콜백 (재귀적 호출)
 */
static void lora_init_command_callback(bool success, void *user_data)
{
  lora_init_context_t *ctx = (lora_init_context_t *)user_data;

  if (!ctx)
  {
    LOG_ERR("LoRa init context is NULL");
    return;
  }

  if (success)
  {
    // 명령어 성공
    LOG_INFO("LoRa init step %d/%d OK: %s",
             ctx->current_step + 1, ctx->cmd_count,
             ctx->cmd_list[ctx->current_step]);

    // 다음 단계로
    ctx->current_step++;
    ctx->retry_count = 0;

    // 모든 단계 완료?
    if (ctx->current_step >= ctx->cmd_count)
    {
      LOG_INFO("LoRa init sequence complete!");
      if (ctx->callback)
      {
        ctx->callback(true, NULL);
      }
      // 컨텍스트 메모리 해제
      vPortFree(ctx);
      return;
    }

    // 다음 명령어 전송
    // work_mode 명령어는 응답 파싱 건너뛰기
    bool skip = (strstr(ctx->cmd_list[ctx->current_step], "work_mode") != NULL);
    lora_send_command_async(ctx->cmd_list[ctx->current_step],
                            LORA_INIT_TIMEOUT_MS, lora_init_command_callback, ctx, skip);
  }
  else
  {
    // 명령어 실패
    ctx->retry_count++;

    if (ctx->retry_count < LORA_INIT_MAX_RETRY)
    {
      // 재시도
      LOG_WARN("LoRa init step %d/%d failed, retrying (%d/%d): %s",
               ctx->current_step + 1, ctx->cmd_count,
               ctx->retry_count, LORA_INIT_MAX_RETRY,
               ctx->cmd_list[ctx->current_step]);

      // 같은 명령어 재전송
      // work_mode 명령어는 응답 파싱 건너뛰기
      bool skip = (strstr(ctx->cmd_list[ctx->current_step], "work_mode") != NULL);
      lora_send_command_async(ctx->cmd_list[ctx->current_step],
                              LORA_INIT_TIMEOUT_MS, lora_init_command_callback, ctx, skip);
    }
    else
    {
      // 최대 재시도 초과
      LOG_ERR("LoRa init failed at step %d/%d after %d retries: %s",
              ctx->current_step + 1, ctx->cmd_count,
              LORA_INIT_MAX_RETRY, ctx->cmd_list[ctx->current_step]);

      if (ctx->callback)
      {
        ctx->callback(false, NULL);
      }
      // 컨텍스트 메모리 해제
      vPortFree(ctx);
    }
  }
}

/**
 * @brief LoRa P2P BASE 모드 초기화 (비동기)
 */
static bool lora_init_p2p_base_async(lora_init_callback_t callback)
{
  // 초기화 컨텍스트 생성 (동적 할당)
  lora_init_context_t *ctx = (lora_init_context_t *)pvPortMalloc(sizeof(lora_init_context_t));
  if (!ctx)
  {
    LOG_ERR("Failed to allocate LoRa init context");
    return false;
  }

  // 컨텍스트 초기화
  ctx->current_step = 0;
  ctx->retry_count = 0;
  ctx->cmd_list = lora_p2p_base_cmds;
  ctx->cmd_count = LORA_P2P_BASE_CMD_COUNT;
  ctx->callback = callback;

  LOG_INFO("Starting LoRa P2P BASE init sequence (%d commands)", ctx->cmd_count);

  // 첫 번째 명령어 전송
  // work_mode 명령어는 응답 파싱 건너뛰기
  bool skip = (strstr(ctx->cmd_list[0], "work_mode") != NULL);
  if (!lora_send_command_async(ctx->cmd_list[0], LORA_INIT_TIMEOUT_MS,
                               lora_init_command_callback, ctx, skip))
  {
    LOG_ERR("Failed to start LoRa init sequence");
    vPortFree(ctx);
    return false;
  }

  return true;
}

/**
 * @brief LoRa P2P ROVER 모드 초기화 (비동기)
 */
static bool lora_init_p2p_rover_async(lora_init_callback_t callback)
{
  // 초기화 컨텍스트 생성 (동적 할당)
  lora_init_context_t *ctx = (lora_init_context_t *)pvPortMalloc(sizeof(lora_init_context_t));
  if (!ctx)
  {
    LOG_ERR("Failed to allocate LoRa init context");
    return false;
  }

  // 컨텍스트 초기화
  ctx->current_step = 0;
  ctx->retry_count = 0;
  ctx->cmd_list = lora_p2p_rover_cmds;
  ctx->cmd_count = LORA_P2P_ROVER_CMD_COUNT;
  ctx->callback = callback;

  LOG_INFO("Starting LoRa P2P ROVER init sequence (%d commands)", ctx->cmd_count);

  // 첫 번째 명령어 전송
  // work_mode 명령어는 응답 파싱 건너뛰기
  bool skip = (strstr(ctx->cmd_list[0], "work_mode") != NULL);
  if (!lora_send_command_async(ctx->cmd_list[0], LORA_INIT_TIMEOUT_MS,
                               lora_init_command_callback, ctx, skip))
  {
    LOG_ERR("Failed to start LoRa init sequence");
    vPortFree(ctx);
    return false;
  }

  return true;
}

/**
 * @brief AT 명령어 응답 파싱 (OK/ERROR 감지)
 *
 * @param data 응답 데이터
 * @param len 데이터 길이
 * @return true: OK, false: ERROR or 미감지
 */
static bool lora_parse_at_response(const char *data, size_t len)
{
  // OK 응답 감지 (다양한 형식 지원)
  // - "OK\r\n", "OK\n" (일반 응답)
  // - "Initialization OK" (work_mode 변경 시)
  // - 대소문자 무시
  if (strstr(data, "OK") != NULL || strstr(data, "ok") != NULL)
  {
    return true;
  }

  // ERROR 응답 감지
  if (strstr(data, "ERROR") != NULL || strstr(data, "error") != NULL)
  {
    return false;
  }

  return false;
}

/**
 * @brief AT+RECV 응답 파싱
 *
 * Format: at+recv=<RSSI>,<SNR>,<Data Length>:<Data>
 * Example: at+recv=-50,10,5:48656C6C6F
 *
 * @param data 응답 데이터
 * @param recv_data 출력 구조체
 * @return true: 파싱 성공, false: 파싱 실패
 */
static bool lora_parse_p2p_recv(const char *data, lora_p2p_recv_data_t *recv_data)
{
  // "at+recv=" 또는 "AT+RECV=" 찾기
  const char *start = strstr(data, "at+recv=");
  if (!start)
  {
    start = strstr(data, "AT+RECV=");
  }
  if (!start)
  {
    return false;
  }

  start += 8; // "at+recv=" 건너뛰기

  // RSSI 파싱
  char *end = NULL;
  recv_data->rssi = (int16_t)strtol(start, &end, 10);
  if (!end || *end != ',')
  {
    return false;
  }

  // SNR 파싱
  start = end + 1;
  recv_data->snr = (int16_t)strtol(start, &end, 10);
  if (!end || *end != ',')
  {
    return false;
  }

  // Data Length 파싱
  start = end + 1;
  recv_data->data_len = (uint16_t)strtol(start, &end, 10);
  if (!end || *end != ':')
  {
    return false;
  }

  // Data 파싱 (HEX string)
  start = end + 1;

  // 데이터 길이 확인
  if (recv_data->data_len > sizeof(recv_data->data) - 1)
  {
    LOG_ERR("P2P recv data too long: %d bytes", recv_data->data_len);
    return false;
  }

  // HEX string을 바이너리로 변환
  for (uint16_t i = 0; i < recv_data->data_len; i++)
  {
    char hex[3] = {start[i * 2], start[i * 2 + 1], '\0'};
    recv_data->data[i] = (char)strtol(hex, NULL, 16);
  }
  recv_data->data[recv_data->data_len] = '\0';

  LOG_INFO("P2P recv: RSSI=%d, SNR=%d, Len=%d",
           recv_data->rssi, recv_data->snr, recv_data->data_len);

  return true;
}

/**
 * @brief LoRa TX Task (명령어 송신 및 응답 대기)
 */
static void lora_tx_task(void *pvParameter)
{
  lora_cmd_request_t cmd_req;

  LOG_INFO("LoRa TX Task started");

  // TX Task 준비 완료 플래그 설정
  instance.tx_task_ready = true;
  LOG_INFO("LoRa TX Task ready");

  while (1)
  {
    if (xQueueReceive(instance.cmd_queue, &cmd_req, portMAX_DELAY) == pdTRUE)
    {
      LOG_INFO("LoRa sending command: %s", cmd_req.cmd);

      // 현재 명령어 요청 저장 (RX Task에서 응답 처리용)
      instance.current_cmd_req = &cmd_req;
      if (cmd_req.is_async)
      {
        cmd_req.async_result = false;
      }
      else
      {
        *(cmd_req.result) = false;
      }

      // 명령어 전송 (UART 충돌 방지를 위해 mutex 사용)
      if (instance.lora.ops && instance.lora.ops->send)
      {
        xSemaphoreTake(instance.mutex, portMAX_DELAY);
        instance.lora.ops->send(cmd_req.cmd, strlen(cmd_req.cmd));
        xSemaphoreGive(instance.mutex);
      }
      else
      {
        LOG_ERR("LoRa send ops not available");
        instance.current_cmd_req = NULL;
        if (cmd_req.is_async)
        {
          cmd_req.async_result = false;
          if (cmd_req.callback)
          {
            cmd_req.callback(false, cmd_req.user_data);
          }
          vSemaphoreDelete(cmd_req.response_sem);
        }
        else
        {
          *(cmd_req.result) = false;
          xSemaphoreGive(cmd_req.response_sem);
        }
        continue;
      }

      // skip_response이면 응답 파싱 건너뛰고 delay 후 성공 처리
      if (cmd_req.skip_response)
      {
        LOG_INFO("Skipping response check, waiting %d ms", cmd_req.timeout_ms);
        vTaskDelay(pdMS_TO_TICKS(cmd_req.timeout_ms));

        // 성공으로 처리
        if (cmd_req.is_async)
        {
          cmd_req.async_result = true;
        }
        else
        {
          *(cmd_req.result) = true;
        }
      }
      else
      {
        // 응답 대기 (타임아웃 적용)
        if (xSemaphoreTake(cmd_req.response_sem, pdMS_TO_TICKS(cmd_req.timeout_ms)) == pdTRUE)
        {
          // 응답 수신 완료 (RX Task가 세마포어를 줌)
          if (cmd_req.is_async)
          {
            LOG_INFO("LoRa response received: %s",
                     cmd_req.async_result ? "OK" : "ERROR");
          }
          else
          {
            LOG_INFO("LoRa response received: %s",
                     *(cmd_req.result) ? "OK" : "ERROR");
          }
        }
        else
        {
          // 타임아웃
          LOG_WARN("LoRa command timeout");
          if (cmd_req.is_async)
          {
            cmd_req.async_result = false;
          }
          else
          {
            *(cmd_req.result) = false;
          }
        }
      }

      // 현재 명령어 요청 초기화
      instance.current_cmd_req = NULL;

      // 비동기: 콜백 호출
      if (cmd_req.is_async)
      {
        if (cmd_req.callback)
        {
          cmd_req.callback(cmd_req.async_result, cmd_req.user_data);
        }

        // 비동기는 세마포어를 TX Task에서 삭제
        vSemaphoreDelete(cmd_req.response_sem);
      }
      else
      {
        // 동기: 외부 호출자에게 처리 완료 알림 (세마포어 반환)
        xSemaphoreGive(cmd_req.response_sem);
      }
    }
  }

  vTaskDelete(NULL);
}

/**
 * @brief LoRa RX Task (수신 데이터 처리)
 */
static void lora_process_task(void *pvParameter)
{
  size_t pos = 0;
  size_t old_pos = 0;
  uint8_t dummy = 0;
  static char temp_buf[1024];
  LOG_INFO("LoRa RX Task started");

  // RX Task 준비 완료 플래그 설정
  instance.rx_task_ready = true;
  LOG_INFO("LoRa RX Task ready");

  // TX/RX Task 모두 준비될 때까지 대기
  while (!instance.tx_task_ready || !instance.rx_task_ready)
  {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  LOG_INFO("Both TX and RX tasks ready, starting LoRa initialization");

  const board_config_t *config = board_get_config();

  if (config->lora_mode == LORA_MODE_BASE)
  {
    lora_init_p2p_base_async(lora_overall_init_complete);
  }
  else if (config->lora_mode == LORA_MODE_ROVER)
  {
    lora_init_p2p_rover_async(lora_overall_init_complete);
  }

  while (1)
  {
    xQueueReceive(instance.queue, &dummy, portMAX_DELAY);

    pos = lora_port_get_rx_pos();
    char *lora_recv = lora_port_get_recv_buf();

    if (pos != old_pos)
    {
      size_t len = 0;

      if (pos > old_pos)
      {
        len = pos - old_pos;

        // 데이터 처리
        memcpy(temp_buf, &lora_recv[old_pos], len);
        temp_buf[len] = '\0';

        LOG_INFO("LoRa recv: %s", temp_buf);

        // AT 명령어 응답 처리
        if (instance.current_cmd_req != NULL)
        {
          LOG_INFO("current_cmd_req is NOT NULL, parsing response...");
          bool result = lora_parse_at_response(temp_buf, len);
          LOG_INFO("Parse result: %s", result ? "OK" : "ERROR/NONE");

          if (strstr(temp_buf, "OK") || strstr(temp_buf, "ERROR"))
          {
            LOG_INFO("OK/ERROR detected, giving semaphore...");
            // 응답 결과 저장
            if (instance.current_cmd_req->is_async)
            {
              instance.current_cmd_req->async_result = result;
            }
            else
            {
              *(instance.current_cmd_req->result) = result;
            }

            // 세마포어 해제 (TX Task로 응답 완료 알림)
            xSemaphoreGive(instance.current_cmd_req->response_sem);
            LOG_INFO("Semaphore given!");
          }
          else
          {
            LOG_WARN("No OK/ERROR in response, ignoring...");
          }
        }
        else
        {
          LOG_WARN("current_cmd_req is NULL, skipping response handling");
        }

        // P2P 수신 데이터 처리 (at+recv=...)
        // 초기화 완료 후에만 처리 (초기화 중 데이터는 무시)

        if ((strstr(temp_buf, "at+recv=") || strstr(temp_buf, "AT+RECV=")) && instance.init_complete)
        {

          lora_p2p_recv_data_t recv_data;

          if (lora_parse_p2p_recv(temp_buf, &recv_data))
          {

            // 콜백이 등록되어 있으면 콜백 호출

            if (instance.p2p_recv_callback)
            {

              instance.p2p_recv_callback(&recv_data, instance.p2p_recv_user_data);
            }
            else
            {

              // 콜백이 없으면 GPS로 자동 전송 (RTCM 데이터)

              gps_t *gps = gps_get_instance_handle(GPS_ID_BASE);

              if (gps && gps->ops && gps->ops->send)
              {

                xSemaphoreTake(gps->mutex, portMAX_DELAY);

                gps->ops->send(recv_data.data, recv_data.data_len);

                xSemaphoreGive(gps->mutex);

                LOG_INFO("P2P data forwarded to GPS (%d bytes)", recv_data.data_len);
              }
            }
          }
        }
        else if ((strstr(temp_buf, "at+recv=") || strstr(temp_buf, "AT+RECV=")) && !instance.init_complete)
        {

          LOG_WARN("Ignoring P2P data during initialization");
        }

        old_pos = pos;
      }
      else
      {
        // Circular buffer wrap-around
        len = LORA_RECV_BUF_SIZE - old_pos + pos;

        size_t first_part = LORA_RECV_BUF_SIZE - old_pos;
        memcpy(temp_buf, &lora_recv[old_pos], first_part);
        memcpy(&temp_buf[first_part], &lora_recv[0], pos);
        temp_buf[len] = '\0';

        LOG_INFO("LoRa recv (wrap): %s", temp_buf);

        // 동일한 처리 로직
        if (instance.current_cmd_req != NULL)
        {
          bool result = lora_parse_at_response(temp_buf, len);

          if (strstr(temp_buf, "OK") || strstr(temp_buf, "ERROR"))
          {
            if (instance.current_cmd_req->is_async)
            {
              instance.current_cmd_req->async_result = result;
            }
            else
            {
              *(instance.current_cmd_req->result) = result;
            }

            xSemaphoreGive(instance.current_cmd_req->response_sem);
          }
        }

        if ((strstr(temp_buf, "at+recv=") || strstr(temp_buf, "AT+RECV=")) && instance.init_complete)
        {

          lora_p2p_recv_data_t recv_data;

          if (lora_parse_p2p_recv(temp_buf, &recv_data))
          {

            if (instance.p2p_recv_callback)
            {

              instance.p2p_recv_callback(&recv_data, instance.p2p_recv_user_data);
            }
            else
            {

              gps_t *gps = gps_get_instance_handle(GPS_ID_BASE);

              if (gps && gps->ops && gps->ops->send)
              {

                xSemaphoreTake(gps->mutex, portMAX_DELAY);

                gps->ops->send(recv_data.data, recv_data.data_len);

                xSemaphoreGive(gps->mutex);

                LOG_INFO("P2P data forwarded to GPS (%d bytes)", recv_data.data_len);
              }
            }
          }
        }
        else if ((strstr(temp_buf, "at+recv=") || strstr(temp_buf, "AT+RECV=")) && !instance.init_complete)
        {

          LOG_WARN("Ignoring P2P data during initialization (wrap)");
        }

        old_pos = pos;
      }
    }
  }

  vTaskDelete(NULL);
}

void lora_instance_init(void)
{
  memset(&instance, 0, sizeof(lora_app_instance_t));
  lora_init(&instance.lora);

  if (lora_port_init_instance(&instance.lora) != 0)
  {
    LOG_ERR("LORA 포트 초기화 실패");
    return;
  }

#if LORA_MODE == LORA_MODE_BASE
  instance.queue = xQueueCreate(10, sizeof(uint8_t));
  if (instance.queue == NULL)
  {
    LOG_ERR("LORA RX 큐 생성 실패");
    return;
  }

#elif LORA_MODE == LORA_MODE_ROVER
  instance.queue = xQueueCreate(10, sizeof(uint8_t));
  if (instance.queue == NULL)
  {
    LOG_ERR("LORA RX 큐 생성 실패");
    return;
  }
#endif

  // TX 명령어 큐 생성
  instance.cmd_queue = xQueueCreate(LORA_CMD_QUEUE_SIZE, sizeof(lora_cmd_request_t));
  if (instance.cmd_queue == NULL)
  {
    LOG_ERR("LORA TX 큐 생성 실패");
    return;
  }

  lora_port_set_queue(instance.queue);
  instance.mutex = xSemaphoreCreateMutex();
  lora_port_start(&instance.lora);

  // RX Task 생성
  BaseType_t ret = xTaskCreate(lora_process_task, "lora_rx", 1024,
                               NULL, tskIDLE_PRIORITY + 3, &instance.rx_task);
  if (ret != pdPASS)
  {
    LOG_ERR("LORA RX Task 생성 실패");
    return;
  }

  // TX Task 생성
  ret = xTaskCreate(lora_tx_task, "lora_tx", 1024,
                    NULL, tskIDLE_PRIORITY + 3, &instance.tx_task);
  if (ret != pdPASS)
  {
    LOG_ERR("LORA TX Task 생성 실패");
    return;
  }

  instance.initialized = true;

  LOG_INFO("LORA 인스턴스 초기화 완료");
}

bool lora_send_command_sync(const char *cmd, uint32_t timeout_ms)
{
  if (!instance.initialized)
  {
    LOG_ERR("LoRa not initialized");
    return false;
  }

  if (!cmd || strlen(cmd) == 0)
  {
    LOG_ERR("Empty command");
    return false;
  }

  // 세마포어 생성
  SemaphoreHandle_t response_sem = xSemaphoreCreateBinary();
  if (response_sem == NULL)
  {
    LOG_ERR("Failed to create semaphore");
    return false;
  }

  bool result = false;

  // 명령어 요청 구조체 생성 (동기 방식)
  lora_cmd_request_t cmd_req = {
      .timeout_ms = timeout_ms,
      .is_async = false,
      .skip_response = false,
      .response_sem = response_sem,
      .result = &result,
      .callback = NULL,
      .user_data = NULL,
  };

  strncpy(cmd_req.cmd, cmd, sizeof(cmd_req.cmd) - 1);
  cmd_req.cmd[sizeof(cmd_req.cmd) - 1] = '\0';

  // TX 태스크로 명령어 전송 요청
  if (xQueueSend(instance.cmd_queue, &cmd_req, pdMS_TO_TICKS(1000)) != pdTRUE)
  {
    LOG_ERR("Failed to send command to TX task");
    vSemaphoreDelete(response_sem);
    return false;
  }

  // TX 태스크에서 처리 완료 대기
  if (xSemaphoreTake(response_sem, pdMS_TO_TICKS(timeout_ms + 1000)) == pdTRUE)
  {
    // 처리 완료
    vSemaphoreDelete(response_sem);
    return result;
  }
  else
  {
    // 외부 타임아웃 (TX 태스크 응답 없음)
    LOG_ERR("TX task did not respond");
    vSemaphoreDelete(response_sem);
    return false;
  }
}

bool lora_send_command_async(const char *cmd, uint32_t timeout_ms,
                             lora_command_callback_t callback, void *user_data,
                             bool skip_response)
{
  if (!instance.initialized)
  {
    LOG_ERR("LoRa not initialized");
    return false;
  }

  if (!cmd || strlen(cmd) == 0)
  {
    LOG_ERR("Empty command");
    return false;
  }

  // 세마포어 생성 (TX Task 내부에서 응답 대기용)
  SemaphoreHandle_t response_sem = xSemaphoreCreateBinary();
  if (response_sem == NULL)
  {
    LOG_ERR("Failed to create semaphore");
    return false;
  }

  // 명령어 요청 구조체 생성 (비동기 방식)
  lora_cmd_request_t cmd_req = {
      .timeout_ms = timeout_ms,
      .is_async = true,
      .skip_response = skip_response,
      .response_sem = response_sem,
      .result = NULL,
      .callback = callback,
      .user_data = user_data,
      .async_result = false,
  };

  strncpy(cmd_req.cmd, cmd, sizeof(cmd_req.cmd) - 1);
  cmd_req.cmd[sizeof(cmd_req.cmd) - 1] = '\0';

  // TX 태스크로 명령어 전송 요청
  if (xQueueSend(instance.cmd_queue, &cmd_req, pdMS_TO_TICKS(1000)) != pdTRUE)
  {
    LOG_ERR("Failed to send command to TX task");
    vSemaphoreDelete(response_sem);
    return false;
  }

  // 즉시 반환 (non-blocking)
  LOG_INFO("Async command queued");
  return true;
}

bool lora_set_work_mode(lora_work_mode_t mode, uint32_t timeout_ms)
{
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "at+set_config=lora:work_mode:%d\r\n", mode);
  return lora_send_command_sync(cmd, timeout_ms);
}

bool lora_set_p2p_config(uint32_t freq, uint8_t sf, uint8_t bw, uint8_t cr,
                         uint16_t preamlen, uint8_t pwr, uint32_t timeout_ms)
{
  char cmd[128];
  snprintf(cmd, sizeof(cmd),
           "at+set_config=lorap2p:%lu:%d:%d:%d:%d:%d\r\n",
           freq, sf, bw, cr, preamlen, pwr);
  return lora_send_command_sync(cmd, timeout_ms);
}

bool lora_set_p2p_transfer_mode(lora_p2p_transfer_mode_t mode, uint32_t timeout_ms)
{
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "at+set_config=lorap2p:transfer_mode:%d\r\n", mode);
  return lora_send_command_sync(cmd, timeout_ms);
}

bool lora_send_p2p_data(const char *data, uint32_t timeout_ms)
{
  if (!data)
  {
    LOG_ERR("NULL data");
    return false;
  }

  char cmd[512];
  snprintf(cmd, sizeof(cmd), "at+send=lorap2p:%s\r\n", data);
  return lora_send_command_sync(cmd, timeout_ms);
}

bool lora_send_p2p_raw(const uint8_t *data, size_t len, uint32_t timeout_ms)
{
  if (!instance.initialized)
  {
    LOG_ERR("LoRa not initialized");
    return false;
  }

  if (!data || len == 0)
  {
    LOG_ERR("NULL data or zero length");
    return false;
  }

  if (len > 236)
  {
    LOG_ERR("Data too large: %d > 236", len);
    return false;
  }

  // Convert binary data to HEX ASCII string
  // Each byte becomes 2 HEX characters, plus null terminator
  char hex_string[512];  // 236 * 2 + 1 = 473 bytes max
  if (len * 2 >= sizeof(hex_string))
  {
    LOG_ERR("HEX string buffer too small");
    return false;
  }

  for (size_t i = 0; i < len; i++)
  {
    snprintf(&hex_string[i * 2], 3, "%02X", data[i]);
  }
  hex_string[len * 2] = '\0';

  LOG_INFO("Sending raw P2P data: %d bytes -> %d HEX chars", len, len * 2);
  if (len >= 4) {
    LOG_INFO("First 4 bytes (binary): %02X %02X %02X %02X", data[0], data[1], data[2], data[3]);
    LOG_INFO("First 8 HEX chars: %.8s", hex_string);
  }

  // Create AT command: at+send=lorap2p:<HEX_STRING>\r\n
  char cmd[600];
  snprintf(cmd, sizeof(cmd), "at+send=lorap2p:%s\r\n", hex_string);

  // Use the standard command sending mechanism
  return lora_send_command_sync(cmd, timeout_ms);
}

void lora_set_p2p_recv_callback(lora_p2p_recv_callback_t callback, void *user_data)
{
  instance.p2p_recv_callback = callback;
  instance.p2p_recv_user_data = user_data;
}

lora_t *lora_get_handle(void)
{
  return &instance.lora;
}
