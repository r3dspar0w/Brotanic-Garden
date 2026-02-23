#undef __ARM_FP
#include "mbed.h"
#include "DHT11.h"
#include "environment.h"

using namespace std::chrono;

// DHT11 data pin connection.
#define DHT11_PIN PB_7

// Latest sensor readings (shared with UI).
volatile int g_temperature_c = -1;
volatile int g_humidity_pct  = -1;

// DHT11 driver instance and sampling timer.
static DHT11 dht11(DHT11_PIN);
static Timer sample_timer;

// Read sensors every 1.5 seconds to avoid DHT11 lockups.
static constexpr auto SAMPLE_PERIOD = 1500ms;  

void env_sensors_init()
{
    // Reset cached values.
    g_temperature_c = -1;
    g_humidity_pct  = -1;

    // Start periodic sampling timer.
    sample_timer.stop();
    sample_timer.reset();
    sample_timer.start();

    
}

void env_sensors_task()
{
    
    if (sample_timer.elapsed_time() < SAMPLE_PERIOD) return;

    sample_timer.reset();

    int t = 0;
    int h = 0;

    
    // Read temperature and humidity (returns 0 on success).
    int result = dht11.readTemperatureHumidity(t, h);

    if (result == 0) {
        // Valid reading.
        g_temperature_c = t;
        g_humidity_pct  = h;
        
    } else {
        // Read failed; keep invalid values.
        g_temperature_c = -1;
        g_humidity_pct  = -1;
        
    }
}

