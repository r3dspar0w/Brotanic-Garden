#undef __ARM_FP
#include "mbed.h"
#include <cstdio>
#include <cstring>

#include "door_control.h"
#include "mositure_control.h"
#include "wifi.h"

using namespace std::chrono;


#define ESP_TX PC_10
#define ESP_RX PC_11

static BufferedSerial esp(ESP_TX, ESP_RX, 115200);


static const char *WIFI_SSID = "MAPP";
static const char *WIFI_PASS = "mapp1234";
static const char *TS_WRITE_KEY = "4GEGJH50BPACVOKJ";
static const char *TS_IP = "184.106.153.149";
static const char *TELEGRAM_SERVER_IP = "192.168.33.191";
static const int TELEGRAM_SERVER_PORT = 5000;
static const char *TELEGRAM_SERVER_PATH = "/send_notification";

// Current plant selection for uploads.
static PlantType g_plant_type = PLANT_WATER_LILY;

// State machine timing and buffers.
static Timer g_clock;
static uint32_t g_next_action_ms = 0;
static uint32_t g_cmd_deadline_ms = 0;
static uint32_t g_next_tg_attempt_ms = 0;

static char g_rx[1800];
static int g_rx_len = 0;

static char g_http_req[420];
static char g_cmd_buf[192];
static char g_notify_msg[120];
static bool g_notify_pending = false;
static bool g_notify_inflight = false;
static int g_last_notified_day = 0;

static const char *g_expect_a = nullptr;
static const char *g_expect_b = nullptr;
static bool g_cmd_active = false;

// Steps for non-blocking ESP control and ThingSpeak upload.
enum TsStep {
    TS_BOOT_DELAY = 0,
    TS_WIFI_AT,
    TS_WIFI_RST,
    TS_WIFI_POST_RST_DELAY,
    TS_WIFI_CWQAP,
    TS_WIFI_CWMODE,
    TS_WIFI_DNS,
    TS_WIFI_JOIN,
    TS_WIFI_CIFSR,
    TS_WIFI_CIPMUX,
    TS_IDLE_WAIT_UPLOAD,
    TS_OPEN_TCP_DOMAIN,
    TS_OPEN_TCP_IP,
    TS_CIPSEND_LEN,
    TS_SEND_HTTP,
    TS_CIPCLOSE,
    TS_NOTIFY_OPEN_TCP,
    TS_NOTIFY_CIPSEND_LEN,
    TS_NOTIFY_SEND_HTTP,
    TS_NOTIFY_CIPCLOSE,
    TS_BACKOFF
};

static TsStep g_step = TS_BOOT_DELAY;

static inline uint32_t now_ms()
{
    return duration_cast<milliseconds>(g_clock.elapsed_time()).count();
}

// Flush any pending serial bytes from ESP.
static void flush_esp()
{
    char c;
    while (esp.readable()) {
        esp.read(&c, 1);
    }
}

static void rx_reset()
{
    g_rx_len = 0;
    g_rx[0] = '\0';
}

static void rx_pump()
{
    // Pull a small batch of bytes each call to stay non-blocking.
    char c;
    int guard = 0;
    while (esp.readable() && guard < 256) {
        esp.read(&c, 1);
        guard++;

        if (g_rx_len < (int)sizeof(g_rx) - 1) {
            g_rx[g_rx_len++] = c;
            g_rx[g_rx_len] = '\0';
        } else {
            // Keep the newest half of the buffer if we overflow.
            const int keep = (int)sizeof(g_rx) / 2;
            memmove(g_rx, g_rx + g_rx_len - keep, keep);
            g_rx_len = keep;
            g_rx[g_rx_len++] = c;
            g_rx[g_rx_len] = '\0';
        }
    }
}

static void send_raw(const char *s)
{
    esp.write(s, strlen(s));
}

static void start_cmd_wait(const char *cmd, const char *ok_a, const char *ok_b, int timeout_ms)
{
    // Start a command and set expected tokens.
    flush_esp();
    rx_reset();
    send_raw(cmd);

    g_expect_a = ok_a;
    g_expect_b = ok_b;
    g_cmd_deadline_ms = now_ms() + (uint32_t)timeout_ms;
    g_cmd_active = true;
}

enum CmdResult {
    CMD_PENDING = 0,
    CMD_OK,
    CMD_FAIL
};

static CmdResult poll_cmd_wait()
{
    // Poll for expected tokens or timeout.
    if (!g_cmd_active) return CMD_FAIL;

    rx_pump();

    if (strstr(g_rx, "ERROR") || strstr(g_rx, "FAIL") || strstr(g_rx, "DNS Fail") || strstr(g_rx, "CLOSED")) {
        g_cmd_active = false;
        return CMD_FAIL;
    }

    if ((g_expect_a && g_expect_a[0] && strstr(g_rx, g_expect_a)) ||
        (g_expect_b && g_expect_b[0] && strstr(g_rx, g_expect_b))) {
        g_cmd_active = false;
        return CMD_OK;
    }

    if (now_ms() > g_cmd_deadline_ms) {
        g_cmd_active = false;
        return CMD_FAIL;
    }

    return CMD_PENDING;
}

static void build_http_request(int door_code, int who_code, int plant_code, int day)
{
    // Build a compact GET request for ThingSpeak update.
    snprintf(g_http_req, sizeof(g_http_req),
             "GET /update?api_key=%s&field1=%d&field2=%d&field3=%d&field4=%d HTTP/1.1\r\n"
             "Host: api.thingspeak.com\r\n"
             "Connection: close\r\n"
             "\r\n",
             TS_WRITE_KEY, door_code, who_code, plant_code, day);
}

static void url_encode_simple(const char *src, char *dst, size_t dst_len)
{
    size_t out = 0;
    for (size_t i = 0; src[i] != '\0' && out + 1 < dst_len; i++) {
        unsigned char c = static_cast<unsigned char>(src[i]);
        bool safe = (c >= 'A' && c <= 'Z') ||
                    (c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') ||
                    c == '-' || c == '_' || c == '.' || c == '~';
        if (safe) {
            dst[out++] = (char)c;
        } else {
            if (out + 3 >= dst_len) break;
            static const char hex[] = "0123456789ABCDEF";
            dst[out++] = '%';
            dst[out++] = hex[(c >> 4) & 0xF];
            dst[out++] = hex[c & 0xF];
        }
    }
    dst[out] = '\0';
}

static void build_telegram_request(const char *msg)
{
    char encoded[128];
    url_encode_simple(msg, encoded, sizeof(encoded));

    snprintf(g_http_req, sizeof(g_http_req),
             "GET %s?message=%s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n",
             TELEGRAM_SERVER_PATH, encoded, TELEGRAM_SERVER_IP);
}

static void enter_step(TsStep next)
{
    // Transition to a new state and kick off the next action.
    g_step = next;

    switch (g_step) {
        case TS_BOOT_DELAY:
            g_next_action_ms = now_ms() + 1500;
            break;

        case TS_WIFI_AT:
            printf("[TS] connecting WiFi SSID=%s\r\n", WIFI_SSID);
            start_cmd_wait("AT\r\n", "OK", nullptr, 1500);
            break;

        case TS_WIFI_RST:
            start_cmd_wait("AT+RST\r\n", "OK", nullptr, 3000);
            break;

        case TS_WIFI_POST_RST_DELAY:
            g_next_action_ms = now_ms() + 1500;
            break;

        case TS_WIFI_CWQAP:
            start_cmd_wait("AT+CWQAP\r\n", "OK", nullptr, 2000);
            break;

        case TS_WIFI_CWMODE:
            start_cmd_wait("AT+CWMODE=1\r\n", "OK", nullptr, 2000);
            break;

        case TS_WIFI_DNS:
            start_cmd_wait("AT+CIPDNS=1,\"8.8.8.8\",\"1.1.1.1\"\r\n", "OK", nullptr, 2000);
            break;

        case TS_WIFI_JOIN:
            snprintf(g_cmd_buf, sizeof(g_cmd_buf), "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASS);
            start_cmd_wait(g_cmd_buf, "OK", nullptr, 20000);
            break;

        case TS_WIFI_CIFSR:
            start_cmd_wait("AT+CIFSR\r\n", "OK", nullptr, 3000);
            break;

        case TS_WIFI_CIPMUX:
            start_cmd_wait("AT+CIPMUX=0\r\n", "OK", nullptr, 2000);
            break;

        case TS_IDLE_WAIT_UPLOAD:
            printf("[TS] WiFi OK\r\n");
            g_next_action_ms = now_ms() + 500;
            break;

        case TS_OPEN_TCP_DOMAIN:
            start_cmd_wait("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80\r\n", "CONNECT", "OK", 10000);
            break;

        case TS_OPEN_TCP_IP:
            snprintf(g_cmd_buf, sizeof(g_cmd_buf), "AT+CIPSTART=\"TCP\",\"%s\",80\r\n", TS_IP);
            start_cmd_wait(g_cmd_buf, "CONNECT", "OK", 10000);
            break;

        case TS_CIPSEND_LEN:
            snprintf(g_cmd_buf, sizeof(g_cmd_buf), "AT+CIPSEND=%d\r\n", (int)strlen(g_http_req));
            start_cmd_wait(g_cmd_buf, ">", nullptr, 6000);
            break;

        case TS_SEND_HTTP:
            start_cmd_wait(g_http_req, "SEND OK", nullptr, 12000);
            break;

        case TS_CIPCLOSE:
            start_cmd_wait("AT+CIPCLOSE\r\n", "OK", nullptr, 3000);
            break;

        case TS_NOTIFY_OPEN_TCP:
            snprintf(g_cmd_buf, sizeof(g_cmd_buf),
                     "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n",
                     TELEGRAM_SERVER_IP, TELEGRAM_SERVER_PORT);
            start_cmd_wait(g_cmd_buf, "CONNECT", "OK", 8000);
            break;

        case TS_NOTIFY_CIPSEND_LEN:
            snprintf(g_cmd_buf, sizeof(g_cmd_buf), "AT+CIPSEND=%d\r\n", (int)strlen(g_http_req));
            start_cmd_wait(g_cmd_buf, ">", nullptr, 6000);
            break;

        case TS_NOTIFY_SEND_HTTP:
            start_cmd_wait(g_http_req, "SEND OK", nullptr, 12000);
            break;

        case TS_NOTIFY_CIPCLOSE:
            start_cmd_wait("AT+CIPCLOSE\r\n", "OK", nullptr, 3000);
            break;

        case TS_BACKOFF:
            printf("[TS] WiFi/upload fail, retrying...\r\n");
            send_raw("AT+CIPCLOSE\r\n");
            g_next_action_ms = now_ms() + 3000;
            break;
    }
}

void thingspeak_bridge_init()
{
    // Initialize state machine and timers.
    g_clock.stop();
    g_clock.reset();
    g_clock.start();

    thingspeak_bridge_set_plant_type(PLANT_WATER_LILY);
    rx_reset();
    g_cmd_active = false;
    printf("[TS] bridge init\r\n");
    g_next_tg_attempt_ms = now_ms() + 2000;
    g_notify_pending = false;
    g_notify_inflight = false;
    g_last_notified_day = 0;
    enter_step(TS_BOOT_DELAY);
}

void thingspeak_bridge_task()
{
    // Run one step of the non-blocking state machine.
    const uint32_t now = now_ms();

    switch (g_step) {
        case TS_BOOT_DELAY:
            if (now >= g_next_action_ms) enter_step(TS_WIFI_AT);
            break;

        case TS_WIFI_AT:
            if (poll_cmd_wait() == CMD_OK) enter_step(TS_WIFI_RST);
            else if (!g_cmd_active) enter_step(TS_BACKOFF);
            break;

        case TS_WIFI_RST:
            // RST response differs across ESP firmwares; continue once command completes.
            if (poll_cmd_wait() != CMD_PENDING) enter_step(TS_WIFI_POST_RST_DELAY);
            break;

        case TS_WIFI_POST_RST_DELAY:
            if (now >= g_next_action_ms) enter_step(TS_WIFI_CWQAP);
            break;

        case TS_WIFI_CWQAP:
            // CWQAP may return ERROR when already disconnected. Treat as non-fatal.
            if (poll_cmd_wait() != CMD_PENDING) enter_step(TS_WIFI_CWMODE);
            break;

        case TS_WIFI_CWMODE:
            // Keep behavior compatible with previous implementation (best-effort).
            if (poll_cmd_wait() != CMD_PENDING) enter_step(TS_WIFI_DNS);
            break;

        case TS_WIFI_DNS:
            // DNS setup can fail on some firmware/hotspots; continue anyway.
            if (poll_cmd_wait() != CMD_PENDING) enter_step(TS_WIFI_JOIN);
            break;

        case TS_WIFI_JOIN:
            if (poll_cmd_wait() == CMD_OK) enter_step(TS_WIFI_CIFSR);
            else if (!g_cmd_active) enter_step(TS_BACKOFF);
            break;

        case TS_WIFI_CIFSR:
            // Informational only; do not fail connection flow.
            if (poll_cmd_wait() != CMD_PENDING) enter_step(TS_WIFI_CIPMUX);
            break;

        case TS_WIFI_CIPMUX:
            // Keep best-effort behavior to avoid false negatives.
            if (poll_cmd_wait() != CMD_PENDING) enter_step(TS_IDLE_WAIT_UPLOAD);
            break;

        case TS_IDLE_WAIT_UPLOAD:
            if (now >= g_next_action_ms) {
                // Translate current states to numeric fields for ThingSpeak charts.
                int field1 = 0; // 0=CLOSE, 1=OPEN
                int field2 = 0; // 0=NONE, 1=USER, 2=ADMIN
                int field3 = 1; // 1=WATER, 2=PEACE, 3=SPIDER
                int field4 = 1; // day

                if (strcmp(door_status, "OPEN") == 0) field1 = 1;
                if (strcmp(door_owner, "ADMIN") == 0) field2 = 2;
                else if (strcmp(door_owner, "USER") == 0) field2 = 1;

                if (g_plant_type == PLANT_PEACE_LILY) field3 = 2;
                else if (g_plant_type == PLANT_SPIDER_LILY) field3 = 3;

                int day = wave_moisture_get_day_count();
                if (day >= 1 && day <= 5) field4 = day;

                build_http_request(field1, field2, field3, field4);
                printf("[TS] upload f1=%d f2=%d f3=%d f4=%d\r\n", field1, field2, field3, field4);
                enter_step(TS_OPEN_TCP_DOMAIN);
                break;
            }
            if (g_notify_pending && !g_notify_inflight && now >= g_next_tg_attempt_ms) {
                build_telegram_request(g_notify_msg);
                g_notify_inflight = true;
                enter_step(TS_NOTIFY_OPEN_TCP);
            }
            break;

        case TS_OPEN_TCP_DOMAIN: {
            CmdResult r = poll_cmd_wait();
            if (r == CMD_OK) enter_step(TS_CIPSEND_LEN);
            else if (r == CMD_FAIL) enter_step(TS_OPEN_TCP_IP);
            break;
        }

        case TS_OPEN_TCP_IP: {
            CmdResult r = poll_cmd_wait();
            if (r == CMD_OK) enter_step(TS_CIPSEND_LEN);
            else if (r == CMD_FAIL) enter_step(TS_BACKOFF);
            break;
        }

        case TS_CIPSEND_LEN:
            if (poll_cmd_wait() == CMD_OK) enter_step(TS_SEND_HTTP);
            else if (!g_cmd_active) enter_step(TS_BACKOFF);
            break;

        case TS_SEND_HTTP: {
            CmdResult r = poll_cmd_wait();
            if (r == CMD_OK) {
                printf("[TS] upload OK\r\n");
                enter_step(TS_CIPCLOSE);
            } else if (r == CMD_FAIL) {
                enter_step(TS_BACKOFF);
            }
            break;
        }

        case TS_CIPCLOSE: {
            CmdResult r = poll_cmd_wait();
            if (r == CMD_OK) {
                g_next_action_ms = now_ms() + 20000; // ThingSpeak >= 15s
                g_step = TS_IDLE_WAIT_UPLOAD;
            } else if (r == CMD_FAIL) {
                enter_step(TS_BACKOFF);
            }
            break;
        }

        case TS_NOTIFY_OPEN_TCP: {
            CmdResult r = poll_cmd_wait();
            if (r == CMD_OK) enter_step(TS_NOTIFY_CIPSEND_LEN);
            else if (r == CMD_FAIL) {
                g_notify_inflight = false;
                g_next_tg_attempt_ms = now_ms() + 15000;
                enter_step(TS_BACKOFF);
            }
            break;
        }

        case TS_NOTIFY_CIPSEND_LEN:
            if (poll_cmd_wait() == CMD_OK) enter_step(TS_NOTIFY_SEND_HTTP);
            else if (!g_cmd_active) {
                g_notify_inflight = false;
                g_next_tg_attempt_ms = now_ms() + 15000;
                enter_step(TS_BACKOFF);
            }
            break;

        case TS_NOTIFY_SEND_HTTP: {
            CmdResult r = poll_cmd_wait();
            if (r == CMD_OK) {
                enter_step(TS_NOTIFY_CIPCLOSE);
            } else if (r == CMD_FAIL) {
                g_notify_inflight = false;
                g_next_tg_attempt_ms = now_ms() + 15000;
                enter_step(TS_BACKOFF);
            }
            break;
        }

        case TS_NOTIFY_CIPCLOSE: {
            CmdResult r = poll_cmd_wait();
            if (r == CMD_OK) {
                g_notify_pending = false;
                g_notify_inflight = false;
                g_next_tg_attempt_ms = now_ms() + 15000;
                g_next_action_ms = now_ms() + 500;
                g_step = TS_IDLE_WAIT_UPLOAD;
            } else if (r == CMD_FAIL) {
                g_notify_inflight = false;
                g_next_tg_attempt_ms = now_ms() + 15000;
                enter_step(TS_BACKOFF);
            }
            break;
        }

        case TS_BACKOFF:
            if (now >= g_next_action_ms) enter_step(TS_WIFI_AT);
            break;
    }
}

void thingspeak_bridge_set_plant_type(PlantType plant)
{
    // Update plant selection used by uploads.
    g_plant_type = plant;
}

PlantType thingspeak_bridge_get_plant_type()
{
    return g_plant_type;
}

void telegram_notify_day_complete(int day)
{
    if (day != 3) return;
    if (day <= g_last_notified_day) return;

    g_last_notified_day = day;

    strncpy(g_notify_msg, "PLEASE REPLACE THE WATER", sizeof(g_notify_msg) - 1);
    g_notify_msg[sizeof(g_notify_msg) - 1] = '\0';
    g_notify_pending = true;

    if (g_step == TS_IDLE_WAIT_UPLOAD) {
        g_next_action_ms = now_ms();
    }
}

void telegram_notify_new_plant(PlantType plant)
{
    (void)plant;
    g_last_notified_day = 0;
}
