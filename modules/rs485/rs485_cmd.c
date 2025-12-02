#include "rs485_cmd.h"
#include "board_config.h"
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "flash_params.h"

#ifndef TAG
#define TAG "RS485_CMD"
#endif

#include "log.h"

static const char *rs485_resp_str_ok = "OK\r";
static const char *rs485_resp_str_invalid = "+E01\r";
static const char *rs485_resp_str_param_err = "+E02\r";
static const char *rs485_resp_str_not_rdy = "+E03\r";
static const char *rs485_resp_str_err = "+ERROR\r";

#define RS485_AT_RESP_SEND(data) rs485_send(data, strlen(data))
#define RS485_AT_RESP_SEND_OK() RS485_AT_RESP_SEND(rs485_resp_str_ok)
#define RS485_AT_RESP_SEND_INVALID() RS485_AT_RESP_SEND(rs485_resp_str_invalid)
#define RS485_AT_RESP_SEND_PARAM_ERR() RS485_AT_RESP_SEND(rs485_resp_str_param_err)
#define RS485_AT_RESP_SEND_NOT_RDY() RS485_AT_RESP_SEND(rs485_resp_str_not_rdy)
#define RS485_AT_RESP_SEND_ERR() RS485_AT_RESP_SEND(rs485_resp_str_err)

static void at_handler(const char *param);
static void atz_handler(const char *param);
static void atandz_handler(const char *param);
static void at_ver_handler(const char *param);
static void at_gps_manuf_handler(const char *param);
static void at_read_config_handler(const char *param);
static void at_set_baseline_handler(const char *param);
static void at_set_ntrip_ip_handler(const char *param);
static void at_set_ntrip_id_handler(const char *param);
static void at_set_ntrip_mountpoint_handler(const char *param);
static void at_set_ntrip_passwd_handler(const char *param);
static void at_set_rtk_start_handler(const char *param);
static void at_set_rtk_stop_handler(const char *param);

static const at_cmd_entry_t at_cmd_table[] = {
    {"AT", at_handler},
    {"ATZ", atz_handler},
    {"AT&Z", atandz_handler},
    {"AT+VER?", at_ver_handler},
    {"AT+GPSMANUF?", at_gps_manuf_handler},
    {"AT+CONFIG?", at_read_config_handler},
    {"AT+SETBASELINE:", at_set_baseline_handler},
    {"AT+CASTER:", at_set_ntrip_ip_handler},
    {"AT+ID=", at_set_ntrip_id_handler},
    {"AT+MOUNTPOINT=", at_set_ntrip_mountpoint_handler},
    {"AT+PASSWD=", at_set_ntrip_passwd_handler},
    {"AT+GUGUSTART", at_set_rtk_start_handler},
    {"AT+GUGUSTOP", at_set_rtk_stop_handler},
    {NULL, NULL}};

void rs485_at_cmd_handler(rs485_instance_t *inst)
{
    for (int i = 0; at_cmd_table[i].name != NULL; i++)
    {
        size_t name_len = strlen(at_cmd_table[i].name);

        if (strncmp(inst->parser.data, at_cmd_table[i].name, name_len) == 0)
        {
            at_cmd_table[i].handler(inst->parser.data + name_len);
            return;
        }
    }
}

static void at_handler(const char *param)
{
    RS485_AT_RESP_SEND_OK();
}

static void atz_handler(const char *param)
{
    xSemaphoreTake(rs485_get_instance()->mutex, portMAX_DELAY);
    rs485_get_handle()->ops->send("+RESET\r", 7);
    xSemaphoreGive(rs485_get_instance()->mutex);

    HAL_NVIC_SystemReset();
}

static void atandz_handler(const char *param)
{
    flash_params_erase();
    RS485_AT_RESP_SEND("+CONFIGINIT\r");
}

static void at_ver_handler(const char *param)
{
    char version_str[20];
    snprintf(version_str, sizeof(version_str), "V%s\r", BOARD_VERSION);
    RS485_AT_RESP_SEND(version_str);
}

static void at_gps_manuf_handler(const char *param)
{
    const board_config_t *config = board_get_config();
    char manuf_str[20];

    if (config->board == BOARD_TYPE_BASE_F9P || config->board == BOARD_TYPE_ROVER_F9P)
    {
        sprintf(manuf_str, sizeof(manuf_str), "+Ublox\r");
    }
    else if (config->board == BOARD_TYPE_BASE_UM982 || config->board == BOARD_TYPE_ROVER_UM982)
    {
        sprintf(manuf_str, sizeof(manuf_str), "+Unicore\r");
    }
    else
    {
        RS485_AT_RESP_SEND_ERR();
        return;
    }

    RS485_AT_RESP_SEND(manuf_str);
}

static void at_read_config_handler(const char *param)
{
    user_params_t *params = flash_params_get_current();

    char resp_str[512];

    sprintf(resp_str, sizeof(resp_str),
             "+CONFIG=%s,%s,%s,%s,%s,%s,%s,%s,%s,%lf,%s\r",
             params->ntrip_url,
             params->ntrip_port,
             params->ntrip_id,
             params->ntrip_pw,
             params->ntrip_mountpoint,
             params->use_manual_position ? "MANUAL" : "AUTO",
             params->lat,
             params->lon,
             params->alt,
             params->baseline_len,
             params->ble_device_name);

    RS485_AT_RESP_SEND(resp_str);
}

static void at_set_baseline_handler(const char *param)
{
    
}

static void at_set_ntrip_ip_handler(const char *param)
{
    user_params_t *params = flash_params_get_current();
    char url[72];

    sprintf("+CASTER:%s:%s\r", params->ntrip_url, params->ntrip_port);
    RS485_AT_RESP_SEND(url);
}

static void at_set_ntrip_id_handler(const char *param)
{
    user_params_t *params = flash_params_get_current();
    char id[32];

    sprintf("+ID=%s\r", params->ntrip_id);
    RS485_AT_RESP_SEND(id);
}

static void at_set_ntrip_mountpoint_handler(const char *param)
{
    user_params_t *params = flash_params_get_current();
    char mountpoint[32];

    sprintf(mountpoint, "+MOUNTPOINT=%s\r", params->ntrip_mountpoint);
    RS485_AT_RESP_SEND(mountpoint);
}

static void at_set_ntrip_passwd_handler(const char *param)
{
    user_params_t *params = flash_params_get_current();
    char passwd[32];

    sprintf("+PASSWORD=%s\r", params->ntrip_pw);
    RS485_AT_RESP_SEND(passwd);
}

static void at_set_rtk_start_handler(const char *param)
{

}

static void at_set_rtk_stop_handler(const char *param)
{
    
}
