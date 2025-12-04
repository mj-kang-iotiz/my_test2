#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "gps_ubx.h"
#include "ubx_init.h"

#ifndef TAG
    #define TAG "UBX_INIT"
#endif

#include "log.h"

#define CFG_GLL_UART1 (0x209100caU)
#define CFG_GSA_UART1 (0x209100c0U)
#define CFG_GSV_UART1 (0x209100c5U)
#define CFG_RMC_UART1 (0x209100acU)
#define CFG_VTG_UART1 (0x209100b1U)

#define CFG_NAV_HPPOSLLH_UART1 (0x20910034U)
#define CFG_NAV_RELPOSNED_UART1 (0x2091008eU)

#define CFG_RTCM_1005_UART1 (0x209102beU) // antenna
#define CFG_RTCM_1005_UART2 (0x209102bfU) // antenna
#define CFG_RTCM_1074_UART1 (0x2091035fU) // GPS
#define CFG_RTCM_1074_UART2 (0x20910360U) // GPS
#define CFG_RTCM_1084_UART1 (0x20910364U) // GLONASS
#define CFG_RTCM_1084_UART2 (0x20910365U) // GLONASS
#define CFG_RTCM_1094_UART1 (0x20910369U) // GALLILEO
#define CFG_RTCM_1094_UART2 (0x2091036aU) // GALLILEO
#define CFG_RTCM_1124_UART1 (0x2091036eU) // BEIDU
#define CFG_RTCM_1124_UART2 (0x2091036fU) // BEIDU
#define CFG_RTCM_4072_0_UART2 (0x20910300U) // RTCM4072_0
#define CFG_RTCM_4072_1_UART2 (0x20910383U) // RTCM4072_1

#define CFG_BAUDRATE_UART1 (0x40520001U) // 4바이트
#define CFG_BAUDRATE_UART2 (0x40530001U) // 4바이트

#define CFG_UART2INPROT_RTCM3X (0x10750004U)
#define CFG_UART2OUTPROT_RTCM3X (0x10760004U)

/* Time Mode 설정 */
#define CFG_TMODE_MODE (0x20030001U)           // E1: 0=disable, 1=survey-in, 2=fixed
#define CFG_TMODE_SVIN_MIN_DUR (0x40030010U)   // U4: Survey-in minimum duration (seconds)
#define CFG_TMODE_SVIN_ACC_LIMIT (0x40030011U) // U4: Survey-in accuracy limit (0.1mm units)
#define CFG_TMODE_LLH_LAT (0x40030009U)        // I4: Latitude (degrees * 1e-7)
#define CFG_TMODE_LLH_LON (0x4003000aU)        // I4: Longitude (degrees * 1e-7)
#define CFG_TMODE_LLH_HEIGHT (0x4003000bU)     // I4: Height above mean sea level (cm)


static const ubx_cfg_item_t ublox_base_configs[] = {

    /* NMEA OUTPUT 설정 */
    {
        .key_id = CFG_GLL_UART1,
        .value = {0},
        .value_len = 1,
    },

    {
        .key_id = CFG_GSA_UART1,
        .value = {0},
        .value_len = 1,
    },

    {
        .key_id = CFG_GSV_UART1,
        .value = {0},
        .value_len = 1,
    },

    {
        .key_id = CFG_RMC_UART1,
        .value = {0},
        .value_len = 1,
    },

    {
        .key_id = CFG_VTG_UART1,
        .value = {0},
        .value_len = 1,
    },

    /* UBX 메시지 설정 */
    {
        .key_id = CFG_NAV_HPPOSLLH_UART1,
        .value = {1},
        .value_len = 1,
    },

    /* RTCM 설정 */
    {
        .key_id = CFG_RTCM_1005_UART1,
        .value = {10},
        .value_len = 1,
    },

    {
        .key_id = CFG_RTCM_1074_UART1,
        .value = {2},
        .value_len = 1,
    },

    // {
    //     .key_id = CFG_RTCM_1084_UART1,
    //     .value = {2},
    //     .value_len = 1,
    // },

    {
        .key_id = CFG_RTCM_1094_UART1,
        .value = {2},
        .value_len = 1,
    },

    {
        .key_id = CFG_RTCM_1124_UART1,
        .value = {2},
        .value_len = 1,
    },
};

static const ubx_cfg_item_t ublox_rover_configs[] = {
    /* NMEA OUTPUT 설정 */
    {
        .key_id = CFG_GLL_UART1,
        .value = {0},
        .value_len = 1,
    },

    {
        .key_id = CFG_GSA_UART1,
        .value = {0},
        .value_len = 1,
    },

    {
        .key_id = CFG_GSV_UART1,
        .value = {0},
        .value_len = 1,
    },

    {
        .key_id = CFG_RMC_UART1,
        .value = {0},
        .value_len = 1,
    },

    {
        .key_id = CFG_VTG_UART1,
        .value = {0},
        .value_len = 1,
    },

    /* UBX 메시지 설정 */

    {
        .key_id = CFG_NAV_RELPOSNED_UART1,
        .value = {1},
        .value_len = 1,
    },

    /* UART2 포트 설정 */
    {
        .key_id = CFG_UART2INPROT_RTCM3X,
        .value = {1},
        .value_len = 1,
    },

    /* F9H는 기본적으로 비활성화 되어있어서 NACK 수신받음 */
    // {
    //     .key_id = CFG_UART2OUTPROT_RTCM3X,
    //     .value = {0},
    //     .value_len = 1,
    // },
};

static const ubx_cfg_item_t ublox_moving_base_configs[] = {
    /* nmea 설정 */
    {
        .key_id = CFG_GLL_UART1,
        .value = {0},
        .value_len = 1,
    },

    {
        .key_id = CFG_GSA_UART1,
        .value = {0},
        .value_len = 1,
    },

    {
        .key_id = CFG_GSV_UART1,
        .value = {0},
        .value_len = 1,
    },

    {
        .key_id = CFG_RMC_UART1,
        .value = {0},
        .value_len = 1,
    },

    {
        .key_id = CFG_VTG_UART1,
        .value = {0},
        .value_len = 1,
    },

    /* ubx 프로토콜 설정 */
    {
        .key_id = CFG_NAV_HPPOSLLH_UART1,
        .value = {1},
        .value_len = 1,
    },
    
    /* RTCM 설정 */
    {
        .key_id = CFG_RTCM_1005_UART2,
        .value = {10},
        .value_len = 1,
    },

    {
        .key_id = CFG_RTCM_1074_UART2,
        .value = {1},
        .value_len = 1,
    },

    {
        .key_id = CFG_RTCM_1084_UART2,
        .value = {1},
        .value_len = 1,
    },

    {
        .key_id = CFG_RTCM_1094_UART2,
        .value = {1},
        .value_len = 1,
    },

    {
        .key_id = CFG_RTCM_1124_UART2,
        .value = {1},
        .value_len = 1,
    },

    {
        .key_id = CFG_RTCM_4072_0_UART2,
        .value = {1},
        .value_len = 1,
    },

        {
        .key_id = CFG_RTCM_4072_1_UART2,
        .value = {1},
        .value_len = 1,
    },
    
    /* UART2 포트 설정 */
    {
        .key_id = CFG_UART2INPROT_RTCM3X,
        .value = {0},
        .value_len = 1,
    },

    {
        .key_id = CFG_UART2OUTPROT_RTCM3X,
        .value = {1},
        .value_len = 1,
    },
};

static void on_init_complete(bool success, size_t failed_step, void *user_data)
{
    if (success)
    {
        LOG_DEBUG("✓ UBX initialization completed successfully!\n");
    }
    else
    {
        LOG_ERR("✗ UBX initialization failed at step %zu\n", failed_step);
    }
}

bool ubx_rover_init(gps_t* gps)
{
    ubx_init_async_start(gps, UBX_CFG_LAYER_RAM,
                          ublox_rover_configs, sizeof(ublox_rover_configs) / sizeof(ublox_rover_configs[0]),
        on_init_complete, NULL);
}

bool ubx_base_init(gps_t* gps)
{
    ubx_init_async_start(gps, UBX_CFG_LAYER_RAM,
                          ublox_base_configs, sizeof(ublox_base_configs) / sizeof(ublox_base_configs[0]),
        on_init_complete, NULL);
}

bool ubx_moving_base_init(gps_t* gps)
{
    ubx_init_async_start(gps, UBX_CFG_LAYER_RAM,
                          ublox_moving_base_configs, sizeof(ublox_moving_base_configs) / sizeof(ublox_moving_base_configs[0]),
        on_init_complete, NULL);
}

static void on_factory_reset_complete(bool ack, void *user_data)
{
    gps_t *gps = (gps_t *)user_data;
    ubx_init_context_t *ctx = &gps->ubx_init_ctx;

    if (ack)
    {

        LOG_DEBUG("✓ F9P factory reset completed successfully!\n");

        ctx->state = UBX_INIT_STATE_DONE;

 

        if (ctx->on_complete)

        {

            ctx->on_complete(true, 0, ctx->user_data);

        }

    }

    else

    {

        LOG_ERR("✗ F9P factory reset failed\n");

        ctx->state = UBX_INIT_STATE_ERROR;

 

        if (ctx->on_complete)

        {

            ctx->on_complete(false, 0, ctx->user_data);

        }

    }

}

 

bool ubx_factory_reset(gps_t* gps, ubx_init_complete_callback_t callback, void *user_data)

{

    ubx_init_context_t *ctx = &gps->ubx_init_ctx;

 

    // 이미 초기화 중이면 실패

    if (ctx->state == UBX_INIT_STATE_RUNNING)

    {

        return false;

    }

 

    // 컨텍스트 초기화

    ctx->state = UBX_INIT_STATE_RUNNING;

    ctx->on_complete = callback;

    ctx->user_data = user_data;

 

    // UBX-CFG-CFG 메시지 전송

    // clearMask: 0x1F (모든 섹션 클리어)

    // saveMask: 0x00 (저장 안 함)

    // loadMask: 0x1F (모든 섹션 로드 = 팩토리 리셋)

    if (!ubx_send_cfg_cfg(gps, 0x0000001F, 0x00000000, 0x0000001F,

                          on_factory_reset_complete, gps))

    {

        ctx->state = UBX_INIT_STATE_ERROR;

        return false;

    }

 

    LOG_DEBUG("F9P factory reset initiated...\n");

    return true;

}

/**
 * @brief Survey-in 모드 시작
 *
 * @param gps GPS 구조체
 * @param min_duration Survey-in 최소 지속 시간 (초), 기본값 300초
 * @param accuracy_limit Survey-in 정확도 제한 (0.1mm 단위), 기본값 50000 (5m)
 * @return true 성공, false 실패
 */
bool ubx_set_survey_in_mode(gps_t* gps, uint32_t min_duration, uint32_t accuracy_limit)
{
    if (!gps) {
        return false;
    }

    ubx_cfg_item_t tmode_configs[3] = {
        {
            .key_id = CFG_TMODE_MODE,
            .value = {1},  // 1 = Survey-in mode
            .value_len = 1,
        },
        {
            .key_id = CFG_TMODE_SVIN_MIN_DUR,
            .value = {
                (min_duration & 0xFF),
                (min_duration >> 8) & 0xFF,
                (min_duration >> 16) & 0xFF,
                (min_duration >> 24) & 0xFF
            },
            .value_len = 4,
        },
        {
            .key_id = CFG_TMODE_SVIN_ACC_LIMIT,
            .value = {
                (accuracy_limit & 0xFF),
                (accuracy_limit >> 8) & 0xFF,
                (accuracy_limit >> 16) & 0xFF,
                (accuracy_limit >> 24) & 0xFF
            },
            .value_len = 4,
        },
    };

    bool result = ubx_send_valset_sync(gps, UBX_CFG_LAYER_RAM, tmode_configs, 3, 3000);

    if (result) {
        LOG_DEBUG("Survey-in mode started (duration: %u s, accuracy: %u mm)\n",
                  min_duration, accuracy_limit / 10);
    } else {
        LOG_ERR("Failed to start survey-in mode\n");
    }

    return result;
}

/**
 * @brief Fixed 모드 설정 (수동 좌표 입력)
 *
 * @param gps GPS 구조체
 * @param lat_str 위도 문자열 (degrees, 예: "37.12345")
 * @param lon_str 경도 문자열 (degrees, 예: "127.12345")
 * @param alt_str 고도 문자열 (meters, 예: "100.5")
 * @return true 성공, false 실패
 */
bool ubx_set_fixed_position(gps_t* gps, const char* lat_str, const char* lon_str, const char* alt_str)
{
    if (!gps || !lat_str || !lon_str || !alt_str) {
        return false;
    }

    // 문자열을 double로 변환
    double lat_deg = strtod(lat_str, NULL);
    double lon_deg = strtod(lon_str, NULL);
    double alt_m = strtod(alt_str, NULL);

    // u-blox 포맷으로 변환
    int32_t lat_e7 = (int32_t)(lat_deg * 1e7);  // degrees * 1e-7
    int32_t lon_e7 = (int32_t)(lon_deg * 1e7);  // degrees * 1e-7
    int32_t height_cm = (int32_t)(alt_m * 100); // cm

    ubx_cfg_item_t tmode_configs[4] = {
        {
            .key_id = CFG_TMODE_MODE,
            .value = {2},  // 2 = Fixed mode
            .value_len = 1,
        },
        {
            .key_id = CFG_TMODE_LLH_LAT,
            .value = {
                (lat_e7 & 0xFF),
                (lat_e7 >> 8) & 0xFF,
                (lat_e7 >> 16) & 0xFF,
                (lat_e7 >> 24) & 0xFF
            },
            .value_len = 4,
        },
        {
            .key_id = CFG_TMODE_LLH_LON,
            .value = {
                (lon_e7 & 0xFF),
                (lon_e7 >> 8) & 0xFF,
                (lon_e7 >> 16) & 0xFF,
                (lon_e7 >> 24) & 0xFF
            },
            .value_len = 4,
        },
        {
            .key_id = CFG_TMODE_LLH_HEIGHT,
            .value = {
                (height_cm & 0xFF),
                (height_cm >> 8) & 0xFF,
                (height_cm >> 16) & 0xFF,
                (height_cm >> 24) & 0xFF
            },
            .value_len = 4,
        },
    };

    bool result = ubx_send_valset_sync(gps, UBX_CFG_LAYER_RAM, tmode_configs, 4, 3000);

    if (result) {
        LOG_DEBUG("Fixed position set (lat: %s, lon: %s, alt: %s m)\n",
                  lat_str, lon_str, alt_str);
    } else {
        LOG_ERR("Failed to set fixed position\n");
    }

    return result;
}

/**
 * @brief Time Mode 비활성화
 *
 * @param gps GPS 구조체
 * @return true 성공, false 실패
 */
bool ubx_disable_time_mode(gps_t* gps)
{
    if (!gps) {
        return false;
    }

    ubx_cfg_item_t tmode_config = {
        .key_id = CFG_TMODE_MODE,
        .value = {0},  // 0 = Disabled
        .value_len = 1,
    };

    bool result = ubx_send_valset_sync(gps, UBX_CFG_LAYER_RAM, &tmode_config, 1, 3000);

    if (result) {
        LOG_DEBUG("Time mode disabled\n");
    } else {
        LOG_ERR("Failed to disable time mode\n");
    }

    return result;
}


