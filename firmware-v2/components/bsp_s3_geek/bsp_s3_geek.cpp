#include "bsp_s3_geek/bsp_s3_geek.hpp"
#include "bsp_s3_geek/geek_pins.hpp"
#include "driver/gpio.h"

namespace bsp_s3_geek {

static const bsp_common::BoardDescriptor k_desc{
    "esp32s3_geek_st7789_114",
    "ESP32-S3 GEEK (ST7789 1.14\")",
    bsp_common::BoardCapabilities{pins::WIDTH, pins::HEIGHT, false, false},
};

esp_err_t backlight_set(bool on)
{
    const int bl = pins::BACKLIGHT;
    gpio_set_level(static_cast<gpio_num_t>(bl), on ? 1 : 0);
    return ESP_OK;
}

esp_err_t init()
{
    const int bl = pins::BACKLIGHT;
    gpio_reset_pin(static_cast<gpio_num_t>(bl));
    gpio_set_direction(static_cast<gpio_num_t>(bl), GPIO_MODE_OUTPUT);
    /* Uit tijdens panel-reset/init; display_port zet aan na `disp_on`. */
    gpio_set_level(static_cast<gpio_num_t>(bl), 0);
    return ESP_OK;
}

const bsp_common::BoardDescriptor &board_descriptor()
{
    return k_desc;
}

} // namespace bsp_s3_geek
