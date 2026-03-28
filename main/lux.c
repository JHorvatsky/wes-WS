/*
 * ESP32 Automatic Screen Brightness Control
 * Sensor : VEML7700 (I2C, address 0x10)
 * Backlight : LEDC PWM peripheral
 * Framework : ESP-IDF (no Arduino)
 *
 * Wiring:
 *   VEML7700 SDA  -> GPIO 21
 *   VEML7700 SCL  -> GPIO 22
 *   VEML7700 VCC  -> 3.3V
 *   VEML7700 GND  -> GND
 *   TFT BL pin    -> GPIO 32
 */

/*Kod za app_main.c:

void app_main(void)
{
    // Hardware init
    i2c_master_init();
    ledc_backlight_init();
    veml7700_init();

    // Brightness runs forever in background on core 1
    xTaskCreatePinnedToCore(brightness_task, "brightness", 4096, NULL, 5, NULL, 1);

    // You are free to start all your other tasks here
    xTaskCreatePinnedToCore(display_task,    "display",    8192, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(touch_task,      "touch",      4096, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(wifi_task,       "wifi",       8192, NULL, 3, NULL, 0);
    // app_main() returns here — that's fine, all tasks keep running
}

*/


#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "brightness";

// ─── Pin config ────────────────────────────────────────────────────
#define I2C_PORT        I2C_NUM_0
#define I2C_SDA_PIN     21
#define I2C_SCL_PIN     22
#define I2C_FREQ_HZ     400000      // 400 kHz fast mode
#define I2C_TIMEOUT_MS  100

#define BACKLIGHT_PIN   32

// ─── LEDC config ───────────────────────────────────────────────────
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_FREQ_HZ    5000
#define LEDC_DUTY_RES   LEDC_TIMER_8_BIT   // 0–255

// ─── VEML7700 I2C address & registers ──────────────────────────────
#define VEML7700_ADDR           0x10

#define VEML7700_REG_ALS_CONF   0x00
#define VEML7700_REG_ALS         0x04
#define VEML7700_REG_WHITE       0x05

// ALS_CONF register bit fields
// [1:0] gain  [5:4] integration time  [3] interrupt enable  [0] shutdown
#define VEML7700_GAIN_1          (0x00 << 11)   // 1x
#define VEML7700_GAIN_2          (0x01 << 11)   // 2x
#define VEML7700_GAIN_1_8        (0x02 << 11)   // 1/8x
#define VEML7700_GAIN_1_4        (0x03 << 11)   // 1/4x

#define VEML7700_IT_100MS        (0x00 << 6)
#define VEML7700_IT_200MS        (0x01 << 6)
#define VEML7700_IT_400MS        (0x02 << 6)
#define VEML7700_IT_800MS        (0x03 << 6)
#define VEML7700_IT_50MS         (0x08 << 6)
#define VEML7700_IT_25MS         (0x0C << 6)

#define VEML7700_POWER_ON        0x00
#define VEML7700_SHUTDOWN        0x01

// ─── Brightness tuning ─────────────────────────────────────────────
#define MIN_BRIGHTNESS   10
#define MAX_BRIGHTNESS   255
#define LUX_LOW          5.0f
#define LUX_HIGH         10000.0f
#define SMOOTH_ALPHA     0.15f
#define SAMPLE_MS        200

// ─── I2C helpers ───────────────────────────────────────────────────

/**
 * Write a 16-bit value to a VEML7700 register (little-endian).
 */
static esp_err_t veml7700_write_reg(uint8_t reg, uint16_t value)
{
    uint8_t data[3] = {
        reg,
        (uint8_t)(value & 0xFF),         // low byte first
        (uint8_t)((value >> 8) & 0xFF)   // high byte second
    };

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (VEML7700_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, sizeof(data), true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd,
                        pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * Read a 16-bit value from a VEML7700 register.
 */
static esp_err_t veml7700_read_reg(uint8_t reg, uint16_t *out)
{
    uint8_t buf[2] = {0};

    // Write register address
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (VEML7700_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);

    // Repeated start, then read 2 bytes
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (VEML7700_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &buf[0], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &buf[1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd,
                        pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        *out = (uint16_t)(buf[0] | (buf[1] << 8));  // little-endian
    }
    return ret;
}

// ─── VEML7700 init ─────────────────────────────────────────────────

static esp_err_t veml7700_init(void)
{
    // ALS config: gain 1x, IT 100ms, power on, no interrupts
    uint16_t config = VEML7700_GAIN_1 | VEML7700_IT_100MS | VEML7700_POWER_ON;

    esp_err_t ret = veml7700_write_reg(VEML7700_REG_ALS_CONF, config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "VEML7700 init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "VEML7700 initialized (gain=1x, IT=100ms)");
    }
    return ret;
}

// ─── Lux calculation ───────────────────────────────────────────────
/*
 * Resolution table from VEML7700 datasheet (Table 1).
 * counts × resolution_factor = lux
 * For gain=1x, IT=100ms → resolution = 0.0576 lux/count
 */
static float veml7700_counts_to_lux(uint16_t counts)
{
    // Gain 1x, IT 100ms resolution factor
    const float resolution = 0.0576f;
    return (float)counts * resolution;
}

static esp_err_t veml7700_read_lux(float *lux_out)
{
    uint16_t raw = 0;
    esp_err_t ret = veml7700_read_reg(VEML7700_REG_ALS, &raw);
    if (ret == ESP_OK) {
        *lux_out = veml7700_counts_to_lux(raw);
    }
    return ret;
}

// ─── I2C peripheral init ───────────────────────────────────────────

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_SDA_PIN,
        .scl_io_num       = I2C_SCL_PIN,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };

    esp_err_t ret = i2c_param_config(I2C_PORT, &conf);
    if (ret != ESP_OK) return ret;

    return i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
}

// ─── LEDC (PWM backlight) init ─────────────────────────────────────

static esp_err_t ledc_backlight_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_MODE,
        .timer_num       = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz         = LEDC_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&timer);
    if (ret != ESP_OK) return ret;

    ledc_channel_config_t channel = {
        .gpio_num   = BACKLIGHT_PIN,
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CHANNEL,
        .timer_sel  = LEDC_TIMER,
        .duty       = MAX_BRIGHTNESS,   // full brightness on boot
        .hpoint     = 0,
    };
    return ledc_channel_config(&channel);
}

static void set_backlight(uint32_t duty)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

// ─── Brightness math ───────────────────────────────────────────────

static uint8_t lux_to_brightness(float lux)
{
    if (lux <= LUX_LOW)  return MIN_BRIGHTNESS;
    if (lux >= LUX_HIGH) return MAX_BRIGHTNESS;

    float log_min = logf(LUX_LOW);
    float log_max = logf(LUX_HIGH);
    float norm    = (logf(lux) - log_min) / (log_max - log_min);

    norm = norm < 0.0f ? 0.0f : (norm > 1.0f ? 1.0f : norm);
    return (uint8_t)(MIN_BRIGHTNESS + norm * (MAX_BRIGHTNESS - MIN_BRIGHTNESS));
}

// ─── Brightness control task ───────────────────────────────────────

static void brightness_task(void *arg)
{
    float smoothed = (float)MAX_BRIGHTNESS;

    while (1) {
        float lux = 0.0f;
        esp_err_t ret = veml7700_read_lux(&lux);

        if (ret == ESP_OK) {
            uint8_t target = lux_to_brightness(lux);

            // Exponential moving average — avoids flicker
            smoothed = SMOOTH_ALPHA * (float)target
                     + (1.0f - SMOOTH_ALPHA) * smoothed;

            uint32_t pwm = (uint32_t)(smoothed + 0.5f);  // round
            if (pwm < MIN_BRIGHTNESS) pwm = MIN_BRIGHTNESS;
            if (pwm > MAX_BRIGHTNESS) pwm = MAX_BRIGHTNESS;

            set_backlight(pwm);

            ESP_LOGD(TAG, "lux=%.2f  target=%u  pwm=%lu",
                     lux, target, pwm);
        } else {
            ESP_LOGW(TAG, "Sensor read error: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_MS));
    }
}

// ─── Entry point ───────────────────────────────────────────────────
/*
void app_main(void)
{
    ESP_LOGI(TAG, "Booting brightness controller");

    // Init I2C bus
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized (SDA=%d SCL=%d @ %d Hz)",
             I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ_HZ);

    // Init LEDC backlight (full brightness while sensor warms up)
    ESP_ERROR_CHECK(ledc_backlight_init());
    ESP_LOGI(TAG, "LEDC backlight on GPIO %d", BACKLIGHT_PIN);

    // Give VEML7700 ≥2.5 ms power-on settling time (datasheet §2.2)
    vTaskDelay(pdMS_TO_TICKS(10));

    // Init VEML7700 — halt on failure so the error is visible
    ESP_ERROR_CHECK(veml7700_init());

    // Spawn the brightness control task on core 1, 4 kB stack
    xTaskCreatePinnedToCore(
        brightness_task,
        "brightness",
        4096,
        NULL,
        5,          // priority
        NULL,
        1           // core 1, leaving core 0 for Wi-Fi/BT if needed
    );
}