#pragma once

#include <cstdint>

namespace deadnet::network {

enum class ConnectionStatus : std::uint8_t {
    Unknown = 0,
    Connected,
    Disconnected
};

}