#pragma once

#include "DisplayBackend.h"

// Forward declaration to keep this header independent from Arduino_GFX includes.
class Arduino_GFX;

// Backend wrapper around the existing global Arduino_GFX instance from the active PINS_*.h.
class DisplayBackend_ArduinoGFX : public DisplayBackend {
public:
    DisplayBackend_ArduinoGFX() = default;
    ~DisplayBackend_ArduinoGFX() override = default;

    bool begin(uint32_t speed) override;
    uint32_t width() const override;
    uint32_t height() const override;

    void setRotation(uint8_t rotation_deg_compatible) override;
    void invertDisplay(bool invert) override;
    void fillScreen(uint16_t rgb565_color) override;

    void flush(const lv_area_t *area, const uint8_t *px_map) override;

private:
    // Provided by PINS_*.h (platform_config.h includes the relevant PINS file).
    // We keep the dependency here so the rest of the app doesn't reference gfx directly.
    Arduino_GFX *gfx_() const;
};

