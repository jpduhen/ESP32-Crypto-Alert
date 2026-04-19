#include "wifi_onboarding/wifi_onboarding.hpp"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "net_runtime/net_runtime.hpp"
#include <cstdlib>
#include <cstring>

#include "sdkconfig.h"

namespace wifi_onboarding {

static const char TAG[] = "wifi_onb";

static SemaphoreHandle_t s_provision_done;
static config_store::RuntimeConfig *s_cfg_ptr;

static const char HTML_PAGE[] =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" "
    "content=\"width=device-width,initial-scale=1\"><title>Crypto Alert WiFi</title></head><body>"
    "<h1>WiFi instellen</h1>"
    "<p>Verbind met dit netwerk (<strong>CryptoAlert</strong>), vul je thuisnetwerk in en sla op.</p>"
    "<form method=\"POST\" action=\"/save\">"
    "<label>SSID<br><input name=\"ssid\" required maxlength=\"32\"></label><br><br>"
    "<label>Wachtwoord<br><input name=\"password\" type=\"password\" maxlength=\"64\"></label><br><br>"
    "<button type=\"submit\">Opslaan en herstarten</button>"
    "</form></body></html>";

static void url_decode_inplace(char *s)
{
    char *src = s;
    char *dst = s;
    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            ++src;
        } else if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], '\0'};
            char *end = nullptr;
            const unsigned long v = strtoul(hex, &end, 16);
            if (end == hex + 2) {
                *dst++ = static_cast<char>(v);
                src += 3;
            } else {
                *dst++ = *src++;
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static bool copy_param(const char *body, const char *key, char *out, size_t outsz)
{
    out[0] = '\0';
    const char *p = strstr(body, key);
    if (!p) {
        return false;
    }
    p += strlen(key);
    const char *amp = strchr(p, '&');
    size_t len = amp ? static_cast<size_t>(amp - p) : strlen(p);
    if (len >= outsz) {
        len = outsz - 1;
    }
    memcpy(out, p, len);
    out[len] = '\0';
    url_decode_inplace(out);
    return true;
}

static bool parse_form_body(const char *body, char *ssid_out, size_t ssid_sz, char *pass_out,
                            size_t pass_sz)
{
    if (!body) {
        return false;
    }
    if (!copy_param(body, "ssid=", ssid_out, ssid_sz)) {
        return false;
    }
    pass_out[0] = '\0';
    (void)copy_param(body, "password=", pass_out, pass_sz);
    return ssid_out[0] != '\0';
}

static esp_err_t uri_get_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, HTML_PAGE, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t uri_get_favicon(httpd_req_t *req)
{
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, "", 0);
}

static esp_err_t uri_post_save(httpd_req_t *req)
{
    /* Ruim genoeg voor url-encoded SSID (max 32) + wachtwoord (max 64) + veldnamen */
    char buf[512];
    const int rlen = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (rlen <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
        return ESP_FAIL;
    }
    buf[rlen] = '\0';

    if (!s_cfg_ptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "cfg");
        return ESP_FAIL;
    }

    char ssid[config_store::kWifiSsidMax]{};
    char pass[config_store::kWifiPassMax]{};
    if (!parse_form_body(buf, ssid, sizeof(ssid), pass, sizeof(pass))) {
        ESP_LOGW(TAG, "parse formulier mislukt");
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "parse");
        return ESP_FAIL;
    }

    strncpy(s_cfg_ptr->wifi_sta_ssid, ssid, sizeof(s_cfg_ptr->wifi_sta_ssid) - 1);
    strncpy(s_cfg_ptr->wifi_sta_pass, pass, sizeof(s_cfg_ptr->wifi_sta_pass) - 1);

    const esp_err_t se = config_store::save(*s_cfg_ptr);
    if (se != ESP_OK) {
        ESP_LOGE(TAG, "NVS save: %s", esp_err_to_name(se));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "credentials opgeslagen (SSID=%s), herstart…", ssid);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_send(req, "OK. Apparaat herstart. Verbind je telefoon weer met je thuisnetwerk.",
                    HTTPD_RESP_USE_STRLEN);

    if (s_provision_done) {
        xSemaphoreGive(s_provision_done);
    }
    return ESP_OK;
}

static httpd_handle_t s_server;

static esp_err_t start_httpd()
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.stack_size = 8192;
    cfg.lru_purge_enable = true;
    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &cfg), TAG, "httpd_start");

    httpd_uri_t uroot{};
    uroot.uri = "/";
    uroot.method = HTTP_GET;
    uroot.handler = uri_get_root;
    httpd_uri_t usave{};
    usave.uri = "/save";
    usave.method = HTTP_POST;
    usave.handler = uri_post_save;
    httpd_uri_t ufav{};
    ufav.uri = "/favicon.ico";
    ufav.method = HTTP_GET;
    ufav.handler = uri_get_favicon;
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &uroot), TAG, "reg /");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &usave), TAG, "reg /save");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &ufav), TAG, "reg favicon");
    ESP_LOGI(TAG, "HTTP server poort 80 (SoftAP)");
    return ESP_OK;
}

static void stop_httpd()
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = nullptr;
    }
}

esp_err_t run(config_store::RuntimeConfig &cfg)
{
#if defined(CONFIG_WIFI_ONBOARDING_FORCE) && CONFIG_WIFI_ONBOARDING_FORCE
    ESP_LOGW(TAG, "WIFI_ONBOARDING_FORCE: credentials wissen");
    ESP_RETURN_ON_ERROR(config_store::clear_wifi_credentials(), TAG, "clear");
    cfg.wifi_sta_ssid[0] = '\0';
    cfg.wifi_sta_pass[0] = '\0';
#endif

    if (config_store::has_wifi_credentials(cfg)) {
        ESP_LOGI(TAG, "WiFi-credentials aanwezig — geen onboarding");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "onboarding: geen WiFi in NVS — SoftAP CryptoAlert + portal");
    s_cfg_ptr = &cfg;
    s_provision_done = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_provision_done, ESP_ERR_NO_MEM, TAG, "sem");

    ESP_RETURN_ON_ERROR(net_runtime::start_softap("CryptoAlert"), TAG, "softap");
    ESP_RETURN_ON_ERROR(start_httpd(), TAG, "httpd");

    ESP_LOGI(TAG, "Verbind met SSID \"CryptoAlert\", open http://192.168.4.1/");
    (void)xSemaphoreTake(s_provision_done, portMAX_DELAY);

    stop_httpd();
    s_cfg_ptr = nullptr;
    vSemaphoreDelete(s_provision_done);
    s_provision_done = nullptr;

    ESP_LOGI(TAG, "Herstart na provisioning");
    net_runtime::stop_softap();
    esp_restart();
    return ESP_OK;
}

esp_err_t clear_credentials_for_reprovision()
{
    return config_store::clear_wifi_credentials();
}

} // namespace wifi_onboarding
