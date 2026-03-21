#include "DisplayBackend_Factory.h"

#include "DisplayBackend_ArduinoGFX.h"

// platform_config vóór AXS15231B-header (JC3248 TE-sync members zijn macro-afhankelijk).
#define MODULE_INCLUDE
#include "../../platform_config.h"
#undef MODULE_INCLUDE

#include "DisplayBackend_Axs15231bEspLcd.h"

DisplayBackend *createDisplayBackendForCurrentPlatform() {
#if defined(PLATFORM_ESP32S3_JC3248W535)
    return new DisplayBackend_Axs15231bEspLcd();
#else
    return new DisplayBackend_ArduinoGFX();
#endif
}

