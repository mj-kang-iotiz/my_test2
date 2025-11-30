#include "rtcm.h"
#include "lora_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

#ifndef TAG
  #define TAG "RTCM"
#endif

#include "log.h"

#define RTCM_MAX_LORA_SIZE 236  // Max LoRa transmission size (bytes)

// LoRa Time on Air calculation (SF7, BW125, CR4/5, Preamble 8)
// Measured: 236 bytes = 350ms, with 20% margin = 420ms
#define LORA_TOA_BASE_BYTES 236
#define LORA_TOA_BASE_MS 350
#define LORA_TOA_MARGIN_PERCENT 20  // 20% margin

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
 * @brief Convert binary data to HEX string
 *
 * @param data Input binary data
 * @param len Input data length (bytes)
 * @param hex_str Output HEX string buffer
 * @param hex_str_size Output buffer size
 * @return true Success, false Failure
 */
static bool binary_to_hex_string(const uint8_t *data, size_t len, char *hex_str, size_t hex_str_size) {
  if (!data || !hex_str || len == 0) {
    return false;
  }

  // HEX string requires 2 chars per byte + NULL terminator
  if (hex_str_size < (len * 2 + 1)) {
    LOG_ERR("HEX string buffer too small: %d < %d", hex_str_size, len * 2 + 1);
    return false;
  }

  for (size_t i = 0; i < len; i++) {
    snprintf(&hex_str[i * 2], 3, "%02X", data[i]);
  }

  return true;
}

bool rtcm_send_to_lora(gps_t *gps) {
  if (!gps) {
    LOG_ERR("GPS handle is NULL");
    return false;
  }

  // RTCM packet total length
  size_t rtcm_len = gps->rtcm.total_len;

  if (rtcm_len == 0) {
    LOG_ERR("RTCM length is zero");
    return false;
  }

  // Add padding for odd-byte data
  size_t actual_len = rtcm_len;
  uint8_t padded_data[RTCM_MAX_LORA_SIZE + 1];

  // Copy RTCM data from payload
  memcpy(padded_data, gps->payload, rtcm_len);

  // RAK4270 module constraint: raw data transmission requires even byte count
  if (rtcm_len % 2 != 0) {
    // Odd bytes -> Add 0x00 padding at the end
    padded_data[rtcm_len] = 0x00;
    actual_len = rtcm_len + 1;
    LOG_INFO("RTCM odd-byte padding: %d -> %d bytes", rtcm_len, actual_len);
  }

  // Check padded length (MUST be after padding calculation)
  if (actual_len > RTCM_MAX_LORA_SIZE) {
    LOG_ERR("RTCM length too large after padding: %d > %d (max)", actual_len, RTCM_MAX_LORA_SIZE);
    return false;
  }

  // Convert binary data to HEX string
  // HEX string size: actual_len * 2 + 1 (NULL terminator)
  char hex_str[RTCM_MAX_LORA_SIZE * 2 + 3];  // Sufficient buffer size

  if (!binary_to_hex_string(padded_data, actual_len, hex_str, sizeof(hex_str))) {
    LOG_ERR("Failed to convert RTCM to HEX string");
    return false;
  }

  // Calculate Time on Air (ToA) for this packet
  uint32_t toa_ms = calculate_lora_toa(actual_len);

  // Send via LoRa P2P with ToA-based timeout
  // Note: lora_send_p2p_data() internally checks if LoRa is initialized
  LOG_INFO("Sending RTCM to LoRa: type=%d, len=%d, padded_len=%d, ToA=%dms",
           gps->rtcm.msg_type, rtcm_len, actual_len, toa_ms);

  // Record start time
  TickType_t start_tick = xTaskGetTickCount();

  if (!lora_send_p2p_data(hex_str, toa_ms)) {
    LOG_ERR("Failed to send RTCM via LoRa");
    return false;
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

  LOG_INFO("RTCM sent successfully (type=%d)", gps->rtcm.msg_type);
  return true;
}
