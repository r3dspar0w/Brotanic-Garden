#undef __ARM_FP
#include "mbed.h"
#include <cstring>
#include <cstdio>

#include "lcd.h"
#include "keypad.h"
#include "door_control.h"
#include "rfid_admin.h"
#include "keypad_display.h"
#include "environment.h"   
#include "mositure_control.h"
#include "wifi.h"

using namespace std::chrono;

// UI timing settings.
static constexpr auto UI_INACTIVITY_TIMEOUT   = 20s; 
static constexpr auto DOOR_INACTIVITY_TIMEOUT = 20s; 


static constexpr auto SERVO_FINISH_MS = 1500ms;

// LCD helper functions (cursor and text primitives).
static void lcd_set_cursor(int row, int col)
{
    int addr = (row == 0) ? 0x80 : 0xC0;
    lcd_write_cmd(addr + col);
}

static void lcd_write_string(const char *s)
{
    while (*s) lcd_write_data((unsigned char)(*s++));
}

static void lcd_clear_row(int row)
{
    lcd_set_cursor(row, 0);
    for (int i = 0; i < 20; i++) lcd_write_data(' ');
    lcd_set_cursor(row, 0);
}

static void lcd_clear_all()
{
    lcd_clear_row(0);
    lcd_clear_row(1);
}

static void lcd_backlight_on()
{
}

static void lcd_create_char(uint8_t location, const uint8_t charmap[8])
{
    location &= 0x7;
    lcd_write_cmd(0x40 | (location << 3));
    for (int i = 0; i < 8; i++) lcd_write_data(charmap[i]);
}

// Custom LCD glyphs for UI icons.
static const uint8_t CHAR_LOCK[8]   = {0b01110,0b10001,0b10001,0b11111,0b11011,0b11011,0b11111,0b00000};
static const uint8_t CHAR_UNLOCK[8] = {0b01110,0b10001,0b00001,0b11111,0b11011,0b11011,0b11111,0b00000};
static const uint8_t CHAR_TEMP[8]   = {0b00100,0b01010,0b01010,0b01010,0b01010,0b11111,0b11111,0b01110};
static const uint8_t CHAR_HUMID[8]  = {0b00100,0b00100,0b01010,0b01010,0b10001,0b10001,0b01110,0b00000};
static const uint8_t CHAR_MOIST[8]  = {0b00100,0b01110,0b11111,0b11111,0b11111,0b01110,0b00100,0b00000};
static const uint8_t CHAR_WATER[8]  = {0b00100,0b00100,0b01010,0b01010,0b10001,0b10001,0b01110,0b00000};
static const uint8_t CHAR_PEACE[8]  = {0b00100,0b00100,0b01110,0b11111,0b11111,0b01110,0b00100,0b00000};
static const uint8_t CHAR_SPIDER[8] = {0b00100,0b10101,0b01110,0b11111,0b11111,0b01110,0b10101,0b00100};

// Screen render helpers.
static void draw_home()
{
    lcd_clear_all();
    lcd_set_cursor(0,0);
    lcd_write_data(1);
    lcd_write_string("PRESS A TO UNLOCK");
    lcd_set_cursor(1,0);
    lcd_write_string("PRESS B FOR STATS");
}

static void draw_pin_screen()
{
    lcd_clear_all();
    lcd_set_cursor(0,0);
    lcd_write_string("ENTER PASSWORD:");
    lcd_set_cursor(1,0);
    lcd_write_string("____ A=OK B=CLR");
    lcd_set_cursor(1,0);
}

static void draw_ready_close()
{
    lcd_clear_all();
    lcd_set_cursor(0,0);
    lcd_write_data(1);
    lcd_write_string("READY TO CLOSE");
    lcd_set_cursor(1,0);
    lcd_write_string("A=CLOSE DOOR");
}


static void draw_ready_close_after_correct()
{
    lcd_clear_row(0);
    lcd_set_cursor(0,0);
    lcd_write_data(1);
    lcd_write_string("READY TO CLOSE");

    lcd_set_cursor(1,0);
    lcd_write_string("A=CLOSE DOOR        ");
}

static void draw_correct()
{
    lcd_clear_all();
    lcd_set_cursor(0,0);
    lcd_write_string("CORRECT");
}

static void draw_wrong()
{
    lcd_clear_all();
    lcd_set_cursor(0,0);
    lcd_write_string("WRONG");
    lcd_set_cursor(1,0);
    lcd_write_string("TRY AGAIN");
}

static void draw_admin_open()
{
    lcd_clear_all();
    lcd_set_cursor(0,0);
    lcd_write_data(1);
    lcd_write_string("ADMIN OPENED");
}

static void draw_admin_closed()
{
    lcd_clear_all();
    lcd_set_cursor(0,0);
    lcd_write_data(0);
    lcd_write_string("ADMIN CLOSED");
}

static void draw_stats_menu()
{
    lcd_clear_all();
    lcd_set_cursor(0,0);
    lcd_write_string("STATS 5/6/7");
    lcd_set_cursor(1,0);
    lcd_write_string("5=T 6=H 7=M");
}

static void draw_plant_select()
{
    lcd_clear_all();
    lcd_set_cursor(0,0);
    lcd_write_string("Select plant 1/2/3");
    lcd_set_cursor(1,0);
    lcd_write_data(5);
    lcd_write_string("1 ");
    lcd_write_data(6);
    lcd_write_string("2 ");
    lcd_write_data(7);
    lcd_write_string("3");
}

static void draw_plant_choice(const char *plant_name)
{
    char line[21];
    memset(line, 0, sizeof(line));

    lcd_clear_all();
    lcd_set_cursor(0,0);
    snprintf(line, sizeof(line), "Choice: %s", plant_name);
    lcd_write_string(line);
    lcd_set_cursor(1,0);
    lcd_write_string("Returning home...");
}

static void draw_plant_still_growing()
{
    lcd_clear_all();
    lcd_set_cursor(0,0);
    lcd_write_string("Your plant is still");
    lcd_set_cursor(1,0);
    lcd_write_string("growing!");
}

static void draw_day_completed(int day)
{
    char line[21];
    memset(line, 0, sizeof(line));

    lcd_clear_all();

    lcd_set_cursor(0,0);
    lcd_write_data(5); 
    snprintf(line, sizeof(line), "DAY %d COMPLETED", day);
    lcd_write_string(line);

    lcd_set_cursor(1,0);
    if (day >= 5) {
        lcd_write_data(1); 
        lcd_write_string("PRESS C NEW PLANT");
    } else {
        lcd_write_data(6); 
        lcd_write_string("KEEP GROWING!");
    }
}


static void draw_stat(char k)
{
    lcd_clear_all();
    lcd_set_cursor(0,0);

    char line[21];
    memset(line, 0, sizeof(line));

    if (k == '5') {
        if (g_temperature_c >= 0) {
            
            snprintf(line, sizeof(line), "TEMP: %d C", (int)g_temperature_c);
        } else {
            snprintf(line, sizeof(line), "TEMP: ERR");
        }
        lcd_write_data(2);
        lcd_write_string(line);
    }
    else if (k == '6') {
        if (g_humidity_pct >= 0) {
            
            snprintf(line, sizeof(line), "HUMID: %d %%", (int)g_humidity_pct);
        } else {
            snprintf(line, sizeof(line), "HUMID: ERR");
        }
        lcd_write_data(3);
        lcd_write_string(line);
    }
    else if (k == '7') {
        int moist_pct = wave_moisture_get_percent();
        int moist_do = wave_moisture_get_do_state();

        if (moist_pct >= 0) {
            const char *status = (moist_do == 1) ? "DRY" : "WET";
            snprintf(line, sizeof(line), "MOIST:%3d%% %s", moist_pct, status);
        } else {
            snprintf(line, sizeof(line), "MOIST: ERR");
        }

        lcd_write_data(4);
        lcd_write_string(line);
    }

    lcd_set_cursor(1,0);
    lcd_write_string("AUTO HOME 10S");
}

// UI state machine.
enum UiState {
    UI_HOME = 0,
    UI_PIN,
    UI_READY_CLOSE,
    UI_STATS_MENU,
    UI_STATS_SHOW,
    UI_PLANT_SELECT,
    UI_ADMIN_HOLD_OPEN,
    UI_ADMIN_CLOSED_2S
};

static UiState state = UI_HOME;

// Optional debug helper for state names.
static const char *ui_state_name(UiState s)
{
    switch (s) {
        case UI_HOME: return "HOME";
        case UI_PIN: return "PIN";
        case UI_READY_CLOSE: return "READY_CLOSE";
        case UI_STATS_MENU: return "STATS_MENU";
        case UI_STATS_SHOW: return "STATS_SHOW";
        case UI_PLANT_SELECT: return "PLANT_SELECT";
        case UI_ADMIN_HOLD_OPEN: return "ADMIN_HOLD_OPEN";
        case UI_ADMIN_CLOSED_2S: return "ADMIN_CLOSED_2S";
        default: return "UNKNOWN";
    }
}

static void ui_change_state(UiState next, const char *reason)
{
    if (state != next) {
        
    }
    state = next;
}

// PIN entry buffer and configuration.
static const char PASS[5] = "1234";
static char pinBuf[5];
static int  pinPos = 0;

// Timers for UI behavior.
static Timer ui_inactivity;
static Timer admin_closed_timer;
static Timer stats_timer;


static Timer door_inactivity;
static bool user_opened_door = false;
static bool door_timer_running = false;

// Timer helpers.
static void reset_ui_timer_only()
{
    ui_inactivity.stop();
    ui_inactivity.reset();
    ui_inactivity.start();
}

static void activity_both()
{
    reset_ui_timer_only();

    if (door_timer_running) {
        door_inactivity.stop();
        door_inactivity.reset();
        door_inactivity.start();
    }
}

static bool ui_timeout()
{
    return ui_inactivity.elapsed_time() >= UI_INACTIVITY_TIMEOUT;
}

static void start_user_door_timer()
{
    door_inactivity.stop();
    door_inactivity.reset();
    door_inactivity.start();
    door_timer_running = true;
}

static void pin_reset_display()
{
    pinPos = 0;
    pinBuf[0] = '\0';

    lcd_set_cursor(1,0);
    lcd_write_data('_');
    lcd_write_data('_');
    lcd_write_data('_');
    lcd_write_data('_');
    lcd_set_cursor(1,0);
}

static void pin_put_star(int idx)
{
    lcd_set_cursor(1, idx);
    lcd_write_data('*');
    lcd_set_cursor(1, idx+1);
}

void ui_init()
{
    // LCD init and custom characters.
    lcd_init();
    lcd_backlight_on();

    lcd_create_char(0, CHAR_LOCK);
    lcd_create_char(1, CHAR_UNLOCK);
    lcd_create_char(2, CHAR_TEMP);
    lcd_create_char(3, CHAR_HUMID);
    lcd_create_char(4, CHAR_MOIST);
    lcd_create_char(5, CHAR_WATER);
    lcd_create_char(6, CHAR_PEACE);
    lcd_create_char(7, CHAR_SPIDER);

    draw_home();
    ui_change_state(UI_HOME, "init");
    reset_ui_timer_only();
    
}

void ui_task()
{
    // RFID admin card has priority and can override the UI state.
    rfid_admin_task();

    
    AdminEvent ev = admin_get_event();
    if (ev != ADMIN_NONE) {
        activity_both();
        

        if (ev == ADMIN_OPENED) {
            user_opened_door = false;
            door_timer_running = false;

            draw_admin_open();
            ui_change_state(UI_ADMIN_HOLD_OPEN, "admin opened");
            return;
        }

        if (ev == ADMIN_CLOSED) {
            user_opened_door = false;
            door_timer_running = false;

            draw_admin_closed();
            admin_closed_timer.stop();
            admin_closed_timer.reset();
            admin_closed_timer.start();
            ui_change_state(UI_ADMIN_CLOSED_2S, "admin closed");
            return;
        }
    }

    if (admin_is_using()) {
        if (state != UI_ADMIN_HOLD_OPEN) {
            draw_admin_open();
            ui_change_state(UI_ADMIN_HOLD_OPEN, "admin hold");
        }
        return;
    }

    if (state == UI_ADMIN_CLOSED_2S) {
        if (admin_closed_timer.elapsed_time() >= 2s) {
            admin_closed_timer.stop();
            draw_home();
            ui_change_state(UI_HOME, "admin closed timeout");
            reset_ui_timer_only();
        }
        return;
    }

    // Auto-close if user leaves door open too long.
    if (user_opened_door && door_timer_running) {
        if (door_inactivity.elapsed_time() >= DOOR_INACTIVITY_TIMEOUT) {

            lcd_clear_all();
            door_close();
            ThisThread::sleep_for(SERVO_FINISH_MS);

            user_opened_door = false;
            door_timer_running = false;

            draw_home();
            ui_change_state(UI_HOME, "door inactivity auto-close");
            reset_ui_timer_only();
            return;
        }
    }

    // UI timeout returns to home.
    if (state != UI_HOME && state != UI_READY_CLOSE && ui_timeout()) {
        draw_home();
        ui_change_state(UI_HOME, "ui inactivity");
        reset_ui_timer_only();
        return;
    }

    int day_update = wave_moisture_take_day_update();
    if (day_update >= 1 && day_update <= 5) {
        draw_day_completed(day_update);
        telegram_notify_day_complete(day_update);
        ThisThread::sleep_for(2s);
        draw_home();
        ui_change_state(UI_HOME, "day update");
        reset_ui_timer_only();
        return;
    }

    // Stats screen auto-timeout.
    if (state == UI_STATS_SHOW) {
        if (stats_timer.elapsed_time() >= 10s) {
            stats_timer.stop();
            draw_home();
            ui_change_state(UI_HOME, "stats timeout");
            reset_ui_timer_only();
        }
        return;
    }

    // Non-blocking keypad read.
    char k = keypad_getkey_nb();
    if (k == 0) return;
    

    activity_both();

    // Plant day reset logic.
    if (k == 'C') {
        if (wave_moisture_get_day_count() >= 5) {
            wave_moisture_reset_day_count();
            draw_plant_select();
            ui_change_state(UI_PLANT_SELECT, "growth complete");
        } else {
            draw_plant_still_growing();
            ThisThread::sleep_for(5s);
            draw_home();
            ui_change_state(UI_HOME, "growth incomplete");
            reset_ui_timer_only();
        }
        return;
    }

    if (state == UI_HOME) {
        if (k == 'A') {
            draw_pin_screen();
            ui_change_state(UI_PIN, "A from home");
            pin_reset_display();
        } else if (k == 'B') {
            draw_stats_menu();
            ui_change_state(UI_STATS_MENU, "B from home");
        }
        return;
    }

    if (state == UI_PIN) {
        if (k >= '0' && k <= '9') {
            if (pinPos < 4) {
                pinBuf[pinPos++] = k;
                pinBuf[pinPos] = '\0';
                pin_put_star(pinPos - 1);
            }
            return;
        }

        if (k == 'B') {
            pin_reset_display();
            return;
        }

        if (k == 'A') {
            if (pinPos == 4 && strcmp(pinBuf, PASS) == 0) {
                draw_correct();
                ThisThread::sleep_for(350ms);

                door_open();
                door_set_owner("USER");

                user_opened_door = true;
                start_user_door_timer();

                draw_ready_close_after_correct();
                ui_change_state(UI_READY_CLOSE, "pin correct");
            } else {
                draw_wrong();
                ThisThread::sleep_for(650ms);
                draw_pin_screen();
                pin_reset_display();
            }
            return;
        }
        return;
    }

    if (state == UI_READY_CLOSE) {
        if (k == 'A') {
            lcd_clear_all();
            door_close();
            ThisThread::sleep_for(SERVO_FINISH_MS);

            user_opened_door = false;
            door_timer_running = false;

            draw_home();
            ui_change_state(UI_HOME, "manual close");
            reset_ui_timer_only();
            return;
        }
        return;
    }

    if (state == UI_STATS_MENU) {
        if (k == 'B') {
            draw_home();
            ui_change_state(UI_HOME, "stats back");
            reset_ui_timer_only();
            return;
        }

        if (k == '5' || k == '6' || k == '7') {
            draw_stat(k);
            stats_timer.stop();
            stats_timer.reset();
            stats_timer.start();
            ui_change_state(UI_STATS_SHOW, "stats selection");
            return;
        }
        return;
    }

    if (state == UI_PLANT_SELECT) {
        if (k == '1') {
            thingspeak_bridge_set_plant_type(PLANT_WATER_LILY);
            telegram_notify_new_plant(PLANT_WATER_LILY);
            draw_plant_choice("Water Lily");
            ThisThread::sleep_for(2s);
            draw_home();
            ui_change_state(UI_HOME, "plant 1 selected");
            reset_ui_timer_only();
            return;
        }

        if (k == '2') {
            thingspeak_bridge_set_plant_type(PLANT_PEACE_LILY);
            telegram_notify_new_plant(PLANT_PEACE_LILY);
            draw_plant_choice("Peace Lily");
            ThisThread::sleep_for(2s);
            draw_home();
            ui_change_state(UI_HOME, "plant 2 selected");
            reset_ui_timer_only();
            return;
        }

        if (k == '3') {
            thingspeak_bridge_set_plant_type(PLANT_SPIDER_LILY);
            telegram_notify_new_plant(PLANT_SPIDER_LILY);
            draw_plant_choice("Spider Lily");
            ThisThread::sleep_for(2s);
            draw_home();
            ui_change_state(UI_HOME, "plant 3 selected");
            reset_ui_timer_only();
            return;
        }
        return;
    }
}



