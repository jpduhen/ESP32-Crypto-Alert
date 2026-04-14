/**
 * REST bootstrap: Bitvavo GET /v2/ticker/price (TLS, certificate bundle).
 *
 * M-002b: één `esp_http_client`-handle in deze module (lazy init, file-static).
 * Per call: `set_url` + open → read → close; handle blijft bestaan voor hergebruik
 * (minder `init`/`cleanup` per request dan vroeger — typisch minder TLS-work per poll).
 * Bij open/read/HTTP-fout: `cleanup` zodat de volgende poging een schone client krijgt.
 * Alleen aangeroepen vanuit `exchange_bitvavo::tick` (één taak); volledige call blijft
 * onder `net_runtime::net_mutex` — zelfde synchronisatie als voorheen.
 */
#include "diagnostics/diagnostics.hpp"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "net_runtime/net_runtime.hpp"
#include <cstdlib>
#include <cstring>

namespace exchange_bitvavo::rest {

namespace {

static bool parse_price_json(const char *buf, double *out)
{
    const char *p = strstr(buf, "\"price\"");
    if (!p) {
        return false;
    }
    p = strchr(p, ':');
    if (!p) {
        return false;
    }
    ++p;
    while (*p == ' ' || *p == '\"') {
        ++p;
    }
    char *end = nullptr;
    double v = strtod(p, &end);
    if (end == p) {
        return false;
    }
    *out = v;
    return true;
}

/** Module-eigenaar: blijft bestaan tussen succesvolle REST-calls (TLS-context herbruikbaar). */
static esp_http_client_handle_t s_http = nullptr;

static void http_invalidate()
{
    if (s_http) {
        esp_http_client_cleanup(s_http);
        s_http = nullptr;
    }
}

static esp_err_t http_ensure(char *err_detail, size_t err_len)
{
    if (s_http) {
        return ESP_OK;
    }
    esp_http_client_config_t cfg{};
    cfg.url = "https://api.bitvavo.com/";
    cfg.method = HTTP_METHOD_GET;
    cfg.timeout_ms = 15000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;

    s_http = esp_http_client_init(&cfg);
    if (!s_http) {
        if (err_detail && err_len) {
            strncpy(err_detail, "esp_http_client_init failed", err_len - 1);
            err_detail[err_len - 1] = '\0';
        }
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

} // namespace

esp_err_t fetch_ticker_price(const char *market, double *out_eur, char *err_detail, size_t err_len)
{
    if (!out_eur || !market || !market[0]) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!net_runtime::net_mutex_take(pdMS_TO_TICKS(20000))) {
        if (err_detail && err_len) {
            strncpy(err_detail, "net_mutex timeout", err_len - 1);
            err_detail[err_len - 1] = '\0';
        }
        return ESP_ERR_TIMEOUT;
    }

    const esp_err_t ens = http_ensure(err_detail, err_len);
    if (ens != ESP_OK) {
        net_runtime::net_mutex_give();
        return ens;
    }

    char url[192];
    snprintf(url, sizeof(url), "https://api.bitvavo.com/v2/ticker/price?market=%s", market);

    if (esp_http_client_set_url(s_http, url) != ESP_OK) {
        ESP_LOGW(DIAG_TAG_BV_FEED, "REST set_url failed market=%s", market);
        if (err_detail && err_len) {
            strncpy(err_detail, "set_url", err_len - 1);
            err_detail[err_len - 1] = '\0';
        }
        http_invalidate();
        net_runtime::net_mutex_give();
        return ESP_FAIL;
    }
    esp_http_client_set_method(s_http, HTTP_METHOD_GET);

    const int64_t t0_us = esp_timer_get_time();
    ESP_LOGI(DIAG_TAG_BV_FEED, "REST TLS market=%s (client hergebruik)", market);

    esp_err_t err = esp_http_client_open(s_http, 0);
    if (err != ESP_OK) {
        ESP_LOGW(DIAG_TAG_BV_FEED, "REST TLS end market=%s err=%s dt_ms=%lld", market, esp_err_to_name(err),
                 (long long)((esp_timer_get_time() - t0_us) / 1000));
        if (err_detail && err_len) {
            snprintf(err_detail, err_len, "open %s", esp_err_to_name(err));
        }
        http_invalidate();
        net_runtime::net_mutex_give();
        return err;
    }

    (void)esp_http_client_fetch_headers(s_http);
    const int status = esp_http_client_get_status_code(s_http);
    if (status < 200 || status >= 300) {
        ESP_LOGW(DIAG_TAG_BV_FEED, "REST TLS end market=%s HTTP=%d dt_ms=%lld", market, status,
                 (long long)((esp_timer_get_time() - t0_us) / 1000));
        if (err_detail && err_len) {
            snprintf(err_detail, err_len, "HTTP %d", status);
        }
        esp_http_client_close(s_http);
        http_invalidate();
        net_runtime::net_mutex_give();
        return ESP_FAIL;
    }

    char buf[512]{};
    int rtotal = 0;
    for (;;) {
        const int r = esp_http_client_read(s_http, buf + rtotal, static_cast<int>(sizeof(buf) - 1 - rtotal));
        if (r < 0) {
            ESP_LOGW(DIAG_TAG_BV_FEED, "REST TLS end market=%s read_err dt_ms=%lld", market,
                     (long long)((esp_timer_get_time() - t0_us) / 1000));
            if (err_detail && err_len) {
                strncpy(err_detail, "read err", err_len - 1);
                err_detail[err_len - 1] = '\0';
            }
            esp_http_client_close(s_http);
            http_invalidate();
            net_runtime::net_mutex_give();
            return ESP_FAIL;
        }
        if (r == 0) {
            break;
        }
        rtotal += r;
        if (rtotal >= static_cast<int>(sizeof(buf) - 1)) {
            break;
        }
    }
    esp_http_client_close(s_http);
    net_runtime::net_mutex_give();

    if (rtotal <= 0) {
        ESP_LOGW(DIAG_TAG_BV_FEED, "REST TLS end market=%s empty dt_ms=%lld", market,
                 (long long)((esp_timer_get_time() - t0_us) / 1000));
        if (err_detail && err_len) {
            strncpy(err_detail, "empty body", err_len - 1);
            err_detail[err_len - 1] = '\0';
        }
        http_invalidate();
        return ESP_FAIL;
    }
    buf[rtotal] = '\0';

    if (!parse_price_json(buf, out_eur)) {
        ESP_LOGW(DIAG_TAG_BV_FEED, "REST TLS end market=%s parse_fail dt_ms=%lld", market,
                 (long long)((esp_timer_get_time() - t0_us) / 1000));
        if (err_detail && err_len) {
            strncpy(err_detail, "parse price", err_len - 1);
            err_detail[err_len - 1] = '\0';
        }
        return ESP_FAIL;
    }

    ESP_LOGI(DIAG_TAG_BV_FEED, "REST TLS ok market=%s price=%.4f dt_ms=%lld", market, *out_eur,
             (long long)((esp_timer_get_time() - t0_us) / 1000));
    ESP_LOGD(DIAG_TAG_MARKET, "REST ticker %s -> %.4f", market, *out_eur);
    return ESP_OK;
}

} // namespace exchange_bitvavo::rest
