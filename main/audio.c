#include <driver/i2s.h>
#include <math.h>

#define I2S_PORT I2S_NUM_0
#define I2S_BCLK 25
#define I2S_LRC  26
#define I2S_DOUT 22
#define SAMPLE_RATE 44100
#define FREQ 500
#define AMPLITUDE 16000  // Safe for 16-bit

int16_t buffer[256 * 2];  // Stereo buffer

void setup() {
  Serial.begin(115200);
  
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true
  };
  
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
}

void loop() {
  static float phase = 0.0f;
  float inc = 2 * M_PI * FREQ / SAMPLE_RATE;
  
  for (int i = 0; i < 256; i++) {
    float sample = sin(phase) * AMPLITUDE;
    buffer[2*i] = sample;     // Left
    buffer[2*i+1] = sample;   // Right (mono mix)
    phase += inc;
    if (phase > 2 * M_PI) phase -= 2 * M_PI;
  }
  
  size_t bw;
  i2s_write(I2S_PORT, buffer, sizeof(buffer), &bw, portMAX_DELAY);
}