#include <cstdio>
// another librray that performs input output operations 
#include <cstring>

#undef __ARM_FP
#include "mbed.h"
#include "MFRC522.h"
#include "rfid_admin.h"
#include "door_control.h"

using namespace std::chrono;

#define RFID_RST_PIN PB_1
#define RFID_SS_PIN  PB_2


//craeate a RC522 object using SS and and RST pins 
static MFRC522 mfrc522(RFID_SS_PIN, RFID_RST_PIN);
static MFRC522::MIFARE_Key rkey;

//tags UID 
static char tagUID[] = "433EF905";
static char tagID[9];

//ttracks whether admin currently “owns” the door state 
static bool is_admin_using = false;
//stores the last admin event (opened/closed)
static volatile AdminEvent rfid_event = ADMIN_NONE;
//timer used to debounce repeated reads so the loop doesn’t spam toggles
static Timer admin_cooldown;

char admin_door[12] = "admin close";

//Checks if a new RFID card is present, then reads its UID using the MFRC522 library
//Returns true only when a UID was successfully read
static bool RFID_read_uid(char outUid[9])
{
    if (!mfrc522.PICC_IsNewCardPresent()) return false;
    if (!mfrc522.PICC_ReadCardSerial()) return false;

    char hex[3];
    for (uint8_t i = 0; i < 4; i++) {
        sprintf(hex, "%02X", mfrc522.uid.uidByte[i]);
        outUid[i * 2]     = hex[0];
        outUid[i * 2 + 1] = hex[1];
    }
    outUid[8] = '\0';
    

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return true;
}

//Resets the admin state
//timer used to debounce repeated reads
void rfid_admin_init()
{
    mfrc522.PCD_Init();
    for (byte i = 0; i < 6; i++) rkey.keyByte[i] = 0xFF;

    strcpy(admin_door, "admin close");
    is_admin_using = false;
    rfid_event = ADMIN_NONE;
    admin_cooldown.stop();
    admin_cooldown.reset();
    admin_cooldown.start();
    
}

//Main code to open and close door 
void rfid_admin_task()
{
    // Debounce repeated detections without blocking the main loop.
    if (admin_cooldown.elapsed_time() < 400ms) {
        return;
    }

    // Read UID once per polling cycle to avoid intermittent misses.
    if (!RFID_read_uid(tagID)) {
        return;
    }

    if (strcmp(tagID, tagUID) != 0) {
        admin_cooldown.reset();
        return;
    }

    if (!door_is_open()) {
        door_open();
        door_set_owner("ADMIN");
        is_admin_using = true;
        strcpy(admin_door, "admin open");
        rfid_event = ADMIN_OPENED;
        
    } else {
        door_close();
        is_admin_using = false;
        strcpy(admin_door, "admin close");
        rfid_event = ADMIN_CLOSED;
        
    }
    admin_cooldown.reset();
}

//getter function 
bool admin_is_using()
{
    return is_admin_using;
}

//getter function for keypad 
//use enum to represent the RFID event states as constants 
//any finite state or event we just use enum cause its the best and
//you can see the purpose of it easily 
AdminEvent admin_get_event()
{
    AdminEvent e = rfid_event;
    if (e != ADMIN_NONE) rfid_event = ADMIN_NONE;
    return e;
}
