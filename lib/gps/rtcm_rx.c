#include "rtcm_rx.h"
#include "gps_app.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>

#ifndef TAG
  #define TAG "RTCM_RX"
#endif

#include "log.h"

// RTCM3 Preamble
#define RTCM3_PREAMBLE 0xD3

// RTCM RX 상태
typedef struct {
  uint8_t buffer[RTCM_RX_BUFFER_SIZE];  // Fragment 재조합 버퍼
  uint16_t buffer_pos;                  // 현재 버퍼 위치
  uint32_t last_fragment_time;          // 마지막 fragment 수신 시간 (timeout 체크용)
  SemaphoreHandle_t mutex;              // 버퍼 보호용 mutex
  bool receiving;                       // Fragment 수신 중 여부
} rtcm_rx_state_t;

static rtcm_rx_state_t rtcm_rx_state;

/**
 * @brief CRC24Q 계산 (RTCM3 표준)
 *
 * Polynomial: 0x1864CFB (CRC24Q)
 *
 * @param data 데이터
 * @param len 데이터 길이
 * @return CRC24 값 (24비트)
 */
static uint32_t rtcm3_crc24q(const uint8_t *data, size_t len) {
  uint32_t crc = 0;

  for (size_t i = 0; i < len; i++) {
    crc ^= ((uint32_t)data[i]) << 16;

    for (int j = 0; j < 8; j++) {
      crc <<= 1;
      if (crc & 0x1000000) {
        crc ^= 0x1864CFB;
      }
    }
  }

  return crc & 0xFFFFFF;
}

/**
 * @brief RTCM 패킷 검증 및 GPS 전송
 *
 * @param packet RTCM 패킷 (완전한 패킷)
 * @param packet_len 패킷 길이
 * @param gps_id GPS ID
 * @return true: 성공, false: 검증 실패
 */
static bool rtcm_validate_and_send(const uint8_t *packet, size_t packet_len, gps_id_t gps_id) {
  // 최소 길이 확인 (헤더 3 + 최소 페이로드 1 + CRC 3 = 7)
  if (packet_len < 7) {
    LOG_ERR("RTCM packet too short: %d bytes", packet_len);
    return false;
  }

  // Preamble 확인
  if (packet[0] != RTCM3_PREAMBLE) {
    LOG_ERR("Invalid RTCM preamble: 0x%02X (expected 0xD3)", packet[0]);
    return false;
  }

  // Length 파싱 (10 bits)
  uint16_t payload_len = ((packet[1] & 0x03) << 8) | packet[2];

  // 전체 길이 확인
  uint16_t expected_len = 3 + payload_len + 3;  // header + payload + CRC
  if (packet_len != expected_len) {
    LOG_ERR("RTCM length mismatch: got %d, expected %d (payload=%d)",
            packet_len, expected_len, payload_len);
    return false;
  }

  // Message Type 파싱 (12 bits)
  uint16_t msg_type = (packet[3] << 4) | (packet[4] >> 4);

  // CRC24 검증
  uint32_t calc_crc = rtcm3_crc24q(packet, packet_len - 3);
  uint32_t recv_crc = ((uint32_t)packet[packet_len - 3] << 16) |
                      ((uint32_t)packet[packet_len - 2] << 8) |
                      ((uint32_t)packet[packet_len - 1]);

  if (calc_crc != recv_crc) {
    LOG_ERR("RTCM CRC mismatch: calc=0x%06X, recv=0x%06X", calc_crc, recv_crc);
    return false;
  }

  LOG_INFO("RTCM packet valid: type=%d, len=%d bytes", msg_type, packet_len);

  // GPS UART로 전송
  gps_t *gps = gps_get_instance_handle(gps_id);
  if (!gps || !gps->ops || !gps->ops->send) {
    LOG_ERR("GPS[%d] not available", gps_id);
    return false;
  }

  xSemaphoreTake(gps->mutex, portMAX_DELAY);
  gps->ops->send((const char*)packet, packet_len);
  xSemaphoreGive(gps->mutex);

  LOG_INFO("RTCM packet sent to GPS[%d]: type=%d, %d bytes", gps_id, msg_type, packet_len);

  return true;
}

void rtcm_rx_init(void) {
  memset(&rtcm_rx_state, 0, sizeof(rtcm_rx_state));
  rtcm_rx_state.mutex = xSemaphoreCreateMutex();

  if (!rtcm_rx_state.mutex) {
    LOG_ERR("Failed to create RTCM RX mutex");
    return;
  }

  LOG_INFO("RTCM RX initialized");
}

bool rtcm_rx_process_fragment(const uint8_t *data, size_t len, gps_id_t gps_id) {
  if (!data || len == 0) {
    LOG_ERR("Invalid fragment data");
    return false;
  }

  xSemaphoreTake(rtcm_rx_state.mutex, portMAX_DELAY);

  // Fragment 추가
  if (rtcm_rx_state.buffer_pos + len > RTCM_RX_BUFFER_SIZE) {
    LOG_ERR("RTCM RX buffer overflow: pos=%d, add=%d, max=%d",
            rtcm_rx_state.buffer_pos, len, RTCM_RX_BUFFER_SIZE);
    rtcm_rx_state.buffer_pos = 0;
    rtcm_rx_state.receiving = false;
    xSemaphoreGive(rtcm_rx_state.mutex);
    return false;
  }

  memcpy(&rtcm_rx_state.buffer[rtcm_rx_state.buffer_pos], data, len);
  rtcm_rx_state.buffer_pos += len;
  rtcm_rx_state.receiving = true;
  rtcm_rx_state.last_fragment_time = xTaskGetTickCount();

  LOG_INFO("RTCM fragment received: %d bytes (total: %d bytes)", len, rtcm_rx_state.buffer_pos);

  // RTCM 패킷 완성 여부 확인
  // 최소 3바이트 있어야 헤더를 읽을 수 있음
  if (rtcm_rx_state.buffer_pos < 3) {
    xSemaphoreGive(rtcm_rx_state.mutex);
    return true;  // 더 기다림
  }

  // Preamble 확인
  if (rtcm_rx_state.buffer[0] != RTCM3_PREAMBLE) {
    LOG_ERR("Invalid RTCM preamble in buffer: 0x%02X", rtcm_rx_state.buffer[0]);
    rtcm_rx_state.buffer_pos = 0;
    rtcm_rx_state.receiving = false;
    xSemaphoreGive(rtcm_rx_state.mutex);
    return false;
  }

  // Length 파싱
  uint16_t payload_len = ((rtcm_rx_state.buffer[1] & 0x03) << 8) | rtcm_rx_state.buffer[2];
  uint16_t expected_total = 3 + payload_len + 3;  // header + payload + CRC

  // 패킷 완성 확인
  if (rtcm_rx_state.buffer_pos >= expected_total) {
    // 완전한 패킷 수신
    LOG_INFO("Complete RTCM packet received: %d bytes (expected: %d)",
             rtcm_rx_state.buffer_pos, expected_total);

    // 검증 및 전송
    bool result = rtcm_validate_and_send(rtcm_rx_state.buffer, expected_total, gps_id);

    // 버퍼 리셋
    rtcm_rx_state.buffer_pos = 0;
    rtcm_rx_state.receiving = false;

    xSemaphoreGive(rtcm_rx_state.mutex);
    return result;
  } else {
    // 아직 더 기다려야 함
    LOG_INFO("Waiting for more fragments: %d/%d bytes", rtcm_rx_state.buffer_pos, expected_total);
    xSemaphoreGive(rtcm_rx_state.mutex);
    return true;
  }
}

void rtcm_rx_reset(void) {
  xSemaphoreTake(rtcm_rx_state.mutex, portMAX_DELAY);
  rtcm_rx_state.buffer_pos = 0;
  rtcm_rx_state.receiving = false;
  xSemaphoreGive(rtcm_rx_state.mutex);

  LOG_INFO("RTCM RX buffer reset");
}
