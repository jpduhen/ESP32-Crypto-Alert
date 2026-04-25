#pragma once

#include "esp_err.h"

namespace ui_model {

/**
 * Read-only snapshots voor UI (consument, geen eigenaar van WS/strategie).
 * Later: atomair gelezen view-models.
 */
esp_err_t init();

}  // namespace ui_model
