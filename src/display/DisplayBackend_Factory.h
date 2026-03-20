#pragma once

#include "DisplayBackend.h"

// Factory for selecting the correct display backend for the current platform.
DisplayBackend *createDisplayBackendForCurrentPlatform();

