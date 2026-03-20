#include "DisplayBackend_Factory.h"

#include "DisplayBackend_ArduinoGFX.h"
#include "DisplayBackend_Axs15231bEspLcd.h"

// platform_config.h bevat de PLATFORM_* defines.
// We includen die met MODULE_INCLUDE zodat er GEEN PINS files (bus/gfx) geïncludeerd worden
// in dit translation unit (voorkomt multiple definition errors).
#define MODULE_INCLUDE
#include "../../platform_config.h"
#undef MODULE_INCLUDE

DisplayBackend *createDisplayBackendForCurrentPlatform() {
#if defined(PLATFORM_ESP32S3_JC3248W535)
    return new DisplayBackend_Axs15231bEspLcd();
#else
    return new DisplayBackend_ArduinoGFX();
#endif
}

