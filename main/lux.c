/*
 * ESP32 Automatic Screen Brightness Control
 * Sensor: VEML7700 (I2C, address 0x10)
 * Backlight: PWM via LEDC peripheral
 *
 * Wiring:
 *   VEML7700 SDA  -> ESP32 GPIO 21 (default SDA)
 *   VEML7700 SCL  -> ESP32 GPIO 22 (default SCL)
 *   VEML7700 VCC  -> 3.3V
 *   VEML7700 GND  -> GND
 *   TFT BL pin    -> ESP32 GPIO 32 (BACKLIGHT_PIN)
 *
 * Library required: Adafruit_VEML7700 (install via Library Manager)
 */

#include <Wire.h>
#include <Adafruit_VEML7700.h>

// ─── Pin & PWM config ──────────────────────────────────────────────
#define BACKLIGHT_PIN     32
#define LEDC_CHANNEL      0
#define LEDC_FREQ_HZ      5000    // 5 kHz — above audible range
#define LEDC_RESOLUTION   8       // 8-bit: 0–255

// ─── Brightness tuning ─────────────────────────────────────────────
#define MIN_BRIGHTNESS    10      // Never fully off (still readable)
#define MAX_BRIGHTNESS    255     // Full brightness
#define LUX_LOW           5.0f    // Below this → minimum brightness
#define LUX_HIGH          10000.0f // Above this → maximum brightness

// EMA smoothing factor (0.0–1.0). Lower = smoother but slower response
#define SMOOTH_ALPHA      0.15f

// How often to sample the sensor (milliseconds)
#define SAMPLE_INTERVAL_MS  200

// ─── Globals ───────────────────────────────────────────────────────
Adafruit_VEML7700 veml;

float   smoothedBrightness = MIN_BRIGHTNESS;
uint32_t lastSampleTime    = 0;

// ─── Map lux → brightness (logarithmic, matches human eye) ─────────
uint8_t luxToBrightness(float lux) {
  if (lux <= LUX_LOW)  return MIN_BRIGHTNESS;
  if (lux >= LUX_HIGH) return MAX_BRIGHTNESS;

  // Logarithmic mapping: log(lux/LUX_LOW) / log(LUX_HIGH/LUX_LOW)
  float logMin = log(LUX_LOW   > 0 ? LUX_LOW  : 0.01f);
  float logMax = log(LUX_HIGH);
  float logLux = log(lux);

  float normalized = (logLux - logMin) / (logMax - logMin);
  normalized = constrain(normalized, 0.0f, 1.0f);

  return (uint8_t)(MIN_BRIGHTNESS + normalized * (MAX_BRIGHTNESS - MIN_BRIGHTNESS));
}

// ─── Apply EMA smoothing to avoid flicker ──────────────────────────
float emaSmooth(float current, float newValue, float alpha) {
  return alpha * newValue + (1.0f - alpha) * current;
}

// ─── Setup ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Wire.begin();  // SDA=21, SCL=22 by default on ESP32

  // Configure LEDC (PWM) for backlight
  ledcSetup(LEDC_CHANNEL, LEDC_FREQ_HZ, LEDC_RESOLUTION);
  ledcAttachPin(BACKLIGHT_PIN, LEDC_CHANNEL);
  ledcWrite(LEDC_CHANNEL, MAX_BRIGHTNESS);  // Full brightness on boot

  // Initialize VEML7700
  if (!veml.begin()) {
    Serial.println("[ERROR] VEML7700 not found! Check wiring.");
    // Keep display on full brightness as fallback
    while (true) {
      ledcWrite(LEDC_CHANNEL, MAX_BRIGHTNESS);
      delay(1000);
    }
  }

  // ─── VEML7700 configuration ────────────────────────────────────
  // Gain: 1x (options: 1x, 2x, 1/4x, 1/8x)
  veml.setGain(VEML7700_GAIN_1);

  // Integration time: 100ms (options: 25, 50, 100, 200, 400, 800 ms)
  // Longer = more accurate in low light, slower response
  veml.setIntegrationTime(VEML7700_IT_100MS);

  Serial.println("[OK] VEML7700 initialized.");
  Serial.printf("Gain: 1x | Integration time: 100ms\n");
}

// ─── Main loop ─────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = now;

    // Read ambient light in lux
    float lux = veml.readLux();

    // Auto-range: VEML7700 can saturate at high gain + bright light
    // Adafruit library handles this internally if you use readLux(VEML_LUX_AUTO)
    // Uncomment the line below for auto-ranging mode instead:
    // float lux = veml.readLux(VEML_LUX_AUTO);

    if (lux < 0) {
      Serial.println("[WARN] Invalid lux reading, skipping.");
      return;
    }

    // Convert lux to target brightness
    uint8_t targetBrightness = luxToBrightness(lux);

    // Apply EMA smoothing
    smoothedBrightness = emaSmooth(smoothedBrightness, (float)targetBrightness, SMOOTH_ALPHA);

    // Write to PWM backlight
    uint8_t pwmValue = (uint8_t)constrain(smoothedBrightness, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
    ledcWrite(LEDC_CHANNEL, pwmValue);

    // Debug output
    Serial.printf("Lux: %8.2f | Target: %3d | Smoothed PWM: %3d\n",
                  lux, targetBrightness, pwmValue);
  }

  // Other tasks (display updates, touch, etc.) can run here without blocking
}