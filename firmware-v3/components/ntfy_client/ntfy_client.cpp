#include "ntfy_client/ntfy_client.hpp"

#include <cstdio>
#include <cstring>

#include "esp_http_client.h"
#include "esp_log.h"
#include "sdkconfig.h"

namespace {

const char *TAG = "NTFY";

bool s_enabled = false;
char s_url[256]{};

#if CONFIG_CRYPTO_ALERT_NTFY_ENABLE
void trim_slash(char *s) {
    if (s == nullptr) {
        return;
    }
    const size_t n = std::strlen(s);
    if (n > 0 && s[n - 1] == '/') {
        s[n - 1] = '\0';
    }
}
#endif

}  // namespace

namespace ntfy_client {

esp_err_t init() {
#if CONFIG_CRYPTO_ALERT_NTFY_ENABLE
    const bool topic_ok = CONFIG_CRYPTO_ALERT_NTFY_TOPIC[0] != '\0';
    const bool url_ok = CONFIG_CRYPTO_ALERT_NTFY_SERVER_URL[0] != '\0';
    s_enabled = topic_ok && url_ok;

    if (!url_ok) {
        ESP_LOGW(TAG, "enabled=1 maar server_url leeg; send disabled");
        return ESP_OK;
    }

    char base[192];
    std::snprintf(base, sizeof(base), "%s", CONFIG_CRYPTO_ALERT_NTFY_SERVER_URL);
    trim_slash(base);
    std::snprintf(s_url, sizeof(s_url), "%s/%s", base, CONFIG_CRYPTO_ALERT_NTFY_TOPIC);

    ESP_LOGI(TAG, "enabled=%d topic configured=%d", s_enabled ? 1 : 0, topic_ok ? 1 : 0);
#else
    s_enabled = false;
    s_url[0] = '\0';
    ESP_LOGI(TAG, "enabled=0 (build config)");
#endif
    return ESP_OK;
}

bool enabled() {
    return s_enabled;
}

esp_err_t send(const NtfySendRequest &req) {
    if (!s_enabled) {
        ESP_LOGW(TAG, "send skipped: disabled/topic missing");
        return ESP_ERR_INVALID_STATE;
    }

    if (req.body == nullptr || req.body[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    esp_http_client_config_t cfg = {};
    cfg.url = s_url;
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = 8000;

    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    if (h == nullptr) {
        ESP_LOGE(TAG, "esp_http_client_init failed");
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_header(h, "Content-Type", "text/plain; charset=utf-8");
    if (req.title && req.title[0] != '\0') {
        esp_http_client_set_header(h, "Title", req.title);
    }
    if (req.tags && req.tags[0] != '\0') {
        esp_http_client_set_header(h, "Tags", req.tags);
    }
    if (req.priority > 0) {
        char pbuf[16];
        std::snprintf(pbuf, sizeof(pbuf), "%d", req.priority);
        esp_http_client_set_header(h, "Priority", pbuf);
    }

    esp_http_client_set_post_field(h, req.body, static_cast<int>(std::strlen(req.body)));
    const esp_err_t err = esp_http_client_perform(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "send failed err=%s", esp_err_to_name(err));
        esp_http_client_cleanup(h);
        return err;
    }

    const int status = esp_http_client_get_status_code(h);
    if (status >= 200 && status < 300) {
        ESP_LOGI(TAG, "sent ok status=%d", status);
    } else {
        ESP_LOGE(TAG, "send failed status=%d", status);
    }

    esp_http_client_cleanup(h);
    return (status >= 200 && status < 300) ? ESP_OK : ESP_FAIL;
}

}  // namespace ntfy_client
