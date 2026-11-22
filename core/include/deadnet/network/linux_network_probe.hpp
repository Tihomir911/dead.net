#pragma once

#include "deadnet/network/network_probe.hpp"

namespace deadnet::network {

class LinuxNetworkProbe final : public NetworkProbe {
public:
    NetworkMeasurement measure() override;
};

}