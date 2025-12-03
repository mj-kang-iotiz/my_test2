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
 * F9P 자신의 UART2 보드레이트 변경 (UART1을 통해 설정)
 * F9P 모듈끼리 RTCM 통신용 (F9P UART2 ↔ F9P UART2)
 * @param gps: GPS 핸들 (UART1 연결)
 * @return true if success
 */
bool f9p_change_its_uart2_baudrate_to_115200(gps_t *gps);

/**
 * F9P UART1 보드레이트 변경 (초기화 시점용 - DMA 활성화 전)
 * gps_uart2_init() 후, gps_uart2_comm_start() 전에 호출
 * STM32 UART2 사용
 * @return true if success
 */
bool f9p_init_uart1_baudrate_115200(void);

/**
 * F9P UART2 보드레이트 변경 (초기화 시점용 - DMA 활성화 전)
 * gps_uart4_init() 후, gps_uart4_comm_start() 전에 호출
 * STM32 UART4 사용
 * @return true if success
 */
bool f9p_init_rover_uart1_baudrate_115200(void);

/**
 * F9P UART1 현재 보드레이트 확인 (STM32 UART2)
 * @param gps: GPS 핸들
 * @param baudrate: 읽은 보드레이트 저장
 * @return true if success
 */
bool f9p_poll_uart1_baudrate(gps_t *gps, uint32_t *baudrate);

/**
 * F9P UART2 현재 보드레이트 확인 (UART1을 통해)
 * 주의: F9P UART2는 STM32에 연결 안됨. UART1을 통해 poll만 가능
 * @param gps: GPS 핸들 (UART1 연결)
 * @param baudrate: 읽은 보드레이트 저장
 * @return true if success
 */
bool f9p_poll_uart2_baudrate(gps_t *gps, uint32_t *baudrate);

#endif
