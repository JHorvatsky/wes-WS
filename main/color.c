#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include <math.h>
#include "esp_log.h"

static const char *TAG = "APDS9960";

// I2C Configuration
#define I2C_MASTER_SCL_IO           21      
#define I2C_MASTER_SDA_IO           23      
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define APDS9960_ADDR               0x39    // Default I2C address

// APDS-9960 Registers
#define APDS9960_ENABLE             0x80
#define APDS9960_CONTROL            0x8F
#define APDS9960_CDATAL             0x94    // Clear Data Low
#define APDS9960_RDATAL             0x96    // Red Data Low
#define APDS9960_GDATAL             0x98    // Green Data Low
#define APDS9960_BDATAL             0x9A    // Blue Data Low

// APDS-9960 Proximity Register
#define APDS9960_PDATA      0x9C    // Proximity data register
#define PROX_THRESHOLD      140     // 0-255 (Higher = Closer. ~150 is roughly 2cm)

// Your Target Color in 0-255 format (Example: A specific Orange)
extern int color_start;
extern int TARGET_RGB[3];
extern lv_obj_t * ui_resultColor;
extern lv_obj_t * ui_Label3;
const uint8_t TOLERANCE_255 = 30; 

void color_scan_task_prox_gated(void *pvParameters) {
    // Modify init to also enable Proximity (0x04)
    
    uint16_t r, g, b, c;
    uint8_t prox;

    while (1) {
        // 1. Check Proximity First
        i2c_master_write_read_device(I2C_MASTER_NUM, APDS9960_ADDR, (uint8_t[]){0x9C}, 1, &prox, 1, pdMS_TO_TICKS(100));

        if (prox > PROX_THRESHOLD) {
            // 2. Read RGBC values
            r = apds_read_16bit(APDS9960_RDATAL);
            g = apds_read_16bit(APDS9960_GDATAL);
            b = apds_read_16bit(APDS9960_BDATAL);
            c = apds_read_16bit(APDS9960_CDATAL);

            if (c > 0) {
                // 3. Normalize and convert to 0-255 scale
                // We use 'c' (Clear) as the brightness reference
                uint8_t r255 = (uint8_t)((float)r / c * 255.0);
                uint8_t g255 = (uint8_t)((float)g / c * 255.0);
                uint8_t b255 = (uint8_t)((float)b / c * 255.0);

                // 4. Compare with 0-255 Target
                if (abs(r255 - TARGET_RGB[0]) > TOLERANCE_255) {
                    
                }
                             (abs(g255 - TARGET_RGB[1]) < TOLERANCE_255) &&
                             (abs(b255 - TARGET_RGB[2]) < TOLERANCE_255);

                if (match) {
                    ESP_LOGI("COLOR", "MATCH! RGB(%d, %d, %d)", r255, g255, b255);
                    color_start=0;
                } else {
                    ESP_LOGD("COLOR", "Object detected: RGB(%d, %d, %d)", r255, g255, b255);
                }
            }
        } else {
            // Optional: Log that nothing is close
            ESP_LOGD("PROX", "No object in range (Prox: %d)", prox);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// Helper to write to I2C
esp_err_t apds_write_reg(uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, APDS9960_ADDR, write_buf, sizeof(write_buf), pdMS_TO_TICKS(100));
}

// Helper to read 16-bit values (Registers are Low/High byte pairs)
uint16_t apds_read_16bit(uint8_t reg_low) {
    uint8_t data[2];
    i2c_master_write_read_device(I2C_MASTER_NUM, APDS9960_ADDR, &reg_low, 1, data, 2, pdMS_TO_TICKS(100));
    return (uint16_t)(data[1] << 8 | data[0]);
}

void apds9960_init() {
    // 1. Setup I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    // 2. Initialize APDS9960
    apds_write_reg(APDS9960_ENABLE, 0x01); // Power ON
    vTaskDelay(pdMS_TO_TICKS(10));
    apds_write_reg(APDS9960_CONTROL, 0x01); // Set Gain (4x)
    apds_write_reg(APDS9960_ENABLE, 0x01 | 0x02 | 0x04); 
}

