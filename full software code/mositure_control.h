#ifndef MOSITURE_CONTROL_H
#define MOSITURE_CONTROL_H

void wave_moisture_init();

void wave_moisture_task();

extern int g_day_count;

int wave_moisture_get_day_count();
void wave_moisture_reset_day_count();
int wave_moisture_take_day_update();

int wave_moisture_get_do_state();
int wave_moisture_get_ao_raw();
int wave_moisture_get_percent();

#endif
