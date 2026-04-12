// Alleen transport-macros — geen PINS/display (ApiClient.cpp mag dit includen zonder bus/gfx duplicate link errors).
#pragma once
#ifndef TRANSPORT_DIAG_FETCHPRICE
#define TRANSPORT_DIAG_FETCHPRICE 1
#endif
// Nauwere probe: stream/client onder HTTPClient::GET() (zelfde begrenzing als [TXDIAG]).
#ifndef TRANSPORT_DIAG_FETCHPRICE_STREAM
#define TRANSPORT_DIAG_FETCHPRICE_STREAM 1
#endif
