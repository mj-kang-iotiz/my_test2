#include "rtcm.h"
#include "lora_app.h"
#include <string.h>
#include <stdio.h>

#ifndef TAG
  #define TAG "RTCM"
#endif

#include "log.h"

#define RTCM_MAX_LORA_SIZE 236  // Max LoRa transmission size (bytes)

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

  if (rtcm_len > RTCM_MAX_LORA_SIZE) {
    LOG_ERR("RTCM length too large: %d > %d (max)", rtcm_len, RTCM_MAX_LORA_SIZE);
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

  // Convert binary data to HEX string
  // HEX string size: actual_len * 2 + 1 (NULL terminator)
  char hex_str[RTCM_MAX_LORA_SIZE * 2 + 3];  // Sufficient buffer size

  if (!binary_to_hex_string(padded_data, actual_len, hex_str, sizeof(hex_str))) {
    LOG_ERR("Failed to convert RTCM to HEX string");
    return false;
  }

  // Send via LoRa P2P
  // Note: lora_send_p2p_data() internally checks if LoRa is initialized
  LOG_INFO("Sending RTCM to LoRa: type=%d, len=%d, padded_len=%d",
           gps->rtcm.msg_type, rtcm_len, actual_len);

  if (!lora_send_p2p_data(hex_str, 2000)) {
    LOG_ERR("Failed to send RTCM via LoRa");
    return false;
  }

  LOG_INFO("RTCM sent successfully (type=%d)", gps->rtcm.msg_type);
  return true;
}
