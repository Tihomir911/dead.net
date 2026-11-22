#pragma once

#include <string>

#include "deadnet/network/network_probe.hpp"

namespace deadnet::network {

class LinuxNetworkProbe final : public NetworkProbe {
public:
    NetworkMeasurement measure() override;

private:
    std::string getActiveInterface() const;
    std::string getLocalIpAddress() const;
};

}