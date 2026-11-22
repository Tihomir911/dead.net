#pragma once

#include <string>

namespace deadnet::network {

    struct NetworkInterface {
        std::string name{};
        std::string identifier{};
    };
}