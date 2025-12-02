#ifndef RS485_APP_H
#define RS485_APP_H

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "rs485.h"
#include <stdbool.h>
#include <stdint.h>

#define RS485_UART_MAX_RECV_SIZE 1024

typedef enum
{
  RS485_CMD_PARSE_STATE_NONE,
  RS485_CMD_PARSE_STATE_GOT_A,
  RS485_CMD_PARSE_STATE_DATA,
}rs485_cmd_parse_state_t;

typedef struct {
  char data[512];
  size_t len;
} rs485_tx_request_t;

typedef struct
{
  char data[100];
  size_t pos;
  char prev_char;
}rs485_cmd_parser_t;

typedef struct {
  rs485_t rs485;
  rs485_cmd_parse_state_t parse_stae;
  rs485_cmd_parser_t parser;
  QueueHandle_t rx_queue;
  TaskHandle_t rx_task;
  bool enabled;

  QueueHandle_t tx_queue;
  TaskHandle_t tx_task;

  SemaphoreHandle_t mutex;
} rs485_instance_t;

void rs485_init_all(void);

rs485_t *rs485_get_handle(void);
rs485_instance_t* rs485_get_instance(void);
bool rs485_send(const char *data, size_t len);

#endif