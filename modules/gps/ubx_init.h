#ifndef UBX_INIT_H
#define UBX_INIT_H

#include "gps.h"
#include <stdbool.h>
#include <stdint.h>

bool ubx_rover_init(gps_t* gps);
bool ubx_base_init(gps_t* gps);
bool ubx_moving_base_init(gps_t* gps);

bool ubx_factory_reset(gps_t* gps, ubx_init_complete_callback_t callback, void *user_data);

/**
 * @brief Survey-in 모드 시작
 *
 * @param gps GPS 구조체
 * @param min_duration Survey-in 최소 지속 시간 (초), 권장값: 60~300
 * @param accuracy_limit Survey-in 정확도 제한 (0.1mm 단위), 권장값: 50000 (5m)
 * @return true 성공, false 실패
 *
 * @note 이 함수는 ubx_base_init() 완료 후 호출해야 합니다.
 * @note Survey-in이 완료되면 자동으로 Fixed 모드로 전환됩니다.
 */
bool ubx_set_survey_in_mode(gps_t* gps, uint32_t min_duration, uint32_t accuracy_limit);

/**
 * @brief Fixed 모드 설정 (수동 좌표 입력)
 *
 * @param gps GPS 구조체
 * @param lat_str 위도 문자열 (degrees, 예: "37.12345")
 * @param lon_str 경도 문자열 (degrees, 예: "127.12345")
 * @param alt_str 고도 문자열 (meters, 예: "100.5")
 * @return true 성공, false 실패
 *
 * @note 이 함수는 ubx_base_init() 완료 후 호출해야 합니다.
 * @note user_params_t의 lat, lon, alt 값을 직접 전달할 수 있습니다.
 */
bool ubx_set_fixed_position(gps_t* gps, const char* lat_str, const char* lon_str, const char* alt_str);

/**
 * @brief Time Mode 비활성화
 *
 * @param gps GPS 구조체
 * @return true 성공, false 실패
 */
bool ubx_disable_time_mode(gps_t* gps);

#endif
