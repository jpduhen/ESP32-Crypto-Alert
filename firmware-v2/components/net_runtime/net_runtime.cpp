/**
 * net_runtime — M-002: WiFi/netif + globale net_mutex; géén HTTP(S)/WS naar exchanges.
 * Reconnect STA: on_wifi_sta_event. TODO: optionele backoff; MQTT/NTFY later zelfde mutex of queue.
 */
#include "net_runtime/net_runtime.hpp"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/semphr.h"
#include <cstring>

#include "sdkconfig.h"

namespace net_runtime {

static const char TAG[] = "net_runtime";

static SemaphoreHandle_t s_net_mutex;
static bool s_has_ip;
static bool s_early_done;
static bool s_wifi_inited;
static bool s_sta_handlers_registered;
static esp_netif_t *s_ap_netif;
static esp_netif_t *s_sta_netif;

static void on_wifi_sta_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    (void)event_data;
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_has_ip = false;
        ESP_LOGW(TAG, "STA disconnected, retry");
        esp_wifi_connect();
    }
}

static void on_ip_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;
    (void)event_data;
    if (event_id == IP_EVENT_STA_GOT_IP) {
        s_has_ip = true;
        ESP_LOGI(TAG, "STA got IP");
    }
}

static esp_err_t register_sta_events_once()
{
    if (s_sta_handlers_registered) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_sta_event, nullptr), TAG,
        "reg wifi sta");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_ip_event, nullptr), TAG, "reg ip");
    s_sta_handlers_registered = true;
    return ESP_OK;
}

esp_err_t early_init()
{
    if (s_early_done) {
        return ESP_OK;
    }
    s_net_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_net_mutex, ESP_ERR_NO_MEM, TAG, "mutex");

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event_loop");
    s_early_done = true;
    return ESP_OK;
}

static esp_err_t ensure_wifi_init()
{
    if (s_wifi_inited) {
        return ESP_OK;
    }
    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&wcfg), TAG, "esp_wifi_init");
    s_wifi_inited = true;
    return ESP_OK;
}

esp_err_t start_softap(const char *ap_ssid)
{
    ESP_RETURN_ON_FALSE(ap_ssid && ap_ssid[0], ESP_ERR_INVALID_ARG, TAG, "ap_ssid");
    ESP_RETURN_ON_ERROR(ensure_wifi_init(), TAG, "wifi_init");

    s_ap_netif = esp_netif_create_default_wifi_ap();
    ESP_RETURN_ON_FALSE(s_ap_netif, ESP_ERR_NO_MEM, TAG, "ap netif");

    wifi_config_t wcfg = {};
    const size_t n = strlen(ap_ssid);
    if (n >= sizeof(wcfg.ap.ssid)) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(wcfg.ap.ssid, ap_ssid, n);
    wcfg.ap.ssid[n] = '\0';
    wcfg.ap.ssid_len = static_cast<uint8_t>(n);
    wcfg.ap.channel = 1;
    wcfg.ap.authmode = WIFI_AUTH_OPEN;
    wcfg.ap.max_connection = 4;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "mode AP");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wcfg), TAG, "ap config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi_start AP");

    ESP_LOGI(TAG, "SoftAP gestart: SSID=%s (open)", ap_ssid);
    return ESP_OK;
}

esp_err_t stop_softap()
{
    ESP_RETURN_ON_ERROR(esp_wifi_stop(), TAG, "wifi_stop");
    if (s_ap_netif) {
        esp_netif_destroy(s_ap_netif);
        s_ap_netif = nullptr;
    }
    ESP_RETURN_ON_ERROR(esp_wifi_deinit(), TAG, "wifi_deinit");
    s_wifi_inited = false;
    ESP_LOGI(TAG, "SoftAP gestopt");
    return ESP_OK;
}

esp_err_t start_sta(const char *sta_ssid, const char *sta_pass)
{
    const char *ssid = sta_ssid;
    const char *pass = sta_pass ? sta_pass : "";
#if defined(CONFIG_NET_WIFI_STA_SSID)
    if (!ssid || !ssid[0]) {
        if (strlen(CONFIG_NET_WIFI_STA_SSID) > 0) {
            ssid = CONFIG_NET_WIFI_STA_SSID;
            pass = CONFIG_NET_WIFI_STA_PASS;
            ESP_LOGI(TAG, "STA: fallback menuconfig SSID");
        }
    }
#endif
    if (!ssid || !ssid[0]) {
        ESP_LOGW(TAG, "Geen STA SSID — geen WiFi-client.");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(ensure_wifi_init(), TAG, "wifi_init");

    s_sta_netif = esp_netif_create_default_wifi_sta();
    ESP_RETURN_ON_FALSE(s_sta_netif, ESP_ERR_NO_MEM, TAG, "sta netif");

    ESP_RETURN_ON_ERROR(register_sta_events_once(), TAG, "sta events");

    wifi_config_t cfg = {};
    strncpy(reinterpret_cast<char *>(cfg.sta.ssid), ssid, sizeof(cfg.sta.ssid) - 1);
    strncpy(reinterpret_cast<char *>(cfg.sta.password), pass, sizeof(cfg.sta.password) - 1);
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "mode STA");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &cfg), TAG, "sta config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi_start STA");
    ESP_LOGI(TAG, "WiFi STA start (SSID=%s)", ssid);
    return ESP_OK;
}

bool has_ip()
{
    return s_has_ip;
}

bool net_mutex_take(TickType_t timeout_ticks)
{
    return s_net_mutex != nullptr && xSemaphoreTake(s_net_mutex, timeout_ticks) == pdTRUE;
}

void net_mutex_give()
{
    if (s_net_mutex) {
        xSemaphoreGive(s_net_mutex);
    }
}

} // namespace net_runtime
