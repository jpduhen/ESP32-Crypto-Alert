/**
 * M-013d: minimale read-only alert-observability (RAM-ring, geen NVS).
 * S30-3: 30m-ring parallel aan 5m.
 * S2H-3: 2h-ring parallel aan 30m.
 */
#include "alert_observability/alert_observability.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <cstdio>
#include <cstring>

namespace alert_observability {

namespace {

static const char TAG[] = "alert_obs";

static constexpr size_t k_ring = 8;

struct Entry {
    char symbol[24]{};
    bool up{true};
    double price_eur{0.0};
    double pct_1m{0.0};
    int64_t ts_ms{0};
};

static SemaphoreHandle_t s_mu{};
static Entry s_ring[k_ring]{};
static uint32_t s_seq{0}; // monotoon; nieuwste = hoogste seq

static Entry s_ring5[k_ring]{};
static uint32_t s_seq5{0};

static Entry s_ring30[k_ring]{};
static uint32_t s_seq30{0};

static Entry s_ring2h[k_ring]{};
static uint32_t s_seq2h{0};

struct EntryConf {
    char symbol[24]{};
    bool up{true};
    double price_eur{0.0};
    double pct_1m{0.0};
    double pct_5m{0.0};
    int64_t ts_ms{0};
};

static EntryConf s_ring_conf[k_ring]{};
static uint32_t s_seq_conf{0};

static void html_escape_text(const char *src, char *dst, size_t dst_sz)
{
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 8 < dst_sz; ++i) {
        switch (src[i]) {
        case '&':
            std::memcpy(dst + j, "&amp;", 5);
            j += 5;
            break;
        case '"':
            std::memcpy(dst + j, "&quot;", 6);
            j += 6;
            break;
        case '<':
            std::memcpy(dst + j, "&lt;", 4);
            j += 4;
            break;
        default:
            dst[j++] = src[i];
            break;
        }
    }
    dst[j] = '\0';
}

static void add_one_timeframe_to_root(cJSON *root,
                                      const char *key_json,
                                      Entry *ring,
                                      uint32_t *seq_ptr,
                                      const char *pct_json_name)
{
    Entry copy[k_ring]{};
    uint32_t seq_copy = 0;
    if (!root || !s_mu) {
        return;
    }
    if (xSemaphoreTake(s_mu, pdMS_TO_TICKS(100)) != pdTRUE) {
        cJSON *wrap = cJSON_CreateObject();
        cJSON *arr = cJSON_CreateArray();
        if (wrap && arr) {
            cJSON_AddItemToObject(wrap, "items", arr);
            cJSON_AddNumberToObject(wrap, "count", 0);
            cJSON_AddItemToObject(root, key_json, wrap);
        } else {
            cJSON_Delete(wrap);
            cJSON_Delete(arr);
        }
        return;
    }
    seq_copy = *seq_ptr;
    const size_t nvalid =
        (seq_copy < static_cast<uint32_t>(k_ring)) ? static_cast<size_t>(seq_copy) : k_ring;
    for (size_t i = 0; i < nvalid; ++i) {
        const size_t idx = (seq_copy > 0 ? (seq_copy - 1u - i) : 0u) % k_ring;
        copy[i] = ring[idx];
    }
    xSemaphoreGive(s_mu);

    cJSON *wrap = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    if (!wrap || !arr) {
        cJSON_Delete(wrap);
        cJSON_Delete(arr);
        return;
    }
    for (size_t i = 0; i < nvalid; ++i) {
        const Entry &e = copy[i];
        cJSON *o = cJSON_CreateObject();
        if (!o) {
            continue;
        }
        cJSON_AddStringToObject(o, "symbol", e.symbol[0] ? e.symbol : "?");
        cJSON_AddBoolToObject(o, "up", e.up);
        cJSON_AddNumberToObject(o, "price_eur", e.price_eur);
        cJSON_AddNumberToObject(o, pct_json_name, e.pct_1m);
        cJSON_AddNumberToObject(o, "ts_ms", static_cast<double>(e.ts_ms));
        cJSON_AddItemToArray(arr, o);
    }
    cJSON_AddItemToObject(wrap, "items", arr);
    cJSON_AddNumberToObject(wrap, "count", static_cast<double>(nvalid));
    cJSON_AddItemToObject(root, key_json, wrap);
}

static void add_conf_to_root(cJSON *root)
{
    EntryConf copy[k_ring]{};
    uint32_t seq_copy = 0;
    if (!root || !s_mu) {
        return;
    }
    if (xSemaphoreTake(s_mu, pdMS_TO_TICKS(100)) != pdTRUE) {
        cJSON *wrap = cJSON_CreateObject();
        cJSON *arr = cJSON_CreateArray();
        if (wrap && arr) {
            cJSON_AddItemToObject(wrap, "items", arr);
            cJSON_AddNumberToObject(wrap, "count", 0);
            cJSON_AddItemToObject(root, "alerts_conf_1m5m", wrap);
        } else {
            cJSON_Delete(wrap);
            cJSON_Delete(arr);
        }
        return;
    }
    seq_copy = s_seq_conf;
    const size_t nvalid =
        (seq_copy < static_cast<uint32_t>(k_ring)) ? static_cast<size_t>(seq_copy) : k_ring;
    for (size_t i = 0; i < nvalid; ++i) {
        const size_t idx = (seq_copy > 0 ? (seq_copy - 1u - i) : 0u) % k_ring;
        copy[i] = s_ring_conf[idx];
    }
    xSemaphoreGive(s_mu);

    cJSON *wrap = cJSON_CreateObject();
    cJSON *arr = cJSON_CreateArray();
    if (!wrap || !arr) {
        cJSON_Delete(wrap);
        cJSON_Delete(arr);
        return;
    }
    for (size_t i = 0; i < nvalid; ++i) {
        const EntryConf &e = copy[i];
        cJSON *o = cJSON_CreateObject();
        if (!o) {
            continue;
        }
        cJSON_AddStringToObject(o, "symbol", e.symbol[0] ? e.symbol : "?");
        cJSON_AddBoolToObject(o, "up", e.up);
        cJSON_AddNumberToObject(o, "price_eur", e.price_eur);
        cJSON_AddNumberToObject(o, "pct_1m", e.pct_1m);
        cJSON_AddNumberToObject(o, "pct_5m", e.pct_5m);
        cJSON_AddNumberToObject(o, "ts_ms", static_cast<double>(e.ts_ms));
        cJSON_AddItemToArray(arr, o);
    }
    cJSON_AddItemToObject(wrap, "items", arr);
    cJSON_AddNumberToObject(wrap, "count", static_cast<double>(nvalid));
    cJSON_AddItemToObject(root, "alerts_conf_1m5m", wrap);
}

} // namespace (anonymous)

esp_err_t init()
{
    if (s_mu) {
        return ESP_OK;
    }
    s_mu = xSemaphoreCreateMutex();
    if (!s_mu) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG,
             "M-013d/M-010c/d/S30-3/S2H-3: alert observability ready (1m+5m+30m+2h+conf ring=%u)",
             static_cast<unsigned>(k_ring));
    return ESP_OK;
}

void record_1m_alert(const char *symbol, bool up, double price_eur, double pct_1m, int64_t ts_ms)
{
    if (!s_mu) {
        return;
    }
    const char *sym = (symbol && symbol[0] != '\0') ? symbol : "?";
    if (xSemaphoreTake(s_mu, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGW(TAG, "M-013d: record skip (mutex timeout)");
        return;
    }
    Entry &e = s_ring[s_seq % k_ring];
    std::strncpy(e.symbol, sym, sizeof(e.symbol) - 1);
    e.symbol[sizeof(e.symbol) - 1] = '\0';
    e.up = up;
    e.price_eur = price_eur;
    e.pct_1m = pct_1m;
    e.ts_ms = ts_ms;
    ++s_seq;
    xSemaphoreGive(s_mu);
    ESP_LOGD(TAG, "M-013d: recorded 1m alert sym=%s up=%d pct=%.4f", e.symbol, up ? 1 : 0, pct_1m);
}

void record_5m_alert(const char *symbol, bool up, double price_eur, double pct_5m, int64_t ts_ms)
{
    if (!s_mu) {
        return;
    }
    const char *sym = (symbol && symbol[0] != '\0') ? symbol : "?";
    if (xSemaphoreTake(s_mu, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGW(TAG, "M-010c: record 5m skip (mutex timeout)");
        return;
    }
    Entry &e = s_ring5[s_seq5 % k_ring];
    std::strncpy(e.symbol, sym, sizeof(e.symbol) - 1);
    e.symbol[sizeof(e.symbol) - 1] = '\0';
    e.up = up;
    e.price_eur = price_eur;
    e.pct_1m = pct_5m; // hergebruik veldnaam in Entry voor compactheid
    e.ts_ms = ts_ms;
    ++s_seq5;
    xSemaphoreGive(s_mu);
    ESP_LOGD(TAG, "M-010c: recorded 5m alert sym=%s up=%d pct=%.4f", e.symbol, up ? 1 : 0, pct_5m);
}

void record_30m_alert(const char *symbol, bool up, double price_eur, double pct_30m, int64_t ts_ms)
{
    if (!s_mu) {
        return;
    }
    const char *sym = (symbol && symbol[0] != '\0') ? symbol : "?";
    if (xSemaphoreTake(s_mu, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGW(TAG, "S30-3: record 30m skip (mutex timeout)");
        return;
    }
    Entry &e = s_ring30[s_seq30 % k_ring];
    std::strncpy(e.symbol, sym, sizeof(e.symbol) - 1);
    e.symbol[sizeof(e.symbol) - 1] = '\0';
    e.up = up;
    e.price_eur = price_eur;
    e.pct_1m = pct_30m;
    e.ts_ms = ts_ms;
    ++s_seq30;
    xSemaphoreGive(s_mu);
    ESP_LOGD(TAG, "S30-3: recorded 30m alert sym=%s up=%d pct=%.4f", e.symbol, up ? 1 : 0, pct_30m);
}

void record_2h_alert(const char *symbol, bool up, double price_eur, double pct_2h, int64_t ts_ms)
{
    if (!s_mu) {
        return;
    }
    const char *sym = (symbol && symbol[0] != '\0') ? symbol : "?";
    if (xSemaphoreTake(s_mu, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGW(TAG, "S2H-3: record 2h skip (mutex timeout)");
        return;
    }
    Entry &e = s_ring2h[s_seq2h % k_ring];
    std::strncpy(e.symbol, sym, sizeof(e.symbol) - 1);
    e.symbol[sizeof(e.symbol) - 1] = '\0';
    e.up = up;
    e.price_eur = price_eur;
    e.pct_1m = pct_2h;
    e.ts_ms = ts_ms;
    ++s_seq2h;
    xSemaphoreGive(s_mu);
    ESP_LOGD(TAG, "S2H-3: recorded 2h alert sym=%s up=%d pct=%.4f", e.symbol, up ? 1 : 0, pct_2h);
}

void record_conf_1m5m_alert(const char *symbol,
                          bool up,
                          double price_eur,
                          double pct_1m,
                          double pct_5m,
                          int64_t ts_ms)
{
    if (!s_mu) {
        return;
    }
    const char *sym = (symbol && symbol[0] != '\0') ? symbol : "?";
    if (xSemaphoreTake(s_mu, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGW(TAG, "M-010d: record conf skip (mutex timeout)");
        return;
    }
    EntryConf &e = s_ring_conf[s_seq_conf % k_ring];
    std::strncpy(e.symbol, sym, sizeof(e.symbol) - 1);
    e.symbol[sizeof(e.symbol) - 1] = '\0';
    e.up = up;
    e.price_eur = price_eur;
    e.pct_1m = pct_1m;
    e.pct_5m = pct_5m;
    e.ts_ms = ts_ms;
    ++s_seq_conf;
    xSemaphoreGive(s_mu);
    ESP_LOGD(TAG, "M-010d: recorded conf sym=%s 1m=%.4f 5m=%.4f", e.symbol, pct_1m, pct_5m);
}

void add_alerts_to_cjson(cJSON *root)
{
    if (!root) {
        return;
    }
    add_one_timeframe_to_root(root, "alerts_1m", s_ring, &s_seq, "pct_1m");
    add_one_timeframe_to_root(root, "alerts_5m", s_ring5, &s_seq5, "pct_5m");
    add_one_timeframe_to_root(root, "alerts_30m", s_ring30, &s_seq30, "pct_30m");
    add_one_timeframe_to_root(root, "alerts_2h", s_ring2h, &s_seq2h, "pct_2h");
    add_conf_to_root(root);
}

int append_alerts_html_section(char *out, size_t cap)
{
    if (!out || cap < 64u) {
        return -1;
    }
    Entry copy[k_ring]{};
    uint32_t seq_copy = 0;
    size_t nvalid = 0;
    if (!s_mu || xSemaphoreTake(s_mu, pdMS_TO_TICKS(100)) != pdTRUE) {
        return std::snprintf(out,
                             cap,
                             "<h2>Recente 1m-alerts (M-013d)</h2>"
                             "<p class=\"hint\">Observability tijdelijk niet beschikbaar.</p>");
    }
    seq_copy = s_seq;
    nvalid = (seq_copy < static_cast<uint32_t>(k_ring)) ? static_cast<size_t>(seq_copy) : k_ring;
    for (size_t i = 0; i < nvalid; ++i) {
        const size_t idx = (seq_copy > 0 ? (seq_copy - 1u - i) : 0u) % k_ring;
        copy[i] = s_ring[idx];
    }
    xSemaphoreGive(s_mu);

    char esc[sizeof(Entry::symbol) * 6]{};
    int n = std::snprintf(out,
                          cap,
                          "<h2>Recente 1m-alerts (M-013d)</h2>"
                          "<p class=\"hint\">Read-only RAM na dispatch; max %u; geen historiek na reboot.</p>",
                          static_cast<unsigned>(k_ring));
    if (n <= 0 || static_cast<size_t>(n) >= cap) {
        return -1;
    }
    size_t off = static_cast<size_t>(n);
    if (nvalid == 0u) {
        const int n2 = std::snprintf(out + off, cap - off, "<p><em>Nog geen alerts sinds boot.</em></p>");
        if (n2 <= 0 || off + static_cast<size_t>(n2) >= cap) {
            return -1;
        }
        return static_cast<int>(off + static_cast<size_t>(n2));
    }
    int n3 = std::snprintf(out + off, cap - off, "<ul>");
    if (n3 <= 0 || off + static_cast<size_t>(n3) >= cap) {
        return -1;
    }
    off += static_cast<size_t>(n3);
    for (size_t i = 0; i < nvalid; ++i) {
        const Entry &e = copy[i];
        html_escape_text(e.symbol[0] ? e.symbol : "?", esc, sizeof(esc));
        const char *dir = e.up ? "UP" : "DOWN";
        n3 = std::snprintf(out + off,
                           cap - off,
                           "<li><strong>%s</strong> %s · prijs %.4f EUR · 1m %+.4f %% · ts_ms %lld</li>",
                           esc,
                           dir,
                           e.price_eur,
                           e.pct_1m,
                           (long long)e.ts_ms);
        if (n3 <= 0 || off + static_cast<size_t>(n3) >= cap) {
            return -1;
        }
        off += static_cast<size_t>(n3);
    }
    n3 = std::snprintf(out + off, cap - off, "</ul>");
    if (n3 <= 0 || off + static_cast<size_t>(n3) >= cap) {
        return -1;
    }
    off += static_cast<size_t>(n3);
    return static_cast<int>(off);
}

int append_alerts_5m_html_section(char *out, size_t cap)
{
    if (!out || cap < 64u) {
        return -1;
    }
    Entry copy[k_ring]{};
    uint32_t seq_copy = 0;
    size_t nvalid = 0;
    if (!s_mu || xSemaphoreTake(s_mu, pdMS_TO_TICKS(100)) != pdTRUE) {
        return std::snprintf(out,
                             cap,
                             "<h2>Recente 5m-alerts (M-010c)</h2>"
                             "<p class=\"hint\">Observability tijdelijk niet beschikbaar.</p>");
    }
    seq_copy = s_seq5;
    nvalid = (seq_copy < static_cast<uint32_t>(k_ring)) ? static_cast<size_t>(seq_copy) : k_ring;
    for (size_t i = 0; i < nvalid; ++i) {
        const size_t idx = (seq_copy > 0 ? (seq_copy - 1u - i) : 0u) % k_ring;
        copy[i] = s_ring5[idx];
    }
    xSemaphoreGive(s_mu);

    char esc[sizeof(Entry::symbol) * 6]{};
    int n = std::snprintf(out,
                          cap,
                          "<h2>Recente 5m-alerts (M-010c)</h2>"
                          "<p class=\"hint\">Read-only RAM na dispatch; max %u; geen historiek na reboot.</p>",
                          static_cast<unsigned>(k_ring));
    if (n <= 0 || static_cast<size_t>(n) >= cap) {
        return -1;
    }
    size_t off = static_cast<size_t>(n);
    if (nvalid == 0u) {
        const int n2 = std::snprintf(out + off, cap - off, "<p><em>Nog geen 5m-alerts sinds boot.</em></p>");
        if (n2 <= 0 || off + static_cast<size_t>(n2) >= cap) {
            return -1;
        }
        return static_cast<int>(off + static_cast<size_t>(n2));
    }
    int n3 = std::snprintf(out + off, cap - off, "<ul>");
    if (n3 <= 0 || off + static_cast<size_t>(n3) >= cap) {
        return -1;
    }
    off += static_cast<size_t>(n3);
    for (size_t i = 0; i < nvalid; ++i) {
        const Entry &e = copy[i];
        html_escape_text(e.symbol[0] ? e.symbol : "?", esc, sizeof(esc));
        const char *dir = e.up ? "UP" : "DOWN";
        n3 = std::snprintf(out + off,
                           cap - off,
                           "<li><strong>%s</strong> %s · prijs %.4f EUR · 5m %+.4f %% · ts_ms %lld</li>",
                           esc,
                           dir,
                           e.price_eur,
                           e.pct_1m,
                           (long long)e.ts_ms);
        if (n3 <= 0 || off + static_cast<size_t>(n3) >= cap) {
            return -1;
        }
        off += static_cast<size_t>(n3);
    }
    n3 = std::snprintf(out + off, cap - off, "</ul>");
    if (n3 <= 0 || off + static_cast<size_t>(n3) >= cap) {
        return -1;
    }
    off += static_cast<size_t>(n3);
    return static_cast<int>(off);
}

int append_alerts_30m_html_section(char *out, size_t cap)
{
    if (!out || cap < 64u) {
        return -1;
    }
    Entry copy[k_ring]{};
    uint32_t seq_copy = 0;
    size_t nvalid = 0;
    if (!s_mu || xSemaphoreTake(s_mu, pdMS_TO_TICKS(100)) != pdTRUE) {
        return std::snprintf(out,
                             cap,
                             "<h2>Recente 30m-alerts (S30-3)</h2>"
                             "<p class=\"hint\">Observability tijdelijk niet beschikbaar.</p>");
    }
    seq_copy = s_seq30;
    nvalid = (seq_copy < static_cast<uint32_t>(k_ring)) ? static_cast<size_t>(seq_copy) : k_ring;
    for (size_t i = 0; i < nvalid; ++i) {
        const size_t idx = (seq_copy > 0 ? (seq_copy - 1u - i) : 0u) % k_ring;
        copy[i] = s_ring30[idx];
    }
    xSemaphoreGive(s_mu);

    char esc[sizeof(Entry::symbol) * 6]{};
    int n = std::snprintf(out,
                          cap,
                          "<h2>Recente 30m-alerts (S30-3)</h2>"
                          "<p class=\"hint\">Read-only RAM na dispatch; max %u; geen historiek na reboot.</p>",
                          static_cast<unsigned>(k_ring));
    if (n <= 0 || static_cast<size_t>(n) >= cap) {
        return -1;
    }
    size_t off = static_cast<size_t>(n);
    if (nvalid == 0u) {
        const int n2 = std::snprintf(out + off, cap - off, "<p><em>Nog geen 30m-alerts sinds boot.</em></p>");
        if (n2 <= 0 || off + static_cast<size_t>(n2) >= cap) {
            return -1;
        }
        return static_cast<int>(off + static_cast<size_t>(n2));
    }
    int n3 = std::snprintf(out + off, cap - off, "<ul>");
    if (n3 <= 0 || off + static_cast<size_t>(n3) >= cap) {
        return -1;
    }
    off += static_cast<size_t>(n3);
    for (size_t i = 0; i < nvalid; ++i) {
        const Entry &e = copy[i];
        html_escape_text(e.symbol[0] ? e.symbol : "?", esc, sizeof(esc));
        const char *dir = e.up ? "UP" : "DOWN";
        n3 = std::snprintf(out + off,
                           cap - off,
                           "<li><strong>%s</strong> %s · prijs %.4f EUR · 30m %+.4f %% · ts_ms %lld</li>",
                           esc,
                           dir,
                           e.price_eur,
                           e.pct_1m,
                           (long long)e.ts_ms);
        if (n3 <= 0 || off + static_cast<size_t>(n3) >= cap) {
            return -1;
        }
        off += static_cast<size_t>(n3);
    }
    n3 = std::snprintf(out + off, cap - off, "</ul>");
    if (n3 <= 0 || off + static_cast<size_t>(n3) >= cap) {
        return -1;
    }
    off += static_cast<size_t>(n3);
    return static_cast<int>(off);
}

int append_alerts_2h_html_section(char *out, size_t cap)
{
    if (!out || cap < 64u) {
        return -1;
    }
    Entry copy[k_ring]{};
    uint32_t seq_copy = 0;
    size_t nvalid = 0;
    if (!s_mu || xSemaphoreTake(s_mu, pdMS_TO_TICKS(100)) != pdTRUE) {
        return std::snprintf(out,
                             cap,
                             "<h2>Recente 2h-alerts (S2H-3)</h2>"
                             "<p class=\"hint\">Observability tijdelijk niet beschikbaar.</p>");
    }
    seq_copy = s_seq2h;
    nvalid = (seq_copy < static_cast<uint32_t>(k_ring)) ? static_cast<size_t>(seq_copy) : k_ring;
    for (size_t i = 0; i < nvalid; ++i) {
        const size_t idx = (seq_copy > 0 ? (seq_copy - 1u - i) : 0u) % k_ring;
        copy[i] = s_ring2h[idx];
    }
    xSemaphoreGive(s_mu);

    char esc[sizeof(Entry::symbol) * 6]{};
    int n = std::snprintf(out,
                          cap,
                          "<h2>Recente 2h-alerts (S2H-3)</h2>"
                          "<p class=\"hint\">Read-only RAM na dispatch; max %u; geen historiek na reboot.</p>",
                          static_cast<unsigned>(k_ring));
    if (n <= 0 || static_cast<size_t>(n) >= cap) {
        return -1;
    }
    size_t off = static_cast<size_t>(n);
    if (nvalid == 0u) {
        const int n2 = std::snprintf(out + off, cap - off, "<p><em>Nog geen 2h-alerts sinds boot.</em></p>");
        if (n2 <= 0 || off + static_cast<size_t>(n2) >= cap) {
            return -1;
        }
        return static_cast<int>(off + static_cast<size_t>(n2));
    }
    int n3 = std::snprintf(out + off, cap - off, "<ul>");
    if (n3 <= 0 || off + static_cast<size_t>(n3) >= cap) {
        return -1;
    }
    off += static_cast<size_t>(n3);
    for (size_t i = 0; i < nvalid; ++i) {
        const Entry &e = copy[i];
        html_escape_text(e.symbol[0] ? e.symbol : "?", esc, sizeof(esc));
        const char *dir = e.up ? "UP" : "DOWN";
        n3 = std::snprintf(out + off,
                           cap - off,
                           "<li><strong>%s</strong> %s · prijs %.4f EUR · 2h %+.4f %% · ts_ms %lld</li>",
                           esc,
                           dir,
                           e.price_eur,
                           e.pct_1m,
                           (long long)e.ts_ms);
        if (n3 <= 0 || off + static_cast<size_t>(n3) >= cap) {
            return -1;
        }
        off += static_cast<size_t>(n3);
    }
    n3 = std::snprintf(out + off, cap - off, "</ul>");
    if (n3 <= 0 || off + static_cast<size_t>(n3) >= cap) {
        return -1;
    }
    off += static_cast<size_t>(n3);
    return static_cast<int>(off);
}

int append_alerts_conf_1m5m_html_section(char *out, size_t cap)
{
    if (!out || cap < 64u) {
        return -1;
    }
    EntryConf copy[k_ring]{};
    uint32_t seq_copy = 0;
    size_t nvalid = 0;
    if (!s_mu || xSemaphoreTake(s_mu, pdMS_TO_TICKS(100)) != pdTRUE) {
        return std::snprintf(out,
                             cap,
                             "<h2>Recente 1m+5m-confluence (M-010d)</h2>"
                             "<p class=\"hint\">Observability tijdelijk niet beschikbaar.</p>");
    }
    seq_copy = s_seq_conf;
    nvalid = (seq_copy < static_cast<uint32_t>(k_ring)) ? static_cast<size_t>(seq_copy) : k_ring;
    for (size_t i = 0; i < nvalid; ++i) {
        const size_t idx = (seq_copy > 0 ? (seq_copy - 1u - i) : 0u) % k_ring;
        copy[i] = s_ring_conf[idx];
    }
    xSemaphoreGive(s_mu);

    char esc[sizeof(EntryConf::symbol) * 6]{};
    int n = std::snprintf(out,
                          cap,
                          "<h2>Recente 1m+5m-confluence (M-010d)</h2>"
                          "<p class=\"hint\">Read-only RAM; max %u; geen historiek na reboot.</p>",
                          static_cast<unsigned>(k_ring));
    if (n <= 0 || static_cast<size_t>(n) >= cap) {
        return -1;
    }
    size_t off = static_cast<size_t>(n);
    if (nvalid == 0u) {
        const int n2 =
            std::snprintf(out + off, cap - off, "<p><em>Nog geen confluence-events sinds boot.</em></p>");
        if (n2 <= 0 || off + static_cast<size_t>(n2) >= cap) {
            return -1;
        }
        return static_cast<int>(off + static_cast<size_t>(n2));
    }
    int n3 = std::snprintf(out + off, cap - off, "<ul>");
    if (n3 <= 0 || off + static_cast<size_t>(n3) >= cap) {
        return -1;
    }
    off += static_cast<size_t>(n3);
    for (size_t i = 0; i < nvalid; ++i) {
        const EntryConf &e = copy[i];
        html_escape_text(e.symbol[0] ? e.symbol : "?", esc, sizeof(esc));
        const char *dir = e.up ? "UP" : "DOWN";
        n3 = std::snprintf(out + off,
                           cap - off,
                           "<li><strong>%s</strong> %s · prijs %.4f EUR · 1m %+.4f %% · 5m %+.4f %% · "
                           "ts_ms %lld</li>",
                           esc,
                           dir,
                           e.price_eur,
                           e.pct_1m,
                           e.pct_5m,
                           (long long)e.ts_ms);
        if (n3 <= 0 || off + static_cast<size_t>(n3) >= cap) {
            return -1;
        }
        off += static_cast<size_t>(n3);
    }
    n3 = std::snprintf(out + off, cap - off, "</ul>");
    if (n3 <= 0 || off + static_cast<size_t>(n3) >= cap) {
        return -1;
    }
    off += static_cast<size_t>(n3);
    return static_cast<int>(off);
}

} // namespace alert_observability
