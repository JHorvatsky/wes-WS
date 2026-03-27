//--------------------------------- INCLUDES ----------------------------------
#include "gui.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"

/* Littlevgl specific */
#include "lvgl.h"
#include "lvgl_helpers.h"

#include "ui_app/ui_app.h"

//---------------------------------- MACROS -----------------------------------
#define LV_TICK_PERIOD_MS (1U)

//-------------------------------- DATA TYPES ---------------------------------

//---------------------- PRIVATE FUNCTION PROTOTYPES --------------------------
static void _create_demo_application(void);
static void _lv_tick_timer(void *p_arg);
static void _gui_task(void *p_parameter);

//------------------------- STATIC DATA & CONSTANTS ---------------------------
static SemaphoreHandle_t p_gui_semaphore;

//------------------------------- GLOBAL DATA ---------------------------------

//------------------------------ PUBLIC FUNCTIONS -----------------------------

void gui_init(void)
{
    p_gui_semaphore = xSemaphoreCreateMutex();
    
    /* Pinning to Core 1 to avoid interference with WiFi/BT on Core 0 */
    xTaskCreatePinnedToCore(_gui_task, "gui", 4096 * 2, NULL, 0, NULL, 1);
}

/**
 * @brief Executes a callback function while holding the GUI semaphore.
 * Use this to update UI elements from other FreeRTOS tasks.
 */
void gui_run_under_lock(void (*callback)(void))
{
    if (p_gui_semaphore != NULL)
    {
        if (xSemaphoreTake(p_gui_semaphore, portMAX_DELAY) == pdTRUE)
        {
            callback();
            xSemaphoreGive(p_gui_semaphore);
        }
    }
}

//---------------------------- PRIVATE FUNCTIONS ------------------------------

static void _create_demo_application(void)
{
    ui_app_init();
}

static void _lv_tick_timer(void *p_arg)
{
    (void)p_arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

static void _gui_task(void *p_parameter)
{
    (void)p_parameter;

    lv_init();

    /* Initialize SPI or I2C bus used by the drivers */
    lvgl_driver_init();

    lv_color_t *p_buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(NULL != p_buf1);

    lv_color_t *p_buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(NULL != p_buf2);
    
    static lv_disp_draw_buf_t disp_draw_buf;
    lv_disp_draw_buf_init(&disp_draw_buf, p_buf1, p_buf2, DISP_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.draw_buf = &disp_draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = touch_driver_read;
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    lv_indev_drv_register(&indev_drv);

    /* Timer for lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = { 
        .callback = &_lv_tick_timer, 
        .name = "periodic_gui" 
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    _create_demo_application();

    for(;;)
    {
        /* LVGL recommends 5-30ms delay */
        vTaskDelay(pdMS_TO_TICKS(10));

        if(xSemaphoreTake(p_gui_semaphore, portMAX_DELAY) == pdTRUE)
        {
            lv_task_handler();
            xSemaphoreGive(p_gui_semaphore);
        }
    }

    /* Cleanup (Task should never reach here) */
    free(p_buf1);
    free(p_buf2);
    vTaskDelete(NULL);
}