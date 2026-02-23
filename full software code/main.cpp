#undef __ARM_FP
#include "mbed.h"


// Project modules.
#include "door_control.h"
#include "rfid_admin.h"
#include "keypad_display.h"
#include "environment.h"
#include "mositure_control.h"
#include "ldr_fan_control.h"
#include "wifi.h"




using namespace std::chrono;


// USB serial connection for console output.
static BufferedSerial pc(USBTX, USBRX, 115200);



// mbed console override so printf uses USB serial.
FileHandle *mbed_override_console(int fd)
{
    (void)fd;
    return &pc;
}



int main()
{
    // Startup banner.
    printf("\r\n[BROTANIC] Booting...\r\n");

    // Watchdog to recover from hangs.
    Watchdog &wd = Watchdog::get_instance();
    wd.start(8000);
    
    door_control();
    rfid_admin_init();
    env_sensors_init();
    wave_moisture_init();
    ldr_fan_control_init();
    
    ui_init();
    thingspeak_bridge_init();
    
    // Heartbeat timer for debug output.
    Timer heartbeat_t;
    heartbeat_t.start();

    while (true) {
        env_sensors_task();
        wave_moisture_task();
        ldr_fan_control_task();
        
        ui_task();
        thingspeak_bridge_task();

        wd.kick();
        // Print a heartbeat every 5 seconds.
        if (heartbeat_t.elapsed_time() >= 5s) {
            heartbeat_t.reset();
            printf("[BROTANIC] alive\r\n");
        }
        // Small delay so the CPU is not maxed out.
        ThisThread::sleep_for(10ms);
    }
}
