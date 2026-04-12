#include "ui/ui.hpp"
#include "display_port/display_port.hpp"
#include "diagnostics/diagnostics.hpp"
#include "esp_check.h"
#include "esp_log.h"
#include "market_data/types.hpp"

#include "esp_lvgl_port.h"
#include "lvgl.h"

#include <cstdio>

namespace ui {

namespace {

static const char TAG[] = "ui";

#if LVGL_VERSION_MAJOR >= 9
static lv_obj_t *screen_root()
{
    return lv_screen_active();
}
#else
static lv_obj_t *screen_root()
{
    return lv_scr_act();
}
#endif

static lv_obj_t *s_lbl_symbol = nullptr;
static lv_obj_t *s_lbl_price = nullptr;
static lv_obj_t *s_lbl_source = nullptr;

static const char *tick_source_str(market_data::TickSource s)
{
    switch (s) {
    case market_data::TickSource::Ws:
        return "WS";
    case market_data::TickSource::Rest:
        return "REST";
    case market_data::TickSource::None:
        return "—";
    }
    return "—";
}

} // namespace

esp_err_t init()
{
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "lvgl_port_init");

    lvgl_port_display_cfg_t disp_cfg = {};
    disp_cfg.io_handle = display_port::io_handle();
    disp_cfg.panel_handle = display_port::panel_handle();
    disp_cfg.buffer_size = display_port::panel_width() * 40;
    disp_cfg.double_buffer = true;
    disp_cfg.hres = display_port::panel_width();
    disp_cfg.vres = display_port::panel_height();
    disp_cfg.rotation.swap_xy = false;
    disp_cfg.rotation.mirror_x = false;
    disp_cfg.rotation.mirror_y = false;
    disp_cfg.flags.buff_dma = 1;
#if LVGL_VERSION_MAJOR >= 9
    disp_cfg.color_format = LV_COLOR_FORMAT_RGB565;
    disp_cfg.flags.swap_bytes = 1;
#endif

#if LVGL_VERSION_MAJOR >= 9
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);
#else
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);
#endif
    if (disp == nullptr) {
        ESP_LOGE(TAG, "lvgl_port_add_disp failed");
        return ESP_FAIL;
    }
    (void)disp;

    if (!lvgl_port_lock(2000)) {
        ESP_LOGE(TAG, "lvgl_port_lock failed");
        return ESP_ERR_TIMEOUT;
    }

    lv_obj_t *scr = screen_root();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_lbl_symbol = lv_label_create(scr);
    lv_label_set_text(s_lbl_symbol, "—");
    lv_obj_set_style_text_color(s_lbl_symbol, lv_color_white(), 0);
    lv_obj_align(s_lbl_symbol, LV_ALIGN_TOP_MID, 0, 8);

    s_lbl_price = lv_label_create(scr);
    lv_label_set_text(s_lbl_price, "—");
    lv_obj_set_style_text_color(s_lbl_price, lv_color_hex(0x6EE7B7), 0);
    lv_obj_align(s_lbl_price, LV_ALIGN_CENTER, 0, -10);

    s_lbl_source = lv_label_create(scr);
    lv_label_set_text(s_lbl_source, "Bron: —");
    lv_obj_set_style_text_color(s_lbl_source, lv_color_hex(0xAAAAAA), 0);
    lv_obj_align(s_lbl_source, LV_ALIGN_BOTTOM_MID, 0, -16);

    lvgl_port_unlock();

    ESP_LOGI(DIAG_TAG_UI, "ui: LVGL minimale view (sym / prijs / bron) — data via market_data::snapshot");
    return ESP_OK;
}

void refresh_from_snapshot(const market_data::MarketSnapshot &snap)
{
    if (!s_lbl_symbol || !s_lbl_price || !s_lbl_source) {
        return;
    }
    if (!lvgl_port_lock(100)) {
        return;
    }

    const char *sym = snap.market_label[0] ? snap.market_label : "—";

    char price_line[48];
    if (snap.valid) {
        std::snprintf(price_line, sizeof(price_line), "%.2f EUR", snap.last_tick.price_eur);
    } else {
        std::snprintf(price_line, sizeof(price_line), "—");
    }

    char src_line[40];
    std::snprintf(src_line, sizeof(src_line), "Bron: %s", tick_source_str(snap.last_tick_source));

    lv_label_set_text(s_lbl_symbol, sym);
    lv_label_set_text(s_lbl_price, price_line);
    lv_label_set_text(s_lbl_source, src_line);

    lvgl_port_unlock();
}

} // namespace ui
