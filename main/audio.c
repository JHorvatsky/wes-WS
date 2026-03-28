/*
  ESP32 Whoop - Simple Sine Tone Player
  Speaker:   SPKM.28.8.A
  Amplifier: MAX98357A (I2S)
  Display:   Touchscreen (XPT2046 touch + ILI9341 or similar SPI LCD)

  I2S Wiring (MAX98357A):
    BCLK  -> GPIO 26
    LRC   -> GPIO 25
    DIN   -> GPIO 22

  Touchscreen SPI Wiring (adjust to your board):
    MOSI  -> GPIO 23
    MISO  -> GPIO 19
    CLK   -> GPIO 18
    CS    -> GPIO 5   (display)
    DC    -> GPIO 2
    RST   -> GPIO 4
    T_CS  -> GPIO 15  (touch)
*/

#include <driver/i2s.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>

// ──────────────────────────────────────────────
//  I2S / Audio config
// ──────────────────────────────────────────────
#define I2S_PORT        I2S_NUM_0
#define I2S_BCLK        26
#define I2S_LRC         25
#define I2S_DOUT        22

#define SAMPLE_RATE     44100
#define BITS_PER_SAMPLE 16
#define DMA_BUF_COUNT   8
#define DMA_BUF_LEN     512

// ──────────────────────────────────────────────
//  Display pins
// ──────────────────────────────────────────────
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4
#define TOUCH_CS 15

// ──────────────────────────────────────────────
//  Tone library (3 sine tones)
// ──────────────────────────────────────────────
const float TONES[] = { 440.0f, 528.0f, 880.0f };   // A4, C5(approx), A5
const char* TONE_NAMES[] = { "A4  440 Hz", "C5  528 Hz", "A5  880 Hz" };
const int   NUM_TONES    = 3;

// ──────────────────────────────────────────────
//  State
// ──────────────────────────────────────────────
enum PlayerState { STOPPED, PLAYING };
PlayerState playerState = STOPPED;
int         currentTone = 0;

// Sine wave generator state
float phaseAccum    = 0.0f;
float phaseInc      = 0.0f;   // updated when tone changes

// ──────────────────────────────────────────────
//  Objects
// ──────────────────────────────────────────────
Adafruit_ILI9341   tft   = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS);

// ──────────────────────────────────────────────
//  I2S init
// ──────────────────────────────────────────────
void initI2S() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = DMA_BUF_COUNT,
        .dma_buf_len          = DMA_BUF_LEN,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
        .fixed_mclk           = 0
    };
    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_BCLK,
        .ws_io_num    = I2S_LRC,
        .data_out_num = I2S_DOUT,
        .data_in_num  = I2S_PIN_NO_CHANGE
    };
    i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
    i2s_set_pin(I2S_PORT, &pins);
    i2s_zero_dma_buffer(I2S_PORT);
}

// ──────────────────────────────────────────────
//  Audio task — runs on core 1
// ──────────────────────────────────────────────
void audioTask(void* param) {
    const int CHUNK = DMA_BUF_LEN;
    int16_t  buf[CHUNK * 2];  // stereo (L+R)

    while (true) {
        if (playerState == PLAYING) {
            for (int i = 0; i < CHUNK; i++) {
                float sample = sinf(phaseAccum) * 16383.0f;
                int16_t s    = (int16_t)sample;
                buf[i * 2]     = s;   // L
                buf[i * 2 + 1] = s;   // R
                phaseAccum += phaseInc;
                if (phaseAccum > TWO_PI) phaseAccum -= TWO_PI;
            }
            size_t written = 0;
            i2s_write(I2S_PORT, buf, CHUNK * 2 * sizeof(int16_t), &written, portMAX_DELAY);
        } else {
            // Send silence so DMA buffer doesn't underrun
            memset(buf, 0, sizeof(buf));
            size_t written = 0;
            i2s_write(I2S_PORT, buf, sizeof(buf), &written, portMAX_DELAY);
        }
    }
}

// ──────────────────────────────────────────────
//  Set current tone frequency
// ──────────────────────────────────────────────
void setTone(int idx) {
    currentTone = idx;
    phaseAccum  = 0.0f;
    phaseInc    = (TWO_PI * TONES[idx]) / (float)SAMPLE_RATE;
}

// ──────────────────────────────────────────────
//  UI layout constants  (320 x 240 landscape)
// ──────────────────────────────────────────────
#define SCR_W  320
#define SCR_H  240

// Colour palette
#define COL_BG      0x0841   // near-black blue
#define COL_PANEL   0x1082   // dark panel
#define COL_ACCENT  0x07FF   // cyan
#define COL_PLAY    0x07E0   // green
#define COL_STOP    0xF800   // red
#define COL_NAV     0x867E   // grey-blue
#define COL_TEXT    0xFFFF   // white
#define COL_DIM     0x7BEF   // grey

// Button rects [x, y, w, h]
struct Btn { int16_t x, y, w, h; };

const Btn BTN_PREV = { 10,  160, 60, 50 };
const Btn BTN_PLAY = { 80,  160, 70, 50 };
const Btn BTN_STOP = { 160, 160, 70, 50 };
const Btn BTN_NEXT = { 240, 160, 70, 50 };

// ──────────────────────────────────────────────
//  Draw helpers
// ──────────────────────────────────────────────
void drawRoundBtn(const Btn& b, uint16_t col, const char* label, uint16_t textCol = COL_TEXT) {
    tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, col);
    tft.drawRoundRect(b.x, b.y, b.w, b.h, 8, COL_ACCENT);
    tft.setTextColor(textCol);
    tft.setTextSize(2);
    int tw = strlen(label) * 12;
    tft.setCursor(b.x + (b.w - tw) / 2, b.y + (b.h - 14) / 2);
    tft.print(label);
}

void drawUI() {
    // Background
    tft.fillScreen(COL_BG);

    // Title bar
    tft.fillRect(0, 0, SCR_W, 30, COL_PANEL);
    tft.setTextColor(COL_ACCENT);
    tft.setTextSize(2);
    tft.setCursor(8, 7);
    tft.print("ESP32 Tone Player");

    // Tone info panel
    tft.fillRoundRect(10, 40, 300, 60, 6, COL_PANEL);
    tft.drawRoundRect(10, 40, 300, 60, 6, COL_ACCENT);
    tft.setTextColor(COL_DIM);
    tft.setTextSize(1);
    tft.setCursor(18, 48);
    tft.print("NOW PLAYING");

    // Status bar area
    tft.fillRoundRect(10, 115, 300, 35, 6, COL_PANEL);
    tft.drawRoundRect(10, 115, 300, 35, 6, COL_ACCENT);

    // Buttons
    drawRoundBtn(BTN_PREV, COL_NAV,  "|<");
    drawRoundBtn(BTN_PLAY, COL_PLAY, "PLAY");
    drawRoundBtn(BTN_STOP, COL_STOP, "STOP");
    drawRoundBtn(BTN_NEXT, COL_NAV,  ">|");
}

void updateToneDisplay() {
    tft.fillRect(14, 60, 292, 32, COL_PANEL);
    tft.setTextColor(COL_TEXT);
    tft.setTextSize(2);
    // Center the name
    int tw = strlen(TONE_NAMES[currentTone]) * 12;
    tft.setCursor(18 + (284 - tw) / 2, 68);
    tft.print(TONE_NAMES[currentTone]);

    // Track indicator dots
    tft.fillRect(120, 90, 80, 8, COL_PANEL);
    for (int i = 0; i < NUM_TONES; i++) {
        uint16_t c = (i == currentTone) ? COL_ACCENT : COL_DIM;
        tft.fillCircle(148 + i * 12, 94, 3, c);
    }
}

void updateStatusDisplay() {
    tft.fillRect(14, 118, 292, 28, COL_PANEL);
    tft.setTextColor(playerState == PLAYING ? COL_PLAY : COL_STOP);
    tft.setTextSize(2);
    const char* st = playerState == PLAYING ? ">> PLAYING" : "[] STOPPED";
    int tw = strlen(st) * 12;
    tft.setCursor(14 + (292 - tw) / 2, 126);
    tft.print(st);
}

// ──────────────────────────────────────────────
//  Touch calibration (adjust for your screen)
//  Raw touch values -> screen pixels
// ──────────────────────────────────────────────
#define TS_MINX  200
#define TS_MAXX  3800
#define TS_MINY  200
#define TS_MAXY  3800

bool inBtn(const Btn& b, int16_t tx, int16_t ty) {
    return tx >= b.x && tx < b.x + b.w &&
           ty >= b.y && ty < b.y + b.h;
}

// ──────────────────────────────────────────────
//  Setup
// ──────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    // Display
    tft.begin();
    tft.setRotation(1);   // landscape

    // Touch
    touch.begin();
    touch.setRotation(1);

    // I2S
    initI2S();
    setTone(currentTone);

    // Draw initial UI
    drawUI();
    updateToneDisplay();
    updateStatusDisplay();

    // Audio task on core 1
    xTaskCreatePinnedToCore(audioTask, "audio", 4096, NULL, 1, NULL, 1);

    Serial.println("ESP32 Tone Player ready.");
}

// ──────────────────────────────────────────────
//  Main loop — touch handling
// ──────────────────────────────────────────────
unsigned long lastTouch = 0;
const unsigned long DEBOUNCE_MS = 300;

void loop() {
    if (!touch.touched()) return;

    unsigned long now = millis();
    if (now - lastTouch < DEBOUNCE_MS) return;
    lastTouch = now;

    TS_Point p = touch.getPoint();

    // Map raw touch coords to screen coords
    int16_t tx = map(p.x, TS_MINX, TS_MAXX, 0, SCR_W);
    int16_t ty = map(p.y, TS_MINY, TS_MAXY, 0, SCR_H);

    Serial.printf("Touch: raw(%d,%d) -> screen(%d,%d)\n", p.x, p.y, tx, ty);

    bool stateChanged = false;

    if (inBtn(BTN_PLAY, tx, ty)) {
        playerState  = PLAYING;
        stateChanged = true;
        Serial.println("Action: PLAY");
    }
    else if (inBtn(BTN_STOP, tx, ty)) {
        playerState  = STOPPED;
        phaseAccum   = 0.0f;
        stateChanged = true;
        Serial.println("Action: STOP");
    }
    else if (inBtn(BTN_NEXT, tx, ty)) {
        currentTone = (currentTone + 1) % NUM_TONES;
        setTone(currentTone);
        updateToneDisplay();
        Serial.printf("Action: NEXT -> tone %d\n", currentTone);
    }
    else if (inBtn(BTN_PREV, tx, ty)) {
        currentTone = (currentTone - 1 + NUM_TONES) % NUM_TONES;
        setTone(currentTone);
        updateToneDisplay();
        Serial.printf("Action: PREV -> tone %d\n", currentTone);
    }

    if (stateChanged) {
        updateStatusDisplay();
    }
}
