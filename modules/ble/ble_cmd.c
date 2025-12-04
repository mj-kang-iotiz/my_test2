#include "ble_cmd.h"
#include "board_config.h"
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "flash_params.h"
#include "ble.h"
#include "ble_app.h"

#ifndef TAG
#define TAG "BLE_CMD"
#endif

#include "log.h"

static const char *ble_resp_str_ok = "OK\n";
static const char *ble_resp_str_invalid = "+E01\n";
static const char *ble_resp_str_param_err = "+E02\n";
static const char *ble_resp_str_not_rdy = "+E03\n";
static const char *ble_resp_str_err = "+ERROR\n";

#define BLE_AT_RESP_SEND(data) ble_send(data, strlen(data), false)
                                 
#define BLE_AT_RESP_SEND_OK() BLE_AT_RESP_SEND(ble_resp_str_ok)
#define BLE_AT_RESP_SEND_INVALID() BLE_AT_RESP_SEND(ble_resp_str_invalid)
#define BLE_AT_RESP_SEND_PARAM_ERR() BLE_AT_RESP_SEND(ble_resp_str_param_err)
#define BLE_AT_RESP_SEND_NOT_RDY() BLE_AT_RESP_SEND(ble_resp_str_not_rdy)
#define BLE_AT_RESP_SEND_ERR() BLE_AT_RESP_SEND(ble_resp_str_err)

static void sd_handler(ble_instance_t *inst, const char *param);
static void sc_handler(ble_instance_t *inst, const char *param);
static void sm_handler(ble_instance_t *inst, const char *param);
static void si_handler(ble_instance_t *inst, const char *param);
static void sp_handler(ble_instance_t *inst, const char *param);
static void sg_handler(ble_instance_t *inst, const char *param);
static void gd_handler(ble_instance_t *inst, const char *param);
static void gi_handler(ble_instance_t *inst, const char *param);
static void gp_handler(ble_instance_t *inst, const char *param);
static void gg_handler(ble_instance_t *inst, const char *param);
static void rs_handler(ble_instance_t *inst, const char *param);


void bot_ok_handler(ble_instance_t *inst, const char *param)
{

}

void bot_err_handler(ble_instance_t *inst, const char *param)
{

}

void bot_rdy_handler(ble_instance_t *inst, const char *param)
{

}

void bot_advertising_handler(ble_instance_t *inst, const char *param)
{

}

void bot_connected_handler(ble_instance_t *inst, const char *param)
{

}

void bot_disconnected_handler(ble_instance_t *inst, const char *param)
{

}

// AT+UART=xxxx
// AT+MANUF=xxxxxxxx
static const ble_at_cmd_entry_t bot_cmd_table[] = {
		{"+OK", bot_ok_handler},
		{"+ERROR", bot_err_handler},
		{"+READY", bot_rdy_handler},
		{"+ADVERTISING", bot_advertising_handler},
		{"+CONNECTED", bot_connected_handler},
		{"+DISCONNECTED", bot_disconnected_handler},
};


static const ble_at_cmd_entry_t at_cmd_table[] = {
    {"SD", sd_handler},
    {"SC", sc_handler},
    {"SM", sm_handler},
    {"SI", si_handler},
    {"SP", sp_handler},
    {"SG", sg_handler},
    {"GD", gd_handler},
    {"GI", gi_handler},
    {"GP", gp_handler},
    {"GG", gg_handler},
    {"RS", rs_handler},
    {NULL, NULL}};

void ble_app_cmd_handler(ble_instance_t *inst)
{
    for (int i = 0; at_cmd_table[i].name != NULL; i++)
    {
        size_t name_len = strlen(at_cmd_table[i].name);

        if (strncmp(inst->parser.data, at_cmd_table[i].name, name_len) == 0)
        {
            at_cmd_table[i].handler(inst, inst->parser.data + name_len);
            return;
        }
    }
}

void ble_at_cmd_handler(ble_instance_t *inst)
{
    // 비동기 AT 커맨드 요청이 있는지 확인
    if (inst->async_request != NULL && inst->async_request->status == BLE_AT_STATUS_PENDING) {
        // 기대하는 응답과 매칭되는지 확인
        size_t expected_len = strlen(inst->async_request->expected_response);
        if (strncmp(inst->parser.data, inst->async_request->expected_response, expected_len) == 0) {
            // 응답 저장
            size_t data_len = strlen(inst->parser.data);
            if (data_len < BLE_AT_RESPONSE_MAX_SIZE) {
                memcpy(inst->async_request->response_buf, inst->parser.data, data_len);
                inst->async_request->response_len = data_len;
                inst->async_request->response_buf[data_len] = '\0';
            } else {
                memcpy(inst->async_request->response_buf, inst->parser.data, BLE_AT_RESPONSE_MAX_SIZE - 1);
                inst->async_request->response_len = BLE_AT_RESPONSE_MAX_SIZE - 1;
                inst->async_request->response_buf[BLE_AT_RESPONSE_MAX_SIZE - 1] = '\0';
            }

            // 상태 업데이트
            if (strncmp(inst->parser.data, "+OK", 3) == 0) {
                inst->async_request->status = BLE_AT_STATUS_COMPLETED;
            } else if (strncmp(inst->parser.data, "+ERROR", 6) == 0) {
                inst->async_request->status = BLE_AT_STATUS_ERROR;
            } else {
                inst->async_request->status = BLE_AT_STATUS_COMPLETED;
            }

            // 세마포어 해제 (대기 중인 태스크 깨우기)
            if (inst->async_request->wait_sem != NULL) {
                xSemaphoreGive(inst->async_request->wait_sem);
            }

            LOG_INFO("Async AT response matched: %s", inst->parser.data);
            return;
        }
    }

    // 비동기 요청이 없거나 매칭되지 않으면 기존 핸들러 실행
    for (int i = 0; bot_cmd_table[i].name != NULL; i++)
    {
        size_t name_len = strlen(bot_cmd_table[i].name);

        if (strncmp(inst->parser.data, bot_cmd_table[i].name, name_len) == 0)
        {
            bot_cmd_table[i].handler(inst, inst->parser.data + name_len);
            return;
        }
    }
}

static void sd_handler(ble_instance_t *inst, const char *param)
{

}
static void sc_handler(ble_instance_t *inst, const char *param)
{

}
static void sm_handler(ble_instance_t *inst, const char *param)
{

}
static void si_handler(ble_instance_t *inst, const char *param)
{

}
static void sp_handler(ble_instance_t *inst, const char *param)
{

}
static void sg_handler(ble_instance_t *inst, const char *param)
{

}
static void gd_handler(ble_instance_t *inst, const char *param)
{
    user_params_t *params = flash_params_get_current();
    char device_name[32];

    sprintf(device_name, "Get %s\n", params->ble_device_name);
    BLE_AT_RESP_SEND(device_name);
}
static void gi_handler(ble_instance_t *inst, const char *param)
{
    user_params_t *params = flash_params_get_current();
    char id[32];

    sprintf(id, "Get %s\n", params->ntrip_id);
    BLE_AT_RESP_SEND(id);
}
static void gp_handler(ble_instance_t *inst, const char *param)
{
    user_params_t *params = flash_params_get_current();
    char pw[32];

    sprintf(pw, "Get %s\n", params->ntrip_pw);
    BLE_AT_RESP_SEND(pw);
}
static void gg_handler(ble_instance_t *inst, const char *param)
{
    user_params_t *params = flash_params_get_current();
    char loc[64];

    sprintf(loc, "Get %s,%s,%s\n", params->lat, params->lon, params->alt);
    BLE_AT_RESP_SEND(loc);
}
static void rs_handler(ble_instance_t *inst, const char *param)
{
 ble_get_handle()->ops->send("Device Reset\n", strlen("Device Reset\n"));
 vTaskDelay(pdMS_TO_TICKS(100));
NVIC_SystemReset();
}
