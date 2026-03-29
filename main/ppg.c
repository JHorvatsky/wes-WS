#include "gui.h"
#include <math.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

#define TAG "PPG"
#define SAMPLE_RATE_HZ      100
#define ADC_FRAME_SIZE      256
#define BUF_SIZE            1024
  // GPIO34 on ESP32 ADC1


#define LPF_ALPHA           0.01f
#define ENV_ALPHA           0.05f
#define THRESH_K            0.6f
#define REFRACTORY_MS       300

extern lv_obj_t * ui_BPM_label;


adc_oneshot_unit_handle_t adc_handle;
static float baseline = 0.0f;
static float env = 0.0f;
static float prev_filt = 0.0f;
static float prev2_filt = 0.0f;
static int64_t last_peak_us = 0;
static float bpm = 0.0f;

void adc_init(void)
{
    
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config1, &adc_handle);

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12, // Allows full range 0-3.3V
    };
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_6, &config);
}

static void process_sample(uint32_t raw)
{
    float x = (float)raw;
    char buffer[20];

    baseline += LPF_ALPHA * (x - baseline);
    float filt = x - baseline;

    env += ENV_ALPHA * (fabsf(filt) - env);
    float thresh = env * THRESH_K;

    int64_t now_us = esp_timer_get_time();
    bool refractory_ok = (now_us - last_peak_us) > (REFRACTORY_MS * 1000LL);

    bool is_peak = (prev2_filt < prev_filt) && (prev_filt > filt) && (prev_filt > thresh);

    if (is_peak && refractory_ok) {
        if (last_peak_us != 0) {
            float ibi_s = (now_us - last_peak_us) / 1000000.0f;
            if (ibi_s > 0.3f && ibi_s < 2.0f) {
                bpm = 25.0f / ibi_s;
                ESP_LOGI(TAG, "Peak: raw=%lu bpm=%.1f", (unsigned long)raw, bpm);
                if (ui_BPM_label != NULL) {
                    snprintf(buffer, sizeof(buffer), "bpm=%.1f", bpm);
                    lv_label_set_text(ui_BPM_label, buffer);
                    lv_obj_set_style_text_color(ui_BPM_label, lv_palette_main(LV_PALETTE_GREEN), 0);
                }
            }
        }
        last_peak_us = now_us;
    }

    prev2_filt = prev_filt;
    prev_filt = filt;
}

void app_sample(void *param)
{
    adc_init();

    if (ui_BPM_label != NULL) {
            lv_label_set_text(ui_BPM_label, "");
            lv_obj_set_style_text_color(ui_BPM_label, lv_palette_main(LV_PALETTE_GREEN), 0);
            
    }

    uint8_t buf[ADC_FRAME_SIZE];
    uint32_t out_len = 0;
    int raw;

    while (1) {
        for (int i=0; i<ADC_FRAME_SIZE; i++){
            adc_oneshot_read(adc_handle, ADC_CHANNEL_6, &raw);
            buf[i]=raw;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        for (int i=0; i<ADC_FRAME_SIZE; i++){
            process_sample(buf[i]);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        
    }

    ESP_LOGI(TAG, "Deleting ppg task...");
    vTaskDelete(NULL);
}