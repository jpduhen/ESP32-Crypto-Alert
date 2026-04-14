/**
 * M-014a: minimale OTA-upload — alleen HTTP-entry via webui; geen signing/auth.
 */
#include "ota_service/ota_service.hpp"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

namespace ota_service {

namespace {

static const char TAG[] = "ota_svc";

/** Gelijk aan `ota_0`/`ota_1` size in `partitions_v2_16mb_ota.csv` (3 MiB). */
static constexpr size_t k_ota_slot_size = 0x300000;

/** Ruwe ontvangstbuffer (stack httpd-task). */
static constexpr size_t k_recv_chunk = 1024;

static constexpr size_t k_min_image = 1024;

static esp_err_t send_json(httpd_req_t *req, const char *status, const char *json)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

} // namespace

esp_err_t init()
{
    const esp_partition_t *run = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
    ESP_LOGI(TAG,
             "M-014a: running=%s @0x%x (%u KiB), next_update=%s",
             run ? run->label : "?",
             run ? (unsigned)run->address : 0u,
             run ? (unsigned)(run->size / 1024) : 0u,
             next ? next->label : "?");
    return ESP_OK;
}

esp_err_t handle_firmware_upload(httpd_req_t *req)
{
    if (req->content_len <= 0) {
        ESP_LOGW(TAG, "OTA: geen Content-Length / lege body");
        return send_json(req, "400 Bad Request",
                         "{\"ok\":false,\"error\":\"Content-Length verplicht (raw .bin)\"}");
    }

    const size_t total = static_cast<size_t>(req->content_len);
    if (total < k_min_image) {
        ESP_LOGW(TAG, "OTA: image te klein (%u B)", (unsigned)total);
        return send_json(req, "400 Bad Request", "{\"ok\":false,\"error\":\"image te klein\"}");
    }
    if (total > k_ota_slot_size) {
        ESP_LOGW(TAG, "OTA: image groter dan OTA-slot (%u > %u)", (unsigned)total,
                 (unsigned)k_ota_slot_size);
        return send_json(req, "413 Payload Too Large",
                         "{\"ok\":false,\"error\":\"image groter dan OTA-partitie (3 MiB)\"}");
    }

    char ct[96]{};
    if (httpd_req_get_hdr_value_str(req, "Content-Type", ct, sizeof(ct)) != ESP_OK) {
        ct[0] = '\0';
    }
    if (std::strstr(ct, "application/octet-stream") == nullptr) {
        ESP_LOGW(TAG, "OTA: Content-Type niet application/octet-stream: %s", ct);
        return send_json(req, "415 Unsupported Media Type",
                         "{\"ok\":false,\"error\":\"Content-Type: application/octet-stream vereist\"}");
    }

    const esp_partition_t *update = esp_ota_get_next_update_partition(nullptr);
    if (!update) {
        ESP_LOGE(TAG, "OTA: geen next_update partition");
        return send_json(req, "500 Internal Server Error",
                         "{\"ok\":false,\"error\":\"geen OTA-doelpartitie\"}");
    }

    esp_ota_handle_t oh{};
    esp_err_t err = esp_ota_begin(update, total, &oh);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin: %s", esp_err_to_name(err));
        return send_json(req, "500 Internal Server Error",
                         "{\"ok\":false,\"error\":\"esp_ota_begin mislukt\"}");
    }

    uint8_t buf[k_recv_chunk]{};
    size_t remaining = total;
    while (remaining > 0) {
        const size_t want = remaining > sizeof(buf) ? sizeof(buf) : remaining;
        const int r = httpd_req_recv(req, reinterpret_cast<char *>(buf), want);
        if (r < 0) {
            ESP_LOGE(TAG, "OTA: recv err %d", r);
            esp_ota_abort(oh);
            return send_json(req, "500 Internal Server Error",
                             "{\"ok\":false,\"error\":\"recv onderbroken\"}");
        }
        if (r == 0) {
            ESP_LOGE(TAG, "OTA: recv 0 met remaining=%u", (unsigned)remaining);
            esp_ota_abort(oh);
            return send_json(req, "400 Bad Request", "{\"ok\":false,\"error\":\"body te kort\"}");
        }
        err = esp_ota_write(oh, buf, static_cast<size_t>(r));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write: %s", esp_err_to_name(err));
            esp_ota_abort(oh);
            return send_json(req, "500 Internal Server Error",
                             "{\"ok\":false,\"error\":\"schrijffout flash\"}");
        }
        remaining -= static_cast<size_t>(r);
    }

    err = esp_ota_end(oh);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end: %s", esp_err_to_name(err));
        return send_json(req, "400 Bad Request",
                         "{\"ok\":false,\"error\":\"image validatie mislukt (esp_ota_end)\"}");
    }

    err = esp_ota_set_boot_partition(update);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition: %s", esp_err_to_name(err));
        return send_json(req, "500 Internal Server Error",
                         "{\"ok\":false,\"error\":\"boot-partitie zetten mislukt\"}");
    }

    ESP_LOGI(TAG, "OTA: ok — boot=%s, herstart over ~300 ms", update->label);
    send_json(req, "200 OK",
              "{\"ok\":true,\"message\":\"Firmware geaccepteerd. Apparaat herstart…\",\"reboot\":true}");
    vTaskDelay(pdMS_TO_TICKS(300));
    esp_restart();
}

} // namespace ota_service
