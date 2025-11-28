#include "ntrip_app.h"
#include "FreeRTOS.h"
#include "gps_app.h"
#include "led.h"
#include "queue.h"
#include "task.h"
#include "tcp_socket.h"
#include <string.h>

#ifndef TAG
#define TAG "NTRIP"
#endif

#include "log.h"

// NTRIP 서버 정보
#define NTRIP_SERVER_IP "ntrip.hi-rtk.io"
// #define NTRIP_SERVER_IP "time.nist.gov"
#define NTRIP_SERVER_PORT 2101
// #define NTRIP_SERVER_PORT 13
#define NTRIP_CONNECT_ID 0 // 소켓 ID (0-11)
#define NTRIP_CONTEXT_ID 1 // PDP context ID

#define NTRIP_MAX_CONNECT_RETRY 3
#define NTRIP_MAX_TIMEOUT_COUNT 3 // 연속 타임아웃 최대 허용 횟수
#define NTRIP_RECONNECT_DELAY_MS 500 // 재연결 대기 시간 (ms)

#define NTRIP_GGA_QUEUE_SIZE 15 // GGA 전송 큐 크기 (재연결 중 버퍼링)
#define NTRIP_GGA_MAX_LEN 100  // GGA 문장 최대 길이
#define NTRIP_GGA_SEND_BATCH 5  // 한 루프당 최대 GGA 전송 개수 (GSM 버퍼 보호)

// GGA 전송 큐 아이템
typedef struct {
  char data[NTRIP_GGA_MAX_LEN];
  uint8_t len;
} ntrip_gga_queue_item_t;

// NTRIP HTTP 요청
static const char NTRIP_HTTP_REQUEST[] =
    "GET /RTK_SMT_MSG HTTP/1.0\r\n"
    "User-Agent: NTRIP GUGU SYSTEM\r\n"
    "Accept: */*\r\n"
    "Connection: close\r\n"
    "Authorization: Basic aW90aXoxOjEyMzQ=\r\n"
    "\r\n";

uint8_t recv_buf[1500];

// NTRIP TCP 소켓 (GGA 전송용)
static tcp_socket_t *g_ntrip_socket = NULL;
static bool g_ntrip_connected = false;

// GGA 전송 큐
static QueueHandle_t g_gga_send_queue = NULL;

// GGA 송신 태스크 핸들
static TaskHandle_t g_gga_send_task_handle = NULL;

static int ntrip_connect_to_server(tcp_socket_t *sock) {
  int ret;
  int retry_count = 0;

  while (retry_count < NTRIP_MAX_CONNECT_RETRY) {
    LOG_INFO("NTRIP 서버 연결 시도 [%d/%d]: %s:%d", retry_count + 1,
             NTRIP_MAX_CONNECT_RETRY, NTRIP_SERVER_IP, NTRIP_SERVER_PORT);

    ret = tcp_connect(sock, NTRIP_CONTEXT_ID, NTRIP_SERVER_IP,
                      NTRIP_SERVER_PORT, 10000);

    if (ret == 0 && tcp_get_socket_state(sock, NTRIP_CONNECT_ID) ==
                        GSM_TCP_STATE_CONNECTED) {
      LOG_INFO("TCP 연결 성공");

      // HTTP 요청 전송
      LOG_INFO("NTRIP HTTP 요청 전송");
      ret = tcp_send(sock, (const uint8_t *)NTRIP_HTTP_REQUEST,
                     strlen(NTRIP_HTTP_REQUEST));

      if (ret < 0) {
        LOG_ERR("HTTP 요청 전송 실패: %d", ret);
        tcp_close_force(sock);
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry_count++;
        continue;
      }

      LOG_INFO("HTTP 요청 전송 완료 (%d bytes)", ret);

      // 수신 타임아웃 설정 (5초 - 네트워크 지연 고려)
      tcp_set_recv_timeout(sock, 5000);

      // ICY 200 OK 수신
      ret = tcp_recv(sock, recv_buf, sizeof(recv_buf), 0);
      if (ret > 0) {
        LOG_INFO("서버 응답 수신 (%d bytes)", ret);
        return 0; // 연결 성공
      }
    }

    // 연결 실패 - 강제 닫기 후 재시도
    LOG_WARN("TCP 연결 실패 (ret=%d), 강제 닫기 후 재시도...", ret);
    led_set_color(LED_ID_1, LED_COLOR_YELLOW);
    tcp_close_force(sock);

    // 재시도 전 대기
    vTaskDelay(pdMS_TO_TICKS(1000));
    retry_count++;
  }

  LOG_ERR("TCP 연결 최대 재시도 횟수 초과");

  return -1;
}

/**
 * @brief GGA 송신 전용 태스크
 *
 * GGA 큐에서 데이터를 블로킹 대기하여 즉시 전송
 * - 폴링 없이 이벤트 기반 동작
 * - RTCM 수신과 완전히 독립적으로 동작
 */
static void ntrip_gga_send_task(void *pvParameter) {
  tcp_socket_t *sock = (tcp_socket_t *)pvParameter;
  ntrip_gga_queue_item_t gga_item;

  LOG_INFO("GGA 송신 태스크 시작");

  while (1) {
    // GGA 큐에서 블로킹 대기 (데이터 도착 즉시 깨어남)
    if (xQueueReceive(g_gga_send_queue, &gga_item, portMAX_DELAY) == pdTRUE) {

      // 연결 상태 확인
      if (!g_ntrip_connected) {
        LOG_DEBUG("NTRIP 연결 안 됨, GGA 전송 스킵");
        continue;
      }

      // GGA 전송
      int ret = tcp_send(sock, (const uint8_t *)gga_item.data, gga_item.len);

      if (ret > 0) {
        LOG_INFO("GGA 전송 완료 (%d bytes): %.*s",
                 gga_item.len, gga_item.len, gga_item.data);
      } else {
        LOG_WARN("GGA 전송 실패: %d", ret);
        // 전송 실패해도 계속 진행 (다음 GGA는 다시 시도)
        // 연결이 끊어진 경우 수신 태스크에서 재연결 처리
      }
    }
  }
}

/**
 * @brief NTRIP TCP 수신 태스크
 */
static void ntrip_tcp_recv_task(void *pvParameter) {
  gsm_t *gsm = (gsm_t *)pvParameter;
  tcp_socket_t *sock = NULL;
  gps_t *gps_handle = gps_get_instance_handle(0);

  int ret;
  int timeout_count = 0;   // 연속 타임아웃 카운터
  int reconnect_count = 0; // 총 재연결 시도 횟수

  LOG_INFO("NTRIP 태스크 시작");

  // GGA 전송 큐 생성
  if (!g_gga_send_queue) {
    g_gga_send_queue = xQueueCreate(NTRIP_GGA_QUEUE_SIZE, sizeof(ntrip_gga_queue_item_t));
    if (!g_gga_send_queue) {
      LOG_ERR("Failed to create GGA send queue");
      vTaskDelete(NULL);
      return;
    }
    LOG_INFO("GGA send queue created (size=%d)", NTRIP_GGA_QUEUE_SIZE);
  }

  // TCP 소켓 생성
  sock = tcp_socket_create(gsm, NTRIP_CONNECT_ID);
  if (!sock) {
    LOG_ERR("TCP 소켓 생성 실패");
    vTaskDelete(NULL);
    return;
  }
  LOG_INFO("TCP 소켓 생성 완료");

  // 전역 소켓 저장
  g_ntrip_socket = sock;

  if (ntrip_connect_to_server(sock) != 0) {
    LOG_ERR("초기 연결 실패");
    g_ntrip_connected = false;
    tcp_socket_destroy(sock);
    vTaskDelete(NULL);
    return;
  }

  g_ntrip_connected = true;
  gsm_socket_monitor_start();

  // GGA 송신 태스크 생성
  if (g_gga_send_task_handle == NULL) {
    BaseType_t ret = xTaskCreate(ntrip_gga_send_task, "gga_send", 512, sock,
                                  tskIDLE_PRIORITY + 2, &g_gga_send_task_handle);
    if (ret != pdPASS) {
      LOG_ERR("GGA 송신 태스크 생성 실패 (heap 부족: %d bytes 남음)",
              xPortGetFreeHeapSize());
      // 태스크 생성 실패해도 수신은 계속 동작
    } else {
      LOG_INFO("GGA 송신 태스크 생성 완료 (heap 남음: %d bytes)",
               xPortGetFreeHeapSize());
    }
  }

  // HTTP 요청 전송 (한 번만)
  LOG_INFO("NTRIP HTTP 요청 전송");
  ret = tcp_send(sock, (const uint8_t *)NTRIP_HTTP_REQUEST,
                 strlen(NTRIP_HTTP_REQUEST));
  if (ret < 0) {
    LOG_ERR("HTTP 요청 전송 실패: %d", ret);
    tcp_close(sock);
    tcp_socket_destroy(sock);
    vTaskDelete(NULL);
    return;
  }

  LOG_INFO("HTTP 요청 전송 완료 (%d bytes)", ret);

  // ICY 200 OK\r\n\r\n 수신
  ret = tcp_recv(sock, recv_buf, sizeof(recv_buf), 0);
  led_set_color(1, LED_COLOR_GREEN);

  // 무한 루프: 데이터 수신 (GGA 전송은 별도 태스크에서 처리)
  while (1) {
    // RTCM 데이터 수신 (기본 타임아웃 5초 사용)
    // GGA 전송은 별도 태스크가 독립적으로 처리하므로 여기서는 신경 쓸 필요 없음
    ret = tcp_recv(sock, recv_buf, sizeof(recv_buf), 0);

    if (ret > 0) {
      // 수신 성공
      timeout_count = 0;
      LOG_INFO("수신 데이터 (%d bytes):", ret);

      // 데이터를 16진수로 출력
      for (int i = 0; i < ret; i += 16) {
        char hex_str[64] = {0};
        char ascii_str[20] = {0};
        int line_len = (ret - i) > 16 ? 16 : (ret - i);

        // 16진수 문자열 생성
        for (int j = 0; j < line_len; j++) {
          sprintf(&hex_str[j * 3], "%02X ", recv_buf[i + j]);

          // ASCII 출력용 (출력 가능한 문자만)
          if (recv_buf[i + j] >= 0x20 && recv_buf[i + j] <= 0x7E) {
            ascii_str[j] = recv_buf[i + j];
          } else {
            ascii_str[j] = '.';
          }
        }

        LOG_INFO("  %04X: %-48s | %s", i, hex_str, ascii_str);
      }
      gps_handle->ops->send((const char *)recv_buf, ret);
    } else if (ret == 0) {
      // 타임아웃
      timeout_count++;
      LOG_WARN("수신 타임아웃 (%d/%d)", timeout_count, NTRIP_MAX_TIMEOUT_COUNT);

      // 소켓 상태 확인
      gsm_tcp_state_t state = tcp_get_socket_state(sock, NTRIP_CONNECT_ID);
      if (state == GSM_TCP_STATE_CLOSING || state == GSM_TCP_STATE_CLOSED) {
        LOG_ERR("소켓 상태 비정상 (state=%d), 재연결 필요", state);
        timeout_count = NTRIP_MAX_TIMEOUT_COUNT; // 즉시 재연결
      }

      // 연속 타임아웃 최대 횟수 초과 시 재연결
      if (timeout_count >= NTRIP_MAX_TIMEOUT_COUNT) {
        // 재연결 시작 전 큐 상태 확인
        UBaseType_t queued_gga = uxQueueMessagesWaiting(g_gga_send_queue);
        LOG_WARN("연속 타임아웃 발생 또는 소켓 끊김, 재연결 시도... (큐에 GGA %d개 대기중)", queued_gga);
        led_set_color(1, LED_COLOR_YELLOW);

        // ★ 연결 상태만 false로 설정 (태스크는 살려둠)
        // GGA 송신 태스크가 g_ntrip_connected를 확인하여 전송 스킵
        g_ntrip_connected = false;

        // 기존 연결 닫기
        tcp_close_force(sock);
        vTaskDelay(pdMS_TO_TICKS(NTRIP_RECONNECT_DELAY_MS));

        // 재연결 시도
        if (ntrip_connect_to_server(sock) != 0) {
          reconnect_count++;
          LOG_ERR("재연결 실패 (%d회)", reconnect_count);

          // 재연결 실패 시 더 긴 대기
          vTaskDelay(pdMS_TO_TICKS(NTRIP_RECONNECT_DELAY_MS * 2));
        } else {
          LOG_INFO("재연결 성공");
          led_set_color(1, LED_COLOR_GREEN);
          timeout_count = 0; // 타임아웃 카운터 리셋

          // 재연결 중 쌓인 오래된 GGA 버리기 (최신 1개만 유지)
          UBaseType_t queued_gga = uxQueueMessagesWaiting(g_gga_send_queue);
          if (queued_gga > 1) {
            LOG_INFO("재연결 완료 - 오래된 GGA %d개 드롭, 최신 1개만 전송", queued_gga - 1);

            // 오래된 데이터 모두 제거
            ntrip_gga_queue_item_t old_item;
            while (queued_gga > 1) {
              xQueueReceive(g_gga_send_queue, &old_item, 0);
              queued_gga--;
            }
          } else if (queued_gga == 1) {
            LOG_INFO("재연결 완료 - 최신 GGA 1개 전송 예정");
          }

          // ★ 연결 상태 true로 복원 (태스크는 계속 실행 중)
          // GGA 송신 태스크가 다시 정상 전송 시작
          g_ntrip_connected = true;
        }
      }
    } else {
      // 에러
      LOG_ERR("수신 에러: %d", ret);

      // 소켓 상태 확인
      gsm_tcp_state_t state = tcp_get_socket_state(sock, NTRIP_CONNECT_ID);
      LOG_ERR("현재 소켓 상태: %d", state);

      // 에러 발생 시 재연결 시도
      LOG_WARN("에러 발생, 재연결 시도...");
      led_set_color(1, LED_COLOR_YELLOW);

      // ★ 연결 상태만 false로 설정 (태스크는 살려둠)
      g_ntrip_connected = false;

      tcp_close_force(sock);
      vTaskDelay(pdMS_TO_TICKS(NTRIP_RECONNECT_DELAY_MS));

      if (ntrip_connect_to_server(sock) != 0) {
        reconnect_count++;
        LOG_ERR("재연결 실패 (%d회)", reconnect_count);
        vTaskDelay(pdMS_TO_TICKS(NTRIP_RECONNECT_DELAY_MS * 2));
      } else {
        LOG_INFO("재연결 성공");
        timeout_count = 0;
        led_set_color(1, LED_COLOR_GREEN);

        // 재연결 중 쌓인 오래된 GGA 버리기 (최신 1개만 유지)
        UBaseType_t queued_gga = uxQueueMessagesWaiting(g_gga_send_queue);
        if (queued_gga > 1) {
          LOG_INFO("재연결 완료 - 오래된 GGA %d개 드롭, 최신 1개만 전송", queued_gga - 1);

          // 오래된 데이터 모두 제거
          ntrip_gga_queue_item_t old_item;
          while (queued_gga > 1) {
            xQueueReceive(g_gga_send_queue, &old_item, 0);
            queued_gga--;
          }
        } else if (queued_gga == 1) {
          LOG_INFO("재연결 완료 - 최신 GGA 1개 전송 예정");
        }

        // ★ 연결 상태 true로 복원 (태스크는 계속 실행 중)
        g_ntrip_connected = true;
      }
    }
  }

  // 연결 종료
  LOG_INFO("TCP 연결 종료");

  // GGA 송신 태스크 삭제
  if (g_gga_send_task_handle != NULL) {
    vTaskDelete(g_gga_send_task_handle);
    g_gga_send_task_handle = NULL;
    LOG_INFO("GGA 송신 태스크 삭제");
  }

  tcp_close(sock);
  tcp_socket_destroy(sock);

  vTaskDelete(NULL);
}

void ntrip_task_create(gsm_t *gsm) {
  xTaskCreate(ntrip_tcp_recv_task, "ntrip_recv", 2048, gsm,
              tskIDLE_PRIORITY + 3, NULL);
}

int ntrip_send_gga_data(const char *data, uint8_t len) {
  if (!data || len == 0 || len >= NTRIP_GGA_MAX_LEN) {
    LOG_ERR("Invalid GGA data (len=%d)", len);
    return -1;
  }

  if (!g_gga_send_queue) {
    LOG_WARN("GGA send queue not initialized");
    return -2;
  }

  // 큐에 넣을 아이템 생성
  ntrip_gga_queue_item_t item;
  memcpy(item.data, data, len);
  item.data[len] = '\0';
  item.len = len;

  // 큐에 넣기 (블로킹 안 함, 큐가 가득 차면 실패)
  if (xQueueSend(g_gga_send_queue, &item, 0) != pdTRUE) {
    // 큐가 가득 참 - 오래된 데이터 하나 제거하고 다시 시도
    ntrip_gga_queue_item_t old_item;
    xQueueReceive(g_gga_send_queue, &old_item, 0);

    if (xQueueSend(g_gga_send_queue, &item, 0) != pdTRUE) {
      LOG_WARN("Failed to queue GGA data (queue full)");
      return -3;
    }
    LOG_DEBUG("GGA queue was full, dropped oldest item");
  }

  return len;
}
