# LoRa P2P 사용 가이드 (RAK4270)

## 개요

이 구현은 RAK4270 모듈을 사용한 LoRa P2P 통신을 지원합니다.
- **동기/비동기 AT 명령어 전송**
- **자동 OK/ERROR 응답 처리**
- **P2P 수신 데이터 → GPS UART 자동 전송** (RTCM 데이터용)
- **GPS TX Task와 UART 충돌 방지** (Mutex 보호)

## 아키텍처

```
┌─────────────┐         ┌──────────────┐         ┌──────────────┐
│ User Code   │ ──cmd──>│ LoRa TX Task │ ──UART──>│ RAK4270      │
│             │         │              │         │              │
│             │ <─resp──│              │ <─UART───│              │
└─────────────┘         └──────────────┘         └──────────────┘
                               │
                               │ (Mutex 보호)
                               ▼
┌─────────────┐         ┌──────────────┐         ┌──────────────┐
│ GPS Module  │ <─RTCM──│ LoRa RX Task │ <─recv───│ RAK4270      │
│ (UART2)     │         │              │         │ (UART3)      │
└─────────────┘         └──────────────┘         └──────────────┘
```

## 초기화

`main.c`의 `initThread`에서 자동으로 호출됩니다:

```c
void initThread(void *pvParameter) {
  led_init();
  gps_init_all();
  gsm_task_create(NULL);
  lora_instance_init();  // LoRa 초기화
  vTaskDelete(NULL);
}
```

## 기본 사용법

### 1. LoRa P2P 모드 설정

```c
#include "lora_app.h"

// P2P 모드로 전환
bool result = lora_set_work_mode(LORA_WORK_MODE_P2P, 2000);
if (result) {
  LOG_INFO("P2P mode OK");
} else {
  LOG_ERR("P2P mode failed");
}
```

### 2. LoRa P2P 파라미터 설정

```c
// P2P 설정: 920.9MHz, SF7, BW125kHz, CR4/5, Preamble=8, Power=14dBm
bool result = lora_set_p2p_config(
    920900000,  // freq: 920.9MHz (KR920)
    7,          // sf: Spreading Factor 7
    0,          // bw: 0=125kHz, 1=250kHz, 2=500kHz
    1,          // cr: Coding Rate 4/5
    8,          // preamlen: Preamble length
    14,         // pwr: TX Power 14dBm
    2000        // timeout_ms
);

if (result) {
  LOG_INFO("P2P config OK");
} else {
  LOG_ERR("P2P config failed");
}
```

### 3. P2P 전송 모드 설정

```c
// Event-driven 모드 (수신 시 at+recv=... 출력)
bool result = lora_set_p2p_transfer_mode(LORA_P2P_TRANSFER_MODE_EVENT, 2000);

// Continuous 모드 (수신 시 즉시 출력, at+recv 없음)
// bool result = lora_set_p2p_transfer_mode(LORA_P2P_TRANSFER_MODE_CONTINUOUS, 2000);
```

### 4. P2P 데이터 전송

```c
// HEX string으로 전송 (ASCII "Hello" = 48656C6C6F)
bool result = lora_send_p2p_data("48656C6C6F", 2000);

if (result) {
  LOG_INFO("P2P send OK");
} else {
  LOG_ERR("P2P send failed");
}
```

**바이너리 → HEX 변환 예제:**

```c
// RTCM 데이터를 HEX string으로 변환
char rtcm_data[] = {0xD3, 0x00, 0x13, ...};  // RTCM binary
char hex_string[512];

for (int i = 0; i < rtcm_len; i++) {
  sprintf(&hex_string[i * 2], "%02X", rtcm_data[i]);
}

lora_send_p2p_data(hex_string, 2000);
```

### 5. P2P 데이터 수신

**자동 GPS 전송 (기본값):**

콜백을 등록하지 않으면 수신 데이터가 자동으로 GPS UART로 전송됩니다:

```c
// 아무것도 하지 않으면 자동으로 GPS로 전송됨
// lora_set_p2p_recv_callback(NULL, NULL);  // 기본값
```

**수동 처리 (콜백 등록):**

```c
void my_p2p_recv_callback(lora_p2p_recv_data_t *recv_data, void *user_data) {
  LOG_INFO("P2P recv: RSSI=%d, SNR=%d, Len=%d",
           recv_data->rssi, recv_data->snr, recv_data->data_len);

  // 수신 데이터 처리 (바이너리)
  for (int i = 0; i < recv_data->data_len; i++) {
    printf("%02X ", recv_data->data[i]);
  }
  printf("\n");

  // 원한다면 GPS로 수동 전송
  gps_t *gps = gps_get_instance_handle(GPS_ID_BASE);
  if (gps && gps->ops && gps->ops->send) {
    xSemaphoreTake(gps->mutex, portMAX_DELAY);
    gps->ops->send(recv_data->data, recv_data->data_len);
    xSemaphoreGive(gps->mutex);
  }
}

// 콜백 등록
lora_set_p2p_recv_callback(my_p2p_recv_callback, NULL);
```

## 고급 사용법

### 동기 vs 비동기 명령어 전송

**동기 (Blocking):**

```c
// 응답을 기다림 (타임아웃까지)
bool result = lora_send_command_sync("AT+SET_CONFIG=lora:work_mode:0\r\n", 2000);

if (result) {
  LOG_INFO("Command OK");
} else {
  LOG_ERR("Command failed or timeout");
}
```

**비동기 (Non-blocking):**

```c
void my_callback(bool success, void *user_data) {
  if (success) {
    LOG_INFO("Command OK");
  } else {
    LOG_ERR("Command failed");
  }
}

// 즉시 반환, 콜백으로 결과 통보
bool queued = lora_send_command_async(
    "AT+SET_CONFIG=lora:work_mode:0\r\n",
    2000,
    my_callback,
    NULL
);

if (!queued) {
  LOG_ERR("Failed to queue command");
}
```

### 원시 AT 명령어 전송

```c
// RAK4270 AT 명령어 직접 전송
lora_send_command_sync("AT+VERSION\r\n", 2000);
lora_send_command_sync("AT+GET_CONFIG=lora:status\r\n", 2000);
lora_send_command_sync("AT+RESET\r\n", 2000);
```

## 완전한 초기화 시퀀스 예제

```c
void lora_p2p_init_sequence(void) {
  // 1. P2P 모드로 전환
  if (!lora_set_work_mode(LORA_WORK_MODE_P2P, 2000)) {
    LOG_ERR("Failed to set P2P mode");
    return;
  }

  vTaskDelay(pdMS_TO_TICKS(100));

  // 2. P2P 파라미터 설정
  if (!lora_set_p2p_config(920900000, 7, 0, 1, 8, 14, 2000)) {
    LOG_ERR("Failed to set P2P config");
    return;
  }

  vTaskDelay(pdMS_TO_TICKS(100));

  // 3. Event-driven 모드 설정
  if (!lora_set_p2p_transfer_mode(LORA_P2P_TRANSFER_MODE_EVENT, 2000)) {
    LOG_ERR("Failed to set transfer mode");
    return;
  }

  LOG_INFO("LoRa P2P initialized successfully");

  // 4. 수신 데이터는 자동으로 GPS로 전송됨 (기본 동작)
}
```

## 성능 특성

### UART 송신 시간

- **UART 속도**: 115200 bps
- **1 byte 전송 시간**: ~87 μs (10 bits: 1 start + 8 data + 1 stop)
- **1KB 전송 시간**: ~87 ms

### 동시성 보장

- **GPS TX Task**: GPS Mutex로 UART 보호
- **LoRa TX Task**: GPS Mutex로 UART 보호 (GPS로 전송 시)
- **LoRa RX Task**: GPS Mutex로 UART 보호 (GPS로 전송 시)

**데드락 없음** ✅ (단일 Mutex, Hold and Wait 없음)

### 메모리 사용량

- **RX Task 스택**: 2048 bytes
- **TX Task 스택**: 2048 bytes
- **RX Queue**: 10 items × 1 byte = 10 bytes
- **TX Queue**: 10 items × sizeof(lora_cmd_request_t) = 10 × 256 = 2560 bytes
- **총 메모리**: ~4.6 KB

## 문제 해결

### 명령어 타임아웃

```c
// 타임아웃 늘리기
lora_send_command_sync("AT+RESET\r\n", 5000);  // 5초 타임아웃
```

### P2P 수신이 안 됨

1. Transfer mode 확인: Event-driven 모드인지 확인
2. 주파수/SF/BW 일치 여부 확인
3. UART3 연결 확인 (PB10=TX, PB11=RX)

### GPS로 데이터가 전송 안 됨

1. `gps_get_instance_handle(GPS_ID_BASE)` 반환값 확인
2. GPS mutex 초기화 여부 확인
3. GPS TX Task와 충돌 시 mutex 대기 시간 확인

## API 레퍼런스

자세한 API는 `lora_app.h` 참조.

주요 함수:
- `lora_instance_init()` - 초기화
- `lora_set_work_mode()` - P2P/LoRaWAN 모드 설정
- `lora_set_p2p_config()` - P2P 파라미터 설정
- `lora_set_p2p_transfer_mode()` - Event/Continuous 모드
- `lora_send_p2p_data()` - P2P 데이터 전송
- `lora_set_p2p_recv_callback()` - 수신 콜백 등록
- `lora_send_command_sync()` - 동기 명령어 전송
- `lora_send_command_async()` - 비동기 명령어 전송
