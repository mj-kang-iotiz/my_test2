# 두 베이스 스테이션 비동기 설정 가이드

이 문서는 두 개의 베이스 스테이션을 비동기적으로 동시에 설정하는 방법을 설명합니다.

## 개요

UM982 GPS 모듈을 사용하여 두 가지 방식의 베이스 스테이션을 설정할 수 있습니다:

1. **Fixed Position Mode**: 미리 알고 있는 정확한 좌표로 베이스 스테이션 설정
2. **Survey-in Mode**: 일정 시간 동안 위치를 측정하여 자동으로 베이스 스테이션 설정

이 기능은 두 베이스 스테이션을 **동시에 비동기적으로** 초기화하여 효율적으로 설정할 수 있습니다.

## 주요 기능

### 1. Fixed Position 베이스 스테이션

```c
bool gps_init_um982_base_fixed_async(
    gps_id_t id,              // GPS ID
    double lat,               // 위도 (도)
    double lon,               // 경도 (도)
    double alt,               // 고도 (m)
    gps_init_callback_t callback  // 완료 콜백
);
```

**명령어 예시:**
```
MODE BASE 40.45628476579 116.2859754968 58.0984
```

**특징:**
- 정확한 좌표를 사용하여 즉시 베이스 스테이션 모드로 전환
- 측정 시간 불필요
- 이미 알고 있는 기준점에 적합

### 2. Survey-in 베이스 스테이션

```c
bool gps_init_um982_base_surveyin_async(
    gps_id_t id,              // GPS ID
    uint32_t time_sec,        // 측정 시간 (초)
    float accuracy_m,         // 목표 정확도 (m)
    gps_init_callback_t callback  // 완료 콜백
);
```

**명령어 예시:**
```
MODE BASE TIME 120 0.1
```

**특징:**
- 지정된 시간 동안 위치를 측정하여 평균 계산
- 정확도 목표치 설정 가능
- 정확한 좌표를 모를 때 유용

### 3. 두 베이스 스테이션 동시 초기화

```c
bool gps_init_dual_base_async(
    gps_id_t base1_id,        // Base1 ID (Fixed)
    double base1_lat,         // Base1 위도
    double base1_lon,         // Base1 경도
    double base1_alt,         // Base1 고도
    gps_id_t base2_id,        // Base2 ID (Survey-in)
    uint32_t base2_time,      // Base2 측정 시간
    float base2_accuracy,     // Base2 정확도
    gps_init_callback_t callback  // 완료 콜백
);
```

**특징:**
- 두 베이스 스테이션을 **병렬로** 초기화
- Base1 (Fixed)과 Base2 (Survey-in)를 동시에 설정
- 모든 초기화가 완료되면 콜백 호출
- 효율적인 시간 활용

## 사용 예제

### 예제 1: 단일 Fixed Position 베이스

```c
void setup_fixed_base(void) {
    // GPS ID
    gps_id_t base_id = GPS_ID_BASE;

    // 좌표 (베이징 근처)
    double lat = 40.45628476579;
    double lon = 116.2859754968;
    double alt = 58.0984;

    // 초기화
    gps_init_um982_base_fixed_async(
        base_id, lat, lon, alt,
        my_callback
    );
}
```

### 예제 2: 단일 Survey-in 베이스

```c
void setup_surveyin_base(void) {
    // GPS ID
    gps_id_t base_id = GPS_ID_BASE;

    // Survey-in 파라미터
    uint32_t time = 120;      // 120초
    float accuracy = 0.1f;    // 0.1m

    // 초기화
    gps_init_um982_base_surveyin_async(
        base_id, time, accuracy,
        my_callback
    );
}
```

### 예제 3: 두 베이스 동시 초기화 (권장!)

```c
void setup_dual_base(void) {
    // 완료 콜백
    void dual_callback(bool success, void *user_data) {
        if (success) {
            printf("두 베이스 스테이션 모두 준비 완료!\n");
        } else {
            printf("초기화 실패\n");
        }
    }

    // 두 베이스 동시 초기화
    gps_init_dual_base_async(
        GPS_ID_BASE, 40.45628476579, 116.2859754968, 58.0984,  // Base1: Fixed
        GPS_ID_ROVER, 120, 0.1f,                                // Base2: Survey-in
        dual_callback
    );
}
```

## 초기화 과정

### Fixed Position 초기화 순서

1. GNSS 시스템 활성화 (BDS, GPS, GLO, GAL)
2. MODE BASE 명령으로 고정 좌표 설정
3. RTCM 메시지 활성화 (1033, 1006, 1074, 1124, 1084, 1094)
4. NMEA GGA 출력 활성화
5. BESTNAVB 바이너리 출력 활성화

### Survey-in 초기화 순서

1. GNSS 시스템 활성화 (BDS, GPS, GLO, GAL)
2. MODE BASE TIME 명령으로 Survey-in 시작
3. 지정된 시간 동안 위치 측정
4. RTCM 메시지 활성화
5. NMEA GGA 출력 활성화
6. BESTNAVB 바이너리 출력 활성화

### 비동기 동작 방식

```
시작
  │
  ├─→ Base1 (Fixed) 초기화 시작
  │     └─→ 명령 1 → 명령 2 → ... → 완료 → 콜백
  │
  └─→ Base2 (Survey-in) 초기화 시작
        └─→ 명령 1 → 명령 2 → ... → 완료 → 콜백
                                              │
                                         두 초기화 모두 완료
                                              │
                                        최종 콜백 호출
```

## 실제 사용 방법

### main.c에 통합

```c
#include "gps_app.h"

// 콜백 함수 정의
static void base_init_complete(bool success, void *user_data) {
    if (success) {
        LOG_INFO("베이스 스테이션 초기화 성공");
        // RTCM 데이터 전송 시작 등
    } else {
        LOG_ERR("베이스 스테이션 초기화 실패");
    }
}

void initThread(void *pvParameter) {
    // 기본 초기화
    flash_params_init();
    led_init();
    gps_init_all();  // GPS 인스턴스 생성

    // 두 베이스 스테이션 비동기 초기화
    gps_init_dual_base_async(
        GPS_ID_BASE,                            // Base1 ID
        40.45628476579,                         // Base1 위도
        116.2859754968,                         // Base1 경도
        58.0984,                                // Base1 고도
        GPS_ID_ROVER,                           // Base2 ID
        120,                                    // Base2 측정 시간 (초)
        0.1f,                                   // Base2 정확도 (m)
        base_init_complete                      // 완료 콜백
    );

    // 다른 초기화 계속...
    gsm_task_create(NULL);
    lora_instance_init();

    vTaskDelete(NULL);
}
```

## 로그 출력 예시

```
[GPS_APP] === Starting Dual Base Station Async Init ===
[GPS_APP]   Base1 (GPS[0]): Fixed position (40.45628476579, 116.2859754968, 58.0984)
[GPS_APP]   Base2 (GPS[1]): Survey-in (120 sec, 0.10 m)
[GPS_APP] GPS[0] Starting UM982 base FIXED init sequence (13 commands)
[GPS_APP] GPS[0] Fixed position: lat=40.45628476579, lon=116.2859754968, alt=58.0984
[GPS_APP] GPS[1] Starting UM982 base SURVEY-IN init sequence (13 commands)
[GPS_APP] GPS[1] Survey-in: time=120 sec, accuracy=0.10 m
[GPS_APP] Both base stations are initializing asynchronously...
[GPS_APP] GPS[0] Init step 1/13 OK: unmask BDS
[GPS_APP] GPS[1] Init step 1/13 OK: unmask BDS
[GPS_APP] GPS[0] Init step 2/13 OK: unmask GPS
[GPS_APP] GPS[1] Init step 2/13 OK: unmask GPS
...
[GPS_APP] GPS[0] Init sequence complete!
[GPS_APP] GPS[0] (Base1 - Fixed) init succeeded
[GPS_APP] GPS[1] Init sequence complete!
[GPS_APP] GPS[1] (Base2 - Survey-in) init succeeded
[GPS_APP] === Dual Base Station Init Complete ===
[GPS_APP]   Base1 (GPS[0] Fixed): OK
[GPS_APP]   Base2 (GPS[1] Survey-in): OK
[GPS_APP]   Overall: SUCCESS
```

## 에러 처리

### 재시도 메커니즘

각 명령은 최대 3회까지 재시도됩니다:

```c
#define GPS_INIT_MAX_RETRY 3
#define GPS_INIT_TIMEOUT_MS 1000
```

### 실패 시나리오

1. **타임아웃**: 명령 전송 후 1초 이내에 응답 없음
2. **에러 응답**: GPS 모듈이 ERROR 응답
3. **메모리 부족**: 동적 메모리 할당 실패

### 콜백에서의 처리

```c
void my_callback(bool success, void *user_data) {
    if (success) {
        // 성공 처리
        start_rtcm_transmission();
    } else {
        // 실패 처리
        retry_initialization();
        // 또는
        switch_to_fallback_mode();
    }
}
```

## 메모리 관리

### 자동 메모리 관리

- 동적으로 할당된 명령어 배열은 초기화 완료 시 **자동으로 해제**됩니다
- 콜백 실행 후 컨텍스트 구조체도 자동 해제
- 사용자는 메모리 관리를 신경 쓸 필요 없음

### 메모리 사용량

- 각 초기화 컨텍스트: ~100 bytes
- 명령어 배열: ~1KB (13개 명령 × 64-128 bytes)
- Dual 초기화 컨텍스트: ~32 bytes

## 주의사항

1. **GPS ID 확인**: base1_id와 base2_id는 서로 달라야 함
2. **GPS 인스턴스**: `gps_init_all()` 먼저 호출하여 GPS 인스턴스 생성 필요
3. **콜백 실행**: 콜백은 GPS TX 태스크에서 실행됨 (우선순위 고려)
4. **동시 초기화**: 같은 GPS ID로 여러 초기화를 동시에 실행하지 말 것

## API 레퍼런스

자세한 API 문서는 `gps_app.h` 참조

## 관련 파일

- `modules/gps/gps_app.h` - API 선언
- `modules/gps/gps_app.c` - 구현
- `examples/dual_base_station_example.c` - 예제 코드

## 문의

구현에 대한 질문이나 버그 리포트는 이슈 트래커에 등록해주세요.
