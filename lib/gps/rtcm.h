#ifndef RTCM_H
#define RTCM_H

#include <stdint.h>
#include <stdbool.h>
#include "gps.h"

/**
 * @brief RTCM 데이터를 LoRa로 전송
 *
 * - LoRa가 초기화된 경우에만 전송
 * - 최대 236바이트까지 전송 가능
 * - 홀수 바이트인 경우 0x00 패딩 추가 (RAK4270 모듈 제약)
 * - Raw 데이터를 HEX string으로 변환하여 전송
 *
 * @param gps GPS 핸들
 * @return true 성공, false 실패
 */
bool rtcm_send_to_lora(gps_t *gps);

#endif