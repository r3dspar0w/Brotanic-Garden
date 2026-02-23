#undef __ARM_FP
#include "mbed.h"
#include "keypad.h"

using namespace std::chrono;

// 74C922 keypad decoder inputs and data-available line.
static DigitalIn kA(PB_8,  PullDown);
static DigitalIn kB(PB_9,  PullDown);
static DigitalIn kC(PB_10, PullDown);
static DigitalIn kD(PB_11, PullDown);
static DigitalIn kDA(PB_13, PullDown);

// Decoder output code to human-readable key mapping.
static const char CODE_TO_KEY[16] = {
    '1','2','3','F',
    '4','5','6','E',
    '7','8','9','D',
    'A','0','B','C'
};

// Debounce timer for data-available pulses.
static Timer deb;
static bool deb_started = false;
static bool last_da = false;

static uint8_t read_code()
{
    // Read 4-bit code from keypad decoder.
    uint8_t a = (uint8_t)kA.read();
    uint8_t b = (uint8_t)kB.read();
    uint8_t c = (uint8_t)kC.read();
    uint8_t d = (uint8_t)kD.read();
    return (a << 0) | (b << 1) | (c << 2) | (d << 3);
}

char keypad_getkey_nb()
{
    // One-time start for debounce timer.
    if (!deb_started) {
        deb.start();
        deb_started = true;
    }

    // Data available signal from decoder.
    bool da = (kDA.read() != 0);

    // Rising edge means a new key code is ready.
    if (!last_da && da) {
        if (deb.elapsed_time() < 25ms) {
            last_da = da;
            return 0;
        }
        deb.reset();

        uint8_t code = read_code();
        last_da = da;

        if (code < 16) {
            char k = CODE_TO_KEY[code];
            
            return k;
        }
        
        return 0;
    }

    last_da = da;
    return 0;
}

