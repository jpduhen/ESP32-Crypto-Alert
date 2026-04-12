#pragma once

#include "esp_err.h"

namespace diagnostics {

/** Eerste init: mag vóór NVS; alleen logging-basis. */
esp_err_t init_early();

/** Optioneel: health-buffer / tick (skeleton: no-op met periodieke log). */
void tick_heartbeat();

} // namespace diagnostics

/** Logtags — gebruik met ESP_LOGx(DIAG_TAG_*, ...). */
#define DIAG_TAG_BOOT     "diag_boot"
#define DIAG_TAG_CFG      "diag_cfg"
#define DIAG_TAG_BSP      "diag_bsp"
#define DIAG_TAG_DISP     "diag_disp"
#define DIAG_TAG_UI       "diag_ui"
#define DIAG_TAG_MARKET   "diag_mkt"
#define DIAG_TAG_HEALTH   "diag_health"
/** Exchange/TLS feed: REST/WS lifecycle (T-103b diagnose; grep `bv_feed`) */
#define DIAG_TAG_BV_FEED  "bv_feed"
