/**
 * T-102: ST7789 via ESP-IDF esp_lcd (SPI). LVGL zit in `ui` (esp_lvgl_port); hier alleen hardware.
 *
 * Orientatie: gelijk aan V1 (`Arduino_ST7789` rotatie 0) — **geen** `swap_xy(true)`.
 * Eerdere bug: met MV (swap_xy) wijkt de mapping af van V1 → slechts deelvenster (ca. 135×135)
 * en oude GRAM-inhoud bleef zichtbaar.
 */
#include "display_port/display_port.hpp"
#include "bsp_s3_geek/bsp_s3_geek.hpp"
#include "bsp_s3_geek/geek_pins.hpp"
#include "diagnostics/diagnostics.hpp"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/lcd_types.h"

#ifndef DISPLAY_PORT_RGBK_DIAG
/** 1 = kort R→G→B→K na zwarte clear (hardwaretest); 0 = alleen clear + groen (snellere boot). */
#define DISPLAY_PORT_RGBK_DIAG 0
#endif

namespace display_port {

static const char TAG[] = "display_port";

namespace {

using namespace bsp_s3_geek::pins;

constexpr uint16_t kRgb565Black = 0x0000;
constexpr uint16_t kRgb565Red = 0xF800;
constexpr uint16_t kRgb565Green = 0x07E0;
constexpr uint16_t kRgb565Blue = 0x001F;
constexpr uint16_t kRgb565White = 0xFFFF;
/** Donkergroen — afwisselen met wit in validatiestrip (T-103 field). */
constexpr uint16_t kRgb565GreenDim = 0x0720;

static esp_lcd_panel_io_handle_t s_io_handle = nullptr;
static esp_lcd_panel_handle_t s_validation_panel = nullptr;

static esp_err_t validation_fill_rect(esp_lcd_panel_handle_t panel, int x, int y, int rw, int rh, uint16_t rgb565)
{
    if (rw <= 0 || rh <= 0) {
        return ESP_OK;
    }
    const size_t n = static_cast<size_t>(rw) * static_cast<size_t>(rh);
    uint16_t *const buf = static_cast<uint16_t *>(
        heap_caps_malloc(n * sizeof(uint16_t), MALLOC_CAP_DMA));
    ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "validation_fill_rect malloc");
    for (size_t i = 0; i < n; ++i) {
        buf[i] = rgb565;
    }
    const esp_err_t err = esp_lcd_panel_draw_bitmap(panel, x, y, x + rw, y + rh, buf);
    heap_caps_free(buf);
    return err;
}

static bool s_validation_strip_phase = false;

static esp_err_t draw_fullscreen_color(esp_lcd_panel_handle_t panel, int w, int h, uint16_t rgb565)
{
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    uint16_t *const buf = static_cast<uint16_t *>(
        heap_caps_malloc(n * sizeof(uint16_t), MALLOC_CAP_DMA));
    ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "heap_caps_malloc framebuffer");
    for (size_t i = 0; i < n; ++i) {
        buf[i] = rgb565;
    }
    /* esp_lcd ST7789: x_end / y_end exclusief (driver zet CASET/RASET tot x_end-1, y_end-1). */
    const esp_err_t err = esp_lcd_panel_draw_bitmap(panel, 0, 0, w, h, buf);
    heap_caps_free(buf);
    return err;
}

#if DISPLAY_PORT_RGBK_DIAG
/** Kleine gevulde rechthoek (hoekmarkers); w,h klein houden. */
static esp_err_t draw_filled_rect(esp_lcd_panel_handle_t panel, int x, int y, int rw, int rh, uint16_t rgb565)
{
    if (rw <= 0 || rh <= 0) {
        return ESP_OK;
    }
    const size_t n = static_cast<size_t>(rw) * static_cast<size_t>(rh);
    uint16_t *const buf = static_cast<uint16_t *>(
        heap_caps_malloc(n * sizeof(uint16_t), MALLOC_CAP_DMA));
    ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "heap_caps_malloc rect");
    for (size_t i = 0; i < n; ++i) {
        buf[i] = rgb565;
    }
    const esp_err_t err =
        esp_lcd_panel_draw_bitmap(panel, x, y, x + rw, y + rh, buf);
    heap_caps_free(buf);
    return err;
}

static esp_err_t diag_rgbk_cycle(esp_lcd_panel_handle_t panel)
{
    const uint16_t seq[] = {kRgb565Red, kRgb565Green, kRgb565Blue, kRgb565Black};
    const char *const names[] = {"rood", "groen", "blauw", "zwart"};
    for (unsigned i = 0; i < sizeof(seq) / sizeof(seq[0]); ++i) {
        ESP_RETURN_ON_ERROR(draw_fullscreen_color(panel, WIDTH, HEIGHT, seq[i]), TAG, "diag fill");
        ESP_LOGI(DIAG_TAG_DISP, "T-102 diag: full-screen %s", names[i]);
        vTaskDelay(pdMS_TO_TICKS(350));
    }
    return ESP_OK;
}

/** Witte hoekjes (4×4) om randbereik te zien. */
static esp_err_t draw_corner_marks(esp_lcd_panel_handle_t panel)
{
    constexpr int s = 4;
    ESP_RETURN_ON_ERROR(draw_filled_rect(panel, 0, 0, s, s, kRgb565White), TAG, "mark TL");
    ESP_RETURN_ON_ERROR(draw_filled_rect(panel, WIDTH - s, 0, s, s, kRgb565White), TAG, "mark TR");
    ESP_RETURN_ON_ERROR(draw_filled_rect(panel, 0, HEIGHT - s, s, s, kRgb565White), TAG, "mark BL");
    ESP_RETURN_ON_ERROR(draw_filled_rect(panel, WIDTH - s, HEIGHT - s, s, s, kRgb565White), TAG, "mark BR");
    return ESP_OK;
}
#endif

} // namespace

esp_err_t init()
{
    const auto &d = bsp_s3_geek::board_descriptor();
    ESP_LOGI(DIAG_TAG_DISP, "T-102: esp_lcd + ST7789 SPI (GEEK %s) %ux%u, geen swap_xy (zoals V1 rot=0)",
             d.id, (unsigned)WIDTH, (unsigned)HEIGHT);

    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num = SCLK;
    buscfg.mosi_io_num = MOSI;
    buscfg.miso_io_num = -1;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = static_cast<int>(WIDTH) * HEIGHT * static_cast<int>(sizeof(uint16_t)) + 8;

    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "spi_bus_initialize");

    esp_lcd_panel_io_handle_t io_handle = nullptr;
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = CS;
    io_config.dc_gpio_num = DC;
    io_config.spi_mode = 0;
    io_config.pclk_hz = SPI_PIXEL_CLOCK_HZ;
    io_config.trans_queue_depth = 10;
    io_config.lcd_cmd_bits = 8;
    io_config.lcd_param_bits = 8;

    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi(static_cast<esp_lcd_spi_bus_handle_t>(SPI2_HOST), &io_config, &io_handle),
        TAG, "esp_lcd_new_panel_io_spi");
    s_io_handle = io_handle;

    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = RST;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = 16;
    panel_config.data_endian = LCD_RGB_DATA_ENDIAN_LITTLE;

    esp_lcd_panel_handle_t panel_handle = nullptr;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle), TAG,
                        "esp_lcd_new_panel_st7789");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel_handle), TAG, "panel_reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel_handle), TAG, "panel_init");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(panel_handle, true), TAG, "invert_color (IPS)");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(panel_handle, ST7789_X_GAP, ST7789_Y_GAP), TAG, "set_gap");

    /* V1 gebruikt rotatie 0 zonder MV — hier expliciet geen assen wisselen. */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(panel_handle, false), TAG, "swap_xy off");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(panel_handle, true), TAG, "disp_on");

    ESP_RETURN_ON_ERROR(bsp_s3_geek::backlight_set(true), TAG, "backlight on");

    /* Volledige GRAM overschrijven (rest oude firmware / V1 UI). */
    ESP_RETURN_ON_ERROR(draw_fullscreen_color(panel_handle, WIDTH, HEIGHT, kRgb565Black), TAG, "clear black");

#if DISPLAY_PORT_RGBK_DIAG
    ESP_RETURN_ON_ERROR(diag_rgbk_cycle(panel_handle), TAG, "diag rgbk");
#endif

    /* Zwarte basis vóór LVGL (`ui`): geen effen testgroen meer als eindstaat (T-102 hardware is bevestigd). */
    ESP_RETURN_ON_ERROR(draw_fullscreen_color(panel_handle, WIDTH, HEIGHT, kRgb565Black), TAG, "pre-LVGL clear");

#if DISPLAY_PORT_RGBK_DIAG
    ESP_RETURN_ON_ERROR(draw_corner_marks(panel_handle), TAG, "corner marks");
#endif

    ESP_LOGI(DIAG_TAG_DISP,
             "T-102/ADR-004: %ux%u klaar voor LVGL (swap_xy uit, gap %d,%d).",
             (unsigned)WIDTH, (unsigned)HEIGHT, ST7789_X_GAP, ST7789_Y_GAP);
#if DISPLAY_PORT_RGBK_DIAG
    ESP_LOGI(DIAG_TAG_DISP, "T-102: RGBK-diag + witte hoekmarkers actief.");
#endif
    s_validation_panel = panel_handle;
    return ESP_OK;
}

esp_lcd_panel_io_handle_t io_handle()
{
    return s_io_handle;
}

esp_lcd_panel_handle_t panel_handle()
{
    return s_validation_panel;
}

unsigned panel_width()
{
    using namespace bsp_s3_geek::pins;
    return static_cast<unsigned>(WIDTH);
}

unsigned panel_height()
{
    using namespace bsp_s3_geek::pins;
    return static_cast<unsigned>(HEIGHT);
}

void ws_live_validation_strip_toggle()
{
    if (!s_validation_panel) {
        return;
    }
    using namespace bsp_s3_geek::pins;
    s_validation_strip_phase = !s_validation_strip_phase;
    const uint16_t c = s_validation_strip_phase ? kRgb565White : kRgb565GreenDim;
    const int band_h = 14;
    const int y0 = HEIGHT - band_h;
    if (y0 < 0) {
        return;
    }
    const esp_err_t e = validation_fill_rect(s_validation_panel, 0, y0, WIDTH, band_h, c);
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "validation strip draw: %s", esp_err_to_name(e));
    } else {
        ESP_LOGD(DIAG_TAG_DISP, "T-103 field: onderband getoggled (WS validatie, geen LVGL)");
    }
}

} // namespace display_port
