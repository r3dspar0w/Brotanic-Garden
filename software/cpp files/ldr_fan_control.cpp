#undef __ARM_FP
#include "mbed.h"

#include "ldr_fan_control.h"

using namespace std::chrono;

// LDR sensor inputs and outputs.
static DigitalIn ldr_do(PD_2);
static AnalogIn ldr_ao(PA_0);
static DigitalOut motor(PC_4);
static DigitalOut led_stop(PB_14);
static DigitalOut led_auto(PB_15);
static InterruptIn btn_stop(PC_12);
static InterruptIn btn_resume(PA_15);

// Fan control mode.
enum FanMode {
    FAN_MODE_AUTO = 0,
    FAN_MODE_FORCED_STOP
};

static volatile FanMode s_mode = FAN_MODE_AUTO;
static volatile bool s_stop_event = false;
static volatile bool s_resume_event = false;

// Debounce and sampling timers.
static Timer stop_debounce_t;
static Timer resume_debounce_t;
static Timer sample_t;
static Timer log_t;

// Lower voltage means brighter light (LDR pull-up).
static constexpr float LDR_ON_MAX_V = 0.30f;

static void on_stop_pressed()
{
    // Ignore bounce within 200ms.
    int ms = duration_cast<milliseconds>(stop_debounce_t.elapsed_time()).count();
    if (ms < 200) {
        return;
    }
    stop_debounce_t.reset();
    s_stop_event = true;
}

static void on_resume_pressed()
{
    // Ignore bounce within 200ms.
    int ms = duration_cast<milliseconds>(resume_debounce_t.elapsed_time()).count();
    if (ms < 200) {
        return;
    }
    resume_debounce_t.reset();
    s_resume_event = true;
}

void ldr_fan_control_init()
{
    // Initial outputs and mode.
    motor = 0;
    led_stop = 0;
    led_auto = 1;
    s_mode = FAN_MODE_AUTO;

    // Use 3.3V reference for analog conversion.
    ldr_ao.set_reference_voltage(3.3f);

    stop_debounce_t.stop();
    stop_debounce_t.reset();
    stop_debounce_t.start();

    resume_debounce_t.stop();
    resume_debounce_t.reset();
    resume_debounce_t.start();

    sample_t.stop();
    sample_t.reset();
    sample_t.start();

    log_t.stop();
    log_t.reset();
    log_t.start();

    btn_stop.fall(&on_stop_pressed);
    btn_resume.fall(&on_resume_pressed);

    
}

void ldr_fan_control_task()
{
    // Handle stop/resume events from buttons.
    if (s_stop_event) {
        core_util_critical_section_enter();
        s_stop_event = false;
        s_mode = FAN_MODE_FORCED_STOP;
        core_util_critical_section_exit();

        motor = 0;
        led_stop = 1;
        led_auto = 0;
        
    }

    if (s_resume_event) {
        core_util_critical_section_enter();
        s_resume_event = false;
        s_mode = FAN_MODE_AUTO;
        core_util_critical_section_exit();

        led_stop = 0;
        led_auto = 1;
        
    }

    // Sample LDR at 10 Hz.
    if (sample_t.elapsed_time() < 100ms) {
        return;
    }
    sample_t.reset();

    float ldr_v = ldr_ao.read() * 3.3f;
    int ldr_digital = ldr_do.read();

    bool should_log = (log_t.elapsed_time() >= 8s);
    if (should_log) {
        log_t.reset();
    }

    if (s_mode == FAN_MODE_AUTO) {
        // Auto mode: turn fan on if light is strong enough.
        bool hot_by_light = (ldr_v <= LDR_ON_MAX_V);
        motor = hot_by_light ? 1 : 0;
        led_stop = 0;
        led_auto = 1;

        if (should_log) {
            
        }
    } else {
        // Forced stop mode.
        motor = 0;
        led_stop = 1;
        led_auto = 0;

        if (should_log) {
            
        }
    }
}
