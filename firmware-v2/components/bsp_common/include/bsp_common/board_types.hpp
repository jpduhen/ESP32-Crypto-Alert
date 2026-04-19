#pragma once

#include <cstdint>

namespace bsp_common {

/** Display + IO-mogelijkheden (uitbreidbaar voor LCDWIKI / JC3248). */
struct BoardCapabilities {
    uint16_t display_width{0};
    uint16_t display_height{0};
    bool has_touch{false};
    bool has_psram{false};

    constexpr BoardCapabilities(uint16_t w = 0, uint16_t h = 0, bool touch = false, bool psram = false)
        : display_width(w), display_height(h), has_touch(touch), has_psram(psram)
    {
    }
};

/**
 * Statische board-metadata — geen heap-allocs in hot path.
 * V2: één actieve BSP per build; later eventueel factory + registratie.
 */
struct BoardDescriptor {
    const char *id{nullptr};
    const char *display_name{nullptr};
    BoardCapabilities caps{};
};

} // namespace bsp_common
