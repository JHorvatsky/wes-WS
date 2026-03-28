/**
* @file main.c

* @brief 
* 
* COPYRIGHT NOTICE: (c) 2022 Byte Lab Grupa d.o.o.
* All rights reserved.
*/

//--------------------------------- INCLUDES ----------------------------------
#include "gui.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
//---------------------------------- MACROS -----------------------------------

#define BTN1_GPIO 36
#define JOY_X_ADC_CH ADC_CHANNEL_6 // GPIO 34
#define JOY_Y_ADC_CH ADC_CHANNEL_7
//-------------------------------- DATA TYPES ---------------------------------
//extern lv_obj_t * ui_uibtn1;
//extern lv_obj_t * ui_Joyx;
//extern lv_obj_t * ui_Joyy;
adc_oneshot_unit_handle_t adc1_handle;
//---------------------- PRIVATE FUNCTION PROTOTYPES --------------------------
void Per_task (void *param);
void per_init(void);
//------------------------- STATIC DATA & CONSTANTS ---------------------------

//------------------------------- GLOBAL DATA ---------------------------------

//------------------------------ PUBLIC FUNCTIONS -----------------------------
void app_main(void)
{
   
   per_init();
   gui_init();

   BaseType_t xReturned;
   TaskHandle_t taskPer= NULL;

    /* Create the task, storing the handle. */
    xReturned = xTaskCreatePinnedToCore(Per_task, "per", 4096, NULL, 0, &taskPer, 0);      /* Used to pass out the created task's handle. */

}

//---------------------------- PRIVATE FUNCTIONS ------------------------------

void per_init(void){
    //btn1 conf
    gpio_reset_pin(BTN1_GPIO);
    gpio_set_direction(BTN1_GPIO, GPIO_MODE_INPUT);

    // --- 2. Configure ADC for Joystick ---
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config1, &adc1_handle);

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12, // Allows full range 0-3.3V
    };
    adc_oneshot_config_channel(adc1_handle, JOY_X_ADC_CH, &config);
    adc_oneshot_config_channel(adc1_handle, JOY_Y_ADC_CH, &config);
}
void nista();
void Per_task (void *param){
   while (1) {

        // 2. Read the level
        int btn_state = gpio_get_level(BTN1_GPIO);
        char buf[32]; // Buffer for string formatting
        int raw_x, raw_y;
        // 3. Update LVGL Label
        // Important: LVGL is not thread-safe. If your LVGL timer/task 
        // is running in another core, use a mutex or check if ui_uibtn1 exists.
        /*if (ui_uibtn1 != NULL) {
            

            if (btn_state == 0) {
                lv_label_set_text(ui_uibtn1, "BTN1: PRESSED");
                lv_obj_set_style_text_color(ui_uibtn1, lv_palette_main(LV_PALETTE_GREEN), 0);
            } else {
                lv_label_set_text(ui_uibtn1, "BTN1: RELEASED");
                lv_obj_set_style_text_color(ui_uibtn1, lv_palette_main(LV_PALETTE_RED), 0);
            }


            // --- READ & UPDATE JOYSTICK ---
            adc_oneshot_read(adc1_handle, JOY_X_ADC_CH, &raw_x);
            adc_oneshot_read(adc1_handle, JOY_Y_ADC_CH, &raw_y);

            if (ui_Joyx != NULL) {
                snprintf(buf, sizeof(buf), "X - cor - %d", raw_x);
                lv_label_set_text(ui_Joyx, buf);
            }

            if (ui_Joyy != NULL) {
                snprintf(buf, sizeof(buf), "Y - cor - %d", raw_y);
                lv_label_set_text(ui_Joyy, buf);
            }
        }


        // 3. Small delay to prevent watchdog issues and console flooding
        vTaskDelay(pdMS_TO_TICKS(100));*/
      }

}

//---------------------------- INTERRUPT HANDLERS -----------------------------

