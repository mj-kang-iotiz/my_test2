/**
 * @file dual_base_station_example.c
 * @brief 두 개의 베이스 스테이션을 비동기적으로 설정하는 예제
 *
 * 이 예제는 다음을 보여줍니다:
 * 1. Fixed position 모드로 베이스 스테이션 설정
 * 2. Survey-in 모드로 베이스 스테이션 설정
 * 3. 두 베이스 스테이션을 동시에 비동기적으로 초기화
 */

#include "gps_app.h"
#include "board_config.h"
#include "log.h"

#define TAG "DUAL_BASE_EXAMPLE"

/**
 * @brief 전체 초기화 완료 콜백
 */
static void dual_base_complete_callback(bool success, void *user_data) {
    if (success) {
        LOG_INFO("=== 두 베이스 스테이션 초기화 성공! ===");
        LOG_INFO("Fixed 베이스와 Survey-in 베이스 모두 준비 완료");
    } else {
        LOG_ERR("=== 두 베이스 스테이션 초기화 실패 ===");
        LOG_ERR("하나 이상의 베이스 스테이션 초기화에 문제가 발생했습니다");
    }
}

/**
 * @brief Fixed position 베이스 완료 콜백
 */
static void fixed_base_callback(bool success, void *user_data) {
    if (success) {
        LOG_INFO("Fixed position 베이스 스테이션 초기화 성공");
    } else {
        LOG_ERR("Fixed position 베이스 스테이션 초기화 실패");
    }
}

/**
 * @brief Survey-in 베이스 완료 콜백
 */
static void surveyin_base_callback(bool success, void *user_data) {
    if (success) {
        LOG_INFO("Survey-in 베이스 스테이션 초기화 성공");
    } else {
        LOG_ERR("Survey-in 베이스 스테이션 초기화 실패");
    }
}

/**
 * @brief 예제 1: Fixed position 모드로 단일 베이스 스테이션 설정
 */
void example_1_single_fixed_base(void) {
    LOG_INFO("\n=== Example 1: Single Fixed Position Base ===");

    // GPS ID (보통 GPS_ID_BASE = 0)
    gps_id_t base_id = GPS_ID_BASE;

    // Fixed position 좌표 (베이징 근처)
    double latitude = 40.45628476579;   // 위도 (도)
    double longitude = 116.2859754968;  // 경도 (도)
    double altitude = 58.0984;          // 고도 (m)

    LOG_INFO("베이스 스테이션 설정:");
    LOG_INFO("  위도: %.11f", latitude);
    LOG_INFO("  경도: %.11f", longitude);
    LOG_INFO("  고도: %.4f m", altitude);

    // 비동기 초기화 시작
    bool result = gps_init_um982_base_fixed_async(
        base_id,
        latitude,
        longitude,
        altitude,
        fixed_base_callback
    );

    if (result) {
        LOG_INFO("Fixed 베이스 스테이션 초기화 시작됨");
    } else {
        LOG_ERR("Fixed 베이스 스테이션 초기화 실패");
    }
}

/**
 * @brief 예제 2: Survey-in 모드로 단일 베이스 스테이션 설정
 */
void example_2_single_surveyin_base(void) {
    LOG_INFO("\n=== Example 2: Single Survey-in Base ===");

    // GPS ID
    gps_id_t base_id = GPS_ID_BASE;

    // Survey-in 파라미터
    uint32_t survey_time = 120;    // 120초 동안 측정
    float survey_accuracy = 0.1f;  // 0.1m 정확도

    LOG_INFO("Survey-in 베이스 스테이션 설정:");
    LOG_INFO("  측정 시간: %u 초", survey_time);
    LOG_INFO("  목표 정확도: %.2f m", survey_accuracy);

    // 비동기 초기화 시작
    bool result = gps_init_um982_base_surveyin_async(
        base_id,
        survey_time,
        survey_accuracy,
        surveyin_base_callback
    );

    if (result) {
        LOG_INFO("Survey-in 베이스 스테이션 초기화 시작됨");
        LOG_INFO("위치 측정 중... (%u초 소요)", survey_time);
    } else {
        LOG_ERR("Survey-in 베이스 스테이션 초기화 실패");
    }
}

/**
 * @brief 예제 3: 두 베이스 스테이션을 동시에 비동기적으로 설정
 *
 * 이것이 메인 예제입니다!
 * - Base1: Fixed position 모드
 * - Base2: Survey-in 모드
 * 두 베이스가 동시에 초기화되며, 둘 다 완료되면 콜백이 호출됩니다.
 */
void example_3_dual_base_async(void) {
    LOG_INFO("\n=== Example 3: Dual Base Station Async Init ===");

    // Base1: Fixed position 설정
    gps_id_t base1_id = GPS_ID_BASE;    // GPS ID 0
    double base1_lat = 40.45628476579;
    double base1_lon = 116.2859754968;
    double base1_alt = 58.0984;

    // Base2: Survey-in 설정
    gps_id_t base2_id = GPS_ID_ROVER;   // GPS ID 1 (두 번째 GPS)
    uint32_t base2_time = 120;          // 120초
    float base2_accuracy = 0.1f;        // 0.1m

    LOG_INFO("두 개의 베이스 스테이션을 동시에 초기화합니다:");
    LOG_INFO("");
    LOG_INFO("Base1 (GPS[%d]) - Fixed Position:", base1_id);
    LOG_INFO("  위도: %.11f", base1_lat);
    LOG_INFO("  경도: %.11f", base1_lon);
    LOG_INFO("  고도: %.4f m", base1_alt);
    LOG_INFO("");
    LOG_INFO("Base2 (GPS[%d]) - Survey-in Mode:", base2_id);
    LOG_INFO("  측정 시간: %u 초", base2_time);
    LOG_INFO("  목표 정확도: %.2f m", base2_accuracy);
    LOG_INFO("");

    // 두 베이스 스테이션을 동시에 비동기적으로 초기화
    bool result = gps_init_dual_base_async(
        base1_id, base1_lat, base1_lon, base1_alt,  // Base1: Fixed
        base2_id, base2_time, base2_accuracy,        // Base2: Survey-in
        dual_base_complete_callback                   // 완료 콜백
    );

    if (result) {
        LOG_INFO("✓ 두 베이스 스테이션 초기화 시작!");
        LOG_INFO("  - Base1은 즉시 고정 위치로 설정됩니다");
        LOG_INFO("  - Base2는 %u초 동안 위치를 측정합니다", base2_time);
        LOG_INFO("  - 두 초기화는 병렬로 진행됩니다");
        LOG_INFO("");
        LOG_INFO("초기화가 완료되면 콜백이 호출됩니다...");
    } else {
        LOG_ERR("✗ 두 베이스 스테이션 초기화 실패");
    }
}

/**
 * @brief 메인 실행 함수
 *
 * 실제 사용 시에는 이 함수들을 적절한 위치에서 호출하면 됩니다.
 * 예를 들어, main.c의 initThread()에서 호출할 수 있습니다.
 */
void run_dual_base_examples(void) {
    LOG_INFO("====================================");
    LOG_INFO("Dual Base Station Setup Examples");
    LOG_INFO("====================================");

    // 원하는 예제를 선택하여 실행
    // 주의: 한 번에 하나씩만 실행하세요!

    // 예제 1: 단일 Fixed position 베이스
    // example_1_single_fixed_base();

    // 예제 2: 단일 Survey-in 베이스
    // example_2_single_surveyin_base();

    // 예제 3: 두 베이스를 동시에 비동기 초기화 (권장!)
    example_3_dual_base_async();
}

/**
 * @brief 실제 사용 예시
 *
 * main.c의 initThread()에서 다음과 같이 사용:
 *
 * void initThread(void *pvParameter) {
 *     flash_params_init();
 *     led_init();
 *     gps_init_all();  // GPS 인스턴스 생성
 *
 *     // 두 베이스 스테이션 비동기 초기화
 *     gps_init_dual_base_async(
 *         GPS_ID_BASE, 40.45628476579, 116.2859754968, 58.0984,  // Base1: Fixed
 *         GPS_ID_ROVER, 120, 0.1f,                                 // Base2: Survey-in
 *         dual_base_complete_callback
 *     );
 *
 *     // 다른 초기화 계속...
 *     gsm_task_create(NULL);
 *     lora_instance_init();
 *
 *     vTaskDelete(NULL);
 * }
 */
