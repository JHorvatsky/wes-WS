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
extern lv_obj_t *ui_BPM_label;
extern lv_obj_t *ui_imepj;
extern lv_obj_t *ui_ppgscr;
extern lv_obj_t *ui_glazbascr;
//---------------------- PRIVATE FUNCTION PROTOTYPES --------------------------
extern void app_sample(void *param);
extern void audio_sine_task(void *pvParameters);
TaskHandle_t pulsHandle = NULL, songHandle=NULL;

extern int samp_start;
extern int song_start;


void Per_task (void *param);
void per_init(void);
extern void init_i2s_max98357a(void);
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
    init_i2s_max98357a();

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
            vTaskDelete( songHandle );
            songHandle=NULL;
        }

        // 3. Update LVGL Label
        // Important: LVGL is not thread-safe. If your LVGL timer/task 
        // is running in another core, use a mutex or check if ui_uibtn1 exists.
        // 3. Small delay to prevent watchdog issues and console flooding
        vTaskDelay(pdMS_TO_TICKS(200));
      }

}

//---------------------------- INTERRUPT HANDLERS -----------------------------

