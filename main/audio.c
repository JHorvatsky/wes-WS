#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "lvgl.h"


#define SAMPLE_RATE     44100
#define PI              3.14159265358979323846
#define I2S_BCK_IO      GPIO_NUM_26
#define I2S_WS_IO       GPIO_NUM_25
#define I2S_DO_IO       GPIO_NUM_22

#define TAG "Song"

static i2s_chan_handle_t tx_handle = NULL;
extern lv_obj_t * ui_imepj;
extern int curr_freq;
extern int song_start;
static int freq_arr[3]={200,400,440};
i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_DO_IO,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };

void init_i2s_max98357a(void) {
    if (tx_handle != NULL) {return;}
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, &tx_handle, NULL);
    
    
}
void aud_dis(void){
    i2s_channel_enable(tx_handle);
    i2s_del_channel(tx_handle);
}
void audio_sine_task(void *pvParameters) {
    i2s_channel_init_std_mode(tx_handle, &std_cfg);
    i2s_channel_enable(tx_handle);

    const int num_samples = 128;
    int16_t samples[num_samples];
    float phase = 0;
    size_t bytes_written;

    while (1) {
        for (int i = 0; i < num_samples; i++) {
            // Generate sine wave: amplitude is set to 10000 (of max 32767 for 16-bit)
            samples[i] = (int16_t)(10000.0 * sin(phase));
            
            // Increment phase based on desired frequency
            phase += 2.0 * PI * freq_arr[curr_freq] / SAMPLE_RATE;
            if (phase >= 2.0 * PI) phase -= 2.0 * PI;
        }
        // Send data to MAX98357A via I2S DMA
        i2s_channel_write(tx_handle, samples, sizeof(samples), &bytes_written, portMAX_DELAY);
    }

    ESP_LOGI(TAG, "Deleting sine task...");
    aud_dis();
    vTaskDelete(NULL);
}
