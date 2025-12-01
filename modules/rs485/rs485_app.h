#ifndef RS485_APP_H
#define RS485_APP_H

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "rs485.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  char data[256];
  size_t len;
} rs485_tx_request_t;

void rs485_init_all(void);

rs485_t *rs485_get_handle(void);

bool rs485_send(const char *data, size_t len);

#endif