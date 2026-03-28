#include "mqtt_app.h"
#include "esp_mqtt_client.h"
#include "esp_log.h"

static const char *TAG = "mqtt_app";
static esp_mqtt_client_handle_t s_client = NULL;
static bool s_connected = false;

#define MQTT_BROKER_URI  "mqtt://broker.hivemq.com"
#define MQTT_PORT        1883

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            s_connected = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            s_connected = false;
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "Published id=%d", event->msg_id);
            break;
        default:
            break;
    }
}

void mqtt_app_init(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .broker.address.port = MQTT_PORT,
    };
    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
}

void mqtt_app_publish(const char *topic, const char *message)
{
    if (!s_connected) {
        ESP_LOGW(TAG, "Not connected, cannot publish");
        return;
    }
    esp_mqtt_client_publish(s_client, topic, message, 0, 1, 0);
}

bool mqtt_app_is_connected(void)
{
    return s_connected;
}