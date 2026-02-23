#ifndef RFID_ADMIN_H
#define RFID_ADMIN_H

#undef __ARM_FP
#include "mbed.h"

extern char admin_door[12];

enum AdminEvent {
    ADMIN_NONE = 0,
    ADMIN_OPENED,
    ADMIN_CLOSED
};

void rfid_admin_init();
void rfid_admin_task();

bool admin_is_using();
AdminEvent admin_get_event();

#endif
