#ifndef BLE_APP_H
#define BLE_APP_H

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "ble.h"
#include <stdbool.h>
#include <stdint.h>

#define BLE_UART_MAX_RECV_SIZE 1024

typedef enum
{
  BLE_CMD_PARSE_STATE_NONE,
  BLE_CMD_PARSE_STATE_GOT_A,
  BLE_CMD_PARSE_STATE_DATA,
}ble_cmd_parse_state_t;

typedef struct {
  char data[512];
  size_t len;
  bool is_at;
} ble_tx_request_t;

typedef struct
{
  char data[100];
  size_t pos;
  char prev_char;
}ble_cmd_parser_t;

typedef struct {
  ble_t ble;
  ble_cmd_parse_state_t parse_stae;
  ble_cmd_parser_t parser;
  QueueHandle_t rx_queue;
  TaskHandle_t rx_task;
  bool enabled;

  QueueHandle_t tx_queue;
  TaskHandle_t tx_task;

  SemaphoreHandle_t mutex;
} ble_instance_t;

void ble_init_all(void);

ble_t *ble_get_handle(void);
ble_instance_t* ble_get_instance(void);
bool ble_send(const char *data, size_t len, bool is_at);

#endif
