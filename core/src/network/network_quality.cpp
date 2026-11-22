#include "deadnet/network/network_quality.hpp"

namespace deadnet::network {

NetworkQuality evaluateNetworkQuality(
    const NetworkMeasurement& measurement
) {
    if (measurement.packetLossPercent >= 10.0 || measurement.latencyMs >= 500.0) {
        return NetworkQuality::Poor;
    }

    if (measurement.packetLossPercent >= 5.0 || measurement.latencyMs >= 250.0 || measurement.jitterMs >= 50.0) {
        return NetworkQuality::Fair;
    }

    if (measurement.packetLossPercent >= 2.0 || measurement.latencyMs >= 100.0 || measurement.jitterMs >= 20.0) {
        return NetworkQuality::Good;
    }

    if (measurement.latencyMs >= 0.0 && measurement.packetLossPercent >= 0.0) {
        return NetworkQuality::Excellent;
    }

    return NetworkQuality::Unknown;
}

}