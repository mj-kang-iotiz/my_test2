#include "gps.h"
#include <stdbool.h>

bool ubx_rover_init(gps_t* gps);
bool ubx_base_init(gps_t* gps);
bool ubx_moving_base_init(gps_t* gps);

bool ubx_factory_reset(gps_t* gps, ubx_init_complete_callback_t callback, void *user_data);

typedef enum {
  UBX_INIT_TYPE_BASE = 0,
  UBX_INIT_TYPE_ROVER,
  UBX_INIT_TYPE_MOVING_BASE,
} ubx_init_type_t;

bool ubx_change_baudrate_and_init(gps_t* gps, uint32_t baudrate, gps_id_t gps_id, ubx_init_type_t init_type);