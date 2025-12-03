/**
 * F9P UART Baudrate Configuration
 * STM32F405 + ZED-F9P (LL Library)
 */

#ifndef F9P_BAUDRATE_CONFIG_H
#define F9P_BAUDRATE_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "gps.h"

/**
 * F9P UART1 보드레이트 변경 및 검증 (STM32 UART2)
 * DMA 활성화 전/후 모두 사용 가능 (자동으로 DMA 제어)
 * @param gps: GPS 핸들 (uart2 사용)
 * @return true if success
 */
bool f9p_change_uart1_baudrate_to_115200(gps_t *gps);

/**
 * F9P UART2 보드레이트 변경 및 검증 (STM32 UART4)
 * DMA 활성화 전/후 모두 사용 가능 (자동으로 DMA 제어)
 * @param gps: GPS 핸들 (uart4 사용)
 * @return true if success
 */
bool f9p_change_uart2_baudrate_to_115200(gps_t *gps);

/**
 * F9P UART1 보드레이트 변경 (초기화 시점용 - DMA 활성화 전)
 * gps_uart2_init() 후, gps_uart2_comm_start() 전에 호출
 * @return true if success
 */
bool f9p_init_uart1_baudrate_115200(void);

/**
 * F9P UART2 보드레이트 변경 (초기화 시점용 - DMA 활성화 전)
 * gps_uart4_init() 후, gps_uart4_comm_start() 전에 호출
 * @return true if success
 */
bool f9p_init_uart2_baudrate_115200(void);

/**
 * F9P UART1 현재 보드레이트 확인 (STM32 UART2)
 * @param gps: GPS 핸들
 * @param baudrate: 읽은 보드레이트 저장
 * @return true if success
 */
bool f9p_poll_uart1_baudrate(gps_t *gps, uint32_t *baudrate);

/**
 * F9P UART2 현재 보드레이트 확인 (STM32 UART4)
 * @param gps: GPS 핸들
 * @param baudrate: 읽은 보드레이트 저장
 * @return true if success
 */
bool f9p_poll_uart2_baudrate(gps_t *gps, uint32_t *baudrate);

#endif
