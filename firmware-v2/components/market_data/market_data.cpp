#include "market_data/market_data.hpp"
#include "diagnostics/diagnostics.hpp"
#include "esp_log.h"

#include "sdkconfig.h"

#if CONFIG_MD_USE_EXCHANGE_BITVAVO
#include "exchange_bitvavo/exchange_bitvavo.hpp"
#else
namespace market_data {
esp_err_t mock_init();
void mock_tick();
MarketSnapshot mock_snapshot();
} // namespace market_data
#endif

namespace market_data {

esp_err_t init(const config_store::RuntimeConfig &cfg)
{
#if CONFIG_MD_USE_EXCHANGE_BITVAVO
    ESP_LOGI(DIAG_TAG_MARKET, "provider=Bitvavo exchange (T-103)");
    return exchange_bitvavo::init(cfg.default_symbol);
#else
    ESP_LOGI(DIAG_TAG_MARKET, "provider=mock");
    return mock_init();
#endif
}

void tick()
{
#if CONFIG_MD_USE_EXCHANGE_BITVAVO
    exchange_bitvavo::tick();
#else
    mock_tick();
#endif
}

MarketSnapshot snapshot()
{
#if CONFIG_MD_USE_EXCHANGE_BITVAVO
    return exchange_bitvavo::snapshot();
#else
    return mock_snapshot();
#endif
}

} // namespace market_data
