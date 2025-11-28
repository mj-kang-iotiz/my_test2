#include "gps_app.h"

#ifndef TAG
#define TAG "GPS"
#endif

#include "log.h"

#include "board_config.h"
#include "gps.h"
#include "gps_port.h"
#include "gps_unicore.h"
#include "led.h"
#include "ntrip_app.h"
#include <string.h>

#define GPS_UART_MAX_RECV_SIZE 2048

#define GGA_AVG_SIZE 50
#define HP_AVG_SIZE 50

typedef struct {
  int32_t lon[HP_AVG_SIZE];
  int32_t lat[HP_AVG_SIZE];
  int32_t height[HP_AVG_SIZE];
  int32_t msl[HP_AVG_SIZE];
  int8_t lon_hp[HP_AVG_SIZE];
  int8_t lat_hp[HP_AVG_SIZE];
  int8_t height_hp[HP_AVG_SIZE];
  int8_t msl_hp[HP_AVG_SIZE];
  uint32_t hacc;
  uint32_t vacc;
  double lon_avg;
  double lat_avg;
  double height_avg;
  double msl_avg;
  uint8_t pos;
  uint8_t len;
  bool can_read;
} ubx_hp_avg_data_t;

typedef struct {
  gps_t gps;
  QueueHandle_t queue;
  TaskHandle_t task;
  gps_type_t type;
  gps_id_t id;
  bool enabled;

  ubx_hp_avg_data_t ubx_hp_avg;

  struct {
    double lat[GGA_AVG_SIZE];
    double lon[GGA_AVG_SIZE];
    double alt[GGA_AVG_SIZE];
    double lat_avg;
    double lon_avg;
    double alt_avg;
    uint8_t pos;
    uint8_t len;
    bool can_read;
  } gga_avg_data;

   QueueHandle_t cmd_queue;
  TaskHandle_t tx_task;
  gps_cmd_request_t *current_cmd_req;
} gps_instance_t;

static gps_instance_t gps_instances[GPS_ID_MAX] = {0};

void _add_gga_avg_data(gps_instance_t *inst, double lat, double lon,
                       double alt) {
  uint8_t pos = inst->gga_avg_data.pos;
  double lat_temp = 0.0, lon_temp = 0.0, alt_temp = 0.0;

  inst->gga_avg_data.lat[pos] = lat;
  inst->gga_avg_data.lon[pos] = lon;
  inst->gga_avg_data.alt[pos] = alt;

  inst->gga_avg_data.pos = (inst->gga_avg_data.pos + 1) % GGA_AVG_SIZE;

  /* 정확도를 중시한 코드 */
  if (inst->gga_avg_data.len < GGA_AVG_SIZE) {
    inst->gga_avg_data.len++;

    if (inst->gga_avg_data.len == GGA_AVG_SIZE) {

      for (int i = 0; i < GGA_AVG_SIZE; i++) {
        lat_temp += inst->gga_avg_data.lat[i];
        lon_temp += inst->gga_avg_data.lon[i];
        alt_temp += inst->gga_avg_data.alt[i];
      }

      inst->gga_avg_data.lat_avg = lat_temp / (double)GGA_AVG_SIZE;
      inst->gga_avg_data.lon_avg = lon_temp / (double)GGA_AVG_SIZE;
      inst->gga_avg_data.alt_avg = alt_temp / (double)GGA_AVG_SIZE;

      inst->gga_avg_data.can_read = true;
    }
  } else {
    for (int i = 0; i < GGA_AVG_SIZE; i++) {
      lat_temp += inst->gga_avg_data.lat[i];
      lon_temp += inst->gga_avg_data.lon[i];
      alt_temp += inst->gga_avg_data.alt[i];
    }

    inst->gga_avg_data.lat_avg = lat_temp / (double)GGA_AVG_SIZE;
    inst->gga_avg_data.lon_avg = lon_temp / (double)GGA_AVG_SIZE;
    inst->gga_avg_data.alt_avg = alt_temp / (double)GGA_AVG_SIZE;
  }
}

void _add_hp_avg_data(gps_instance_t *inst) {
  gps_t *gps = &inst->gps;
  uint8_t pos = inst->ubx_hp_avg.pos;
  gps_ubx_nav_hpposllh_t *data = &gps->ubx_data.hpposllh;
  ubx_hp_avg_data_t *avg_data = &inst->ubx_hp_avg;

  int64_t lat_sum = 0, lon_sum = 0, height_sum = 0, msl_sum = 0;
  int16_t lat_hp_sum = 0, lon_hp_sum = 0, height_hp_sum = 0, msl_hp_sum = 0;

  avg_data->lon[pos] = data->lon;
  avg_data->lat[pos] = data->lat;
  avg_data->height[pos] = data->height;
  avg_data->msl[pos] = data->msl;
  avg_data->lon_hp[pos] = data->lon_hp;
  avg_data->lat_hp[pos] = data->lat_hp;
  avg_data->height_hp[pos] = data->height_hp;
  avg_data->msl_hp[pos] = data->msl_hp;
  avg_data->hacc = data->hacc;
  avg_data->vacc = data->vacc;

  avg_data->pos = (avg_data->pos + 1) % HP_AVG_SIZE;

  if (avg_data->len < HP_AVG_SIZE) {
    avg_data->len++;

    if (avg_data->len == HP_AVG_SIZE) {
      for (int i = 0; i < HP_AVG_SIZE; i++) {
        lon_sum += avg_data->lon[i];
        lat_sum += avg_data->lat[i];
        height_sum += avg_data->height[i];
        msl_sum += avg_data->msl[i];
        lon_hp_sum += avg_data->lon_hp[i];
        lat_hp_sum += avg_data->lat_hp[i];
        height_hp_sum += avg_data->height_hp[i];
        msl_hp_sum += avg_data->msl_hp[i];
      }

      avg_data->lon_avg = (lon_sum / (double)HP_AVG_SIZE) +
                          (lon_hp_sum / (double)HP_AVG_SIZE / (double)100);
      avg_data->lat_avg = (lat_sum / (double)HP_AVG_SIZE) +
                          (lat_hp_sum / (double)HP_AVG_SIZE / (double)100);
      avg_data->height_avg = (height_sum / (double)HP_AVG_SIZE) +
                             (height_hp_sum / (double)HP_AVG_SIZE / (double)10);
      avg_data->msl_avg = (msl_sum / (double)HP_AVG_SIZE) +
                          (msl_hp_sum / (double)HP_AVG_SIZE / (double)10);

      avg_data->can_read = true;
    }
  } else if (avg_data->len == HP_AVG_SIZE) {
    for (int i = 0; i < HP_AVG_SIZE; i++) {
      lon_sum += avg_data->lon[i];
      lat_sum += avg_data->lat[i];
      height_sum += avg_data->height[i];
      msl_sum += avg_data->msl[i];
      lon_hp_sum += avg_data->lon_hp[i];
      lat_hp_sum += avg_data->lat_hp[i];
      height_hp_sum += avg_data->height_hp[i];
      msl_hp_sum += avg_data->msl_hp[i];
    }

    avg_data->lon_avg = (lon_sum / (double)HP_AVG_SIZE) +
                        (lon_hp_sum / (double)HP_AVG_SIZE / (double)100);
    avg_data->lat_avg = (lat_sum / (double)HP_AVG_SIZE) +
                        (lat_hp_sum / (double)HP_AVG_SIZE / (double)100);
    avg_data->height_avg = (height_sum / (double)HP_AVG_SIZE) +
                           (height_hp_sum / (double)HP_AVG_SIZE / (double)10);
    avg_data->msl_avg = (msl_sum / (double)HP_AVG_SIZE) +
                        (msl_hp_sum / (double)HP_AVG_SIZE / (double)10);
  } else {
    LOG_ERR("HP AVG LEN mismatch");
  }
}

#define GPS_INIT_MAX_RETRY 3  // 최대 재시도 횟수
#define GPS_INIT_TIMEOUT_MS 1000  // 명령어 타임아웃 (1초)

#define UM982_BASE_CMD_COUNT (sizeof(um982_base_cmds) / sizeof(um982_base_cmds[0]))

static const char *um982_base_cmds[] = {
    "MODE BASE TIME 60\r\n",
	"unmask BDS\r\n",
    "rtcm1033 com1 10\r\n",
    "rtcm1006 com1 10\r\n",
    "rtcm1074 com1 1\r\n",
    "rtcm1124 com1 1\r\n",
    "rtcm1084 com1 1\r\n",
    "rtcm1094 com1 1\r\n",
    "gpgga com1 1\r\n",
    "BESTNAVB 1\r\n",
};

static const char *um982_rover_cmds[] = {
  "MODE ROVER\r\n",
  "unmask BDS\r\n",
  "GNGGA 1\r\n",
  "BESTNAVB 1\r\n",
};

#define UM982_ROVER_CMD_COUNT (sizeof(um982_rover_cmds) / sizeof(um982_rover_cmds[0]))

typedef void (*gps_init_callback_t)(bool success, void *user_data);

typedef struct {
  gps_id_t gps_id;              // GPS ID
  uint8_t current_step;         // 현재 단계 (0 ~ cmd_count-1)
  uint8_t retry_count;          // 현재 단계 재시도 횟수
  const char **cmd_list;        // 명령어 리스트
  uint8_t cmd_count;            // 명령어 개수
  gps_init_callback_t callback; // 완료 콜백
} gps_init_context_t;


static void overall_init_complete(bool success, void *user_data) {
  gps_id_t id = (gps_id_t)(uintptr_t)user_data;
  LOG_INFO("GPS[%d] Overall init %s", id, success ? "succeeded" : "failed");
  // 여기에 init_state 설정이나 다른 시스템 알림 추가 가능
}

static void gps_init_command_callback(bool success, void *user_data) {
  gps_init_context_t *ctx = (gps_init_context_t *)user_data;

  if (!ctx) {
    LOG_ERR("GPS init context is NULL");
    return;
  }

  if (success) {
    // 명령어 성공
    LOG_INFO("GPS[%d] Init step %d/%d OK: %s",
             ctx->gps_id, ctx->current_step + 1, ctx->cmd_count,
             ctx->cmd_list[ctx->current_step]);

    // 다음 단계로
    ctx->current_step++;
    ctx->retry_count = 0;

    // 모든 단계 완료?
    if (ctx->current_step >= ctx->cmd_count) {
      LOG_INFO("GPS[%d] Init sequence complete!", ctx->gps_id);
      if (ctx->callback) {
        ctx->callback(true, NULL);
      }
      // 컨텍스트 메모리 해제
      vPortFree(ctx);
      return;
    }

    // 다음 명령어 전송
    gps_send_command_async(ctx->gps_id, ctx->cmd_list[ctx->current_step],
                           GPS_INIT_TIMEOUT_MS, gps_init_command_callback, ctx);
  } else {
    // 명령어 실패
    ctx->retry_count++;

    if (ctx->retry_count < GPS_INIT_MAX_RETRY) {
      // 재시도
      LOG_WARN("GPS[%d] Init step %d/%d failed, retrying (%d/%d): %s",
               ctx->gps_id, ctx->current_step + 1, ctx->cmd_count,
               ctx->retry_count, GPS_INIT_MAX_RETRY,
               ctx->cmd_list[ctx->current_step]);

      // 같은 명령어 재전송
      gps_send_command_async(ctx->gps_id, ctx->cmd_list[ctx->current_step],
                             GPS_INIT_TIMEOUT_MS, gps_init_command_callback, ctx);
    } else {

      // 최대 재시도 초과
      LOG_ERR("GPS[%d] Init failed at step %d/%d after %d retries: %s",
              ctx->gps_id, ctx->current_step + 1, ctx->cmd_count,
              GPS_INIT_MAX_RETRY, ctx->cmd_list[ctx->current_step]);

      if (ctx->callback) {
        ctx->callback(false, NULL);
      }
      // 컨텍스트 메모리 해제
      vPortFree(ctx);
    }
  }
}



/**

 * @brief GPS UM982 Base 모드 초기화 (비동기)

 */

bool gps_init_um982_base_async(gps_id_t id, gps_init_callback_t callback) {
  if (id >= GPS_ID_MAX || !gps_instances[id].enabled) {
    LOG_ERR("GPS[%d] invalid or disabled", id);
    return false;
  }

  // 초기화 컨텍스트 생성 (동적 할당)
  gps_init_context_t *ctx = (gps_init_context_t *)pvPortMalloc(sizeof(gps_init_context_t));
  if (!ctx) {
    LOG_ERR("GPS[%d] failed to allocate init context", id);
    return false;
  }

  // 컨텍스트 초기화
  ctx->gps_id = id;
  ctx->current_step = 0;
  ctx->retry_count = 0;
  ctx->cmd_list = um982_base_cmds;
  ctx->cmd_count = UM982_BASE_CMD_COUNT;
  ctx->callback = callback;

  LOG_INFO("GPS[%d] Starting UM982 base init sequence (%d commands)",
           id, ctx->cmd_count);

  // 첫 번째 명령어 전송
  if (!gps_send_command_async(id, ctx->cmd_list[0], GPS_INIT_TIMEOUT_MS,
                               gps_init_command_callback, ctx)) {
    LOG_ERR("GPS[%d] failed to start init sequence", id);
    vPortFree(ctx);
    return false;
  }

  return true;
}

/**
 * @brief GPS UM982 Rover 모드 초기화 (비동기)
 */
bool gps_init_um982_rover_async(gps_id_t id, gps_init_callback_t callback) {
  if (id >= GPS_ID_MAX || !gps_instances[id].enabled) {
    LOG_ERR("GPS[%d] invalid or disabled", id);
    return false;
  }

  // 초기화 컨텍스트 생성 (동적 할당)
  gps_init_context_t *ctx = (gps_init_context_t *)pvPortMalloc(sizeof(gps_init_context_t));
  if (!ctx) {
    LOG_ERR("GPS[%d] failed to allocate init context", id);
    return false;
  }

  // 컨텍스트 초기화
  ctx->gps_id = id;
  ctx->current_step = 0;
  ctx->retry_count = 0;
  ctx->cmd_list = um982_rover_cmds;
  ctx->cmd_count = UM982_ROVER_CMD_COUNT;
  ctx->callback = callback;

  LOG_INFO("GPS[%d] Starting UM982 rover init sequence (%d commands)",
           id, ctx->cmd_count);

  // 첫 번째 명령어 전송
  if (!gps_send_command_async(id, ctx->cmd_list[0], GPS_INIT_TIMEOUT_MS,
                               gps_init_command_callback, ctx)) {
    LOG_ERR("GPS[%d] failed to start init sequence", id);
    vPortFree(ctx);
    return false;
  }

  return true;
}

void gps_evt_handler(gps_t *gps, gps_event_t event, gps_procotol_t protocol,
                     gps_msg_t msg) {
  gps_instance_t *inst = NULL;
  for (uint8_t i = 0; i < GPS_CNT; i++) {
    if (gps_instances[i].enabled && &gps_instances[i].gps == gps) {
      inst = &gps_instances[i];
      break;
    }
  }

  if (!inst)
    return;

  switch (protocol) {
  case GPS_PROTOCOL_NMEA:
    if (msg.nmea == GPS_NMEA_MSG_GGA) {
      if (gps->nmea_data.gga.fix >= GPS_FIX_GPS) {
        _add_gga_avg_data(inst, gps->nmea_data.gga.lat, gps->nmea_data.gga.lon,
                          gps->nmea_data.gga.alt);
      }

      // GGA raw 데이터를 NTRIP 서버로 전송
      // ★ get_gga() 호출 시 뮤텍스 재획득으로 데드락 발생하므로
      // 이미 파싱된 데이터를 직접 사용 (이벤트 핸들러 호출 시점에 이미 유효)
      if (gps->nmea_data.gga_is_rdy) {
        ntrip_send_gga_data(gps->nmea_data.gga_raw, gps->nmea_data.gga_raw_pos);
      }
    }
    break;

  case GPS_PROTOCOL_UBX:
    if (msg.ubx.id == GPS_UBX_NAV_ID_HPPOSLLH) {
      if (gps->nmea_data.gga.fix >= GPS_FIX_GPS) {
        _add_hp_avg_data(inst);
      }
    }

  case GPS_PROTOCOL_UNICORE:
    if (inst->current_cmd_req != NULL) {
      gps_unicore_resp_t resp = gps_get_unicore_response(gps);
      if (resp == GPS_UNICORE_RESP_OK) {
        if (inst->current_cmd_req->is_async) {
          inst->current_cmd_req->async_result = true;
        } else {
          *(inst->current_cmd_req->result) = true;
        }
        xSemaphoreGive(inst->current_cmd_req->response_sem);
      } else if (resp == GPS_UNICORE_RESP_ERROR || resp == GPS_UNICORE_RESP_UNKNOWN) {
        if (inst->current_cmd_req->is_async) {
          inst->current_cmd_req->async_result = false;
        } else {
          *(inst->current_cmd_req->result) = false;
        }
        xSemaphoreGive(inst->current_cmd_req->response_sem);
      }
    }

    break;

  default:
    break;
  }
}

static void gps_tx_task(void *pvParameter) {
  gps_id_t id = (gps_id_t)(uintptr_t)pvParameter;
  gps_instance_t *inst = &gps_instances[id];
  gps_cmd_request_t cmd_req;

  LOG_INFO("GPS TX Task[%d] started", id);

  while (1) {
    if (xQueueReceive(inst->cmd_queue, &cmd_req, portMAX_DELAY) == pdTRUE) {
      LOG_INFO("GPS[%d] Sending command: %s", id, cmd_req.cmd);

      // 현재 명령어 요청 저장 (RX Task에서 응답 처리용)
      inst->current_cmd_req = &cmd_req;
      if (cmd_req.is_async) {
        cmd_req.async_result = false;
      } else {
        *(cmd_req.result) = false;
      }

      // 명령어 전송
      if (inst->gps.ops && inst->gps.ops->send) {
        inst->gps.ops->send(cmd_req.cmd, strlen(cmd_req.cmd));
      } else {
        LOG_ERR("GPS[%d] send ops not available", id);
        inst->current_cmd_req = NULL;
        if (cmd_req.is_async) {
          cmd_req.async_result = false;
          if (cmd_req.callback) {
            cmd_req.callback(false, cmd_req.user_data);
          }
          vSemaphoreDelete(cmd_req.response_sem);
        } else {
          *(cmd_req.result) = false;
          xSemaphoreGive(cmd_req.response_sem);
        }
        inst->current_cmd_req = NULL;
        continue;
      }

      // 응답 대기 (타임아웃 적용)
      if (xSemaphoreTake(cmd_req.response_sem, pdMS_TO_TICKS(cmd_req.timeout_ms)) == pdTRUE) {
        // 응답 수신 완료 (RX Task가 세마포어를 줌)
        if (cmd_req.is_async) {
          LOG_INFO("GPS[%d] Response received: %s", id,
                   cmd_req.async_result ? "OK" : "ERROR");
        } else {
          LOG_INFO("GPS[%d] Response received: %s", id,
                   *(cmd_req.result) ? "OK" : "ERROR");
        }
      } else {
        // 타임아웃
        LOG_WARN("GPS[%d] Command timeout", id);
        if (cmd_req.is_async) {
          cmd_req.async_result = false;
        } else {
          *(cmd_req.result) = false;
        }
      }

      // 현재 명령어 요청 초기화
      inst->current_cmd_req = NULL;

      // 비동기: 콜백 호출
      if (cmd_req.is_async) {
        if (cmd_req.callback) {
          cmd_req.callback(cmd_req.async_result, cmd_req.user_data);
        }

        // 비동기는 세마포어를 TX Task에서 삭제
        vSemaphoreDelete(cmd_req.response_sem);
      } else {
        // 동기: 외부 호출자에게 처리 완료 알림 (세마포어 반환)
        xSemaphoreGive(cmd_req.response_sem);
      }
    }
  }
  vTaskDelete(NULL);
}

/**
 * @brief GPS 태스크
 *
 * @param pvParameter
 */

char my_test[100];
uint8_t my_len = 0;

static void gps_process_task(void *pvParameter) {
  gps_id_t id = (gps_id_t)(uintptr_t)pvParameter;
  gps_instance_t *inst = &gps_instances[id];

  size_t pos = 0;
  size_t old_pos = 0;
  uint8_t dummy = 0;
  size_t total_received = 0;

  gps_set_evt_handler(&inst->gps, gps_evt_handler);
  memset(&inst->gga_avg_data, 0, sizeof(inst->gga_avg_data));
  memset(&inst->ubx_hp_avg, 0, sizeof(ubx_hp_avg_data_t));

  bool use_led = (id == GPS_ID_BASE ? 1 : 0);

  if (use_led) {
    led_set_color(2, LED_COLOR_RED);
    led_set_state(2, true);
  }

  vTaskDelay(pdMS_TO_TICKS(2000));
#if defined(BOARD_TYPE_BASE_UNICORE)
  gps_init_um982_base_async(id, overall_init_complete);
#elif defined(BOARD_TYPE_ROVER_UNICORE)
  gps_init_um982_rover_async(id, overall_init_complete);
#endif

  while (1) {
    xQueueReceive(inst->queue, &dummy,
                  portMAX_DELAY); // 단순 신호 전달용. 받는 데이터 없음

    if (inst->gps.nmea_data.gga.fix == GPS_FIX_INVALID) {
      if (use_led) {
        led_set_color(2, LED_COLOR_RED);
      }
    } else if (inst->gps.nmea_data.gga.fix < GPS_FIX_RTK_FIX ||
               inst->gps.nmea_data.gga.fix == GPS_FIX_RTK_FLOAT) {
      if (use_led) {
        led_set_color(2, LED_COLOR_YELLOW);
      }
    } else if (inst->gps.nmea_data.gga.fix < GPS_FIX_RTK_FLOAT) {
      if (use_led) {
        led_set_color(2, LED_COLOR_GREEN);
      }
    } else {
      if (use_led) {
        led_set_color(2, LED_COLOR_NONE);
      }
    }

    if (use_led) {
      led_set_toggle(2);
    }

    xSemaphoreTake(inst->gps.mutex, portMAX_DELAY);
    pos = gps_port_get_rx_pos(id);
    char *gps_recv = gps_port_get_recv_buf(id);

    if (pos != old_pos) {
      if (pos > old_pos) {
        size_t len = pos - old_pos;
        total_received = len;
        LOG_DEBUG_RAW("RAW: ", &gps_recv[old_pos], len);
        gps_parse_process(&inst->gps, &gps_recv[old_pos], pos - old_pos);
      } else {
        size_t len1 = GPS_UART_MAX_RECV_SIZE - old_pos;
        size_t len2 = pos;
        total_received = len1 + len2;
        LOG_DEBUG_RAW("RAW: ", &gps_recv[old_pos], len1);
        gps_parse_process(&inst->gps, &gps_recv[old_pos],
                          GPS_UART_MAX_RECV_SIZE - old_pos);
        if (pos > 0) {
          LOG_DEBUG_RAW("RAW: ", gps_recv, len2);
          gps_parse_process(&inst->gps, gps_recv, pos);
        }
      }
      old_pos = pos;
      if (old_pos == GPS_UART_MAX_RECV_SIZE) {
        old_pos = 0;
      }
    }
    xSemaphoreGive(inst->gps.mutex);

    if (get_gga(&inst->gps, my_test, &my_len)) {
      LOG_ERR("[ID:%d]%s", id, my_test);
    }
  }

  vTaskDelete(NULL);
}

void gps_init_all(void) {
  const board_config_t *config = board_get_config();

  LOG_INFO("GPS 초기화 시작 - 보드: %d, GPS 개수: %d", config->board,
           config->gps_cnt);

  for (uint8_t i = 0; i < config->gps_cnt && i < GPS_ID_MAX; i++) {
    gps_type_t type = config->gps[i];

    LOG_INFO("GPS[%d] 초기화 - 타입: %s", i,
             type == GPS_TYPE_F9P     ? "F9P"
             : type == GPS_TYPE_UM982 ? "UM982"
                                      : "UNKNOWN");

    // GPS 핸들 초기화
    gps_init(&gps_instances[i].gps);
    gps_instances[i].type = type;
    gps_instances[i].id = (gps_id_t)i;
    gps_instances[i].enabled = true;

    // GPS 타입별 초기 상태 설정
    if (type == GPS_TYPE_UM982) {
      // Unicore UM982: RDY 대기
      gps_instances[i].gps.init_state = GPS_INIT_NONE;
      LOG_INFO("GPS[%d] UM982 will wait for RDY signal", i);
    } else if (type == GPS_TYPE_F9P) {
      // Ublox F9P: task에서 부팅 delay 후 완료
      gps_instances[i].gps.init_state = GPS_INIT_NONE; // 임시
      LOG_INFO("GPS[%d] F9P will boot and init", i);
    } else {
      gps_instances[i].gps.init_state = GPS_INIT_NONE;
    }

    // 포트 초기화 (UART 설정 및 HAL ops 자동 할당)
    if (gps_port_init_instance(&gps_instances[i].gps, (gps_id_t)i, type) != 0) {
      LOG_ERR("GPS[%d] 포트 초기화 실패", i);
      gps_instances[i].enabled = false;
      continue;
    }

    // 큐 생성 및 설정
    gps_instances[i].queue = xQueueCreate(10, sizeof(uint8_t));
    if (gps_instances[i].queue == NULL) {
      LOG_ERR("GPS[%d] 큐 생성 실패", i);
      gps_instances[i].enabled = false;
      continue;
    }

    gps_port_set_queue((gps_id_t)i, gps_instances[i].queue);

    gps_instances[i].cmd_queue = xQueueCreate(5, sizeof(gps_cmd_request_t));
    if (gps_instances[i].cmd_queue == NULL) {
      LOG_ERR("GPS[%d] TX 큐 생성 실패", i);
      gps_instances[i].enabled = false;
      continue;
    }

    // UART 시작
    gps_port_start(&gps_instances[i].gps);

    // 태스크 생성
    char task_name[16];
    snprintf(task_name, sizeof(task_name), "gps_rx_%d", i);

    BaseType_t ret =
        xTaskCreate(gps_process_task, task_name, 2048,
                    (void *)(uintptr_t)i, // GPS ID를 파라미터로 전달
                    tskIDLE_PRIORITY + 1, &gps_instances[i].task);

    if (ret != pdPASS) {
      LOG_ERR("GPS[%d] RX 태스크 생성 실패", i);
      gps_instances[i].enabled = false;
      continue;
    }

    snprintf(task_name, sizeof(task_name), "gps_tx_%d", i);
    ret = xTaskCreate(gps_tx_task, task_name, 2048,
                      (void *)(uintptr_t)i, // GPS ID를 파라미터로 전달
                      tskIDLE_PRIORITY + 1, &gps_instances[i].tx_task);

    if (ret != pdPASS) {
      LOG_ERR("GPS[%d] TX 태스크 생성 실패", i);
      gps_instances[i].enabled = false;
      continue;
    }

    LOG_INFO("GPS[%d] 초기화 완료", i);
  }

  LOG_INFO("GPS 전체 초기화 완료");
}

/**
 * @brief 특정 GPS ID의 핸들 가져오기
 */
gps_t *gps_get_instance_handle(gps_id_t id) {
  if (id >= GPS_ID_MAX || !gps_instances[id].enabled) {
    return NULL;
  }

  return &gps_instances[id].gps;
}

/**
 * @brief GGA 평균 데이터 읽기 가능 여부
 */
bool gps_gga_avg_can_read(gps_id_t id) {
  if (id >= GPS_ID_MAX || !gps_instances[id].enabled) {
    return false;
  }

  return gps_instances[id].gga_avg_data.can_read;
}

/**
 * @brief GGA 평균 데이터 가져오기
 */
bool gps_get_gga_avg(gps_id_t id, double *lat, double *lon, double *alt) {
  if (id >= GPS_ID_MAX || !gps_instances[id].enabled) {
    return false;
  }

  if (!gps_instances[id].gga_avg_data.can_read) {
    return false;
  }

  if (lat)
    *lat = gps_instances[id].gga_avg_data.lat_avg;
  if (lon)
    *lon = gps_instances[id].gga_avg_data.lon_avg;
  if (alt)
    *alt = gps_instances[id].gga_avg_data.alt_avg;

  return true;
}


bool gps_send_command_sync(gps_id_t id, const char *cmd, uint32_t timeout_ms) {
  if (id >= GPS_ID_MAX || !gps_instances[id].enabled) {
    LOG_ERR("GPS[%d] invalid or disabled", id);
    return false;
  }

  if (!cmd || strlen(cmd) == 0) {
    LOG_ERR("GPS[%d] empty command", id);
    return false;
  }

  gps_instance_t *inst = &gps_instances[id];

  // 세마포어 생성 (응답 대기용)
  SemaphoreHandle_t response_sem = xSemaphoreCreateBinary();
  if (response_sem == NULL) {
    LOG_ERR("GPS[%d] failed to create semaphore", id);
    return false;
  }

  // 명령어 요청 구조체 생성
  bool result = false;
 gps_cmd_request_t cmd_req = {
      .timeout_ms = timeout_ms,
      .is_async = false,
      .response_sem = response_sem,
      .result = &result,
      .callback = NULL,
      .user_data = NULL,
  };

  strncpy(cmd_req.cmd, cmd, sizeof(cmd_req.cmd) - 1);
  cmd_req.cmd[sizeof(cmd_req.cmd) - 1] = '\0';

  // TX 태스크로 명령어 전송 요청
  if (xQueueSend(inst->cmd_queue, &cmd_req, pdMS_TO_TICKS(1000)) != pdTRUE) {
    LOG_ERR("GPS[%d] failed to send command to TX task", id);
    vSemaphoreDelete(response_sem);
    return false;
  }

  // TX 태스크에서 처리 완료 대기
  // TX 태스크가 응답을 받거나 타임아웃되면 세마포어를 줌
  if (xSemaphoreTake(response_sem, pdMS_TO_TICKS(timeout_ms + 1000)) == pdTRUE) {
    // 처리 완료
    vSemaphoreDelete(response_sem);
    return result;
  } else {
    // 외부 타임아웃 (TX 태스크 응답 없음)
    LOG_ERR("GPS[%d] TX task did not respond", id);
    vSemaphoreDelete(response_sem);
    return false;
  }
}

bool gps_send_command_async(gps_id_t id, const char *cmd, uint32_t timeout_ms,
                             gps_command_callback_t callback, void *user_data) {
  if (id >= GPS_ID_MAX || !gps_instances[id].enabled) {
    LOG_ERR("GPS[%d] invalid or disabled", id);
    return false;
  }

  if (!cmd || strlen(cmd) == 0) {
    LOG_ERR("GPS[%d] empty command", id);
    return false;
  }

  gps_instance_t *inst = &gps_instances[id];

  // 세마포어 생성 (TX Task 내부에서 응답 대기용)
  SemaphoreHandle_t response_sem = xSemaphoreCreateBinary();
  if (response_sem == NULL) {
    LOG_ERR("GPS[%d] failed to create semaphore", id);
    return false;
  }

  // 명령어 요청 구조체 생성 (비동기 방식)
  gps_cmd_request_t cmd_req = {
      .timeout_ms = timeout_ms,
      .is_async = true,
      .response_sem = response_sem,
      .result = NULL,
      .callback = callback,
      .user_data = user_data,
      .async_result = false,
  };

  strncpy(cmd_req.cmd, cmd, sizeof(cmd_req.cmd) - 1);
  cmd_req.cmd[sizeof(cmd_req.cmd) - 1] = '\0';

  // TX 태스크로 명령어 전송 요청
  if (xQueueSend(inst->cmd_queue, &cmd_req, pdMS_TO_TICKS(1000)) != pdTRUE) {
    LOG_ERR("GPS[%d] failed to send command to TX task", id);
    vSemaphoreDelete(response_sem);
    return false;
  }

  // 즉시 반환 (non-blocking)
  // TX Task가 처리를 완료하면 콜백 호출
  LOG_INFO("GPS[%d] Async command queued", id);
  return true;
}
