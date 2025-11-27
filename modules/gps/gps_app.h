#ifndef GPS_APP_H
#define GPS_APP_H

#include "FreeRTOS.h"
#include "board_config.h"
#include "gps.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"

/**
 * @brief GPS 명령어 요청 구조체
 */
typedef struct {
  char cmd[128];                  // 전송할 명령어
  SemaphoreHandle_t response_sem; // 응답 대기용 세마포어
  bool *result;                   // 응답 결과 (true: OK, false: ERROR/TIMEOUT)
  uint32_t timeout_ms;            // 타임아웃 (ms)
} gps_cmd_request_t;

/**
 * @brief GPS 초기화 (board_config 기반)
 *
 * board_config.h의 설정을 읽어서 자동으로 GPS 인스턴스 생성
 */
void gps_init_all(void);

/**
 * @brief GPS 태스크 생성 (레거시 호환용)
 *
 * @param arg 사용하지 않음 (board_config로 자동 설정)
 */
void gps_task_create(void *arg);

/**
 * @brief GPS 핸들 가져오기 (레거시 호환성)
 *
 * @return GPS 핸들 포인터 (첫 번째 GPS)
 */
gps_t *gps_get_handle(void);

/**
 * @brief GPS 인스턴스 핸들 가져오기
 *
 * @param id GPS ID
 * @return GPS 핸들 포인터
 */
gps_t *gps_get_instance_handle(gps_id_t id);

/**
 * @brief GGA 평균 데이터 읽기 가능 여부
 *
 * @param id GPS ID
 * @return true: 읽기 가능, false: 읽기 불가능
 */
bool gps_gga_avg_can_read(gps_id_t id);

/**
 * @brief GGA 평균 데이터 가져오기
 *
 * @param id GPS ID
 * @param lat 위도 출력 (NULL 가능)
 * @param lon 경도 출력 (NULL 가능)
 * @param alt 고도 출력 (NULL 가능)
 * @return true: 성공, false: 실패
 */
bool gps_get_gga_avg(gps_id_t id, double *lat, double *lon, double *alt);

/**
 * @brief GPS 명령어 전송 (동기 방식)
 *
 * @param id GPS ID
 * @param cmd 전송할 명령어 문자열 (예: "mode base time 60\r\n")
 * @param timeout_ms 응답 대기 타임아웃 (ms)
 * @return true: OK 응답 수신, false: ERROR 응답 또는 타임아웃
 */
bool gps_send_command_sync(gps_id_t id, const char *cmd, uint32_t timeout_ms);

#endif
