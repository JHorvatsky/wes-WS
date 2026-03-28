set(COMPONENT_SRCS "mqtt_app.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_REQUIRES mqtt wifi_station esp_log)
register_component()