/**
 * Stap 8b: compacte live view (135×240) — alleen `market_data::snapshot()`, geen exchange-details.
 * Hiërarchie: symbool (secundair) → prijs dominant → EUR-regel → bron (klein).
 */
#include "ui/ui.hpp"
#include "display_port/display_port.hpp"
#include "diagnostics/diagnostics.hpp"
#include "esp_check.h"
#include "esp_log.h"
#include "market_data/types.hpp"

#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "sdkconfig.h"

#include <cstdio>

/** Effectieve LVGL swap_bytes: alleen `DISPLAY_DIAG_PROFILE_*` — oude `CONFIG_UI_LVGL_SWAP_BYTES` in sdkconfig kan anders blijven staan en profiel 1 breken. */
#if defined(CONFIG_DISPLAY_DIAG_PROFILE_SWAP_OFF) && CONFIG_DISPLAY_DIAG_PROFILE_SWAP_OFF
#define UI_LVGL_SWAP_BYTES_EFFECTIVE 0
#else
#define UI_LVGL_SWAP_BYTES_EFFECTIVE 1
#endif

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
/** Regel 1: alleen het bedrag (dominant). */
static lv_obj_t *s_lbl_price = nullptr;
/** Regel 2: eenheid «EUR» (secundair, onder bedrag). */
static lv_obj_t *s_lbl_price_unit = nullptr;
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

static void apply_screen_chrome(lv_obj_t *scr)
{
    /* Rustig donker; geen puur zwart demo-beeld. */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0B0D0C), 0);
    lv_obj_set_style_pad_left(scr, 10, 0);
    lv_obj_set_style_pad_right(scr, 10, 0);
    lv_obj_set_style_pad_top(scr, 14, 0);
    lv_obj_set_style_pad_bottom(scr, 18, 0);
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
    disp_cfg.flags.swap_bytes = UI_LVGL_SWAP_BYTES_EFFECTIVE;
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

#if LVGL_VERSION_MAJOR >= 9
    ESP_LOGI(TAG, "LVGL disp: swap_bytes=%d (afgeleid van DISPLAY_DIAG_PROFILE)", UI_LVGL_SWAP_BYTES_EFFECTIVE);
#endif

    lv_obj_t *scr = screen_root();
    apply_screen_chrome(scr);

    s_lbl_symbol = lv_label_create(scr);
    lv_label_set_text(s_lbl_symbol, "—");
    lv_obj_set_style_text_color(s_lbl_symbol, lv_color_hex(0x9CA3AF), 0);
    lv_obj_set_style_text_align(s_lbl_symbol, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_lbl_symbol, static_cast<lv_coord_t>(display_port::panel_width() - 20));
    lv_obj_align(s_lbl_symbol, LV_ALIGN_TOP_MID, 0, 0);

    s_lbl_price = lv_label_create(scr);
    lv_label_set_text(s_lbl_price, "—");
    lv_obj_set_style_text_color(s_lbl_price, lv_color_hex(0xECFDF5), 0);
    lv_obj_set_style_text_align(s_lbl_price, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_letter_space(s_lbl_price, 1, 0);
    lv_obj_align(s_lbl_price, LV_ALIGN_CENTER, 0, -26);

    s_lbl_price_unit = lv_label_create(scr);
    lv_label_set_text(s_lbl_price_unit, "EUR");
    lv_obj_set_style_text_color(s_lbl_price_unit, lv_color_hex(0x6EE7B7), 0);
    lv_obj_set_style_text_opa(s_lbl_price_unit, LV_OPA_80, 0);
    lv_obj_set_style_text_align(s_lbl_price_unit, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align_to(s_lbl_price_unit, s_lbl_price, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

    s_lbl_source = lv_label_create(scr);
    lv_label_set_text(s_lbl_source, "Bron · —");
    lv_obj_set_style_text_color(s_lbl_source, lv_color_hex(0x6B7280), 0);
    lv_obj_set_style_text_align(s_lbl_source, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_lbl_source, LV_ALIGN_BOTTOM_MID, 0, 0);

    lvgl_port_unlock();

    ESP_LOGI(DIAG_TAG_UI, "ui: Stap 8b live view (hiërarchie/spacing) — data via market_data::snapshot");
    return ESP_OK;
}

void refresh_from_snapshot(const market_data::MarketSnapshot &snap)
{
    if (!s_lbl_symbol || !s_lbl_price || !s_lbl_price_unit || !s_lbl_source) {
        return;
    }
    if (!lvgl_port_lock(100)) {
        return;
    }

    const char *sym = snap.market_label[0] ? snap.market_label : "—";

    char num_buf[32];
    if (snap.valid) {
        std::snprintf(num_buf, sizeof(num_buf), "%.2f", snap.last_tick.price_eur);
    } else {
        std::snprintf(num_buf, sizeof(num_buf), "—");
    }

    char src_line[40];
    std::snprintf(src_line, sizeof(src_line), "Bron · %s", tick_source_str(snap.last_tick_source));

    lv_label_set_text(s_lbl_symbol, sym);
    lv_label_set_text(s_lbl_price, num_buf);
    lv_label_set_text(s_lbl_price_unit, snap.valid ? "EUR" : " ");
    lv_obj_set_style_text_opa(s_lbl_price_unit, snap.valid ? LV_OPA_80 : LV_OPA_TRANSP, 0);
    lv_label_set_text(s_lbl_source, src_line);

    lvgl_port_unlock();
}

} // namespace ui
