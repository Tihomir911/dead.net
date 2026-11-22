#pragma once

#include <cstdint>

namespace deadnet::network {

enum class NetworkQuality : std::uint8_t {
    Unknown = 0,
    Excellent,
    Good,
    Fair,
    Poor
};

}