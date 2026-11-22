#pragma once

#include <string>

namespace deadnet::network {

struct ConnectionInfo {
    std::string networkName{};
    std::string ipAddress{};
};

}