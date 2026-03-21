// platform_config vóór backend-header (JC3248-macro's voor TE-sync in header).
#define MODULE_INCLUDE
#include "../../platform_config.h"
#undef MODULE_INCLUDE

#ifndef CRYPTO_ALERT_AXS15231B_USE_TE_SYNC
#define CRYPTO_ALERT_AXS15231B_USE_TE_SYNC 0
#endif

#include "DisplayBackend_Axs15231bEspLcd.h"

#include <Arduino.h>
#include <stddef.h>
#include <string.h>

#include "esp_lcd_axs15231b/esp_lcd_axs15231b.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include <esp_heap_caps.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#if __has_include(<esp_cache.h>)
#include <esp_cache.h>
#define CRYPTO_ALERT_AXS15231B_HAS_ESP_CACHE 1
#else
#define CRYPTO_ALERT_AXS15231B_HAS_ESP_CACHE 0
#endif

#if defined(PLATFORM_ESP32S3_JC3248W535)
#ifndef CRYPTO_ALERT_AXS15231B_USE_VENDOR_ESPRESSIF_PINS
#define CRYPTO_ALERT_AXS15231B_USE_VENDOR_ESPRESSIF_PINS 1
#endif
#endif

// Pin / resolution mapping for this backend only.
#if defined(PLATFORM_ESP32S3_JC3248W535) && CRYPTO_ALERT_AXS15231B_USE_VENDOR_ESPRESSIF_PINS
// Espressif DEMO_LVGL esp_bsp.h / bsp_display_new() reference mapping (niet JC3248 schema-PINS).
#define AXS15231B_LCD_W 320
#define AXS15231B_LCD_H 480
#define AXS15231B_QSPI_HOST SPI2_HOST
#define AXS15231B_PIN_CS 45
#define AXS15231B_PIN_PCLK 47
#define AXS15231B_PIN_D0 21
#define AXS15231B_PIN_D1 48
#define AXS15231B_PIN_D2 40
#define AXS15231B_PIN_D3 39
#define AXS15231B_PIN_RST (-1)   // GPIO_NUM_NC in BSP
#define AXS15231B_PIN_DC 8
#define AXS15231B_PIN_TE 38      // TE: tearing sync (GPIO ISR) indien CRYPTO_ALERT_AXS15231B_USE_TE_SYNC
#define AXS15231B_PIN_BL 1       // alleen audit; backlight blijft via app (DEV_DEVICE_INIT / PWM)
// Legacy values from PINS_ESP32S3_JC3248W535_AXS15231B.h (schema) — alleen voor vergelijkingslog.
#define AXS15231B_LEGACY_CS 15
#define AXS15231B_LEGACY_SCK 14
#define AXS15231B_LEGACY_D0 13
#define AXS15231B_LEGACY_D1 12
#define AXS15231B_LEGACY_D2 11
#define AXS15231B_LEGACY_D3 10
#define AXS15231B_LEGACY_RST 39
#define AXS15231B_LEGACY_TE 38
#define AXS15231B_LEGACY_BL 1
#elif defined(PLATFORM_ESP32S3_JC3248W535)
// PINS header (JC3248 module schematic).
#define CRYPTO_ALERT_NO_ARDUINO_GFX_BUS
#include "../../PINS_ESP32S3_JC3248W535_AXS15231B.h"
#undef CRYPTO_ALERT_NO_ARDUINO_GFX_BUS
#define AXS15231B_LCD_W GFX_WIDTH
#define AXS15231B_LCD_H GFX_HEIGHT
#define AXS15231B_QSPI_HOST SPI2_HOST
#define AXS15231B_PIN_CS TFT_CS
#define AXS15231B_PIN_PCLK TFT_SCK
#define AXS15231B_PIN_D0 TFT_D0
#define AXS15231B_PIN_D1 TFT_D1
#define AXS15231B_PIN_D2 TFT_D2
#define AXS15231B_PIN_D3 TFT_D3
#define AXS15231B_PIN_RST TFT_RST
#define AXS15231B_PIN_DC (-1)
#define AXS15231B_PIN_TE TFT_TE
#define AXS15231B_PIN_BL GFX_BL
#else
// Andere platforms: deze TU wordt wel gecompileerd maar deze backend wordt niet geïnstantieerd.
#define AXS15231B_LCD_W 320
#define AXS15231B_LCD_H 480
#define AXS15231B_QSPI_HOST SPI2_HOST
#define AXS15231B_PIN_CS 0
#define AXS15231B_PIN_PCLK 0
#define AXS15231B_PIN_D0 0
#define AXS15231B_PIN_D1 0
#define AXS15231B_PIN_D2 0
#define AXS15231B_PIN_D3 0
#define AXS15231B_PIN_RST (-1)
#define AXS15231B_PIN_DC (-1)
#define AXS15231B_PIN_TE (-1)
#define AXS15231B_PIN_BL (-1)
#endif

extern "C" {
    extern const axs15231b_lcd_init_cmd_t axs15231b_lcd_init_cmds_jc3248[];
    extern const uint16_t axs15231b_lcd_init_cmds_jc3248_sz;
}

#ifndef CRYPTO_ALERT_AXS15231B_SWAP_RGB565_BYTES
#define CRYPTO_ALERT_AXS15231B_SWAP_RGB565_BYTES 0
#endif
#ifndef CRYPTO_ALERT_AXS15231B_PCLK_HZ
#define CRYPTO_ALERT_AXS15231B_PCLK_HZ 20000000
#endif
#ifndef CRYPTO_ALERT_AXS15231B_TRANS_QUEUE_DEPTH
#define CRYPTO_ALERT_AXS15231B_TRANS_QUEUE_DEPTH 1
#endif
#ifndef CRYPTO_ALERT_AXS15231B_SELFTEST
#define CRYPTO_ALERT_AXS15231B_SELFTEST 0
#endif
#ifndef CRYPTO_ALERT_AXS15231B_COLOR_ORDER_BGR
#define CRYPTO_ALERT_AXS15231B_COLOR_ORDER_BGR 0
#endif
#ifndef CRYPTO_ALERT_AXS15231B_INVERT_COLORS
#define CRYPTO_ALERT_AXS15231B_INVERT_COLORS 0
#endif
#ifndef CRYPTO_ALERT_AXS15231B_TE_SYNC_TIMEOUT_MS
#define CRYPTO_ALERT_AXS15231B_TE_SYNC_TIMEOUT_MS 35
#endif
#ifndef CRYPTO_ALERT_AXS15231B_TE_GPIO_INTR_TYPE
#define CRYPTO_ALERT_AXS15231B_TE_GPIO_INTR_TYPE GPIO_INTR_POSEDGE
#endif

#if defined(PLATFORM_ESP32S3_JC3248W535) && CRYPTO_ALERT_AXS15231B_USE_TE_SYNC
static void IRAM_ATTR axs15231b_te_gpio_isr(void *arg) {
    SemaphoreHandle_t sem = static_cast<SemaphoreHandle_t>(arg);
    if (!sem) {
        return;
    }
    BaseType_t hp = pdFALSE;
    (void)xSemaphoreGiveFromISR(sem, &hp);
    if (hp == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}
#endif

bool DisplayBackend_Axs15231bEspLcd::onColorTransDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *user_ctx) {
    DisplayBackend_Axs15231bEspLcd *self = static_cast<DisplayBackend_Axs15231bEspLcd *>(user_ctx);
    if (!self || !self->trans_done_sem_) {
        return false;
    }
    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(self->trans_done_sem_, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

bool DisplayBackend_Axs15231bEspLcd::begin(uint32_t /*speed*/) {
    if (panel_ != nullptr) {
        return true; // already initialized
    }

    Serial.println("[DisplayBackend] AXS15231B esp_lcd begin()");

#if defined(PLATFORM_ESP32S3_JC3248W535) && CRYPTO_ALERT_AXS15231B_USE_VENDOR_ESPRESSIF_PINS
    Serial.println("[AXS15231B] === Pin audit ===");
    Serial.println("[AXS15231B] Active pin source: VENDOR (esp_bsp.h / bsp_display_new reference)");
    Serial.printf("[AXS15231B] Legacy PINS header (schema JC3248): CS=%d SCK=%d D0=%d D1=%d D2=%d D3=%d RST=%d TE=%d BL=%d\n",
                  AXS15231B_LEGACY_CS, AXS15231B_LEGACY_SCK, AXS15231B_LEGACY_D0, AXS15231B_LEGACY_D1,
                  AXS15231B_LEGACY_D2, AXS15231B_LEGACY_D3, AXS15231B_LEGACY_RST, AXS15231B_LEGACY_TE,
                  AXS15231B_LEGACY_BL);
    Serial.printf("[AXS15231B] Active (vendor): host=SPI2 CS=%d PCLK=%d D0=%d D1=%d D2=%d D3=%d RST=%d DC=%d TE=%d BL=%d (TE/BL logged only)\n",
                  AXS15231B_PIN_CS, AXS15231B_PIN_PCLK, AXS15231B_PIN_D0, AXS15231B_PIN_D1, AXS15231B_PIN_D2,
                  AXS15231B_PIN_D3, AXS15231B_PIN_RST, AXS15231B_PIN_DC, AXS15231B_PIN_TE, AXS15231B_PIN_BL);
#elif defined(PLATFORM_ESP32S3_JC3248W535)
    Serial.println("[AXS15231B] === Pin audit ===");
    Serial.println("[AXS15231B] Active pin source: PINS_ESP32S3_JC3248W535_AXS15231B.h (schema)");
    Serial.printf("[AXS15231B] Active: host=SPI2 CS=%d PCLK=%d D0=%d D1=%d D2=%d D3=%d RST=%d TE=%d BL=%d\n",
                  AXS15231B_PIN_CS, AXS15231B_PIN_PCLK, AXS15231B_PIN_D0, AXS15231B_PIN_D1, AXS15231B_PIN_D2,
                  AXS15231B_PIN_D3, AXS15231B_PIN_RST, AXS15231B_PIN_TE, AXS15231B_PIN_BL);
    Serial.println("[AXS15231B] Vendor reference (esp_bsp): CS=45 PCLK=47 D0=21 D1=48 D2=40 D3=39 RST=NC DC=8 TE=38 BL=1");
#endif

#if defined(PLATFORM_ESP32S3_JC3248W535)
    // Compacte samenvatting van platform_config.h defaults (JC3248W535CIY + AXS15231B).
    Serial.printf(
        "[AXS15231B] JC3248 defaults (bewezen stabiel): swap565=%d pclk_hz=%u queue=%d vendor_pins=%d BGR=%d invert=%d selftest=%d\n",
        (int)CRYPTO_ALERT_AXS15231B_SWAP_RGB565_BYTES,
        (unsigned)CRYPTO_ALERT_AXS15231B_PCLK_HZ,
        (unsigned)CRYPTO_ALERT_AXS15231B_TRANS_QUEUE_DEPTH,
        (int)CRYPTO_ALERT_AXS15231B_USE_VENDOR_ESPRESSIF_PINS,
        (int)CRYPTO_ALERT_AXS15231B_COLOR_ORDER_BGR,
        (int)CRYPTO_ALERT_AXS15231B_INVERT_COLORS,
        (int)CRYPTO_ALERT_AXS15231B_SELFTEST);
#endif

    const uint32_t full_px = (uint32_t)AXS15231B_LCD_W * (uint32_t)AXS15231B_LCD_H;
    const uint32_t max_transfer_sz = full_px * (uint32_t)sizeof(uint16_t);

    // QSPI bus
    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num = AXS15231B_PIN_PCLK;
    buscfg.data0_io_num = AXS15231B_PIN_D0;
    buscfg.data1_io_num = AXS15231B_PIN_D1;
    buscfg.data2_io_num = AXS15231B_PIN_D2;
    buscfg.data3_io_num = AXS15231B_PIN_D3;
    buscfg.max_transfer_sz = max_transfer_sz;

    esp_err_t err = spi_bus_initialize(AXS15231B_QSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        Serial.printf("[AXS15231B] spi_bus_initialize failed: %d\n", (int)err);
        return false;
    }

    // Panel IO — exact vendor QSPI route: AXS15231B_PANEL_IO_QSPI_CONFIG sets dc_gpio_num = -1 (no DC line).
    esp_lcd_panel_io_spi_config_t io_config = AXS15231B_PANEL_IO_QSPI_CONFIG(AXS15231B_PIN_CS, onColorTransDone, this);
    io_config.pclk_hz = CRYPTO_ALERT_AXS15231B_PCLK_HZ;
    io_config.trans_queue_depth = CRYPTO_ALERT_AXS15231B_TRANS_QUEUE_DEPTH;
    Serial.println("[AXS15231B] QSPI panel IO: AXS15231B_PANEL_IO_QSPI_CONFIG (vendor macro, no dc_gpio override)");
    Serial.printf("[AXS15231B] QSPI IO effective: cs_gpio_num=%d dc_gpio_num=%d spi_mode=%d pclk_hz=%u trans_queue_depth=%u quad_mode=%d\n",
                  (int)io_config.cs_gpio_num, (int)io_config.dc_gpio_num, (int)io_config.spi_mode,
                  (unsigned)io_config.pclk_hz, (unsigned)io_config.trans_queue_depth,
                  (int)(io_config.flags.quad_mode ? 1 : 0));
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)AXS15231B_QSPI_HOST, &io_config, &io_);
    if (err != ESP_OK) {
        Serial.printf("[AXS15231B] esp_lcd_new_panel_io_spi failed: %d\n", (int)err);
        return false;
    }

    // Vendor init commands (manufacturer sequence from esp_bsp.c)
    static const axs15231b_vendor_config_t vendor_config = {
        .init_cmds = axs15231b_lcd_init_cmds_jc3248,
        .init_cmds_size = axs15231b_lcd_init_cmds_jc3248_sz,
        .init_in_command_mode = false,
        .flags = {
            .use_qspi_interface = 1,
            .use_mipi_interface = 0,
        },
    };

    // Panel device config
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = AXS15231B_PIN_RST;
#if CRYPTO_ALERT_AXS15231B_COLOR_ORDER_BGR
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
#else
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
#endif
    // AXS15231B JC3248 route follows the vendor BSP: RGB565 big-endian payload.
    panel_config.data_endian = LCD_RGB_DATA_ENDIAN_BIG;
    panel_config.bits_per_pixel = 16;
    panel_config.flags.reset_active_high = 0;
    panel_config.vendor_config = (void *)&vendor_config;

    err = esp_lcd_new_panel_axs15231b(io_, &panel_config, &panel_);
    if (err != ESP_OK) {
        Serial.printf("[AXS15231B] esp_lcd_new_panel_axs15231b failed: %d\n", (int)err);
        return false;
    }

    esp_lcd_panel_reset(panel_);
    err = esp_lcd_panel_init(panel_);
    if (err != ESP_OK) {
        Serial.printf("[AXS15231B] esp_lcd_panel_init failed: %d\n", (int)err);
        return false;
    }

#if CRYPTO_ALERT_AXS15231B_INVERT_COLORS
    err = esp_lcd_panel_invert_color(panel_, true);
#else
    err = esp_lcd_panel_invert_color(panel_, false);
#endif
    if (err != ESP_OK) {
        Serial.printf("[AXS15231B] esp_lcd_panel_invert_color failed: %d\n", (int)err);
    }

    // Don't rely on digitalWrite backlight here; setDisplayBrigthness() does PWM later.
    // Match manufacturer BSP behavior exactly for initial panel state.
    err = esp_lcd_panel_disp_on_off(panel_, false);
    if (err != ESP_OK) {
        Serial.printf("[AXS15231B] esp_lcd_panel_disp_on_off(false) failed: %d\n", (int)err);
    }

    // TE-sync vóór grote heap-allocaties: minder stackdruk op Arduino setup() en minder race met SPI init.
#if defined(PLATFORM_ESP32S3_JC3248W535) && CRYPTO_ALERT_AXS15231B_USE_TE_SYNC
    Serial.println(F("[AXS15231B][TE] sync init entry (pre-buffer)"));
    (void)teSyncInit();
#endif

    // Allocate buffer for fillScreen()
    fill_buf_px_ = full_px;
    fill_buf_ = (uint16_t *)heap_caps_malloc(full_px * sizeof(uint16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!fill_buf_) {
        Serial.println("[AXS15231B] WARN: fillScreen buffer alloc failed");
    }

    // DMA transport buffers for striped panel transfers.
    // Keep LVGL framebuffer in SPIRAM, but copy stripes into DMA-capable internal RAM.
    trans_done_sem_ = xSemaphoreCreateBinary();
    if (!trans_done_sem_) {
        Serial.println("[AXS15231B] ERROR: failed to create trans_done semaphore");
        return false;
    }

    trans_buf_lines_ = 40; // 40 lines per stripe => 320*40*2 = 25600 bytes
    trans_buf_bytes_ = (size_t)AXS15231B_LCD_W * (size_t)trans_buf_lines_ * sizeof(uint16_t);
    trans_buf_1_ = (uint16_t *)heap_caps_malloc(trans_buf_bytes_, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    trans_buf_2_ = (uint16_t *)heap_caps_malloc(trans_buf_bytes_, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);

    if (!trans_buf_1_) {
        Serial.printf("[AXS15231B] ERROR: DMA transport buffer #1 alloc failed (%u bytes)\n", (unsigned)trans_buf_bytes_);
        return false;
    }
    if (!trans_buf_2_) {
        use_double_trans_buf_ = false;
        Serial.printf("[AXS15231B] WARN: DMA transport buffer #2 alloc failed (%u bytes), using single transport buffer\n",
                      (unsigned)trans_buf_bytes_);
    } else {
        use_double_trans_buf_ = true;
    }
    transport_ready_ = true;
    Serial.printf("[AXS15231B] DMA transport buffer bytes: %u (lines=%u, buffers=%u)\n",
                  (unsigned)trans_buf_bytes_, (unsigned)trans_buf_lines_, use_double_trans_buf_ ? 2 : 1);

#if CRYPTO_ALERT_AXS15231B_SWAP_RGB565_BYTES
    Serial.println("[AXS15231B] RGB565 byte swap: ON");
#else
    Serial.println("[AXS15231B] RGB565 byte swap: OFF");
#endif

    Serial.println("[AXS15231B] RGB565 byte order: BIG-endian (panel_config.data_endian = BIG)");
#if CRYPTO_ALERT_AXS15231B_COLOR_ORDER_BGR
    Serial.println("[AXS15231B] Color element order: BGR (panel_config.rgb_ele_order)");
#else
    Serial.println("[AXS15231B] Color element order: RGB (panel_config.rgb_ele_order)");
#endif
#if CRYPTO_ALERT_AXS15231B_INVERT_COLORS
    Serial.println("[AXS15231B] Panel invert: ON (esp_lcd_panel_invert_color true after init)");
#else
    Serial.println("[AXS15231B] Panel invert: OFF (esp_lcd_panel_invert_color false after init)");
#endif

#if CRYPTO_ALERT_AXS15231B_SELFTEST
    Serial.println("[AXS15231B][SELFTEST] Enabled - rendering direct color bars (no LVGL flush).");
    struct ColorStep { const char *name; uint16_t rgb565; } steps[] = {
        {"RED",   0xF800},
        {"GREEN", 0x07E0},
        {"BLUE",  0x001F},
        {"WHITE", 0xFFFF},
        {"BLACK", 0x0000},
    };
    for (size_t i = 0; i < (sizeof(steps) / sizeof(steps[0])); ++i) {
        Serial.printf("[AXS15231B][SELFTEST] Fill %s (0x%04X)\n", steps[i].name, steps[i].rgb565);
        fillScreen(steps[i].rgb565);
        delay(800);
    }
    Serial.println("[AXS15231B][SELFTEST] Done.");
#endif

    return true;
}

uint32_t DisplayBackend_Axs15231bEspLcd::width() const {
    return (uint32_t)AXS15231B_LCD_W;
}

uint32_t DisplayBackend_Axs15231bEspLcd::height() const {
    return (uint32_t)AXS15231B_LCD_H;
}

void DisplayBackend_Axs15231bEspLcd::setRotation(uint8_t rotation_deg_compatible) {
#if CRYPTO_ALERT_AXS15231B_SELFTEST
    (void)rotation_deg_compatible;
    return;
#else
    // For the app's limited rotation support (0 or 180 degrees), we map:
    //   - 0: identity
    //   - 180: mirror X+Y
    if (!panel_) return;
    const uint8_t rotation_deg_ = rotation_deg_compatible;
    if (rotation_deg_ == 2) {
        (void)esp_lcd_panel_mirror(panel_, true, true);
    } else {
        (void)esp_lcd_panel_mirror(panel_, false, false);
    }
#endif
}

void DisplayBackend_Axs15231bEspLcd::invertDisplay(bool invert) {
#if CRYPTO_ALERT_AXS15231B_SELFTEST
    (void)invert;
    return;
#else
    if (!panel_) return;
    (void)esp_lcd_panel_invert_color(panel_, invert);
#endif
}

void DisplayBackend_Axs15231bEspLcd::fillScreen(uint16_t rgb565_color) {
    if (!panel_ || !fill_buf_) return;

    for (uint32_t i = 0; i < fill_buf_px_; i++) {
        fill_buf_[i] = rgb565_color;
    }

#if defined(PLATFORM_ESP32S3_JC3248W535) && CRYPTO_ALERT_AXS15231B_USE_TE_SYNC
    teSyncWaitBeforeDraw("fillScreen");
#endif
    (void)esp_lcd_panel_draw_bitmap(panel_, 0, 0, (int)AXS15231B_LCD_W, (int)AXS15231B_LCD_H, fill_buf_);
}

void DisplayBackend_Axs15231bEspLcd::flush(const lv_area_t *area, const uint8_t *px_map) {
#if CRYPTO_ALERT_AXS15231B_SELFTEST
    (void)area;
    (void)px_map;
    static bool s_loggedSkipFlush = false;
    if (!s_loggedSkipFlush) {
        Serial.println("[AXS15231B][SELFTEST] LVGL flush skipped.");
        s_loggedSkipFlush = true;
    }
    return;
#else
    if (!panel_ || !area || !px_map) return;
    if (!transport_ready_ || !trans_buf_1_ || !trans_done_sem_) {
        Serial.println("[AXS15231B] ERROR: transport buffers not ready; flush aborted");
        return;
    }

    static bool s_loggedFirstBackendFlush = false;
    if (!s_loggedFirstBackendFlush) {
        Serial.printf("[AXS15231B] First backend flush: area=(%d,%d)-(%d,%d)\n",
                      area->x1, area->y1, area->x2, area->y2);
    }

    // lv_area_t uses inclusive x2/y2, esp_lcd_panel_draw_bitmap uses end-exclusive.
    const int x1 = area->x1;
    const int y1 = area->y1;
    const int x_end = area->x2 + 1;
    const int y_end = area->y2 + 1;
    const uint32_t w = (uint32_t)(x_end - x1);
    const uint32_t h = (uint32_t)(y_end - y1);
    const size_t lenBytes = (size_t)w * (size_t)h * sizeof(uint16_t);

#if CRYPTO_ALERT_AXS15231B_HAS_ESP_CACHE
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_ESP32S3_DEV)
    if (lenBytes > 0 && esp_ptr_external_ram(px_map)) {
        int msync_flags = ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA;
#if defined(ESP_CACHE_MSYNC_FLAG_UNALIGNED)
        msync_flags |= ESP_CACHE_MSYNC_FLAG_UNALIGNED;
#endif
        (void)esp_cache_msync((void *)px_map, lenBytes, msync_flags);
    }
#endif
#endif

    const uint32_t stripeLines = trans_buf_lines_ > 0 ? (uint32_t)trans_buf_lines_ : 1u;
    const uint32_t stripeCount = (h + stripeLines - 1u) / stripeLines;
    if (!s_loggedFirstBackendFlush) {
        const uint32_t firstStripeY1 = (uint32_t)y1;
        const uint32_t firstStripeY2 = firstStripeY1 + ((h > stripeLines) ? stripeLines : h) - 1u;
        Serial.printf("[AXS15231B] Flush stripes: count=%u, firstStripeY=%u..%u\n",
                      (unsigned)stripeCount, (unsigned)firstStripeY1, (unsigned)firstStripeY2);
    }

#if defined(PLATFORM_ESP32S3_JC3248W535) && CRYPTO_ALERT_AXS15231B_USE_TE_SYNC
    // Eén TE-sync vóór de eerste stripe van deze flush (volledige frame-updates blijven uit stripe-DMA pad).
    teSyncWaitBeforeDraw("flush");
#endif

    const uint8_t *src = px_map;
    for (uint32_t stripeIdx = 0; stripeIdx < stripeCount; stripeIdx++) {
        const uint32_t stripeYOff = stripeIdx * stripeLines;
        const uint32_t curLines = ((stripeYOff + stripeLines) <= h) ? stripeLines : (h - stripeYOff);
        const size_t stripeBytes = (size_t)w * (size_t)curLines * sizeof(uint16_t);
        const uint8_t *stripeSrc = src + ((size_t)stripeYOff * (size_t)w * sizeof(uint16_t));

        uint16_t *dst = trans_buf_1_;
        if (use_double_trans_buf_) {
            dst = (trans_buf_toggle_ == 0) ? trans_buf_1_ : trans_buf_2_;
            trans_buf_toggle_ ^= 1;
        }
        memcpy(dst, stripeSrc, stripeBytes);

#if CRYPTO_ALERT_AXS15231B_SWAP_RGB565_BYTES
        uint16_t *pix = dst;
        const size_t pixCount = stripeBytes / sizeof(uint16_t);
        for (size_t i = 0; i < pixCount; ++i) {
            const uint16_t v = pix[i];
            pix[i] = (uint16_t)((v << 8) | (v >> 8));
        }
#endif

        const int stripeY1 = y1 + (int)stripeYOff;
        const int stripeY2Excl = stripeY1 + (int)curLines;
        esp_err_t err = esp_lcd_panel_draw_bitmap(panel_, x1, stripeY1, x_end, stripeY2Excl, dst);
        if (err != ESP_OK) {
            Serial.printf("[AXS15231B] ERROR: draw_bitmap failed on stripe %u/%u, err=%d\n",
                          (unsigned)(stripeIdx + 1u), (unsigned)stripeCount, (int)err);
            return;
        }

        if (xSemaphoreTake(trans_done_sem_, pdMS_TO_TICKS(500)) != pdTRUE) {
            Serial.printf("[AXS15231B] ERROR: timeout waiting transfer completion on stripe %u/%u\n",
                          (unsigned)(stripeIdx + 1u), (unsigned)stripeCount);
            return;
        }
    }

    if (!s_loggedFirstBackendFlush) {
        Serial.println("[AXS15231B] First backend flush: draw_bitmap done");
        s_loggedFirstBackendFlush = true;
    }
#endif
}

#if defined(PLATFORM_ESP32S3_JC3248W535) && CRYPTO_ALERT_AXS15231B_USE_TE_SYNC

// Eén keer per firmware: voorkomt herhaalde install-aanroepen als begin() ooit opnieuw zou lopen (defensief).
static bool s_axs15231b_gpio_isr_service_ready = false;

bool DisplayBackend_Axs15231bEspLcd::teSyncInit() {
    // Al succesvol: geen dubbele GPIO/ISR-registratie (voorkomt race met gpio service).
    if (te_sync_sem_ != nullptr) {
        return true;
    }
    te_sync_ready_ = false;

    const int te_pin = AXS15231B_PIN_TE;
    if (te_pin < 0) {
        Serial.println(F("[AXS15231B][TE] sync disabled (invalid TE pin)"));
        return false;
    }

    te_sync_sem_ = xSemaphoreCreateBinary();
    if (te_sync_sem_ == nullptr) {
        Serial.println(F("[AXS15231B][TE] sync init failed: semaphore alloc"));
        return false;
    }

    // Flag 0 = default allocatie (stabieler met Arduino/core dan alleen ESP_INTR_FLAG_IRAM i.c.m. ISR-dispatcher).
    if (!s_axs15231b_gpio_isr_service_ready) {
        esp_err_t err = gpio_install_isr_service(0);
        if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
            s_axs15231b_gpio_isr_service_ready = true;
        } else {
            Serial.printf("[AXS15231B][TE] sync init failed: gpio_install_isr_service err=%d\n", (int)err);
            vSemaphoreDelete(te_sync_sem_);
            te_sync_sem_ = nullptr;
            return false;
        }
    }

    esp_err_t err = ESP_OK;

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << (unsigned)te_pin);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        Serial.printf("[AXS15231B][TE] sync init failed: gpio_config err=%d\n", (int)err);
        vSemaphoreDelete(te_sync_sem_);
        te_sync_sem_ = nullptr;
        return false;
    }

    err = gpio_set_intr_type(static_cast<gpio_num_t>(te_pin), CRYPTO_ALERT_AXS15231B_TE_GPIO_INTR_TYPE);
    if (err != ESP_OK) {
        Serial.printf("[AXS15231B][TE] sync init failed: gpio_set_intr_type err=%d\n", (int)err);
        vSemaphoreDelete(te_sync_sem_);
        te_sync_sem_ = nullptr;
        return false;
    }

    err = gpio_isr_handler_add(static_cast<gpio_num_t>(te_pin), &axs15231b_te_gpio_isr,
                               static_cast<void *>(te_sync_sem_));
    if (err != ESP_OK) {
        Serial.printf("[AXS15231B][TE] sync init failed: gpio_isr_handler_add err=%d\n", (int)err);
        vSemaphoreDelete(te_sync_sem_);
        te_sync_sem_ = nullptr;
        return false;
    }

    te_sync_ready_ = true;
    Serial.printf(
        "[AXS15231B][TE] sync init ok (pin=%d intr=%d timeout_ms=%u)\n",
        te_pin,
        (int)CRYPTO_ALERT_AXS15231B_TE_GPIO_INTR_TYPE,
        (unsigned)CRYPTO_ALERT_AXS15231B_TE_SYNC_TIMEOUT_MS);
    return true;
}

void DisplayBackend_Axs15231bEspLcd::teSyncWaitBeforeDraw(const char *reason_tag) {
    if (!te_sync_ready_ || te_sync_sem_ == nullptr) {
        return;
    }
    const int te_pin = AXS15231B_PIN_TE;
    if (te_pin < 0) {
        return;
    }

    // Verwijder eventuele oude TE-events zodat we op de *volgende* flank wachten.
    while (xSemaphoreTake(te_sync_sem_, 0) == pdTRUE) {
    }

    static uint32_t s_teVerboseCount = 0;
    const bool verbose = (s_teVerboseCount < 3u);
    if (verbose) {
        Serial.println(F("[AXS15231B][TE] waiting for sync"));
    }

    const TickType_t ticks = pdMS_TO_TICKS(CRYPTO_ALERT_AXS15231B_TE_SYNC_TIMEOUT_MS);
    if (xSemaphoreTake(te_sync_sem_, ticks) != pdTRUE) {
        Serial.printf("[AXS15231B][TE] sync timeout (%s)\n", reason_tag ? reason_tag : "?");
        return;
    }

    if (verbose) {
        Serial.println(F("[AXS15231B][TE] sync acquired"));
        s_teVerboseCount++;
    }
}

#endif // PLATFORM_ESP32S3_JC3248W535 && CRYPTO_ALERT_AXS15231B_USE_TE_SYNC

