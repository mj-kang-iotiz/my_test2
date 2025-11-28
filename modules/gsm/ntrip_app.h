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
int ntrip_send_gga_data(const char *data, uint8_t len);

#endif // NTRIP_TASK_H