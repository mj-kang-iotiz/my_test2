#include "gps_app.h"
#include "board_config.h"
#include "gps.h"
#include "gps_port.h"
#include "gps_unicore.h"
#include "ubx_init.h"
#include "ntrip_app.h"
#include "rtcm.h"
#include "led.h"
#include <string.h>
#include "ubx_init.h"

#ifndef TAG
  #define TAG "GPS_APP"
#endif

#include "log.h"

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

#define GPS_INIT_MAX_RETRY 3
#define GPS_INIT_TIMEOUT_MS 1000

#define UM982_BASE_CMD_COUNT (sizeof(um982_base_cmds) / sizeof(um982_base_cmds[0]))

static const char *um982_base_cmds[] = {
	// "CONFIG ANTENNA POWERON\r\n",
  "unmask BDS\r\n",
  "unmask GPS\r\n",
  "unmask GLO\r\n",
  "unmask GAL\r\n",
  "MODE BASE TIME 120\r\n",
  "rtcm1033 com1 10\r\n",
  "rtcm1006 com1 10\r\n",
  "rtcm1074 com1 2\r\n",
  "rtcm1124 com1 2\r\n",
  "rtcm1084 com1 2\r\n",
  "rtcm1094 com1 2\r\n",
  "gpgga com1 1\r\n",
  // "gpgsv com1 1\r\n",
  "BESTNAVB 1\r\n",
};

static const char *um982_rover_cmds[] = {
  // "CONFIG ANTENNA POWERON\r\n",
  "unmask BDS\r\n",
  "unmask GPS\r\n",
  "unmask GLO\r\n",
  "unmask GAL\r\n",
  "gpgga com1 1\r\n",
  // "gpgsv com1 1\r\n",
  "gpths com1 1\r\n",
  "OBSVHA COM1 1\r\n", // slave antenna
  "BESTNAVB 1\r\n",
  "CONFIG HEADING FIXLENGTH\r\n"
  "config heading length 50 30\r\n",
  "UNIHEADINGA 1\r\n",
};

#define UM982_ROVER_CMD_COUNT (sizeof(um982_rover_cmds) / sizeof(um982_rover_cmds[0]))

typedef void (*gps_init_callback_t)(bool success, void *user_data);

typedef struct {
  gps_id_t gps_id;
  uint8_t current_step;
  uint8_t retry_count;
  const char **cmd_list;
  uint8_t cmd_count;
  gps_init_callback_t callback;
} gps_init_context_t;

#if defined(BOARD_TYPE_BASE_UNICORE) || defined(BOARD_TYPE_ROVER_UNICORE)
static void overall_init_complete(bool success, void *user_data) {
  gps_id_t id = (gps_id_t)(uintptr_t)user_data;
  LOG_INFO("GPS[%d] Overall init %s", id, success ? "succeeded" : "failed");
}
#endif

static void gps_init_command_callback(bool success, void *user_data) {
  gps_init_context_t *ctx = (gps_init_context_t *)user_data;

  if (!ctx) {
    LOG_ERR("GPS init context is NULL");
    return;
  }

  if (success) {
    LOG_INFO("GPS[%d] Init step %d/%d OK: %s",
             ctx->gps_id, ctx->current_step + 1, ctx->cmd_count,
             ctx->cmd_list[ctx->current_step]);

    ctx->current_step++;
    ctx->retry_count = 0;

    if (ctx->current_step >= ctx->cmd_count) {
      LOG_INFO("GPS[%d] Init sequence complete!", ctx->gps_id);
      if (ctx->callback) {
        ctx->callback(true, NULL);
      }

      vPortFree(ctx);
      return;
    }


    gps_send_command_async(ctx->gps_id, ctx->cmd_list[ctx->current_step],
                           GPS_INIT_TIMEOUT_MS, gps_init_command_callback, ctx);
  } else {

    ctx->retry_count++;

    if (ctx->retry_count < GPS_INIT_MAX_RETRY) {

      LOG_WARN("GPS[%d] Init step %d/%d failed, retrying (%d/%d): %s",
               ctx->gps_id, ctx->current_step + 1, ctx->cmd_count,
               ctx->retry_count, GPS_INIT_MAX_RETRY,
               ctx->cmd_list[ctx->current_step]);

      
      gps_send_command_async(ctx->gps_id, ctx->cmd_list[ctx->current_step],
                             GPS_INIT_TIMEOUT_MS, gps_init_command_callback, ctx);
    } else {

      
      LOG_ERR("GPS[%d] Init failed at step %d/%d after %d retries: %s",
              ctx->gps_id, ctx->current_step + 1, ctx->cmd_count,
              GPS_INIT_MAX_RETRY, ctx->cmd_list[ctx->current_step]);

      if (ctx->callback) {
        ctx->callback(false, NULL);
      }
      
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

  LOG_DEBUG("GPS[%d] Starting UM982 base init sequence (%d commands)",
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

  LOG_DEBUG("GPS[%d] Starting UM982 rover init sequence (%d commands)",
           id, ctx->cmd_count);

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
  const board_config_t *config = board_get_config();

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
    	if(config->board == BOARD_TYPE_BASE_F9P || config->board == BOARD_TYPE_BASE_UM982)
    	{
    	    if (gps->nmea_data.gga.fix >= GPS_FIX_RTK_FIX)
    	    {
    	        _add_gga_avg_data(inst, gps->nmea_data.gga.lat, gps->nmea_data.gga.lon,
    	                          gps->nmea_data.gga.alt);
    	    }
    	}

      if (gps->nmea_data.gga_is_rdy)
      {
        if(config->board == BOARD_TYPE_ROVER_F9P)
        {
          if(inst->id == GPS_ID_BASE)
          {
             ntrip_send_gga_data(gps->nmea_data.gga_raw,
                               gps->nmea_data.gga_raw_pos);
          }
        }
        else
        {
          ntrip_send_gga_data(gps->nmea_data.gga_raw,
                            gps->nmea_data.gga_raw_pos);
        }
      } 
    }
    break;

  case GPS_PROTOCOL_UBX:
    if (msg.ubx.id == GPS_UBX_NAV_ID_HPPOSLLH) {
      if(config->board == BOARD_TYPE_BASE_F9P)
      {
        if (gps->nmea_data.gga.fix >= GPS_FIX_RTK_FIX) {
          _add_hp_avg_data(inst);
        }
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

  case GPS_PROTOCOL_UNICORE_BIN:
    switch (msg.unicore_bin.msg) {
      case GPS_UNICORE_BIN_MSG_BESTNAV: {
        hpd_unicore_bestnavb_t *bestnav = &gps->unicore_bin_data.bestnav;

        LOG_INFO("GPS[%d] BESTNAV: lat=%.8f, lon=%.8f, height=%.3f, sv=%d, used_sv=%d, hor_speed=%.3f",
                 inst->id,
                 bestnav->lat,
                 bestnav->lon,
                 bestnav->height,
                 bestnav->sv,
                 bestnav->used_sv,
                 bestnav->hor_speed);

        // 여기에 추가 처리 로직을 넣을 수 있습니다
        // 예: 특정 조건에서 콜백 호출, 데이터 저장 등

        break;
      }
      default:
        break;
    }
    break;

  case GPS_PROTOCOL_RTCM:
    if(config->lora_mode == LORA_MODE_BASE)
    {
      rtcm_send_to_lora(gps);
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

  LOG_INFO("GPS TX 태스크[%d] 시작", id);

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
        xSemaphoreTake(inst->gps.mutex, pdMS_TO_TICKS(1000));
        inst->gps.ops->send(cmd_req.cmd, strlen(cmd_req.cmd));
        xSemaphoreGive(inst->gps.mutex);
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

void callback_function(bool success, void *user_data) {
    if (success) {
       LOG_INFO(" 팩토리 리셋 성공");
    } else {
      LOG_ERR("팩토리 리셋 실패");
    }
}


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

  vTaskDelay(pdMS_TO_TICKS(1000));

  // gps_factory_reset_async(id, callback_function, NULL);

#if defined(BOARD_TYPE_BASE_UNICORE)
  gps_init_um982_base_async(id, overall_init_complete);
#elif defined(BOARD_TYPE_ROVER_UNICORE)
  gps_init_um982_rover_async(id, overall_init_complete);
#elif defined(BOARD_TYPE_BASE_UBLOX)
  ubx_base_init(&inst->gps);
#elif defined(BOARD_TYPE_ROVER_UBLOX)
  if(id == GPS_ID_BASE)
  {
    ubx_moving_base_init(&inst->gps);
  }
  else if(id == GPS_ID_ROVER)
  {
    ubx_rover_init(&inst->gps);
  }
#endif
  bool init_done = false;
  
  while (1) {
    ubx_init_async_process(&inst->gps);

    if (!init_done) {
            ubx_init_state_t state = ubx_init_async_get_state(&inst->gps);
            if (state == UBX_INIT_STATE_DONE) {
                printf("✓ UBX initialization completed!\n");
                init_done = true;
            } else if (state == UBX_INIT_STATE_ERROR) {
                printf("✗ UBX initialization failed!\n");
                init_done = true;
            }
    }

    xQueueReceive(inst->queue, &dummy,
                  portMAX_DELAY);

    if (inst->gps.nmea_data.gga.fix == GPS_FIX_INVALID) {
      if (use_led) {
        led_set_color(2, LED_COLOR_RED);
      }
    } else if (inst->gps.nmea_data.gga.fix < GPS_FIX_RTK_FIX ||
               inst->gps.nmea_data.gga.fix == GPS_FIX_RTK_FLOAT) {
      if (use_led) {
        led_set_color(2, LED_COLOR_YELLOW);
      }
    } else if (inst->gps.nmea_data.gga.fix ==  GPS_FIX_RTK_FIX) {
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
        LOG_DEBUG("[%d] %d received", id, (int)len);
        LOG_DEBUG_RAW("RAW: ", &gps_recv[old_pos], len);
        gps_parse_process(&inst->gps, &gps_recv[old_pos], pos - old_pos);
      } else {
        size_t len1 = GPS_UART_MAX_RECV_SIZE - old_pos;
        size_t len2 = pos;
        total_received = len1 + len2;
        LOG_DEBUG("[%d] %d received (wrap around)", id, (int)(len1 + len2));
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
  }

  vTaskDelete(NULL);
}

void gps_init_all(void) {
  const board_config_t *config = board_get_config();

  for (uint8_t i = 0; i < config->gps_cnt && i < GPS_ID_MAX; i++) {
    gps_type_t type = config->gps[i];

    LOG_DEBUG("GPS[%d] 초기화 - 타입: %s", i,
             type == GPS_TYPE_F9P     ? "F9P"
             : type == GPS_TYPE_UM982 ? "UM982"
                                      : "UNKNOWN");

    gps_init(&gps_instances[i].gps);
    gps_instances[i].type = type;
    gps_instances[i].id = (gps_id_t)i;
    gps_instances[i].enabled = true;

    if (type == GPS_TYPE_UM982) {
      gps_instances[i].gps.init_state = GPS_INIT_NONE;
    } else if (type == GPS_TYPE_F9P) {
      gps_instances[i].gps.init_state = GPS_INIT_NONE;
    } else {
      gps_instances[i].gps.init_state = GPS_INIT_NONE;
    }

    if (gps_port_init_instance(&gps_instances[i].gps, (gps_id_t)i, type) != 0) {
      LOG_ERR("GPS[%d] 포트 초기화 실패", i);
      gps_instances[i].enabled = false;
      continue;
    }

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

    gps_port_start(&gps_instances[i].gps);

    char task_name[16];
    snprintf(task_name, sizeof(task_name), "gps_rx_%d", i);

    BaseType_t ret =
        xTaskCreate(gps_process_task, task_name, 1024,
                    (void *)(uintptr_t)i, // GPS ID
                    tskIDLE_PRIORITY + 1, &gps_instances[i].task);

    if (ret != pdPASS) {
      LOG_ERR("GPS[%d] RX 태스크 생성 실패", i);
      gps_instances[i].enabled = false;
      continue;
    }

    snprintf(task_name, sizeof(task_name), "gps_tx_%d", i);
    ret = xTaskCreate(gps_tx_task, task_name, 512,
                      (void *)(uintptr_t)i, // GPS ID를 파라미터로 전달
                      tskIDLE_PRIORITY + 1, &gps_instances[i].tx_task);

    if (ret != pdPASS) {
      LOG_ERR("GPS[%d] TX 태스크 생성 실패", i);
      gps_instances[i].enabled = false;
      continue;
    }

    LOG_INFO("GPS[%d] 인스턴스 초기화 완료", i);

  }

  LOG_INFO("GPS 전체 인스턴스 초기화 완료");
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

  if (xQueueSend(inst->cmd_queue, &cmd_req, pdMS_TO_TICKS(1000)) != pdTRUE) {
    LOG_ERR("GPS[%d] failed to send command to TX task", id);
    vSemaphoreDelete(response_sem);
    return false;
  }

  if (xSemaphoreTake(response_sem, pdMS_TO_TICKS(timeout_ms + 1000)) == pdTRUE) {
    // 처리 완료
    vSemaphoreDelete(response_sem);
    return result;
  } else {
    LOG_ERR("GPS[%d] 전송 태스크 응답 없음", id);
    vSemaphoreDelete(response_sem);
    return false;
  }
}

bool gps_send_command_async(gps_id_t id, const char *cmd, uint32_t timeout_ms,
                             gps_command_callback_t callback, void *user_data) {
  if (id >= GPS_ID_MAX || !gps_instances[id].enabled) {
    LOG_ERR("GPS[%d] invalid", id);
    return false;
  }

  if (!cmd || strlen(cmd) == 0) {
    LOG_ERR("GPS[%d] 잘못된 명령", id);
    return false;
  }

  gps_instance_t *inst = &gps_instances[id];

  // 세마포어 생성 (TX Task 내부에서 응답 대기용)
  SemaphoreHandle_t response_sem = xSemaphoreCreateBinary();
  if (response_sem == NULL) {
    LOG_ERR("GPS[%d] 세마포어 생성 실패", id);
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

  if (xQueueSend(inst->cmd_queue, &cmd_req, pdMS_TO_TICKS(1000)) != pdTRUE) {
    LOG_ERR("GPS[%d] 메시지큐 전송 실패", id);
    vSemaphoreDelete(response_sem);
    return false;
  }

  LOG_DEBUG("GPS[%d] 비동기 명령 메시지큐 전송", id);
  return true;
}

bool gps_send_raw_data(gps_id_t id, const uint8_t *data, size_t len) {
  if (id >= GPS_ID_MAX || !gps_instances[id].enabled) {
    LOG_ERR("GPS[%d] invalid", id);
    return false;
  }

  if (!data || len == 0) {
    LOG_ERR("GPS[%d] invalid data", id);
    return false;
  }

  gps_instance_t *inst = &gps_instances[id];

  if (!inst->gps.ops || !inst->gps.ops->send) {
    LOG_ERR("GPS[%d] ops invalid", id);
    return false;
  }

  if (xSemaphoreTake(inst->gps.mutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
    inst->gps.ops->send((const char *)data, len);
    xSemaphoreGive(inst->gps.mutex);
    LOG_DEBUG("GPS[%d] 송신 %d 바이트", id, len);
    return true;
  } else {
    LOG_ERR("GPS[%d] 뮤텍스 타임아웃", id);
    return false;
  }
}


bool gps_factory_reset_async(gps_id_t id, gps_init_callback_t callback, void *user_data)
{
  if (id >= GPS_ID_MAX || !gps_instances[id].enabled) {

    LOG_ERR("GPS[%d] invalid", id);

    return false;

  }

  gps_instance_t *inst = &gps_instances[id];

  if (inst->type != GPS_TYPE_F9P) {
    return false;
  }

  if (!ubx_factory_reset(&inst->gps, callback, user_data)) {

    LOG_ERR("GPS[%d] 팩토리 리셋 실패", id);

    return false;

  }

 

  return true;

}
