/**
 * ESP-IDF entry — bewust minimaal: geen applicatielogica hier.
 * Lifecycle zit in component `app_core`.
 */
#include "app_core/app_core.hpp"
#include "esp_log.h"

static const char TAG[] = "main";

extern "C" void app_main(void)
{
    esp_err_t err = app_core::run();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "app_core::run failed: %s", esp_err_to_name(err));
        abort();
    }
}
