/*
 * ESP32 Whoop — Sine Tone Player
 * Speaker:   SPKM.28.8.A
 * Amplifier: MAX98357A  (I2S)
 * Display:   ILI9341 SPI TFT  (320x240, landscape)
 * Touch:     XPT2046 SPI
 *
 * ┌──────────────────────────────────────────────────────┐
 * │  Pin assignments                                     │
 * │                                                      │
 * │  I2S / MAX98357A                                     │
 * │    BCLK  → GPIO 26                                   │
 * │    LRC   → GPIO 25                                   │
 * │    DIN   → GPIO 22                                   │
 * │                                                      │
 * │  SPI (shared bus, SPI2/HSPI)                         │
 * │    MOSI  → GPIO 23                                   │
 * │    MISO  → GPIO 19                                   │
 * │    CLK   → GPIO 18                                   │
 * │                                                      │
 * │  ILI9341 display                                     │
 * │    CS    → GPIO 5                                    │
 * │    DC    → GPIO 2                                    │
 * │    RST   → GPIO 4                                    │
 * │                                                      │
 * │  XPT2046 touch                                       │
 * │    CS    → GPIO 15                                   │
 * │    IRQ   → GPIO 21                                   │
 * └──────────────────────────────────────────────────────┘
 

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#include "ili9341.h"
#include "xpt2046.h"

static const char *TAG = "TonePlayer";

// ═══════════════════════════════════════════════════════════════════════════════
//  Pin definitions
// ═══════════════════════════════════════════════════════════════════════════════

// I2S
#define PIN_I2S_BCLK   26
#define PIN_I2S_LRC    25
#define PIN_I2S_DOUT   22

// SPI bus
#define PIN_SPI_MOSI   23
#define PIN_SPI_MISO   19
#define PIN_SPI_CLK    18

// ILI9341
#define PIN_TFT_CS     5
#define PIN_TFT_DC     2
#define PIN_TFT_RST    4

// XPT2046
#define PIN_TOUCH_CS   15
#define PIN_TOUCH_IRQ  21

// ═══════════════════════════════════════════════════════════════════════════════
//  Audio constants
// ═══════════════════════════════════════════════════════════════════════════════

#define SAMPLE_RATE      44100
#define DMA_BUF_COUNT    8
#define DMA_BUF_SAMPLES  512
#define AMPLITUDE        16383   // half of INT16_MAX  (~0 dBFS - 6 dB headroom)

// ═══════════════════════════════════════════════════════════════════════════════
//  Tone table
// ═══════════════════════════════════════════════════════════════════════════════

#define NUM_TONES 3

static const float TONE_FREQS[NUM_TONES]    = { 440.0f, 528.0f, 880.0f };
static const char *TONE_NAMES[NUM_TONES]    = { "A4", "C5", "A5" };
static const char *TONE_FREQSTR[NUM_TONES]  = { "440 Hz", "528 Hz", "880 Hz" };

// ═══════════════════════════════════════════════════════════════════════════════
//  Player state  (shared between audio task and UI task)
// ═══════════════════════════════════════════════════════════════════════════════

typedef enum { STATE_STOPPED = 0, STATE_PLAYING } player_state_t;

static volatile player_state_t  g_state        = STATE_STOPPED;
static volatile int             g_tone_index   = 0;
static volatile float           g_phase_inc    = 0.0f;
static volatile float           g_phase_accum  = 0.0f;

// Mutex protecting phase_inc / phase_accum changes
static SemaphoreHandle_t g_audio_mutex;

// ═══════════════════════════════════════════════════════════════════════════════
//  UI layout  (320 × 240 landscape)
// ═══════════════════════════════════════════════════════════════════════════════

#define SCR_W 320
#define SCR_H 240

typedef struct { int16_t x, y, w, h; } btn_rect_t;

static const btn_rect_t BTN_PREV = {  10, 160, 60, 50 };
static const btn_rect_t BTN_PLAY = {  80, 160, 70, 50 };
static const btn_rect_t BTN_STOP = { 160, 160, 70, 50 };
static const btn_rect_t BTN_NEXT = { 240, 160, 70, 50 };

// ═══════════════════════════════════════════════════════════════════════════════
//  Peripheral handles
// ═══════════════════════════════════════════════════════════════════════════════

static ili9341_t  g_tft;
static xpt2046_t  g_touch;

// ═══════════════════════════════════════════════════════════════════════════════
//  I2S init
// ═══════════════════════════════════════════════════════════════════════════════

static void i2s_init(void) {
    i2s_config_t cfg = {
        .mode                 = I2S_MODE_MASTER | I2S_MODE_TX,
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = DMA_BUF_COUNT,
        .dma_buf_len          = DMA_BUF_SAMPLES,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
    };
    i2s_pin_config_t pins = {
        .bck_io_num   = PIN_I2S_BCLK,
        .ws_io_num    = PIN_I2S_LRC,
        .data_out_num = PIN_I2S_DOUT,
        .data_in_num  = I2S_PIN_NO_CHANGE,
    };
    ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL));
    ESP_ERROR_CHECK(i2s_set_pin(I2S_NUM_0, &pins));
    i2s_zero_dma_buffer(I2S_NUM_0);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Audio task  (pinned to Core 1)
// ═══════════════════════════════════════════════════════════════════════════════

static void audio_task(void *arg) {
    static int16_t buf[DMA_BUF_SAMPLES * 2];   // stereo L+R

    while (1) {
        if (g_state == STATE_PLAYING) {
            xSemaphoreTake(g_audio_mutex, portMAX_DELAY);
            float ph   = g_phase_accum;
            float inc  = g_phase_inc;
            xSemaphoreGive(g_audio_mutex);

            for (int i = 0; i < DMA_BUF_SAMPLES; i++) {
                int16_t s    = (int16_t)(sinf(ph) * AMPLITUDE);
                buf[i*2]     = s;   // L
                buf[i*2 + 1] = s;   // R
                ph += inc;
                if (ph > (float)(2.0 * M_PI)) ph -= (float)(2.0 * M_PI);
            }

            xSemaphoreTake(g_audio_mutex, portMAX_DELAY);
            g_phase_accum = ph;
            xSemaphoreGive(g_audio_mutex);
        } else {
            memset(buf, 0, sizeof(buf));
        }

        size_t written = 0;
        i2s_write(I2S_NUM_0, buf, sizeof(buf), &written, portMAX_DELAY);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Player control helpers
// ═══════════════════════════════════════════════════════════════════════════════

static void set_tone(int idx) {
    xSemaphoreTake(g_audio_mutex, portMAX_DELAY);
    g_tone_index  = idx;
    g_phase_accum = 0.0f;
    g_phase_inc   = (float)(2.0 * M_PI) * TONE_FREQS[idx] / (float)SAMPLE_RATE;
    xSemaphoreGive(g_audio_mutex);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  UI draw helpers
// ═══════════════════════════════════════════════════════════════════════════════


// ═══════════════════════════════════════════════════════════════════════════════
//  Touch hit-test
// ═══════════════════════════════════════════════════════════════════════════════


// ═══════════════════════════════════════════════════════════════════════════════
//  UI / touch task  (runs on Core 0 via app_main)
// ═══════════════════════════════════════════════════════════════════════════════

static void ui_task(void *arg) {
    TickType_t last_touch = 0;
    const TickType_t DEBOUNCE = pdMS_TO_TICKS(300);

    while (1) {
        touch_point_t pt;
        if (xpt2046_read(&g_touch, &pt)) {
            TickType_t now = xTaskGetTickCount();
            if (now - last_touch > DEBOUNCE) {
                last_touch = now;

                if (in_btn(&BTN_PLAY, pt.x, pt.y)) {
                    g_state = STATE_PLAYING;
                    update_status_display(STATE_PLAYING);
                }
                else if (in_btn(&BTN_STOP, pt.x, pt.y)) {
                    g_state = STATE_STOPPED;
                    xSemaphoreTake(g_audio_mutex, portMAX_DELAY);
                    g_phase_accum = 0.0f;
                    xSemaphoreGive(g_audio_mutex);
                    update_status_display(STATE_STOPPED);
                }
                else if (in_btn(&BTN_NEXT, pt.x, pt.y)) {
                    int next = (g_tone_index + 1) % NUM_TONES;
                    set_tone(next);
                    update_tone_display(next);
                }
                else if (in_btn(&BTN_PREV, pt.x, pt.y)) {
                    int prev = (g_tone_index - 1 + NUM_TONES) % NUM_TONES;
                    set_tone(prev);
                    update_tone_display(prev);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  app_main
// ═══════════════════════════════════════════════════════════════════════════════

void app_main(void) {
    ESP_LOGI(TAG, "Booting...");

    // Mutex
    g_audio_mutex = xSemaphoreCreateMutex();

    // I2S / audio
    i2s_init();
    set_tone(0);

    // Display — SPI2 (HSPI)
    ili9341_init(&g_tft,
                 SPI2_HOST,
                 PIN_SPI_MOSI, PIN_SPI_MISO, PIN_SPI_CLK,
                 PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST,
                 40 * 1000 * 1000);   // 40 MHz

    // Touch — same SPI bus, lower speed
    xpt2046_init(&g_touch,
                 SPI2_HOST,
                 PIN_TOUCH_CS, PIN_TOUCH_IRQ,
                 2 * 1000 * 1000);    // 2 MHz

    // Optional: tune calibration for your specific screen
    xpt2046_calibrate(&g_touch, 200, 3800, 200, 3800, SCR_W, SCR_H);

    // Draw initial UI
    draw_ui_skeleton();
    update_tone_display(g_tone_index);
    update_status_display(g_state);

    // Audio task on Core 1, high priority
    xTaskCreatePinnedToCore(audio_task, "audio", 4096, NULL, 5, NULL, 1);

    // UI task on Core 0
    xTaskCreatePinnedToCore(ui_task, "ui", 4096, NULL, 3, NULL, 0);

    ESP_LOGI(TAG, "Ready.");
}*/
