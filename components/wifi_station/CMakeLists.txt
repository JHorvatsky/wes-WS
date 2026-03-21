set(COMPONENT_SRCS "wifi_station.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

set(COMPONENT_REQUIRES esp_wifi esp_netif nvs_flash esp_event)

register_component()
