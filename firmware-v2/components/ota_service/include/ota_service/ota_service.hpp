#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

namespace ota_service {

/** Log draaiende / volgende OTA-partitie (M-014a). */
esp_err_t init();

/**
 * Verwerk POST met ruwe firmware-binary (`Content-Type: application/octet-stream`,
 * `Content-Length` verplicht). Schrijft naar de niet-actieve OTA-slot, valideert via
 * `esp_ota_end`, zet boot-partitie, stuurt JSON-respons en herstart bij succes.
 */
esp_err_t handle_firmware_upload(httpd_req_t *req);

} // namespace ota_service
