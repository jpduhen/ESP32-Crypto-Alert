#pragma once

#include "esp_err.h"

namespace ntfy_client {

struct NtfySendRequest {
    const char *title;
    const char *body;
    int priority;
    const char *tags;
};

esp_err_t init();
bool enabled();
esp_err_t send(const NtfySendRequest &req);

}  // namespace ntfy_client
