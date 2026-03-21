#pragma once

#include <stddef.h>

#include "DisplayBackend.h"

#include "esp_lcd_axs15231b/esp_lcd_axs15231b.h"
#include "esp_lcd_axs15231b/esp_lcd_axs15231b_interface.h"
#include "esp_lcd_panel_io.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class DisplayBackend_Axs15231bEspLcd : public DisplayBackend {
public:
    DisplayBackend_Axs15231bEspLcd() = default;
    ~DisplayBackend_Axs15231bEspLcd() override = default;

    bool begin(uint32_t speed) override;

    uint32_t width() const override;
    uint32_t height() const override;

    void setRotation(uint8_t rotation_deg_compatible) override;
    void invertDisplay(bool invert) override;
    void fillScreen(uint16_t rgb565_color) override;

    void flush(const lv_area_t *area, const uint8_t *px_map) override;

private:
    static bool onColorTransDone(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);

    // esp_lcd handles created during begin()
    esp_lcd_panel_handle_t panel_ = nullptr;
    esp_lcd_panel_io_handle_t io_ = nullptr;

    // Scratch buffer used by fillScreen() (optional)
    uint16_t *fill_buf_ = nullptr;
    uint32_t fill_buf_px_ = 0;

    // DMA transport buffers for striped flush from SPIRAM LVGL framebuffer.
    uint16_t *trans_buf_1_ = nullptr;
    uint16_t *trans_buf_2_ = nullptr;
    size_t trans_buf_bytes_ = 0;
    uint16_t trans_buf_lines_ = 0;
    bool use_double_trans_buf_ = false;
    uint8_t trans_buf_toggle_ = 0;
    SemaphoreHandle_t trans_done_sem_ = nullptr;
    bool transport_ready_ = false;

#if defined(PLATFORM_ESP32S3_JC3248W535) && CRYPTO_ALERT_AXS15231B_USE_TE_SYNC
    // TE (tearing effect) sync: GPIO-ISR geeft semaphore op TE-flank; vóór draw wachten op volgende edge
    // (vergelijkbaar met vendor draw_wait_cb / bsp_display_sync_cb, zonder volledige BSP).
    SemaphoreHandle_t te_sync_sem_ = nullptr;
    bool te_sync_ready_ = false;
    bool teSyncInit();
    void teSyncWaitBeforeDraw(const char *reason_tag);
#endif
};

