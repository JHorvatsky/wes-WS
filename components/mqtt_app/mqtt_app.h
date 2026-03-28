#ifndef __MQTT_APP_H__
#define __MQTT_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

void mqtt_app_init(void);
void mqtt_app_publish(const char *topic, const char *message);
bool mqtt_app_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif