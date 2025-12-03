#include <stdio.h>
#include <stdbool.h>
#include "gps_ubx.h"
#include "ubx_init.h"
#include "gps_port.h"

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

#define CFG_BAUDRATE_UART1 (0x40520001U) // 4바이트
#define CFG_BAUDRATE_UART2 (0x40530001U) // 4바이트

#define CFG_UART2INPROT_RTCM3X (0x10750004U)
#define CFG_UART2OUTPROT_RTCM3X (0x10760004U)


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
        .key_id = CFG_NAV_HPPOSLLH_UART1,
        .value = {1},
        .value_len = 1,
    },

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

    {
        .key_id = CFG_UART2OUTPROT_RTCM3X,
        .value = {0},
        .value_len = 1,
    },
};

static const ubx_cfg_item_t ublox_moving_base_configs[] = {
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

typedef struct {
    gps_t *gps;
    gps_id_t gps_id;
    ubx_init_type_t init_type;
} baudrate_change_context_t;

static baudrate_change_context_t baudrate_ctx = {0};

static void on_baudrate_change_complete(bool ack, void *user_data)
{
    baudrate_change_context_t *ctx = (baudrate_change_context_t *)user_data;

    if (ack)
    {
        LOG_INFO("F9P baudrate change ACK received!");

        // Delay for GPS to apply baudrate change (ACK는 이전 보드레이트로 전송됨)
        vTaskDelay(pdMS_TO_TICKS(500));

        // Change STM UART baudrate to 115200
        gps_uart_change_baudrate(ctx->gps_id, 115200);
        LOG_INFO("STM UART baudrate changed to 115200");

        // Delay for UART stabilization
        vTaskDelay(pdMS_TO_TICKS(200));

        // Execute initialization function based on type
        switch (ctx->init_type)
        {
            case UBX_INIT_TYPE_BASE:
                ubx_base_init(ctx->gps);
                LOG_INFO("Starting UBX base init after baudrate change");
                break;

            case UBX_INIT_TYPE_ROVER:
                ubx_rover_init(ctx->gps);
                LOG_INFO("Starting UBX rover init after baudrate change");
                break;

            case UBX_INIT_TYPE_MOVING_BASE:
                ubx_moving_base_init(ctx->gps);
                LOG_INFO("Starting UBX moving base init after baudrate change");
                break;

            default:
                LOG_ERR("Unknown init type!");
                break;
        }
    }
    else
    {
        LOG_ERR("F9P baudrate change failed (NAK received)");
    }
}

bool ubx_change_baudrate_and_init(gps_t* gps, uint32_t baudrate, gps_id_t gps_id, ubx_init_type_t init_type)
{
    LOG_INFO("Starting F9P baudrate change to %d bps for GPS ID %d", baudrate, gps_id);

    // Prepare baudrate configuration
    ubx_cfg_item_t baudrate_cfg = {
        .key_id = CFG_BAUDRATE_UART1,
        .value_len = 4,
    };

    // Set baudrate value (little-endian)
    baudrate_cfg.value[0] = (baudrate >> 0) & 0xFF;
    baudrate_cfg.value[1] = (baudrate >> 8) & 0xFF;
    baudrate_cfg.value[2] = (baudrate >> 16) & 0xFF;
    baudrate_cfg.value[3] = (baudrate >> 24) & 0xFF;

    // Send baudrate change command
    if (!ubx_send_valset(gps, UBX_CFG_LAYER_RAM, &baudrate_cfg, 1))
    {
        LOG_ERR("Failed to send baudrate change command");
        return false;
    }

    LOG_INFO("Baudrate change command sent");

    // 간단한 busy-wait 딜레이 (F9P가 명령 처리할 시간)
    for(volatile uint32_t i = 0; i < 1000000; i++);

    // Change STM UART baudrate
    gps_uart_change_baudrate(gps_id, baudrate);
    LOG_INFO("STM UART baudrate changed to %d", baudrate);

    // Execute initialization function based on type
    switch (init_type)
    {
        case UBX_INIT_TYPE_BASE:
            ubx_base_init(gps);
            LOG_INFO("Starting UBX base init after baudrate change");
            break;

        case UBX_INIT_TYPE_ROVER:
            ubx_rover_init(gps);
            LOG_INFO("Starting UBX rover init after baudrate change");
            break;

        case UBX_INIT_TYPE_MOVING_BASE:
            ubx_moving_base_init(gps);
            LOG_INFO("Starting UBX moving base init after baudrate change");
            break;

        default:
            LOG_ERR("Unknown init type!");
            break;
    }

    return true;
}