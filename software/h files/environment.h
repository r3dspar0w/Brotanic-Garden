#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <cstdint>


extern volatile int g_temperature_c;   
extern volatile int g_humidity_pct;    


void env_sensors_init();


void env_sensors_task();

#endif
