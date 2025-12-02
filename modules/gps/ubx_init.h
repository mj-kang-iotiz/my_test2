#include "gps.h"
#include <stdbool.h>

bool ubx_rover_init(gps_t* gps);
bool ubx_base_init(gps_t* gps);
bool ubx_moving_base_init(gps_t* gps);

/**
 * @brief F9P 팩토리 리셋 (비동기)
 *
 * @param[inout] gps GPS 구조체
 * @param[in] callback 완료 콜백
 * @param[in] user_data 사용자 데이터
 * @return true 리셋 시작 성공, false 실패
 */
bool ubx_factory_reset(gps_t* gps, ubx_init_complete_callback_t callback, void *user_data);