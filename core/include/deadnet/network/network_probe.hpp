#pragma once

#include "deadnet/network/network_measurement.hpp"

namespace deadnet::network {

class NetworkProbe {
public:
    virtual ~NetworkProbe() = default;

    virtual NetworkMeasurement measure() = 0;
};

}