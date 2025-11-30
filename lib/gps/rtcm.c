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

// HEX ASCII로 변환하면 데이터가 2배 증가:
// LoRa 최대 236 HEX 문자 = 118 바이트 binary
#define RTCM_MAX_FRAGMENT_SIZE 118  // Max binary size per fragment

// LoRa Time on Air calculation (SF7, BW125, CR4/5, Preamble 8)
// HEX 변환 시: 1 byte -> 2 HEX chars
// ToA는 실제 전송되는 HEX 문자 수 기준 (bytes * 2)
// Measured: 236 HEX chars = 350ms, with 20% margin = 420ms
#define LORA_TOA_BASE_HEX_CHARS 236
#define LORA_TOA_BASE_MS 350
#define LORA_TOA_MARGIN_PERCENT 20  // 20% margin

/**
 * @brief Calculate LoRa Time on Air (ToA) with margin
 *
 * @param binary_bytes Binary payload size (before HEX conversion)
 * @return Time on Air in milliseconds (with 20% margin)
 */
static uint32_t calculate_lora_toa(size_t binary_bytes) {
  // HEX conversion: 1 byte -> 2 HEX chars
  size_t hex_chars = binary_bytes * 2;

  // ToA(ms) = (hex_chars / 236) * 350 * 1.2
  uint32_t toa_ms = (hex_chars * LORA_TOA_BASE_MS / LORA_TOA_BASE_HEX_CHARS);
  toa_ms = toa_ms * (100 + LORA_TOA_MARGIN_PERCENT) / 100;

  // Minimum ToA (작은 패킷도 최소 시간 필요)
  if (toa_ms < 50) {
    toa_ms = 50;
  }

  return toa_ms;
}

// Fragment context for async transmission
typedef struct {
  uint8_t *remaining_data;
  size_t remaining_len;
  uint16_t msg_type;
  uint8_t fragment_idx;
  uint8_t total_fragments;
} rtcm_fragment_ctx_t;

static rtcm_fragment_ctx_t *current_fragment_ctx = NULL;

/**
 * @brief Callback for fragment transmission completion
 */
static void rtcm_fragment_callback(bool success, void *user_data) {
  rtcm_fragment_ctx_t *ctx = (rtcm_fragment_ctx_t *)user_data;

  if (!ctx) {
    LOG_ERR("Fragment context is NULL");
    return;
  }

  if (!success) {
    LOG_ERR("Fragment %d/%d transmission failed", ctx->fragment_idx, ctx->total_fragments);
    vPortFree(ctx);
    current_fragment_ctx = NULL;
    return;
  }

  LOG_INFO("Fragment %d/%d sent successfully", ctx->fragment_idx, ctx->total_fragments);

  // Check if there are more fragments
  if (ctx->remaining_len > 0) {
    // Send next fragment
    size_t fragment_len = (ctx->remaining_len > RTCM_MAX_FRAGMENT_SIZE)
                          ? RTCM_MAX_FRAGMENT_SIZE
                          : ctx->remaining_len;

    uint32_t toa_ms = calculate_lora_toa(fragment_len);

    ctx->fragment_idx++;
    LOG_INFO("Sending fragment %d/%d: %d bytes, ToA=%dms",
             ctx->fragment_idx, ctx->total_fragments, fragment_len, toa_ms);

    if (!lora_send_p2p_raw_async(ctx->remaining_data, fragment_len, toa_ms,
                                  rtcm_fragment_callback, ctx)) {
      LOG_ERR("Failed to queue fragment %d", ctx->fragment_idx);
      vPortFree(ctx);
      current_fragment_ctx = NULL;
      return;
    }

    // Update context for next fragment
    ctx->remaining_data += fragment_len;
    ctx->remaining_len -= fragment_len;
  } else {
    // All fragments sent
    LOG_INFO("RTCM transmission complete (type=%d, %d fragments)",
             ctx->msg_type, ctx->total_fragments);
    vPortFree(ctx);
    current_fragment_ctx = NULL;
  }
}

void rtcm_tx_task_init(void) {
  // No task needed anymore - direct async transmission
  LOG_INFO("RTCM async transmission initialized (no task)");
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

  // Check if there's already a transmission in progress
  if (current_fragment_ctx != NULL) {
    LOG_WARN("RTCM transmission already in progress, packet dropped (type=%d)", gps->rtcm.msg_type);
    return false;
  }

  // Calculate total fragments needed
  uint8_t total_fragments = (rtcm_len + RTCM_MAX_FRAGMENT_SIZE - 1) / RTCM_MAX_FRAGMENT_SIZE;

  // Allocate context (includes data buffer)
  size_t ctx_size = sizeof(rtcm_fragment_ctx_t) + rtcm_len;
  rtcm_fragment_ctx_t *ctx = (rtcm_fragment_ctx_t *)pvPortMalloc(ctx_size);
  if (!ctx) {
    LOG_ERR("Failed to allocate fragment context");
    return false;
  }

  // Copy RTCM data to context (after struct)
  uint8_t *data_copy = (uint8_t *)(ctx + 1);
  memcpy(data_copy, gps->payload, rtcm_len);

  // Initialize context
  ctx->remaining_data = data_copy;
  ctx->remaining_len = rtcm_len;
  ctx->msg_type = gps->rtcm.msg_type;
  ctx->fragment_idx = 1;
  ctx->total_fragments = total_fragments;

  current_fragment_ctx = ctx;

  // Send first fragment
  size_t fragment_len = (rtcm_len > RTCM_MAX_FRAGMENT_SIZE)
                        ? RTCM_MAX_FRAGMENT_SIZE
                        : rtcm_len;

  uint32_t toa_ms = calculate_lora_toa(fragment_len);

  LOG_INFO("RTCM TX: type=%d, len=%d, ToA=%dms", gps->rtcm.msg_type, rtcm_len, toa_ms);
  LOG_INFO("Sending fragment 1/%d: %d bytes, ToA=%dms",
           total_fragments, fragment_len, toa_ms);

  if (!lora_send_p2p_raw_async(ctx->remaining_data, fragment_len, toa_ms,
                                rtcm_fragment_callback, ctx)) {
    LOG_ERR("Failed to queue first fragment");
    vPortFree(ctx);
    current_fragment_ctx = NULL;
    return false;
  }

  // Update context for next fragment (if any)
  ctx->remaining_data += fragment_len;
  ctx->remaining_len -= fragment_len;

  // Return immediately (non-blocking, async transmission)
  return true;
}
