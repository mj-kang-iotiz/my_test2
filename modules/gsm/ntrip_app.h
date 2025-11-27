#ifndef NTRIP_APP_H
#define NTRIP_APP_H

#include "gsm.h"
#include <stdint.h>

/**
 * @brief NTRIP TCP 수신 태스크 생성
 *
 * @param gsm GSM 핸들
 */
void ntrip_task_create(gsm_t *gsm);

/**
 * @brief GGA 데이터를 NTRIP 서버로 전송
 *
 * @param data GGA raw 데이터
 * @param len 데이터 길이
 * @return int 전송된 바이트 수 (실패 시 음수)
 */
int ntrip_send_gga_data(const char *data, uint8_t len);

#endif // NTRIP_TASK_H