// c string is a library that handles c style strings like character arrays
// we used this to store strings like copy OPEN and ADMIN
// we used strcpy mostly which copies string and strcmp which comapres string

#include <cstring>

#undef __ARM_FP
#include "mbed.h"
#include "door_control.h"

using namespace std::chrono;

// define constants is part of having good coding practices
// for a servo motor it uses pwm signal width to set angles
#define PULSE_WIDTH_0_DEGREE 1400
#define PULSE_WIDTH_N_90_DEGREE 600

// why static is because static makes it visible only in thsi file
// making it somewhat a private variable
static PwmOut servo(PA_7);
static bool door_state = false;

// stores strings for LCD display, website and debugging
// array size includes null terminator
char door_status[6] = "CLOSE";
char door_owner[6] = "NONE";

// all the functions have 2   properties
// property1: set door state
// property2 : set door owner

void door_control()
{
    servo.period_ms(20);
    servo.pulsewidth_us(PULSE_WIDTH_0_DEGREE);
    door_state = false;
    strcpy(door_status, "CLOSE");
    strcpy(door_owner, "NONE");
    ThisThread::sleep_for(120ms);
}

void door_open()
{
    servo.pulsewidth_us(PULSE_WIDTH_N_90_DEGREE);
    door_state = true;
    strcpy(door_status, "OPEN");
    ThisThread::sleep_for(120ms);
}

void door_close()
{
    servo.pulsewidth_us(PULSE_WIDTH_0_DEGREE);
    door_state = false;
    strcpy(door_status, "CLOSE");
    strcpy(door_owner, "NONE");
    ThisThread::sleep_for(120ms);
}

// use of getter function
bool door_is_open()
{
    return door_state;
}

// this is setting who opened the door
// especially necesarry to map to website code
//purpose of using const is just to protect the memory 
void door_set_owner(const char *owner)
{
    if (owner == nullptr)
    {
        strcpy(door_owner, "NONE");
        return;
    }

    if (strcmp(owner, "ADMIN") == 0)
    {
        strcpy(door_owner, "ADMIN");
    }
    else if (strcmp(owner, "USER") == 0)
    {
        strcpy(door_owner, "USER");
    }
    else
    {
        strcpy(door_owner, "NONE");
    }
}
