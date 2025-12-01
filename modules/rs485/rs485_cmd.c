#include "rs485_cmd.h"
#include "rs485_app.h"
#include <stdio.h>
#include <string.h>

#ifndef TAG
  #define TAG "RS485_CMD"
#endif

#include "log.h"

// Helper macro to send string literal with automatic length calculation (compile-time)
#define RS485_SEND_STR(str) rs485_send(str, sizeof(str) - 1)

// Helper function to send buffer with automatic length calculation (runtime)
static inline bool rs485_send_buf(const char *buf)
{
    return rs485_send(buf, strlen(buf));
}

// Helper function to send buffer with CRLF appended
static inline bool rs485_send_response(const char *buf)
{
    char temp[256];
    int len = snprintf(temp, sizeof(temp), "%s\r\n", buf);
    if (len > 0 && len < sizeof(temp)) {
        return rs485_send(temp, len);
    }
    return false;
}

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
    RS485_SEND_STR("OK\r\n");
}

static void atz_handler(const char *param)
{
    LOG_INFO("ATZ command handler called");
    RS485_SEND_STR("OK\r\n");
}

static void atandz_handler(const char *param)
{
    LOG_INFO("AT&Z command handler called");
    RS485_SEND_STR("OK\r\n");
}

static void at_ver_handler(const char *param)
{
    LOG_INFO("AT+VER? command handler called");
    RS485_SEND_STR("+VER:1.0.0\r\nOK\r\n");
}

static void at_gps_manuf_handler(const char *param)
{
    LOG_INFO("AT+GPSMANUF? command handler called");
    RS485_SEND_STR("+GPSMANUF:MANUFACTURER\r\nOK\r\n");
}

static void at_read_config_handler(const char *param)
{
    LOG_INFO("AT+CONFIG? command handler called");
    RS485_SEND_STR("+CONFIG:...\r\nOK\r\n");
}

static void at_set_baseline_handler(const char *param)
{
    LOG_INFO("AT+SETBASELINE: command handler called, param: %s", param);
    RS485_SEND_STR("OK\r\n");
}

static void at_set_ntrip_ip_handler(const char *param)
{
    LOG_INFO("AT+CASTER: command handler called, param: %s", param);
    RS485_SEND_STR("OK\r\n");
}

static void at_set_ntrip_id_handler(const char *param)
{
    LOG_INFO("AT+ID= command handler called, param: %s", param);
    RS485_SEND_STR("OK\r\n");
}

static void at_set_ntrip_mountpoint_handler(const char *param)
{
    LOG_INFO("AT+MOUNTPOINT= command handler called, param: %s", param);
    RS485_SEND_STR("OK\r\n");
}

static void at_set_ntrip_passwd_handler(const char *param)
{
    LOG_INFO("AT+PASSWD= command handler called, param: %s", param);
    RS485_SEND_STR("OK\r\n");
}

static void at_set_rtk_start_handler(const char *param)
{
    LOG_INFO("AT+GUGUSTART command handler called");
    RS485_SEND_STR("OK\r\n");
}

static void at_set_rtk_stop_handler(const char *param)
{
    LOG_INFO("AT+GUGUSTOP command handler called");
    RS485_SEND_STR("OK\r\n");
}
