#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#include <cstdint>

namespace ota_service {

/** Snapshot voor WebUI / JSON (M-014b). */
struct OtaStatusSnapshot {
    char running_label[17]{};
    char next_update_label[17]{};
    uint32_t running_address{0};
    uint32_t running_size_bytes{0};
    /** o.a. `valid`, `pending_verify`, `undefined`, `unknown`. */
    char img_state[20]{};
    /** `marked_valid` | `rollback_disabled` | `error:…` */
    char boot_confirm[48]{};
    char reset_reason[24]{};
};

/** Log + post-boot bevestiging (`esp_ota_mark_app_valid_cancel_rollback` indien actief). */
esp_err_t init();

/**
 * Verwerk POST met ruwe firmware-binary (`Content-Type: application/octet-stream`,
 * `Content-Length` verplicht). Schrijft naar de niet-actieve OTA-slot, valideert via
 * `esp_ota_end`, zet boot-partitie, stuurt JSON-respons en herstart bij succes.
 */
esp_err_t handle_firmware_upload(httpd_req_t *req);

/** Huidige partitie-/bootstatus (veilig voor tonen in UI). */
void get_status_snapshot(OtaStatusSnapshot *out);

} // namespace ota_service
