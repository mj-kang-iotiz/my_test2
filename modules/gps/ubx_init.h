#include "gps.h"
#include <stdbool.h>

bool ubx_rover_init(gps_t* gps);
bool ubx_base_init(gps_t* gps);
bool ubx_moving_base_init(gps_t* gps);

bool ubx_factory_reset(gps_t* gps, ubx_init_complete_callback_t callback, void *user_data);