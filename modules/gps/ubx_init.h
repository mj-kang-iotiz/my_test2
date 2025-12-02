#include "gps.h"
#include <stdbool.h>
#include <stdint.h>

bool ubx_rover_init(gps_t* gps);
bool ubx_base_init(gps_t* gps);
bool ubx_moving_base_init(gps_t* gps);

// F9P UART baud rate 변경 (동기)
bool ubx_set_uart_baudrate(gps_t* gps, uint8_t uart_num, uint32_t baudrate, uint32_t timeout_ms);

// F9P 설정 전체 초기화 (공장 초기화)
bool ubx_factory_reset(gps_t* gps, uint32_t timeout_ms);