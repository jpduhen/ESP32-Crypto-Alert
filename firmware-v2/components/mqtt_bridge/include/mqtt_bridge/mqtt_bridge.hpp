#pragma once

#include "esp_err.h"

namespace mqtt_bridge {

/**
 * M-012a: minimale MQTT-transportlaag — geen HA discovery, geen entities.
 * Eén publish-pad voor «application ready» / boot-status (topic uit Kconfig).
 */
esp_err_t init();

/**
 * Markeert dat een boot-/ready-publish gewenst is; wordt verstuurd zodra de
 * client verbonden is (of direct als al verbonden). Zonder broker-URI: no-op.
 */
void request_application_ready_publish();

} // namespace mqtt_bridge
