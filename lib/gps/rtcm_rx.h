#ifndef RTCM_RX_H
#define RTCM_RX_H

#include <stdint.h>
#include <stdbool.h>
#include "board_config.h"
#include "gps.h"

/**
 * @brief LoRa로부터 수신한 RTCM 데이터를 GPS UART로 전송
 *
 * - LoRa로 수신한 HEX ASCII 데이터는 이미 바이너리로 변환됨
 * - Fragment 재조합 (RTCM 패킷은 LoRa로 분할 전송됨)
 * - RTCM 패킷 검증 (preamble, length, CRC24)
 * - GPS UART로 바이너리 전송
 *
 * RTCM3 패킷 구조:
 *   - Preamble: 0xD3 (1 byte)
 *   - Reserved + Length: 6 bits (0) + 10 bits length (2 bytes total)
 *   - Payload: N bytes (length 필드 값)
 *   - CRC24: 3 bytes
 *   - 전체 길이 = 3 + length + 3 = length + 6
 */

#define RTCM_RX_BUFFER_SIZE 1024  // RTCM 재조합 버퍼 크기
#define RTCM_MAX_PACKET_SIZE 1024 // 최대 RTCM 패킷 크기

/**
 * @brief RTCM RX 초기화
 */
void rtcm_rx_init(void);

/**
 * @brief LoRa로부터 수신한 RTCM fragment 처리
 *
 * Fragment를 재조합하고, 완전한 RTCM 패킷이 되면 GPS UART로 전송
 *
 * @param data Fragment 데이터 (바이너리)
 * @param len Fragment 길이
 * @param gps_id GPS ID (전송할 GPS)
 * @return true: 처리 성공, false: 에러
 */
bool rtcm_rx_process_fragment(const uint8_t *data, size_t len, gps_id_t gps_id);

/**
 * @brief RTCM RX 버퍼 리셋 (에러 발생 시)
 */
void rtcm_rx_reset(void);

#endif
