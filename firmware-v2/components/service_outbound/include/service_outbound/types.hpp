#pragma once

#include <cstdint>

namespace service_outbound {

/**
 * Neutrale outbound-signalen (M-002c) — geen MQTT/NTFY/WebUI-protocoltypes.
 * Uitbreiden kan later; waarden blijven stabiel voor vaste ABI in firmware.
 */
enum class Event : uint8_t {
    None = 0,
    /** Eénmalige haak na succesvolle app-start (demo/stub — geen netwerk). */
    ApplicationReady,
};

} // namespace service_outbound
