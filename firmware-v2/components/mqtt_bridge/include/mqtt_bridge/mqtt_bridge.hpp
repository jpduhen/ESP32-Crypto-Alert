#pragma once

#include "esp_err.h"

namespace mqtt_bridge {

/**
 * M-012a: minimale MQTT-transportlaag — geen HA discovery, geen entities.
 * Eén publish-pad voor «application ready» / boot-status (topic uit Kconfig).
 * M-012b: één publish-pad voor 1m-domeinalert (JSON, topic uit Kconfig).
 * M-010c: idem voor 5m-domeinalert (`MQTT_TOPIC_DOMAIN_ALERT_5M`).
 * M-010d: confluence 1m+5m (`MQTT_TOPIC_DOMAIN_ALERT_CONF_1M5M`).
 * S30-3: 30m-domeinalert (`MQTT_TOPIC_DOMAIN_ALERT_30M`).
 * S2H-3: 2h-domeinalert (`MQTT_TOPIC_DOMAIN_ALERT_2H`).
 */
esp_err_t init();

/**
 * Markeert dat een boot-/ready-publish gewenst is; wordt verstuurd zodra de
 * client verbonden is (of direct als al verbonden). Zonder broker-URI: no-op.
 */
void request_application_ready_publish();

/**
 * M-012b: publiceert compacte JSON naar `MQTT_TOPIC_DOMAIN_ALERT_1M` (QoS1, geen retain).
 * Payload bevat o.a. `kind` (`alert_1m`), `symbol`, `dir`, `price_eur`, `pct_1m`, `ts_ms`.
 * Alleen als MQTT build+runtime aan en client verbonden; anders log + no-op.
 */
void publish_domain_alert_1m(const char *symbol,
                             bool up,
                             double price_eur,
                             double pct_1m,
                             int64_t ts_ms);

/** M-010c: JSON met `kind` (`alert_5m`), pct_5m, … naar `MQTT_TOPIC_DOMAIN_ALERT_5M` (QoS1, geen retain). */
void publish_domain_alert_5m(const char *symbol,
                               bool up,
                               double price_eur,
                               double pct_5m,
                               int64_t ts_ms);

/** S30-3: JSON met `kind` (`alert_30m`), pct_30m, … naar `MQTT_TOPIC_DOMAIN_ALERT_30M`. */
void publish_domain_alert_30m(const char *symbol,
                                bool up,
                                double price_eur,
                                double pct_30m,
                                int64_t ts_ms);

/** S2H-3: JSON met `kind` (`alert_2h`), pct_2h, … naar `MQTT_TOPIC_DOMAIN_ALERT_2H`. */
void publish_domain_alert_2h(const char *symbol,
                               bool up,
                               double price_eur,
                               double pct_2h,
                               int64_t ts_ms);

/** M-010d: JSON met `kind` (`alert_confluence_1m5m`), pct_1m + pct_5m. */
void publish_domain_alert_confluence_1m5m(const char *symbol,
                                           bool up,
                                           double price_eur,
                                           double pct_1m,
                                           double pct_5m,
                                           int64_t ts_ms);

} // namespace mqtt_bridge
