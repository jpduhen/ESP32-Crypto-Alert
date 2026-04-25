#pragma once

#include "esp_err.h"

namespace app_core {

/** Eénmalige orchestratie: subsystems in vaste volgorde init/start. */
esp_err_t start();

}  // namespace app_core
