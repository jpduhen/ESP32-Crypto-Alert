#pragma once

#include "esp_err.h"
#include <cstddef>

namespace exchange_bitvavo::rest {

/** GET /v2/ticker/price; interne `esp_http_client` wordt in `bitvavo_rest.cpp` hergebruikt (M-002b). */
esp_err_t fetch_ticker_price(const char *market, double *out_eur, char *err_detail, size_t err_len);

}
