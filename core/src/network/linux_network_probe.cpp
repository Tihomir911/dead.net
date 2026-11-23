
#include "deadnet/network/linux_network_probe.hpp"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

#include <chrono>
#include <memory>
#include <string>

namespace deadnet::network {

namespace {

ConnectionType detectConnectionType(const std::string& interfaceName) {
    if (interfaceName.starts_with("wl")) {
        return ConnectionType::WiFi;
    }

    if (interfaceName.starts_with("eth") || interfaceName.starts_with("en")) {
        return ConnectionType::Ethernet;
    }

    if (interfaceName.starts_with("wwan")) {
        return ConnectionType::Cellular;
    }

    return ConnectionType::Unknown;
}

}

ConnectionInfo LinuxNetworkProbe::getConnectionInfo() const {
    struct ifaddrs* rawInterfaces = nullptr;

    if (getifaddrs(&rawInterfaces) == -1) {
        return {};
    }

    std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> interfaces(
        rawInterfaces,
        &freeifaddrs
    );

    ConnectionInfo result{};

    for (const auto* current = interfaces.get();
         current != nullptr;
         current = current->ifa_next) {

        if (current->ifa_addr == nullptr || current->ifa_name == nullptr) {
            continue;
        }

        if (current->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        if ((current->ifa_flags & IFF_UP) == 0 || (current->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }

        const auto* address =
            reinterpret_cast<const sockaddr_in*>(current->ifa_addr);

        char buffer[INET_ADDRSTRLEN]{};

        if (inet_ntop(
                AF_INET,
                &address->sin_addr,
                buffer,
                sizeof(buffer)
            ) == nullptr) {
            continue;
        }

        result.networkName = current->ifa_name;
        result.ipAddress = buffer;

        break;
    }

    return result;
}

NetworkMeasurement LinuxNetworkProbe::measure() {
    NetworkMeasurement measurement{};

    measurement.timestamp = std::chrono::system_clock::now();

    const ConnectionInfo connection = getConnectionInfo();

    measurement.connectionType =
        detectConnectionType(connection.networkName);

    return measurement;
}

}
