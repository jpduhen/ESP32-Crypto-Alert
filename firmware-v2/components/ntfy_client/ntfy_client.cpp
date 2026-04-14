/**
 * M-011a: eerste echte outbound-sink — ntfy HTTPS publish (geen MQTT/WebUI).
 */
#include "ntfy_client/ntfy_client.hpp"
#include "config_store/config_store.hpp"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "net_runtime/net_runtime.hpp"
#include "sdkconfig.h"

#include <cstring>

namespace ntfy_client {

namespace {

static const char TAG[] = "ntfy";

#if CONFIG_NTFY_CLIENT_ENABLE

static esp_err_t send_locked(const char *title, const char *body)
{
    const config_store::ServiceRuntimeConfig &svc = config_store::service_runtime();
    if (!svc.ntfy_enabled) {
        ESP_LOGD(TAG, "ntfy uit (runtime M-003a)");
        return ESP_OK;
    }
    if (strlen(svc.ntfy_topic) == 0) {
        ESP_LOGD(TAG, "topic leeg — geen push");
        return ESP_OK;
    }

    char url[256];
    const int n = snprintf(url, sizeof(url), "%s/%s", CONFIG_NTFY_SERVER, svc.ntfy_topic);
    if (n <= 0 || n >= static_cast<int>(sizeof(url))) {
        ESP_LOGW(TAG, "URL te lang");
        return ESP_ERR_INVALID_SIZE;
    }

    if (!net_runtime::net_mutex_take(pdMS_TO_TICKS(20000))) {
        return ESP_ERR_TIMEOUT;
    }

    esp_http_client_config_t cfg{};
    cfg.url = url;
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = 15000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        net_runtime::net_mutex_give();
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_header(c, "Content-Type", "text/plain; charset=utf-8");
    esp_http_client_set_header(c, "X-Title", title);
    if (strlen(CONFIG_NTFY_ACCESS_TOKEN) > 0) {
        char auth[160];
        const int na =
            snprintf(auth, sizeof(auth), "Bearer %s", CONFIG_NTFY_ACCESS_TOKEN);
        if (na > 0 && na < static_cast<int>(sizeof(auth))) {
            esp_http_client_set_header(c, "Authorization", auth);
        }
    }

    const int blen = static_cast<int>(strlen(body));
    esp_http_client_set_post_field(c, body, blen);

    const esp_err_t err = esp_http_client_perform(c);
    const int status = esp_http_client_get_status_code(c);
    esp_http_client_cleanup(c);
    net_runtime::net_mutex_give();

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "HTTP perform: %s", esp_err_to_name(err));
        return err;
    }
    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "HTTP status %d", status);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "publish ok (HTTP %d)", status);
    return ESP_OK;
}

#endif // CONFIG_NTFY_CLIENT_ENABLE

} // namespace

esp_err_t init()
{
#if CONFIG_NTFY_CLIENT_ENABLE
    {
        const config_store::ServiceRuntimeConfig &svc = config_store::service_runtime();
        ESP_LOGI(TAG, "M-011a: ntfy_client (server=%s, topic=%s, runtime en=%s)", CONFIG_NTFY_SERVER,
                 (svc.ntfy_topic[0] != '\0') ? "ingesteld" : "leeg", svc.ntfy_enabled ? "aan" : "uit");
    }
#else
    ESP_LOGD(TAG, "ntfy_client uit (Kconfig)");
#endif
    return ESP_OK;
}

esp_err_t send_notification(const char *title, const char *body)
{
#if !CONFIG_NTFY_CLIENT_ENABLE
    (void)title;
    (void)body;
    return ESP_OK;
#else
    if (!title || !body) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!net_runtime::has_ip()) {
        ESP_LOGD(TAG, "geen IP — skip NTFY");
        return ESP_OK;
    }
    return send_locked(title, body);
#endif
}

} // namespace ntfy_client
