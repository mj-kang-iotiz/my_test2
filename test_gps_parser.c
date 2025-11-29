#include "lib/gps/gps.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

// 테스트용 이벤트 핸들러
typedef struct {
  int nmea_count;
  int ubx_count;
  int rtcm_count;
  uint16_t last_rtcm_msg_type;
} test_stats_t;

test_stats_t stats = {0};

void test_evt_handler(gps_t *gps, gps_event_t event, gps_procotol_t protocol, gps_msg_t msg) {
  if (event == GPS_EVENT_DATA_PARSED) {
    switch (protocol) {
      case GPS_PROTOCOL_NMEA:
        stats.nmea_count++;
        printf("✓ NMEA parsed (count: %d)\n", stats.nmea_count);
        break;
      case GPS_PROTOCOL_UBX:
        stats.ubx_count++;
        printf("✓ UBX parsed (count: %d, class: 0x%02X, id: 0x%02X)\n",
               stats.ubx_count, msg.ubx.class, msg.ubx.id);
        break;
      case GPS_PROTOCOL_RTCM:
        stats.rtcm_count++;
        stats.last_rtcm_msg_type = msg.rtcm.msg_type;
        printf("✓ RTCM parsed (count: %d, msg_type: %d)\n",
               stats.rtcm_count, msg.rtcm.msg_type);
        break;
      default:
        break;
    }
  }
}

// 테스트 케이스 실행
void run_test(const char *name, uint8_t *data, size_t len, gps_t *gps) {
  printf("\n=== Test: %s ===\n", name);
  memset(&stats, 0, sizeof(stats));

  // 데이터 파싱
  gps_parse_process(gps, data, len);

  printf("Results: NMEA=%d, UBX=%d, RTCM=%d\n",
         stats.nmea_count, stats.ubx_count, stats.rtcm_count);
}

int main() {
  gps_t gps;
  gps_init(&gps);
  gps_set_evt_handler(&gps, test_evt_handler);

  printf("GPS Protocol Parser Test Suite\n");
  printf("================================\n");

  // 테스트 1: RTCM 단독 (정상 케이스)
  {
    uint8_t rtcm_1005[] = {
      0xD3, 0x00, 0x13,  // Header: preamble + length (19 bytes)
      0x3E, 0xD0,        // Message type 1005 (0x3ED << 4 | 0x0)
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xAB, 0xCD, 0xEF   // CRC-24Q (dummy)
    };
    run_test("RTCM 1005 단독", rtcm_1005, sizeof(rtcm_1005), &gps);
  }

  // 테스트 2: 0xB5 다음 RTCM (문제 시나리오)
  {
    uint8_t mixed_data[] = {
      0xB5,              // UBX sync1 후보 (하지만 UBX 아님)
      0xD3, 0x00, 0x13,  // RTCM 시작
      0x3E, 0xD0,        // Message type 1005
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xAB, 0xCD, 0xEF
    };
    run_test("0xB5 → RTCM (바이트 손실 방지 테스트)", mixed_data, sizeof(mixed_data), &gps);
  }

  // 테스트 3: NMEA 단독
  {
    char nmea[] = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    run_test("NMEA GGA", (uint8_t*)nmea, strlen(nmea), &gps);
  }

  // 테스트 4: RTCM → NMEA (프로토콜 전환)
  {
    uint8_t rtcm_then_nmea[256];
    int pos = 0;

    // RTCM 1077
    uint8_t rtcm[] = {0xD3, 0x00, 0x13, 0x43, 0x50,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x11, 0x22, 0x33};
    memcpy(rtcm_then_nmea + pos, rtcm, sizeof(rtcm));
    pos += sizeof(rtcm);

    // NMEA
    char nmea[] = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";
    memcpy(rtcm_then_nmea + pos, nmea, strlen(nmea));
    pos += strlen(nmea);

    run_test("RTCM 1077 → NMEA RMC", rtcm_then_nmea, pos, &gps);
  }

  // 테스트 5: 여러 프로토콜 연속 (복합 테스트)
  {
    uint8_t complex[512];
    int pos = 0;

    // 1. NMEA
    char nmea1[] = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    memcpy(complex + pos, nmea1, strlen(nmea1));
    pos += strlen(nmea1);

    // 2. RTCM 1005
    uint8_t rtcm1[] = {0xD3, 0x00, 0x13, 0x3E, 0xD0,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0xAA, 0xBB, 0xCC};
    memcpy(complex + pos, rtcm1, sizeof(rtcm1));
    pos += sizeof(rtcm1);

    // 3. 잘못된 UBX 시작 (0xB5만 있고 다음이 RTCM)
    complex[pos++] = 0xB5;

    // 4. RTCM 1077
    uint8_t rtcm2[] = {0xD3, 0x00, 0x13, 0x43, 0x50,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x11, 0x22, 0x33};
    memcpy(complex + pos, rtcm2, sizeof(rtcm2));
    pos += sizeof(rtcm2);

    // 5. NMEA
    char nmea2[] = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";
    memcpy(complex + pos, nmea2, strlen(nmea2));
    pos += strlen(nmea2);

    run_test("복합: NMEA → RTCM → 0xB5 → RTCM → NMEA", complex, pos, &gps);
  }

  // 테스트 6: 0xAA 다음 RTCM (UNICORE 오감지)
  {
    uint8_t unicore_false[] = {
      0xAA,              // UNICORE sync1 후보
      0xD3, 0x00, 0x13,  // RTCM 시작
      0x3E, 0xD0,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xAB, 0xCD, 0xEF
    };
    run_test("0xAA → RTCM (UNICORE 오감지)", unicore_false, sizeof(unicore_false), &gps);
  }

  printf("\n================================\n");
  printf("All tests completed!\n");

  return 0;
}
