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

/**
 * @brief F9P UART baud rate 변경 (동기 방식)
 *
 * @param gps GPS 인스턴스
 * @param uart_num UART 번호 (1 = UART1, 2 = UART2)
 * @param baudrate 새로운 baud rate (예: 115200)
 * @param timeout_ms 타임아웃 (ms)
 * @return true: 성공, false: 실패
 */
bool ubx_set_uart_baudrate(gps_t* gps, uint8_t uart_num, uint32_t baudrate, uint32_t timeout_ms)
{
    if (!gps || uart_num == 0 || uart_num > 2)
    {
        LOG_ERR("Invalid parameters: uart_num=%d", uart_num);
        return false;
    }

    LOG_INFO("F9P UART%d baud rate 변경: %u bps", uart_num, baudrate);

    // UART1 또는 UART2 baud rate 설정 키 선택
    uint32_t baud_key_id = (uart_num == 1) ? CFG_BAUDRATE_UART1 : CFG_BAUDRATE_UART2;

    // Baud rate 설정 (4바이트, little-endian)
    ubx_cfg_item_t baud_cfg = {
        .key_id = baud_key_id,
        .value = {
            (baudrate >> 0) & 0xFF,
            (baudrate >> 8) & 0xFF,
            (baudrate >> 16) & 0xFF,
            (baudrate >> 24) & 0xFF
        },
        .value_len = 4,
    };

    // FLASH에 저장하여 재부팅 후에도 유지 (BBR도 함께)
    bool result = ubx_send_valset_sync(gps, UBX_CFG_LAYER_FLASH | UBX_CFG_LAYER_BBR,
                                       &baud_cfg, 1, timeout_ms);

    if (result)
    {
        LOG_INFO("F9P UART%d baud rate 변경 성공: %u bps", uart_num, baudrate);
    }
    else
    {
        LOG_ERR("F9P UART%d baud rate 변경 실패", uart_num);
    }

    return result;
}

/**
 * @brief F9P 설정 전체 초기화 (공장 초기화)
 *
 * UBX-CFG-CFG 메시지를 사용하여 모든 설정을 공장 초기값으로 복원합니다.
 * 이후 F9P는 자동으로 재부팅됩니다.
 *
 * @param gps GPS 인스턴스
 * @param timeout_ms 타임아웃 (ms)
 * @return true: 성공, false: 실패
 */
bool ubx_factory_reset(gps_t* gps, uint32_t timeout_ms)
{
    if (!gps)
    {
        LOG_ERR("Invalid GPS handle");
        return false;
    }

    LOG_WARN("F9P 전체 설정 초기화 (공장 초기화) 시작...");

    // UBX-CFG-CFG 메시지 생성
    // Class: 0x06 (CFG), ID: 0x09 (CFG-CFG)
    // Payload:
    //   clearMask (4 bytes): 0x0000061F (모든 설정 클리어)
    //   loadMask  (4 bytes): 0x0000061F (모든 설정 로드)
    //   saveMask  (4 bytes): 0x00000000 (저장 안함, 공장 초기화이므로)
    //   deviceMask (1 byte): 0x17 (BBR, Flash, EEPROM)

    uint8_t msg[17];
    size_t offset = 0;

    // Sync bytes
    msg[offset++] = 0xB5;  // UBX_SYNC_1
    msg[offset++] = 0x62;  // UBX_SYNC_2

    // Class & ID
    msg[offset++] = 0x06;  // GPS_UBX_CLASS_CFG
    msg[offset++] = 0x09;  // CFG-CFG

    // Payload length
    msg[offset++] = 0x0D;  // 13 bytes (little-endian)
    msg[offset++] = 0x00;

    // Payload
    // clearMask (0x0000061F): ioPort, msgConf, infMsg, navConf, rxmConf
    msg[offset++] = 0x1F;
    msg[offset++] = 0x06;
    msg[offset++] = 0x00;
    msg[offset++] = 0x00;

    // saveMask (0x00000000): 저장하지 않음 (공장 초기화)
    msg[offset++] = 0x00;
    msg[offset++] = 0x00;
    msg[offset++] = 0x00;
    msg[offset++] = 0x00;

    // loadMask (0x0000061F): 모든 설정 로드
    msg[offset++] = 0x1F;
    msg[offset++] = 0x06;
    msg[offset++] = 0x00;
    msg[offset++] = 0x00;

    // deviceMask (0x17): devBBR, devFlash, devEEPROM
    msg[offset++] = 0x17;

    // Checksum 계산
    uint8_t ck_a, ck_b;
    ubx_calc_checksum(&msg[2], offset - 2, &ck_a, &ck_b);
    msg[offset++] = ck_a;
    msg[offset++] = ck_b;

    // GPS에 전송
    if (gps->ops && gps->ops->send)
    {
        gps->ops->send((const char*)msg, offset);
        LOG_INFO("F9P 공장 초기화 명령 전송 완료 (F9P 재부팅 예상)");

        // 공장 초기화 후 F9P가 재부팅하므로 ACK를 받지 못할 수 있음
        // 따라서 단순히 명령을 보내는 것으로 충분
        return true;
    }
    else
    {
        LOG_ERR("GPS send function not available");
        return false;
    }
}
