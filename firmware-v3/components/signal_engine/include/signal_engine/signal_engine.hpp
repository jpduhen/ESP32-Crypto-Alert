#pragma once

#include "esp_err.h"

namespace signal_engine {

/** Ruwe/composite signalen; single writer vanuit strategie-pipeline. */
esp_err_t init();

}  // namespace signal_engine
