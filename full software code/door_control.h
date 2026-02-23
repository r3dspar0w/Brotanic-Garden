#ifndef DOOR_CONTROL_H
#define DOOR_CONTROL_H

#undef __ARM_FP
#include "mbed.h"

// use of extern is just to state that the function doesnt exist in this file
// because global variables defined in one file may not be always be visible to others
extern char door_status[6]; // "OPEN" / "CLOSE"
extern char door_owner[6];  // "ADMIN" / "USER" / "NONE"

void door_control();
void door_open();
void door_close();
bool door_is_open();
void door_set_owner(const char *owner);

// mark end if conditional compilataion block
#endif
