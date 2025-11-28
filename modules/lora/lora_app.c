#include "lora.h"
#include "lora_app.h"
#include "lora_port.h"
#include "board_config.h"
#include "semphr.h"
#include <string.h>

#ifndef TAG
    #define TAG "LORA_APP"
#endif


#include "log.h"

static void lora_process_task(void *pvParameter);

typedef struct
{
    lora_t lora;
    QueueHandle_t queue;
    TaskHandle_t task;
    SemaphoreHandle_t mutex;
    bool initialized;
}lora_app_instance_t;

lora_app_instance_t instances;

void lora_instance_init(void)
{
    memset(&instances, 0, sizeof(lora_app_instance_t));
    lora_init(&instances.lora);

    lora_port_init_instance(&instances.lora);

#if LORA_MODE == LORA_MODE_BASE
    instances.queue = xQueueCreate(10, sizeof(uint8_t));
    if (instances.queue == NULL) {
      LOG_ERR("LORA 큐 생성 실패");
      return;
    }

#elif LORA_MODE == LORA_MODE_ROVER
    
#endif
    lora_port_set_queue(instances.queue);
    instances.mutex = xSemaphoreCreateMutex();
    lora_port_start(&instances.lora);

    BaseType_t ret =
        xTaskCreate(lora_process_task, "lora_app", 1024,
                    NULL,
                    tskIDLE_PRIORITY + 3, &instances.task);

    instances.initialized = true;

    LOG_INFO("LORA 인스턴스 초기화 완료");
}


static void lora_process_task(void *pvParameter)
{

}
