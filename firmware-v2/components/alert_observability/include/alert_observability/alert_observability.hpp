#pragma once

#include "cJSON.h"
#include "esp_err.h"

#include <cstddef>
#include <cstdint>

namespace alert_observability {

/**
 * M-013d: read-only RAM-buffer van recente 1m-domeinalerts (observability, geen productdomein).
 * M-010c: parallel buffer voor 5m-alerts.
 * M-010d: buffer voor 1m+5m-confluence (pct_1m + pct_5m).
 * Geschreven vanuit `service_outbound` bij dispatch; gelezen door `webui`.
 */
esp_err_t init();

void record_1m_alert(const char *symbol, bool up, double price_eur, double pct_1m, int64_t ts_ms);

void record_5m_alert(const char *symbol, bool up, double price_eur, double pct_5m, int64_t ts_ms);

void record_conf_1m5m_alert(const char *symbol,
                            bool up,
                            double price_eur,
                            double pct_1m,
                            double pct_5m,
                            int64_t ts_ms);

/** Voegt `alerts_1m`, `alerts_5m`, `alerts_conf_1m5m` toe (nieuwste eerst). */
void add_alerts_to_cjson(cJSON *root);

/**
 * Schrijft compacte HTML voor 1m-alerts (h2 + lijst of lege staat).
 * Retourneert aantal geschreven bytes (excl. '\\0'), of -1 bij cap-overflow.
 */
int append_alerts_html_section(char *out, size_t cap);

/** M-010c: idem voor 5m (`pct_5m` in lijstregels). */
int append_alerts_5m_html_section(char *out, size_t cap);

/** M-010d: confluence-sectie (1m + 5m % in lijst). */
int append_alerts_conf_1m5m_html_section(char *out, size_t cap);

} // namespace alert_observability
