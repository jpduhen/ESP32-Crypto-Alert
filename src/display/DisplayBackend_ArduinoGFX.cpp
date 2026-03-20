#include "DisplayBackend_ArduinoGFX.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

extern Arduino_GFX *gfx; // from the active PINS_*.h

// Optional: for PSRAM external draw buffers.
#if __has_include(<esp_cache.h>)
#include <esp_cache.h>
#define CRYPTO_ALERT_LVGL_HAS_ESP_CACHE 1
#else
#define CRYPTO_ALERT_LVGL_HAS_ESP_CACHE 0
#endif

Arduino_GFX *DisplayBackend_ArduinoGFX::gfx_() const {
    return gfx;
}

bool DisplayBackend_ArduinoGFX::begin(uint32_t speed) {
    Arduino_GFX *g = gfx;
    if (!g) {
        return false;
    }

    Serial.println("[DisplayBackend] Arduino_GFX begin()");

    bool ok = false;
    if (speed != 0) {
        ok = g->begin(speed);
    } else {
        ok = g->begin();
    }
    return ok;
}

uint32_t DisplayBackend_ArduinoGFX::width() const {
    return gfx ? gfx->width() : 0;
}

uint32_t DisplayBackend_ArduinoGFX::height() const {
    return gfx ? gfx->height() : 0;
}

void DisplayBackend_ArduinoGFX::setRotation(uint8_t rotation_deg_compatible) {
    if (!gfx) return;
    gfx->setRotation(rotation_deg_compatible);
}

void DisplayBackend_ArduinoGFX::invertDisplay(bool invert) {
    if (!gfx) return;
    gfx->invertDisplay(invert);
}

void DisplayBackend_ArduinoGFX::fillScreen(uint16_t rgb565_color) {
    if (!gfx) return;
    gfx->fillScreen(rgb565_color);
}

void DisplayBackend_ArduinoGFX::flush(const lv_area_t *area, const uint8_t *px_map) {
    if (!gfx || !area || !px_map) {
        return;
    }

    const uint32_t w = lv_area_get_width(area);
    const uint32_t h = lv_area_get_height(area);
    const size_t lenBytes = (size_t)w * (size_t)h * sizeof(uint16_t);

    // When LVGL draw buffer sits in external RAM, ensure DMA sees fresh data.
    // This matches the typical cache-coherency failure pattern (random small color blocks).
#if CRYPTO_ALERT_LVGL_HAS_ESP_CACHE
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_ESP32S3_DEV)
    if (lenBytes > 0) {
        // esp_ptr_external_ram is safe to call even for internal pointers.
        if (esp_ptr_external_ram(px_map)) {
            int msync_flags = ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA;
#if defined(ESP_CACHE_MSYNC_FLAG_UNALIGNED)
            msync_flags |= ESP_CACHE_MSYNC_FLAG_UNALIGNED;
#endif
            (void)esp_cache_msync((void *)px_map, lenBytes, msync_flags);
        }
    }
#endif
#endif

    gfx->draw16bitRGBBitmap(area->x1, area->y1, (const uint16_t *)px_map, w, h);
}

