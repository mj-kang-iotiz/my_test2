#ifndef RTCM_H
#define RTCM_H

#include <stdint.h>
#include <stdbool.h>
#include "gps.h"

/**
 * @brief RTCM 전송 Task 초기화
 *
 * RTCM 데이터를 비동기로 LoRa 전송하는 Task 생성
 */
void rtcm_tx_task_init(void);

/**
 * @brief RTCM 데이터를 LoRa로 전송 (비동기)
 *
 * - 비동기 전송: 큐에 추가하고 즉시 리턴 (GPS Task 블록 안 됨)
 * - LoRa가 초기화된 경우에만 전송
 * - 최대 236바이트까지 전송 가능 (HEX 방식은 118바이트만 가능)
 * - 홀수 바이트인 경우 0x00 패딩 추가 (RAK4270 모듈 제약)
 * - Raw binary 데이터 직접 전송 (HEX 변환 없음)
 * - at+send=lorap2p:[raw binary]\r\n 형식으로 UART 전송
 *
 * @param gps GPS 핸들
 * @return true: 큐에 추가 성공, false: 큐 full 또는 에러
 */
bool rtcm_send_to_lora(gps_t *gps);

#endif