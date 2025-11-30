#include "rtcm.h"
#include "lora_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>
#include <stdio.h>

#ifndef TAG
  #define TAG "RTCM"
#endif

#include "log.h"

#define RTCM_MAX_LORA_SIZE 236  // Max LoRa transmission size (bytes)
#define RTCM_TX_QUEUE_SIZE 10   // RTCM 전송 큐 크기

// LoRa Time on Air calculation (SF7, BW125, CR4/5, Preamble 8)
// Measured: 236 bytes = 350ms, with 20% margin = 420ms
#define LORA_TOA_BASE_BYTES 236
#define LORA_TOA_BASE_MS 350
#define LORA_TOA_MARGIN_PERCENT 20  // 20% margin

// RTCM 전송 큐 데이터 구조
typedef struct {
  uint8_t data[RTCM_MAX_LORA_SIZE + 1];
  size_t len;
  uint16_t msg_type;
} rtcm_tx_item_t;

static QueueHandle_t rtcm_tx_queue = NULL;
static TaskHandle_t rtcm_tx_task_handle = NULL;

/**
 * @brief Calculate LoRa Time on Air (ToA) with margin
 *
 * @param bytes Payload size in bytes
 * @return Time on Air in milliseconds (with 20% margin)
 */
static uint32_t calculate_lora_toa(size_t bytes) {
  // ToA(ms) = (bytes / 236) * 350 * 1.2
  uint32_t toa_ms = (bytes * LORA_TOA_BASE_MS / LORA_TOA_BASE_BYTES);
  toa_ms = toa_ms * (100 + LORA_TOA_MARGIN_PERCENT) / 100;
  return toa_ms;
}

/**
 * @brief RTCM TX Task (큐에서 RTCM 데이터 꺼내서 LoRa 전송)
 */
static void rtcm_tx_task(void *pvParameter) {
  rtcm_tx_item_t item;

  LOG_INFO("RTCM TX Task started");

  while (1) {
    // 큐에서 RTCM 데이터 대기
    if (xQueueReceive(rtcm_tx_queue, &item, portMAX_DELAY) == pdTRUE) {
      // Calculate Time on Air (ToA) for this packet
      uint32_t toa_ms = calculate_lora_toa(item.len);

      LOG_INFO("RTCM TX: type=%d, len=%d, ToA=%dms", item.msg_type, item.len, toa_ms);

      // Record start time
      TickType_t start_tick = xTaskGetTickCount();

      // Send raw binary data directly (at+send=lorap2p:[raw binary]\r\n)
      if (!lora_send_p2p_raw(item.data, item.len, toa_ms)) {
        LOG_ERR("Failed to send RTCM via LoRa");
        continue;
      }

      // Calculate elapsed time (OK response time)
      TickType_t elapsed_tick = xTaskGetTickCount() - start_tick;
      uint32_t elapsed_ms = elapsed_tick * 1000 / configTICK_RATE_HZ;

      // Wait for remaining ToA time
      if (elapsed_ms < toa_ms) {
        uint32_t remaining_ms = toa_ms - elapsed_ms;
        vTaskDelay(pdMS_TO_TICKS(remaining_ms));
        LOG_INFO("RTCM TX complete: OK_time=%dms, wait=%dms, total=%dms",
                 elapsed_ms, remaining_ms, toa_ms);
      } else {
        LOG_WARN("RTCM TX took longer than ToA: %dms > %dms", elapsed_ms, toa_ms);
      }

      LOG_INFO("RTCM sent successfully (type=%d)", item.msg_type);
    }
  }

  vTaskDelete(NULL);
}

void rtcm_tx_task_init(void) {
  // 큐 생성
  rtcm_tx_queue = xQueueCreate(RTCM_TX_QUEUE_SIZE, sizeof(rtcm_tx_item_t));
  if (rtcm_tx_queue == NULL) {
    LOG_ERR("Failed to create RTCM TX queue");
    return;
  }

  // Task 생성
  BaseType_t ret = xTaskCreate(rtcm_tx_task, "rtcm_tx", 1024,
                                NULL, tskIDLE_PRIORITY + 2, &rtcm_tx_task_handle);
  if (ret != pdPASS) {
    LOG_ERR("Failed to create RTCM TX task");
    return;
  }

  LOG_INFO("RTCM TX task initialized");
}

bool rtcm_send_to_lora(gps_t *gps) {
  if (!gps) {
    LOG_ERR("GPS handle is NULL");
    return false;
  }

  if (!rtcm_tx_queue) {
    LOG_ERR("RTCM TX queue not initialized");
    return false;
  }

  // RTCM packet total length
  size_t rtcm_len = gps->rtcm.total_len;

  if (rtcm_len == 0) {
    LOG_ERR("RTCM length is zero");
    return false;
  }

  // Prepare queue item
  rtcm_tx_item_t item;
  item.msg_type = gps->rtcm.msg_type;

  // Copy RTCM raw binary data from payload
  memcpy(item.data, gps->payload, rtcm_len);

  // RAK4270 module constraint: raw data transmission requires even byte count
  if (rtcm_len % 2 != 0) {
    // Odd bytes -> Add 0x00 padding at the end
    item.data[rtcm_len] = 0x00;
    item.len = rtcm_len + 1;
  } else {
    item.len = rtcm_len;
  }

  // Check padded length (MUST be after padding calculation)
  if (item.len > RTCM_MAX_LORA_SIZE) {
    LOG_ERR("RTCM length too large after padding: %d > %d (max)", item.len, RTCM_MAX_LORA_SIZE);
    return false;
  }

  // 큐에 추가 (비동기, GPS Task 블록 안 됨!)
  if (xQueueSend(rtcm_tx_queue, &item, 0) != pdTRUE) {
    LOG_WARN("RTCM TX queue full, packet dropped (type=%d)", item.msg_type);
    return false;
  }

  // 즉시 리턴 (블록킹 없음!)
  return true;
}
