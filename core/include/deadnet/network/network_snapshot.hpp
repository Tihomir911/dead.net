#pragma once 

#include "deadnet/network/connection_status.hpp"
#include "deadnet/network/network_interface.hpp"
#include "deadnet/network/network_measurement.hpp"
#include "deadnet/network/network_quality.hpp"

namecpase deadnet::network {

    struct NetworkSnapshot {

        ConnectionStatus status{connectionStatus::Unknown};

        NetworkQuality qualyti{NetworkQuality::Unknown};
        
        NetworkInterface interface{};

        NetworkMeasurement measurement{};
    };

}