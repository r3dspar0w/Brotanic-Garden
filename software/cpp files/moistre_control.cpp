#undef __ARM_FP
#include "mbed.h"

#include "mositure_control.h"

using namespace std::chrono;

// Moisture sensor and ultrasonic pins.
#define MOISTURE_AO PC_3
#define MOISTURE_DO PC_2
#define TRIG_PIN PC_5
#define ECHO_PIN PC_6
#define PUMP_PIN PC_7

// Hardware interfaces.
static AnalogIn moisture_ao(MOISTURE_AO);
static DigitalIn moisture_do(MOISTURE_DO);
static DigitalOut trig(TRIG_PIN);
static InterruptIn echo(ECHO_PIN);
static DigitalOut pump(PUMP_PIN);

// Timers and scheduling helpers.
static Timer echo_timer;
static Ticker trig_ticker;
static Timeout pump_off_timeout;
static Timer sys_timer;
static Timer second_wave_timer;

// Ultrasonic capture state.
static volatile uint32_t t_rise_us = 0;
static volatile uint32_t pulse_us = 0;
static volatile bool got_pulse = false;

// Moisture session state.
static bool session_active = false;
static int session_last_trigger_ms = -1000000;
static int cooldown_until_ms = 0;

// Day counter and cached sensor values.
int g_day_count = 0;
static volatile int s_moisture_do_state = -1;
static volatile int s_moisture_ao_raw = -1;
static volatile int s_moisture_percent = -1;
static volatile int s_pending_day_update = 0;

// Wave gesture detection for day increments.
enum WaveState {
    WAVE_WAIT_FIRST = 0,
    WAVE_WAIT_FIRST_RELEASE,
    WAVE_WAIT_SECOND,
    WAVE_WAIT_SECOND_RELEASE
};

static WaveState wave_state = WAVE_WAIT_FIRST;

// Thresholds for moisture trigger and wave detection.
static constexpr float MOISTURE_TRIGGER_CM = 5.0f;
static constexpr float WAVE_THRESHOLD_CM = 10.0f;
static constexpr auto SECOND_WAVE_WINDOW = 5s;

// Clamp ADC raw value to a percent.
static int clamp_percent_from_u16(int raw)
{
    if (raw < 0) raw = 0;
    if (raw > 65535) raw = 65535;
    return (raw * 100) / 65535;
}

static void refresh_moisture_cache()
{
    // Update cached sensor values for quick reads.
    int do_state = moisture_do.read();
    int ao_raw = moisture_ao.read_u16();

    s_moisture_do_state = do_state;
    s_moisture_ao_raw = ao_raw;
    s_moisture_percent = clamp_percent_from_u16(ao_raw);
}

static void pump_off()
{
    pump = 0;
}

// Ultrasonic echo capture.
static void on_echo_rise()
{
    echo_timer.reset();
    echo_timer.start();
    t_rise_us = duration_cast<microseconds>(echo_timer.elapsed_time()).count();
}

static void on_echo_fall()
{
    uint32_t t_fall_us = duration_cast<microseconds>(echo_timer.elapsed_time()).count();
    echo_timer.stop();

    if (t_fall_us > t_rise_us) {
        pulse_us = t_fall_us - t_rise_us;
        got_pulse = true;
    }
}

static void send_trig()
{
    // 10us trigger pulse for ultrasonic sensor.
    trig = 0;
    wait_us(2);
    trig = 1;
    wait_us(10);
    trig = 0;
}

static inline int now_ms()
{
    return duration_cast<milliseconds>(sys_timer.elapsed_time()).count();
}

// Detect a hand near the moisture sensor to start a watering session.
static void handle_moisture_trigger(float distance_cm, int now)
{
    if (distance_cm > 1.5f && distance_cm <= MOISTURE_TRIGGER_CM) {
        if (now - session_last_trigger_ms > 1500) {
            session_active = true;
            session_last_trigger_ms = now;
            
        }
    }
}

static void handle_moisture_session(int now)
{
    // If dry, run pump for 1s, then cooldown.
    if (!session_active || now < cooldown_until_ms) {
        return;
    }

    int do_state = s_moisture_do_state;
    int ao_raw = s_moisture_ao_raw;
    float ao = (ao_raw >= 0) ? (ao_raw / 65535.0f) : 0.0f;

    if (do_state == 1) {
        pump = 1;
        pump_off_timeout.detach();
        pump_off_timeout.attach(&pump_off, 1s);
        

        cooldown_until_ms = now + 5000;
    } else {
        // Moist enough; stop session.
        session_active = false;
        cooldown_until_ms = 0;
        pump = 0;
    }
}

// Two-wave gesture increments the day counter.
static void handle_wave_day_marker(float distance_cm)
{
    bool hand_present = (distance_cm > 1.5f && distance_cm < WAVE_THRESHOLD_CM);

    if (wave_state == WAVE_WAIT_FIRST) {
        if (hand_present) {
            wave_state = WAVE_WAIT_FIRST_RELEASE;
            
        }
        return;
    }

    if (wave_state == WAVE_WAIT_FIRST_RELEASE) {
        if (!hand_present) {
            second_wave_timer.stop();
            second_wave_timer.reset();
            second_wave_timer.start();
            wave_state = WAVE_WAIT_SECOND;
            
        }
        return;
    }

    if (wave_state == WAVE_WAIT_SECOND) {
        if (second_wave_timer.elapsed_time() >= SECOND_WAVE_WINDOW) {
            second_wave_timer.stop();
            wave_state = WAVE_WAIT_FIRST;
            
            return;
        }

        if (hand_present) {
            if (g_day_count < 5) {
                g_day_count++;
                s_pending_day_update = g_day_count;
                
            } else {
                
            }
            wave_state = WAVE_WAIT_SECOND_RELEASE;
        }
        return;
    }

    if (wave_state == WAVE_WAIT_SECOND_RELEASE) {
        if (!hand_present) {
            second_wave_timer.stop();
            wave_state = WAVE_WAIT_FIRST;
            
        }
    }
}

void wave_moisture_init()
{
    // Reset outputs and state.
    trig = 0;
    pump = 0;

    session_active = false;
    session_last_trigger_ms = -1000000;
    cooldown_until_ms = 0;
    g_day_count = 0;
    s_moisture_do_state = -1;
    s_moisture_ao_raw = -1;
    s_moisture_percent = -1;
    s_pending_day_update = 0;
    wave_state = WAVE_WAIT_FIRST;

    sys_timer.stop();
    sys_timer.reset();
    sys_timer.start();

    second_wave_timer.stop();
    second_wave_timer.reset();

    echo.rise(&on_echo_rise);
    echo.fall(&on_echo_fall);
    trig_ticker.attach(&send_trig, 100ms);

    
}

void wave_moisture_task()
{
    // Cache sensor values.
    refresh_moisture_cache();

    if (got_pulse) {
        core_util_critical_section_enter();
        uint32_t us = pulse_us;
        got_pulse = false;
        core_util_critical_section_exit();

        float distance_cm = (us * 0.0343f) / 2.0f;
        int now = now_ms();

        handle_moisture_trigger(distance_cm, now);
        handle_wave_day_marker(distance_cm);
    }

    // Run pump control logic.
    handle_moisture_session(now_ms());
}

int wave_moisture_get_day_count()
{
    return g_day_count;
}

void wave_moisture_reset_day_count()
{
    g_day_count = 0;
    wave_state = WAVE_WAIT_FIRST;
    second_wave_timer.stop();
    second_wave_timer.reset();
    s_pending_day_update = 0;
    
}

int wave_moisture_take_day_update()
{
    int day = 0;
    core_util_critical_section_enter();
    day = s_pending_day_update;
    s_pending_day_update = 0;
    core_util_critical_section_exit();
    return day;
}

int wave_moisture_get_do_state()
{
    return s_moisture_do_state;
}

int wave_moisture_get_ao_raw()
{
    return s_moisture_ao_raw;
}

int wave_moisture_get_percent()
{
    return s_moisture_percent;
}

