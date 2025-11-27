#include "gps_app.h"
#include "board_config.h"
#include "gps.h"
#include "gps_port.h"
#include "led.h"
#include <string.h>

#define GPS_UART_MAX_RECV_SIZE 2048

#define GGA_AVG_SIZE 50
#define HP_AVG_SIZE 50

static const gps_init_cmd_t um982_base_cmds[] = {
    {"mode base time 60\r\n"},
    {"rtcm1033 com1 10\r\n"},
    {"rtcm1006 com1 10\r\n"},
    {"rtcm1074 com1 1\r\n"},
    {"rtcm1124 com1 1\r\n"},
    {"rtcm1084 com1 1\r\n"},
    {"rtcm1094 com1 1\r\n"},
    {"gpgga com1 1\r\n"},
    {"BESTNAVA 1\r\n"},
};

// ROVER UM982: 로버 모드
static const gps_init_cmd_t um982_rover_cmds[] = {
    {"CONFIG RESET\r\n"},
    {"MODE ROVER\r\n"},
    {"GNGGA 1\r\n"},
    {"SAVECONFIG\r\n"},
};

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

  switch (event) {
  case GPS_EVENT_READY:
    // RDY 수신 → 설정 명령 전송 플래그 설정
    if (inst->config.seq != NULL) {
      LOG_INFO("GPS[%d] RDY received, will send config commands", inst->id);
      inst->config.need_send_config = true;
    }
    break;

  default:
    break;
  }

  switch (protocol) {
  case GPS_PROTOCOL_NMEA:
    if (msg.nmea == GPS_NMEA_MSG_GGA) {
      if (gps->nmea_data.gga.fix >= GPS_FIX_GPS) {
        _add_gga_avg_data(inst, gps->nmea_data.gga.lat, gps->nmea_data.gga.lon,
                          gps->nmea_data.gga.alt);
      }
    }
    break;

  case GPS_PROTOCOL_UBX:
    if (msg.ubx.id == GPS_UBX_NAV_ID_HPPOSLLH) {
      if (gps->nmea_data.gga.fix >= GPS_FIX_GPS) {
        _add_hp_avg_data(inst);
      }
    }

  default:
    break;
  }
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

    // UART 시작
    gps_port_start(&gps_instances[i].gps);

    // 태스크 생성
    char task_name[16];
    snprintf(task_name, sizeof(task_name), "gps_%d", i);

    BaseType_t ret =
        xTaskCreate(gps_process_task, task_name, 2048,
                    (void *)(uintptr_t)i, // GPS ID를 파라미터로 전달
                    tskIDLE_PRIORITY + 1, &gps_instances[i].task);

    if (ret != pdPASS) {
      LOG_ERR("GPS[%d] 태스크 생성 실패", i);
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
