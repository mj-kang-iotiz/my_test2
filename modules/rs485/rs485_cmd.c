#include "rs485_cmd.h"
#include "rs485_app.h"
#include <stdio.h>
#include <string.h>

#ifndef TAG
  #define TAG "RS485_CMD"
#endif

#include "log.h"

static const char* rs485_resp_str_invalid = "+E01";
static const char* rs485_resp_str_param_err = "+E02";
static const char* rs485_resp_str_not_rdy = "+E03";
static const char* rs485_resp_str_err = "+ERROR";

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
    {NULL, NULL}
};

void rs485_at_cmd_handler(rs485_instance_t *inst)
{
    for (int i = 0; at_cmd_table[i].cmd != NULL; i++) {
        size_t cmd_len = strlen(at_cmd_table[i].cmd);

        if (strncmp(inst->parser.data, at_cmd_table[i].cmd, cmd_len) == 0) {
            LOG_INFO("Matched AT command: %s", at_cmd_table[i].cmd);
            at_cmd_table[i].handler(inst->parser.data + cmd_len);
            return;
        }
    }

    LOG_WARN("Unknown AT command: %s", inst->parser.data);
}

static void at_handler(const char *param)
{
    LOG_INFO("AT command handler called");
    rs485_send("OK\r\n", 4);
}

static void atz_handler(const char *param)
{
    LOG_INFO("ATZ command handler called");
    rs485_send("OK\r\n", 4);
}

static void atandz_handler(const char *param)
{
    LOG_INFO("AT&Z command handler called");
    rs485_send("OK\r\n", 4);
}

static void at_ver_handler(const char *param)
{
    LOG_INFO("AT+VER? command handler called");
    rs485_send("+VER:1.0.0\r\nOK\r\n", 17);
}

static void at_gps_manuf_handler(const char *param)
{
    LOG_INFO("AT+GPSMANUF? command handler called");
    rs485_send("+GPSMANUF:MANUFACTURER\r\nOK\r\n", 29);
}

static void at_read_config_handler(const char *param)
{
    LOG_INFO("AT+CONFIG? command handler called");
    rs485_send("+CONFIG:...\r\nOK\r\n", 17);
}

static void at_set_baseline_handler(const char *param)
{
    LOG_INFO("AT+SETBASELINE: command handler called, param: %s", param);
    rs485_send("OK\r\n", 4);
}

static void at_set_ntrip_ip_handler(const char *param)
{
    LOG_INFO("AT+CASTER: command handler called, param: %s", param);
    rs485_send("OK\r\n", 4);
}

static void at_set_ntrip_id_handler(const char *param)
{
    LOG_INFO("AT+ID= command handler called, param: %s", param);
    rs485_send("OK\r\n", 4);
}

static void at_set_ntrip_mountpoint_handler(const char *param)
{
    LOG_INFO("AT+MOUNTPOINT= command handler called, param: %s", param);
    rs485_send("OK\r\n", 4);
}

static void at_set_ntrip_passwd_handler(const char *param)
{
    LOG_INFO("AT+PASSWD= command handler called, param: %s", param);
    rs485_send("OK\r\n", 4);
}

static void at_set_rtk_start_handler(const char *param)
{
    LOG_INFO("AT+GUGUSTART command handler called");
    rs485_send("OK\r\n", 4);
}

static void at_set_rtk_stop_handler(const char *param)
{
    LOG_INFO("AT+GUGUSTOP command handler called");
    rs485_send("OK\r\n", 4);
}
