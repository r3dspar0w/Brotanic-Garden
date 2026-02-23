#ifndef WIFI_H
#define WIFI_H

enum PlantType {
    PLANT_WATER_LILY = 0,
    PLANT_PEACE_LILY,
    PLANT_SPIDER_LILY
};

void thingspeak_bridge_init();
void thingspeak_bridge_task();
void thingspeak_bridge_set_plant_type(PlantType plant);
PlantType thingspeak_bridge_get_plant_type();
void telegram_notify_day_complete(int day);
void telegram_notify_new_plant(PlantType plant);

#endif
