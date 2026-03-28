/**
* @file gui.h

* @brief See the source file.
* 
* COPYRIGHT NOTICE: (c) 2022 Byte Lab Grupa d.o.o.
* All rights reserved.
*/

#ifndef __GUI_H__
#define __GUI_H__

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------- INCLUDES ----------------------------------
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

//-------------------------------- DATA TYPES ---------------------------------

//---------------------- PUBLIC FUNCTION PROTOTYPES --------------------------

/**
 * @brief Initializes LVGL, TFT drivers and input drivers and starts task needed for GUI operation.
 * 
 */
void gui_init(void);

#ifdef __cplusplus
}
#endif

#endif // __GUI_H__
