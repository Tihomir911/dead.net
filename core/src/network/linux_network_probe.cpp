#include "deadnet/network/linux_network_probe.hpp"

#include <net/if.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string>

namespace deadnet::network {

std::string LinuxNetworkProbe::getActiveInterface() const {
    struct ifaddrs* interfaces = nullptr;

    if (getifaddrs(&interfaces) == -1) {
        return {};
    }

    std::string result;

    for (const auto* current = interfaces;
         current != nullptr;
         current = current->ifa_next) {

        if (current->ifa_addr == nullptr) {
            continue;
        }

        if (current->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        if ((current->ifa_flags & IFF_UP) == 0 ||
            (current->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }

        result = current->ifa_name;
        break;
    }

    freeifaddrs(interfaces);

    return result;
}

std::string LinuxNetworkProbe::getLocalIpAddress() const {
    struct ifaddrs* interfaces = nullptr;

    if (getifaddrs(&interfaces) == -1) {
        return {};
    }

    std::string result;

    for (const auto* current = interfaces;
         current != nullptr;
         current = current->ifa_next) {

        if (current->ifa_addr == nullptr) {
            continue;
        }

        if (current->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        if ((current->ifa_flags & IFF_UP) == 0 ||
            (current->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }

        const auto* address =
            reinterpret_cast<const sockaddr_in*>(current->ifa_addr);

        char buffer[INET_ADDRSTRLEN]{};

        if (inet_ntop(AF_INET, &address->sin_addr,
                      buffer, sizeof(buffer)) != nullptr) {
            result = buffer;
            break;
        }
    }

    freeifaddrs(interfaces);

    return result;
}

NetworkMeasurement LinuxNetworkProbe::measure() {
    NetworkMeasurement measurement{};

    return measurement;
}

}