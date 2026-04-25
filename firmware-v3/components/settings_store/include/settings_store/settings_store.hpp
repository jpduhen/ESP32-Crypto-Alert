#pragma once

#include "esp_err.h"

namespace settings_store {

/** NVS-keyed configuratie (WiFi, pairs, ntfy); apart van domein-engines. */
esp_err_t init();

}  // namespace settings_store
