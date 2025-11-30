#ifndef RTCM_H
#define RTCM_H

#include <stdint.h>
#include <stdbool.h>
#include "gps.h"

/**
 * @brief RTCM 전송 초기화 (task 없음)
 *
 * RTCM 데이터를 비동기로 LoRa TX task를 통해 전송
 * 별도의 RTCM task 없이 LoRa TX task를 직접 사용
 */
void rtcm_tx_task_init(void);

/**
 * @brief RTCM 데이터를 LoRa로 전송 (비동기, 자동 분할)
 *
 * - 완전 비동기 전송: 즉시 리턴 (GPS Task 블록 안 됨)
 * - HEX ASCII 변환으로 인해 최대 118바이트씩 전송
 * - 118바이트 초과 시 자동으로 여러 fragment로 분할 전송
 * - ToA(Time on Air) 자동 계산 (HEX 크기 기준: bytes * 2)
 * - Fragment 단위로 순차 전송 (callback chain 방식)
 * - 전송 중 새로운 RTCM 도착 시 drop (중복 전송 방지)
 *
 * @param gps GPS 핸들
 * @return true: 전송 시작 성공, false: 전송 중이거나 에러
 */
bool rtcm_send_to_lora(gps_t *gps);

#endif
