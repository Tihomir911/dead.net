
#include "deadnet/network/linux_network_probe.hpp"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>

#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace deadnet::network {

namespace {

using Clock = std::chrono::steady_clock;

constexpr int PING_ATTEMPTS = 2;
constexpr int PING_TIMEOUT_MS = 800;
constexpr int PING_INTERVAL_MS = 100;

constexpr std::array<const char*, 4> PROBE_TARGETS{
    "8.8.8.8",
    "8.8.4.4",
    "1.1.1.1",
    "9.9.9.9"
};

struct PingStatistics {
    int attempts{0};
    std::vector<double> latencies;
};

struct IcmpPacket {
    icmphdr header{};
    std::array<std::uint8_t, 16> payload{};
};

class Socket {
public:
    explicit Socket(int descriptor = -1) noexcept
        : descriptor_(descriptor) {
    }

    ~Socket() {
        if (descriptor_ >= 0) {
            close(descriptor_);
        }
    }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    [[nodiscard]] int get() const noexcept {
        return descriptor_;
    }

private:
    int descriptor_;
};

ConnectionType detectConnectionType(
    const std::string& interfaceName
) {
    if (interfaceName.starts_with("wl")) {
        return ConnectionType::WiFi;
    }

    if (interfaceName.starts_with("eth") ||
        interfaceName.starts_with("en")) {
        return ConnectionType::Ethernet;
    }

    if (interfaceName.starts_with("wwan")) {
        return ConnectionType::Cellular;
    }

    return ConnectionType::Unknown;
}

bool waitForReply(int socketFd, int timeoutMs) {
    pollfd descriptor{};

    descriptor.fd = socketFd;
    descriptor.events = POLLIN;

    while (true) {
        const int result = poll(
            &descriptor,
            1,
            timeoutMs
        );

        if (result > 0) {
            if ((descriptor.revents & POLLIN) != 0) {
                return true;
            }

            return false;
        }

        if (result == 0) {
            return false;
        }

        if (errno != EINTR) {
            return false;
        }
    }
}

std::optional<double> pingTarget(
    int socketFd,
    const sockaddr_in& target,
    std::uint16_t sequence
) {
    IcmpPacket packet{};

    packet.header.type = ICMP_ECHO;
    packet.header.code = 0;

    packet.header.un.echo.id = htons(
        static_cast<std::uint16_t>(getpid() & 0xFFFF)
    );

    packet.header.un.echo.sequence = htons(sequence);

    const auto started = Clock::now();

    const ssize_t sent = sendto(
        socketFd,
        &packet,
        sizeof(packet),
        0,
        reinterpret_cast<const sockaddr*>(&target),
        sizeof(target)
    );

    if (sent != static_cast<ssize_t>(sizeof(packet))) {
        return std::nullopt;
    }

    const auto deadline =
        started + std::chrono::milliseconds(PING_TIMEOUT_MS);

    while (Clock::now() < deadline) {
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - Clock::now()
            ).count();

        if (remaining <= 0) {
            return std::nullopt;
        }

        if (!waitForReply(
                socketFd,
                static_cast<int>(remaining)
            )) {
            return std::nullopt;
        }

        std::array<std::uint8_t, 512> buffer{};

        sockaddr_in source{};
        socklen_t sourceLength = sizeof(source);

        const ssize_t received = recvfrom(
            socketFd,
            buffer.data(),
            buffer.size(),
            MSG_DONTWAIT,
            reinterpret_cast<sockaddr*>(&source),
            &sourceLength
        );

        if (received < static_cast<ssize_t>(sizeof(icmphdr))) {
            continue;
        }

        if (source.sin_addr.s_addr != target.sin_addr.s_addr) {
            continue;
        }

        const auto* reply =
            reinterpret_cast<const icmphdr*>(buffer.data());

        if (reply->type != ICMP_ECHOREPLY ||
            reply->code != 0 ||
            ntohs(reply->un.echo.sequence) != sequence) {
            continue;
        }

        const auto elapsed = Clock::now() - started;

        return std::chrono::duration<double, std::milli>(
            elapsed
        ).count();
    }

    return std::nullopt;
}

PingStatistics measureTarget(const char* address) {
    PingStatistics statistics{};

    Socket socketHandle(
        socket(
            AF_INET,
            SOCK_DGRAM | SOCK_CLOEXEC,
            IPPROTO_ICMP
        )
    );

    if (socketHandle.get() == -1) {
        statistics.attempts = PING_ATTEMPTS;
        return statistics;
    }

    sockaddr_in target{};

    target.sin_family = AF_INET;

    if (inet_pton(
            AF_INET,
            address,
            &target.sin_addr
        ) != 1) {
        statistics.attempts = PING_ATTEMPTS;
        return statistics;
    }

    for (int attempt = 0; attempt < PING_ATTEMPTS; ++attempt) {
        ++statistics.attempts;

        const auto latency = pingTarget(
            socketHandle.get(),
            target,
            static_cast<std::uint16_t>(attempt + 1)
        );

        if (latency.has_value()) {
            statistics.latencies.push_back(*latency);
        }

        if (attempt + 1 < PING_ATTEMPTS) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(PING_INTERVAL_MS)
            );
        }
    }

    return statistics;
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

        if (current->ifa_addr == nullptr ||
            current->ifa_name == nullptr) {
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

    double totalLatency = 0.0;
    double totalJitter = 0.0;

    int successfulPings = 0;
    int totalAttempts = 0;
    int jitterSamples = 0;

    for (const char* target : PROBE_TARGETS) {
        const PingStatistics statistics = measureTarget(target);

        totalAttempts += statistics.attempts;

        for (const double latency : statistics.latencies) {
            totalLatency += latency;
            ++successfulPings;
        }

        for (std::size_t i = 1;
             i < statistics.latencies.size();
             ++i) {

            totalJitter += std::abs(
                statistics.latencies[i] -
                statistics.latencies[i - 1]
            );

            ++jitterSamples;
        }
    }

    if (successfulPings > 0) {
        measurement.latencyMs =
            totalLatency / static_cast<double>(successfulPings);
    }

    if (jitterSamples > 0) {
        measurement.jitterMs =
            totalJitter / static_cast<double>(jitterSamples);
    }

    if (totalAttempts > 0) {
        const int lostPings = totalAttempts - successfulPings;

        measurement.packetLossPercent =
            static_cast<double>(lostPings) /
            static_cast<double>(totalAttempts) *
            100.0;
    }

    return measurement;
}

}
