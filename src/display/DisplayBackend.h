#pragma once

#include <stdint.h>
#include <lvgl.h>

class DisplayBackend {
public:
    virtual ~DisplayBackend() = default;

    // Init display hardware for the backend (and make display ready for drawing).
    // speed can be ignored by backends that don't use an explicit bus frequency.
    virtual bool begin(uint32_t speed) = 0;

    virtual uint32_t width() const = 0;
    virtual uint32_t height() const = 0;

    virtual void setRotation(uint8_t rotation_deg_compatible) = 0;
    virtual void invertDisplay(bool invert) = 0;

    // Clear/fill the whole screen (optional but used by existing app logic).
    virtual void fillScreen(uint16_t rgb565_color) = 0;

    // LVGL flush callback hook.
    // px_map points to a contiguous array of lv_color_t (RGB565 by config).
    virtual void flush(const lv_area_t *area, const uint8_t *px_map) = 0;
};

// Global backend instance (set during setupDisplay()).
extern DisplayBackend *g_displayBackend;

