#include "board_support/board_support.hpp"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

namespace {

const char *TAG = "BOARD";

constexpr int PIN_MOSI = 11;
constexpr int PIN_SCLK = 12;
constexpr int PIN_CS = 10;
constexpr int PIN_DC = 8;
constexpr int PIN_RST = 9;
constexpr int PIN_BL = 7;

constexpr int WIDTH = 135;
constexpr int HEIGHT = 240;
constexpr int ST7789_X_GAP = 52;
constexpr int ST7789_Y_GAP = 40;
constexpr int PIXEL_CLOCK_HZ = 27 * 1000 * 1000;

bool s_ready = false;
esp_lcd_panel_io_handle_t s_io = nullptr;
esp_lcd_panel_handle_t s_panel = nullptr;

esp_err_t backlight_set(bool on) {
    gpio_set_level(static_cast<gpio_num_t>(PIN_BL), on ? 1 : 0);
    return ESP_OK;
}

}  // namespace

namespace board_support {

esp_err_t init() {
    if (s_ready) {
        return ESP_OK;
    }

    gpio_reset_pin(static_cast<gpio_num_t>(PIN_BL));
    gpio_set_direction(static_cast<gpio_num_t>(PIN_BL), GPIO_MODE_OUTPUT);
    gpio_set_level(static_cast<gpio_num_t>(PIN_BL), 0);

    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num = static_cast<gpio_num_t>(PIN_SCLK);
    buscfg.mosi_io_num = static_cast<gpio_num_t>(PIN_MOSI);
    buscfg.miso_io_num = -1;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = WIDTH * HEIGHT * static_cast<int>(sizeof(uint16_t)) + 8;

    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "spi bus init");

    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.cs_gpio_num = static_cast<gpio_num_t>(PIN_CS);
    io_cfg.dc_gpio_num = static_cast<gpio_num_t>(PIN_DC);
    io_cfg.spi_mode = 0;
    io_cfg.pclk_hz = PIXEL_CLOCK_HZ;
    io_cfg.trans_queue_depth = 10;
    io_cfg.lcd_cmd_bits = 8;
    io_cfg.lcd_param_bits = 8;

    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(SPI2_HOST), &io_cfg, &s_io),
        TAG, "new panel io");

    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.reset_gpio_num = static_cast<gpio_num_t>(PIN_RST);
    panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panel_cfg.bits_per_pixel = 16;
    panel_cfg.data_endian = LCD_RGB_DATA_ENDIAN_LITTLE;

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(s_io, &panel_cfg, &s_panel), TAG, "new st7789 panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, true), TAG, "panel invert");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(s_panel, ST7789_X_GAP, ST7789_Y_GAP), TAG, "panel gap");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, false), TAG, "panel swap xy");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, false, false), TAG, "panel mirror");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "panel on");

    ESP_RETURN_ON_ERROR(backlight_set(true), TAG, "backlight on");

    s_ready = true;
    ESP_LOGI(TAG, "display init ok (ST7789 %dx%d)", WIDTH, HEIGHT);
    ESP_LOGI(TAG, "backlight on");
    return ESP_OK;
}

bool display_ready() {
    return s_ready && s_panel != nullptr;
}

esp_lcd_panel_handle_t display_panel() {
    return s_panel;
}

unsigned display_width() {
    return static_cast<unsigned>(WIDTH);
}

unsigned display_height() {
    return static_cast<unsigned>(HEIGHT);
}

}  // namespace board_support
