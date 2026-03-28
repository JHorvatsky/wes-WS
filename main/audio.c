#include <driver/i2s.h>
#include <math.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define I2S_PORT I2S_NUM_0
#define I2S_BCLK 25
#define I2S_LRC  26
#define I2S_DOUT 27  // Changed for OLED SCL
#define SAMPLE_RATE 44100
#define FREQ 500
#define AMPLITUDE 16000

#define PLAY_BTN 0
#define STOP_BTN 2

bool playing = false;
float phase = 0.0f;
float inc = 2 * M_PI * FREQ / SAMPLE_RATE;
int16_t buffer[256 * 2];
size_t bytes_written;

void setup() {
  Serial.begin(115200);
  
  pinMode(PLAY_BTN, INPUT_PULLUP);
  pinMode(STOP_BTN, INPUT_PULLUP);
  
  // I2S setup
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
  
  // OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 alloc fail");
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Audio Player Ready");
  display.println("Play: GPIO0");
  display.println("Stop: GPIO2");
  display.display();
}

void loop() {
  // Button handling (debounce simple)
  if (digitalRead(PLAY_BTN) == LOW) {
    playing = true;
    delay(200);  // Debounce
  }
  if (digitalRead(STOP_BTN) == LOW) {
    playing = false;
    i2s_zero_dma_buffer(I2S_PORT);  // Flush
    delay(200);
  }
  
  // Generate & play if active
  if (playing) {
    for (int i = 0; i < 256; i++) {
      float sample = sin(phase) * AMPLITUDE;
      buffer[2*i] = sample;     // Left
      buffer[2*i+1] = sample;   // Right
      phase += inc;
      if (phase > 2 * M_PI) phase -= 2 * M_PI;
    }
    i2s_write(I2S_PORT, buffer, sizeof(buffer), &bytes_written, 10);
  } else {
    // Silence buffer
    memset(buffer, 0, sizeof(buffer));
    i2s_write(I2S_PORT, buffer, sizeof(buffer), &bytes_written, 10);
  }
  
  // Update display every 500ms
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 500) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println(playing ? "PLAYING TONE" : "STOPPED");
    display.print("Freq: "); display.print(FREQ); display.println(" Hz");
    display.print("Status: "); display.println(playing ? "ON" : "OFF");
    display.display();
    lastUpdate = millis();
  }
}