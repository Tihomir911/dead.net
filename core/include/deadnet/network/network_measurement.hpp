
#pragma once

#include <chrono>
#include <cstdint>

namespace deadnet::network {

enum class ConnectionType : std::uint8_t {
    Unknown = 0,
    WiFi,
    Ethernet,
    Cellular
};

struct NetworkMeasurement {
    std::chrono::system_clock::time_point timestamp{};

    double latencyMs{0.0};

    double packetLossPercent{0.0};

    double downloadMbps{0.0};

    double uploadMbps{0.0};

    ConnectionType connectionType{ConnectionType::Unknown};
};

}
