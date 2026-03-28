#include "gui.h"
#include <math.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_adc/adc_continuous.h"

#define TAG "PPG"
#define SAMPLE_RATE_HZ      5000
#define ADC_FRAME_SIZE      256
#define BUF_SIZE            4096

#define ADC_CH              ADC_CHANNEL_6   // GPIO34 on ESP32 ADC1
#define ADC_UNIT_USED       ADC_UNIT_1

#define LPF_ALPHA           0.01f
#define ENV_ALPHA           0.05f
#define THRESH_K            0.6f
#define REFRACTORY_MS       300

extern lv_obj_t * ui_ppgscr;
extern lv_obj_t * ui_Label9;
extern lv_obj_t * ui_BPM_label;


static adc_continuous_handle_t adc_handle;
static float baseline = 0.0f;
static float env = 0.0f;
static float prev_filt = 0.0f;
static float prev2_filt = 0.0f;
static int64_t last_peak_us = 0;
static float bpm = 0.0f;

// Add these globals:
static uint32_t sample_acc = 0;
static uint16_t sample_count = 0;
#define DECIMATION_FACTOR 50  // 5000Hz / 50 = 100Hz

void adc_init(void)
{
    adc_continuous_handle_cfg_t hcfg = {
        .max_store_buf_size = BUF_SIZE,
        .conv_frame_size = ADC_FRAME_SIZE,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&hcfg, &adc_handle));

    adc_digi_pattern_config_t pattern = {0};
    pattern.atten = ADC_ATTEN_DB_12;
    pattern.channel = ADC_CH;
    pattern.unit = ADC_UNIT_USED;
    pattern.bit_width = ADC_BITWIDTH_12;

    adc_continuous_config_t cfg = {
        .sample_freq_hz = SAMPLE_RATE_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
        .pattern_num = 1,
        .adc_pattern = &pattern,
    };

    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &cfg));
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
}



static void process_sample(uint32_t raw)
{
    char buf[20];

    // === DECIMATION: Average 50 samples → 100Hz effective ===
    sample_acc += raw;
    sample_count++;
    
    if (sample_count < DECIMATION_FACTOR) {
        return;  // Skip until full average
    }
    
    float x = (float)(sample_acc / DECIMATION_FACTOR);
    sample_acc = 0;
    sample_count = 0;

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
                bpm = 60.0f / ibi_s;
                ESP_LOGI(TAG, "Peak: raw=%lu bpm=%.1f", (unsigned long)raw, bpm);
                if (ui_ppgscr != NULL) {
                    snprintf(buf, sizeof(buf), "bpm=%.1f", bpm);
                    lv_label_set_text(ui_BPM_label, buf);
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

    if (ui_ppgscr != NULL) {
            lv_label_set_text(ui_BPM_label, "");
            lv_obj_set_style_text_color(ui_BPM_label, lv_palette_main(LV_PALETTE_GREEN), 0);
            
    }

    uint8_t buf[ADC_FRAME_SIZE];
    uint32_t out_len = 0;

    while (1) {
        esp_err_t ret = adc_continuous_read(adc_handle, buf, ADC_FRAME_SIZE, &out_len, 1000);
        if (ret == ESP_OK && out_len > 0) {
            uint32_t samples = out_len / SOC_ADC_DIGI_RESULT_BYTES;
            for (uint32_t i = 0; i < samples; i++) {
                adc_digi_output_data_t *p = (adc_digi_output_data_t *)&buf[i * SOC_ADC_DIGI_RESULT_BYTES];
                uint32_t raw = 0;

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
                raw = p->type1.data;
#else
                raw = p->type2.data;
#endif
                process_sample(raw);
            }
        }
        if (ui_ppgscr == NULL) {break;}
    }

    while (1){ESP_LOGI(TAG, "Error");}
}