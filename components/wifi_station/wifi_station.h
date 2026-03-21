/**
* @file wifi_station.h
*
* @brief WiFi station (STA) component - connect to an access point.
*
* COPYRIGHT NOTICE: (c) 2025 Byte Lab Grupa d.o.o.
* All rights reserved.
*/

#ifndef __WIFI_STATION_H__
#define __WIFI_STATION_H__

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------- INCLUDES ----------------------------------

//---------------------------------- MACROS -----------------------------------

//-------------------------------- DATA TYPES ---------------------------------

//---------------------- PUBLIC FUNCTION PROTOTYPES --------------------------

/**
 * @brief Initializes NVS, TCP/IP stack, default WiFi STA interface and starts
 *        connection to the AP configured in menuconfig (WIFI_STATION_SSID /
 *        WIFI_STATION_PASSWORD). Connection is asynchronous; use events or
 *        wifi_station_wait_connected() to wait for an IP.
 */
void wifi_station_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __WIFI_STATION_H__ */
