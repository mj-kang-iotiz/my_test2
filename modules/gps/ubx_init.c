#include <stdio.h>
#include <stdbool.h>
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


static const ubx_cfg_item_t ublox_base_configs[] = {

    /* UART2 보드레이트 설정 (F9P-to-F9P RTCM 통신) */
    {
        .key_id = CFG_BAUDRATE_UART2,
        .value = {0x00, 0xC2, 0x01, 0x00},  // 115200 (little-endian)
        .value_len = 4,
    },

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

    /* UART2 보드레이트 설정 (F9P-to-F9P RTCM 통신) */
    {
        .key_id = CFG_BAUDRATE_UART2,
        .value = {0x00, 0xC2, 0x01, 0x00},  // 115200 (little-endian)
        .value_len = 4,
    },

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

    /* UART2 보드레이트 설정 (F9P-to-F9P RTCM 통신) */
    {
        .key_id = CFG_BAUDRATE_UART2,
        .value = {0x00, 0xC2, 0x01, 0x00},  // 115200 (little-endian)
        .value_len = 4,
    },

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


