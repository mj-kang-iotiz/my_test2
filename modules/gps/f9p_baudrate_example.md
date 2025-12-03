# F9P Baudrate Configuration Usage

F9P 모듈의 UART1, UART2 보드레이트를 38400에서 115200으로 변경하는 예제입니다.

## 개요

- **STM32 UART2** → **F9P UART1** (보드레이트 변경)
- **STM32 UART4** → **F9P UART2** (보드레이트 변경)
- LL 라이브러리 사용
- Flash 저장 없이 RAM만 변경 (재부팅 시 초기화)

## 사용 방법

### 1. 헤더 파일 포함

```c
#include "f9p_baudrate_config.h"
#include "gps.h"
```

### 2. F9P UART1 보드레이트 변경 (STM32 UART2)

```c
gps_t *gps_base = ...; // GPS 핸들

// F9P UART1 → 115200 bps 변경
if (f9p_change_uart1_baudrate_to_115200(gps_base)) {
    LOG_INFO("F9P UART1 baudrate changed successfully!");
} else {
    LOG_ERR("Failed to change F9P UART1 baudrate");
}
```

### 3. F9P UART2 보드레이트 변경 (STM32 UART4)

```c
gps_t *gps_rover = ...; // GPS 핸들

// F9P UART2 → 115200 bps 변경
if (f9p_change_uart2_baudrate_to_115200(gps_rover)) {
    LOG_INFO("F9P UART2 baudrate changed successfully!");
} else {
    LOG_ERR("Failed to change F9P UART2 baudrate");
}
```

### 4. 현재 보드레이트 확인

```c
uint32_t baudrate = 0;

// F9P UART1 현재 보드레이트 확인
if (f9p_poll_uart1_baudrate(gps_base, &baudrate)) {
    LOG_INFO("F9P UART1 baudrate: %lu", baudrate);
}

// F9P UART2 현재 보드레이트 확인
if (f9p_poll_uart2_baudrate(gps_rover, &baudrate)) {
    LOG_INFO("F9P UART2 baudrate: %lu", baudrate);
}
```

## 동작 과정

### 각 함수의 동작 순서:

1. **현재 보드레이트 확인** (38400 bps에서 Poll)
   - UBX-CFG-PRT Poll 메시지 전송
   - F9P 응답 파싱

2. **F9P 보드레이트 변경 요청**
   - UBX-CFG-PRT Set 메시지 전송 (115200 bps)
   - ACK/NAK 대기 (timeout 허용)

3. **STM32 UART 보드레이트 변경**
   - LL_USART_SetBaudRate() 사용
   - 115200 bps로 변경

4. **변경 검증**
   - 115200 bps에서 Poll 재시도
   - 성공 시 완료, 실패 시 38400으로 복원

## 주의사항

1. **Flash 저장 없음**: 이 코드는 RAM만 변경하므로 F9P 재부팅 시 38400으로 돌아갑니다.
2. **DMA 통신 중단**: 함수 실행 중 DMA RX가 비활성화되어야 정상 동작합니다.
3. **타이밍**: HAL_Delay()로 안정화 시간을 확보합니다.

## 통합 예제

### main.c 또는 gps_app.c에서 사용:

```c
void gps_baudrate_config_test(void)
{
    // GPS 초기화 후 실행
    gps_t *gps_base = gps_get_handle(GPS_ID_BASE);
    gps_t *gps_rover = gps_get_handle(GPS_ID_ROVER);

    LOG_INFO("Starting F9P baudrate configuration...");

    // UART1 변경 (Base GPS)
    if (gps_base) {
        if (f9p_change_uart1_baudrate_to_115200(gps_base)) {
            LOG_INFO("Base GPS (UART1) configured to 115200 bps");
        }
    }

    // UART2 변경 (Rover GPS)
    if (gps_rover) {
        if (f9p_change_uart2_baudrate_to_115200(gps_rover)) {
            LOG_INFO("Rover GPS (UART2) configured to 115200 bps");
        }
    }

    LOG_INFO("F9P baudrate configuration complete");
}
```

## 로그 출력 예시

```
[INFO] === F9P UART1 Baudrate Change Test (STM32 UART2) ===
[INFO] [1] Polling at 38400...
[INFO]     Current: 38400 bps
[INFO] [2] Setting F9P UART1 to 115200...
[INFO] [3] Switching STM32 UART2 to 115200...
[INFO] [4] Verifying at 115200...
[INFO]     SUCCESS! Now: 115200 bps
```

## API Reference

| 함수 | 설명 |
|------|------|
| `f9p_change_uart1_baudrate_to_115200()` | F9P UART1을 115200으로 변경 (STM32 UART2) |
| `f9p_change_uart2_baudrate_to_115200()` | F9P UART2를 115200으로 변경 (STM32 UART4) |
| `f9p_poll_uart1_baudrate()` | F9P UART1 현재 보드레이트 확인 |
| `f9p_poll_uart2_baudrate()` | F9P UART2 현재 보드레이트 확인 |

## 테스트 방법

1. F9P를 38400 bps로 초기화
2. `f9p_change_uart1_baudrate_to_115200()` 또는 `f9p_change_uart2_baudrate_to_115200()` 호출
3. 로그에서 "SUCCESS!" 메시지 확인
4. 이후 GPS 데이터 수신이 115200 bps로 정상 동작하는지 확인
