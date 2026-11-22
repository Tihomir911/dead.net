#pragma once

#include <cstdint>

#include "deadnet/network/network_measurement.hpp"

namespace deadnet::network {

enum class NetworkQuality : std::uint8_t {
    Unknown = 0,
    Excellent,
    Good,
    Fair,
    Poor
};

NetworkQuality evaluateNetworkQuality(
    const NetworkMeasurement& measurement
);

}
