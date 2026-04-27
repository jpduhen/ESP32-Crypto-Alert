#pragma once

#include <cstdint>

namespace app_version {

constexpr uint8_t kMajor = 3;
constexpr uint8_t kTrack = 1;
constexpr uint8_t kMinor = 1;

constexpr const char *kVersionString = "3.01.01";
constexpr const char *kBranchName = "v3/alert-engine";

const char *version_string();
const char *branch_name();

}  // namespace app_version
