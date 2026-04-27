#include "app_version/app_version.hpp"

namespace app_version {

const char *version_string() {
    return kVersionString;
}

const char *branch_name() {
    return kBranchName;
}

}  // namespace app_version
