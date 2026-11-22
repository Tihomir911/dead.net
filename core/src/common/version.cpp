
#include "deadnet/common/version.hpp"

namespace deadnet::common {

std::string versionString() {
    return std::to_string(kVersion.major) + "." +
           std::to_string(kVersion.minor) + "." +
           std::to_string(kVersion.patch);
}

}
