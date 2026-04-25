#pragma once

#include "esp_err.h"

namespace strategy_engine {

/**
 * Orchestratie van strategie (single writer: setups/signalen).
 * Toekomst: consumeert candles/levels/regime; produceert setup-scores en triggers
 * richting alert_engine. Geen websocket/UI-hier.
 *
 * Deellogica:
 * - regime_engine: marktregime / volatiliteit
 * - level_engine: prijsniveaus / zones
 * - signal_engine: ruwe signalen / bevestigingen
 * - setup score: samenvoeging (later)
 */
esp_err_t init();
esp_err_t start();

}  // namespace strategy_engine
