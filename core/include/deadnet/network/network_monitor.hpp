#pragma once

#include "deadnet/network/network_snapshot.hpp"

namespace deadnet::network {

class NetworkMonitor {
public:
    virtual ~NetworkMonitor() = default;

    virtual NetworkSnapshot getSnapshot() const = 0;
};

}