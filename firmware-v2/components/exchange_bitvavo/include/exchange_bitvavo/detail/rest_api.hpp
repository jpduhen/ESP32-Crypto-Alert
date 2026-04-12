#pragma once

#include "esp_err.h"
#include <cstddef>

namespace exchange_bitvavo::rest {

esp_err_t fetch_ticker_price(const char *market, double *out_eur, char *err_detail, size_t err_len);

}
