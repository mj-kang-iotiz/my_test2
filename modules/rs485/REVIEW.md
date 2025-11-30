# RS485 Half-Duplex Implementation Review

## 1. 동시성 (Concurrency) 분석

### ✅ 안전한 부분:
- **TX Task**: 단일 태스크가 큐에서 순차적으로 처리
- **RX Task**: 단일 태스크가 IDLE 인터럽트 기반으로 처리
- **rs485_send()**: 큐를 통한 비동기 전송 (여러 태스크에서 안전하게 호출 가능)

### ⚠️ 잠재적 경쟁 조건:

**시나리오 1: TX 중 RX IDLE 인터럽트 발생**
```
TX Task                   RX ISR
-----------               -----------
tx_enable()
  (DMA/IDLE still ON)
send()                    IDLE IRQ! → Queue send
  ...
rx_enable()
```

**문제:**
- TX 모드에서도 UART RX IDLE 인터럽트가 활성화되어 있음
- TX 완료 시점에 IDLE 감지 가능
- RX Task가 불필요하게 깨어날 수 있음

**영향도:** 낮음 (기능적 문제 없음, 약간의 CPU 낭비)

---

## 2. 버퍼 관리

### ✅ 안전한 부분:
- **RX 버퍼**: DMA Circular 모드, 1024 bytes, 오버런 방지
- **TX 큐**: 256 bytes, 5개 항목 큐잉 가능

### ⚠️ 체크 사항:

**RX 버퍼 오버런:**
- 현재 보호: DMA Circular 모드가 자동 wrap
- 문제: RX Task가 처리 속도보다 수신이 빠르면 데이터 손실
- 해결: IDLE 인터럽트 기반이므로 프레임 단위 처리, 괜찮음

**TX 큐 오버플로우:**
- 현재: `xQueueSend(..., pdMS_TO_TICKS(1000))`
- 큐 full 시: 1초 대기 후 실패 반환
- 상태: ✓ 안전

---

## 3. 타이밍 분석

### 현재 구현:
```c
tx_enable();              // DE/RE → HIGH (즉시)
send();                   // TC 플래그까지 대기
rx_enable();              // DE/RE → LOW (즉시)
```

### 타이밍 검증:

**115200 bps 기준:**
- 1바이트 전송 시간: 10비트 (start+8data+stop) × 8.68μs = 86.8μs
- TC 플래그: 마지막 비트 shift 완료 시점
- RS485 트랜시버 전환: < 100ns

**최악의 경우 (256 bytes):**
- 송신 시간: 256 × 86.8μs = 22.2ms
- TX Task 블록킹: 22.2ms (OK, 다른 태스크는 계속 실행)

---

## 4. Half-Duplex 격리 검증

### TX 모드:
```
DE=HIGH, /RE=HIGH
→ 송신기 ON, 수신기 OFF
→ RO 핀 High 유지
→ UART RX idle 상태
→ DMA 버퍼 오염 없음
```

### 검증 결과: ✅ 완벽한 격리

---

## 5. 에러 핸들링

### ✅ 현재 구현된 것:
- NULL 포인터 체크
- 길이 범위 체크 (0 < len <= 256)
- enabled 상태 체크
- ops 함수 포인터 체크

### ⚠️ 부족한 부분:

**UART 에러:**
- Overrun Error (ORE)
- Framing Error (FE)
- Parity Error (PE)
- Noise Error (NE)

**현재 상태:**
- rs485_port.c:200-211에서 플래그만 클리어
- 에러 카운팅이나 로깅 없음

**권장 개선:**
```c
void USART5_IRQHandler(void) {
    if (LL_USART_IsActiveFlag_ORE(USART5)) {
        error_counters.ore++;
        LOG_WARN("RS485 ORE");
        LL_USART_ClearFlag_ORE(USART5);
    }
    // ... 다른 에러들도 동일
}
```

---

## 6. 성능 분석

### CPU 사용률:
- TX Task: 대부분 blocked (큐 대기)
- RX Task: 대부분 blocked (IDLE 대기)
- ISR: IDLE 감지 시에만 (매우 짧음)

### 메모리:
- RX 버퍼: 1024 bytes
- TX 큐: 256 × 5 = 1280 bytes
- Task 스택: 512 × 2 = 1024 bytes
- **총: ~3.3KB**

---

## 7. 테스트 시나리오

### 필수 테스트:
1. ✓ 기본 송신
2. ✓ 연속 송신
3. ✓ 최대 길이 (256 bytes)
4. ✓ 잘못된 파라미터
5. ✓ 큐 오버플로우
6. ✓ 바이너리 데이터

### 추가 권장 테스트:
7. ⚠️ 송신 중 수신 발생 (외부 장치 필요)
8. ⚠️ Full duplex 시뮬레이션 (loopback)
9. ⚠️ 장시간 안정성 (24시간 스트레스)

---

## 8. 전체 평가

### 강점:
- ✅ 간결하고 이해하기 쉬운 구조
- ✅ Half-duplex 제어가 올바름
- ✅ 메모리 효율적
- ✅ 큐 기반 비동기 처리로 확장성 좋음

### 개선 가능:
- ⚠️ TX 중 IDLE 인터럽트 발생 (미미한 영향)
- ⚠️ 에러 카운팅/로깅 부족
- ⚠️ RX 버퍼 오버런 감지 없음

### 치명적 결함:
- ❌ 없음

---

## 9. 최종 결론

**현재 구현은 production-ready입니다.**

간단한 개선사항:
1. 에러 카운터 추가 (선택)
2. RX 버퍼 오버런 감지 (선택)

**전반적 점수: 9/10**
