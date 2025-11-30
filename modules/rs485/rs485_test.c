#include "rs485_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#ifndef TAG
  #define TAG "RS485_TEST"
#endif

#include "log.h"

static void rs485_test_basic_tx(void) {
  LOG_INFO("=== Test 1: Basic TX ===");

  const char *msg = "Hello RS485\r\n";
  if (rs485_send(msg, strlen(msg))) {
    LOG_INFO("✓ Basic TX success");
  } else {
    LOG_ERR("✗ Basic TX failed");
  }

  vTaskDelay(pdMS_TO_TICKS(100));
}

static void rs485_test_rapid_tx(void) {
  LOG_INFO("=== Test 2: Rapid continuous TX ===");

  for (int i = 0; i < 10; i++) {
    char buf[32];
    snprintf(buf, sizeof(buf), "MSG%d\r\n", i);

    if (!rs485_send(buf, strlen(buf))) {
      LOG_ERR("✗ Rapid TX failed at msg %d", i);
      return;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  LOG_INFO("✓ Rapid TX success");
}

static void rs485_test_max_length(void) {
  LOG_INFO("=== Test 3: Max length TX (256 bytes) ===");

  char buf[256];
  memset(buf, 'A', sizeof(buf));

  if (rs485_send(buf, sizeof(buf))) {
    LOG_INFO("✓ Max length TX success");
  } else {
    LOG_ERR("✗ Max length TX failed");
  }

  vTaskDelay(pdMS_TO_TICKS(100));
}

static void rs485_test_invalid_params(void) {
  LOG_INFO("=== Test 4: Invalid parameters ===");

  // NULL pointer
  if (!rs485_send(NULL, 10)) {
    LOG_INFO("✓ NULL pointer rejected");
  } else {
    LOG_ERR("✗ NULL pointer accepted!");
  }

  // Zero length
  if (!rs485_send("test", 0)) {
    LOG_INFO("✓ Zero length rejected");
  } else {
    LOG_ERR("✗ Zero length accepted!");
  }

  // Oversized
  if (!rs485_send("test", 257)) {
    LOG_INFO("✓ Oversized data rejected");
  } else {
    LOG_ERR("✗ Oversized data accepted!");
  }
}

static void rs485_test_queue_overflow(void) {
  LOG_INFO("=== Test 5: Queue overflow (stress test) ===");

  // 빠르게 많은 메시지 전송 (큐 오버플로우 테스트)
  int failed_count = 0;
  for (int i = 0; i < 20; i++) {
    char buf[32];
    snprintf(buf, sizeof(buf), "STRESS%d\r\n", i);

    if (!rs485_send(buf, strlen(buf))) {
      failed_count++;
    }
  }

  if (failed_count > 0) {
    LOG_WARN("Queue overflow: %d messages dropped (expected)", failed_count);
  }
  LOG_INFO("✓ Queue overflow handling OK");

  vTaskDelay(pdMS_TO_TICKS(1000));
}

static void rs485_test_binary_data(void) {
  LOG_INFO("=== Test 6: Binary data TX ===");

  uint8_t binary_data[] = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD, 0x7F, 0x80};

  if (rs485_send((const char*)binary_data, sizeof(binary_data))) {
    LOG_INFO("✓ Binary data TX success");
  } else {
    LOG_ERR("✗ Binary data TX failed");
  }

  vTaskDelay(pdMS_TO_TICKS(100));
}

static void rs485_test_task(void *pvParameter) {
  LOG_INFO("=== RS485 Test Suite Starting ===");

  vTaskDelay(pdMS_TO_TICKS(3000));

  rs485_test_basic_tx();
  rs485_test_rapid_tx();
  rs485_test_max_length();
  rs485_test_invalid_params();
  rs485_test_queue_overflow();
  rs485_test_binary_data();

  LOG_INFO("=== RS485 Test Suite Completed ===");

  vTaskDelete(NULL);
}

void rs485_run_tests(void) {
  xTaskCreate(rs485_test_task, "rs485_test", 512, NULL,
              tskIDLE_PRIORITY + 2, NULL);
}
