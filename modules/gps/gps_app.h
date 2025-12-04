#ifndef GPS_APP_H
#define GPS_APP_H

#include "FreeRTOS.h"
#include "board_config.h"
#include "gps.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

typedef void (*gps_command_callback_t)(bool success, void *user_data);

typedef struct {
  char cmd[128];                  // 전송할 명령어
  uint32_t timeout_ms;            // 타임아웃 (ms)
  bool is_async;                  // true: 비동기, false: 동기

  SemaphoreHandle_t response_sem; // 응답 대기용 세마포어
  bool *result;                   // 응답 결과 (true: OK, false: ERROR/TIMEOUT)

  gps_command_callback_t callback; // 완료 콜백
  void *user_data;                 // 사용자 데이터
  bool async_result;               // 비동기 결과 저장용
} gps_cmd_request_t;

bool gps_send_command_sync(gps_id_t id, const char *cmd, uint32_t timeout_ms);
bool gps_send_command_async(gps_id_t id, const char *cmd, uint32_t timeout_ms,
                             gps_command_callback_t callback, void *user_data);

bool gps_send_raw_data(gps_id_t id, const uint8_t *data, size_t len);

typedef void (*gps_init_callback_t)(bool success, void *user_data);

bool gps_init_um982_base_async(gps_id_t id, gps_init_callback_t callback);
bool gps_init_um982_rover_async(gps_id_t id, gps_init_callback_t callback);

/**
 * @brief GPS UM982 Base 모드 초기화 - Fixed Position (비동기)
 * @param id GPS ID
 * @param lat 위도 (도)
 * @param lon 경도 (도)
 * @param alt 고도 (m)
 * @param callback 초기화 완료 콜백
 * @return 초기화 시작 성공 여부
 */
bool gps_init_um982_base_fixed_async(gps_id_t id, double lat, double lon, double alt, gps_init_callback_t callback);

/**
 * @brief GPS UM982 Base 모드 초기화 - Survey-in Mode (비동기)
 * @param id GPS ID
 * @param time_sec Survey-in 시간 (초)
 * @param accuracy_m Survey-in 정확도 (m)
 * @param callback 초기화 완료 콜백
 * @return 초기화 시작 성공 여부
 */
bool gps_init_um982_base_surveyin_async(gps_id_t id, uint32_t time_sec, float accuracy_m, gps_init_callback_t callback);

/**
 * @brief 두 개의 베이스 스테이션을 비동기적으로 동시에 초기화
 * @param base1_id 첫 번째 베이스 스테이션 ID (Fixed Position)
 * @param base1_lat 첫 번째 베이스 위도
 * @param base1_lon 첫 번째 베이스 경도
 * @param base1_alt 첫 번째 베이스 고도
 * @param base2_id 두 번째 베이스 스테이션 ID (Survey-in)
 * @param base2_time Survey-in 시간 (초)
 * @param base2_accuracy Survey-in 정확도 (m)
 * @param callback 모든 초기화 완료 콜백
 * @return 초기화 시작 성공 여부
 */
bool gps_init_dual_base_async(gps_id_t base1_id, double base1_lat, double base1_lon, double base1_alt,
                               gps_id_t base2_id, uint32_t base2_time, float base2_accuracy,
                               gps_init_callback_t callback);

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
bool gps_factory_reset_async(gps_id_t id, gps_init_callback_t callback, void *user_data);

#endif
