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
#include "esp_random.h"
#include "driver/i2s_std.h"
#include <string.h>
//---------------------------------- MACROS -----------------------------------

#define BTN1_GPIO 36
#define JOY_X_ADC_CH ADC_CHANNEL_6 // GPIO 34
#define JOY_Y_ADC_CH ADC_CHANNEL_7
//-------------------------------- DATA TYPES ---------------------------------
extern lv_obj_t *ui_BPM_label;
extern lv_obj_t *ui_imepj;
extern lv_obj_t *ui_ppgscr;
extern lv_obj_t *ui_glazbascr;
extern lv_obj_t *ui_Color_Scr;
extern lv_obj_t * ui_resultColor;
extern lv_obj_t * ui_Label3;
extern lv_obj_t * ui_LED_color_label;

//---------------------- PRIVATE FUNCTION PROTOTYPES --------------------------
extern void app_sample(void *param);
extern void audio_sine_task(void *pvParameters);
extern void color_scan_task_prox_gated(void *pvParameters);
TaskHandle_t pulsHandle = NULL, songHandle=NULL, colorHandle=NULL;

extern int samp_start;
extern int song_start;
extern int color_start;

void Per_task (void *param);
void per_init(void);
extern void init_i2s_max98357a(void);
//extern void apds9960_init(void);
//extern void apds9960_deinit(void);
extern void aud_dis();

//------------------------- STATIC DATA & CONSTANTS ---------------------------
const int main_colors[8] = {
    0xFF0000, // Red
    0x00FF00, // Green
    0x0000FF, // Blue
    0xFFA500, // Orange
    0x800080, // Purple
    0xFFC0CB, // Pink
    0xA52A2A, // Brown
    0x808080  // Gray
};

//------------------------------- GLOBAL DATA ---------------------------------
int TARGET_RGB[3]={0,0,0};
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
    init_i2s_max98357a();
    
}

void rand_init(){
    uint32_t random_index = esp_random() % 8;
    //TARGET_RGB[0]=main_colors[random_index]/256;
    //TARGET_RGB[1]=(main_colors[random_index]>>4)%16;
    //TARGET_RGB[2]=main_colors[random_index]%16;
    char msg[11]; 
    for(int i = 0; i < 11; i++) msg[i] = '\0';

    switch (random_index) {
        case 0: strncpy(msg, "Crveno", 11);    break;
        case 1: strncpy(msg, "Zeleno", 11);  break;
        case 2: strncpy(msg, "Plavo", 11);   break;
        case 3: strncpy(msg, "Narancasto", 11); break; // 6 chars + \0
        case 4: strncpy(msg, "Ljubicasto", 11); break; // 6 chars + \0
        case 5: strncpy(msg, "Rozo", 11);   break;
        case 6: strncpy(msg, "Smedje", 11);  break;
        case 7: strncpy(msg, "Sivo", 11);   break;
        default: strncpy(msg, "None", 11);  break;
    }
    if (ui_Label3 != NULL){
        lv_label_set_text(ui_LED_color_label, "Pronadji nesto");
    }
    if (ui_resultColor != NULL){
        lv_obj_set_style_bg_color(ui_resultColor, lv_color_hex(main_colors[random_index]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(ui_Label3, msg);
    }
    
}

void Per_task (void *param){
   while (1) {

        // 2. Read the level
        //int btn_state = gpio_get_level(BTN1_GPIO);
        //char buf[32]; // Buffer for string formatting
        //int raw_x, raw_y

        if (samp_start==1){
            xTaskCreatePinnedToCore(app_sample, "pULS", 10*4096, NULL, 0, &pulsHandle, 0);
            if (ui_ppgscr==NULL){samp_start=0;}
        }
        else if( (pulsHandle != NULL) ){
            vTaskDelete( pulsHandle );
            pulsHandle=NULL;
        }

        if (song_start==1){
            xTaskCreatePinnedToCore(audio_sine_task, "glazba", 10*4096, NULL, 0, &songHandle, 0);
            if (ui_glazbascr==NULL){song_start=0;}
        }
        else if( (songHandle != NULL) ){
            aud_dis();
            vTaskDelete( songHandle );
            songHandle=NULL;
        }

        if (color_start==1){
            rand_init();
            //xTaskCreatePinnedToCore(color_scan_task_prox_gated, "col", 10*4096, NULL, 0, &colorHandle, 0);
            color_start=0;
        }
        //else if( (colorHandle != NULL) ){
        //    vTaskDelete( colorHandle );
        //    colorHandle=NULL;
        //}

        // 3. Update LVGL Label
        // Important: LVGL is not thread-safe. If your LVGL timer/task 
        // is running in another core, use a mutex or check if ui_uibtn1 exists.
        // 3. Small delay to prevent watchdog issues and console flooding
        vTaskDelay(pdMS_TO_TICKS(200));
      }

}

//---------------------------- INTERRUPT HANDLERS -----------------------------

