
#pragma once

#include "deadnet/network/network_probe.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace deadnet::network {

enum class SpeedTestStage : std::uint8_t {
    Idle = 0,
    Latency,
    Download,
    Upload,
    Completed,
    Failed
};

struct SpeedTestServer {
    
    std::string name;
    std::string downloadUrl;
    std::string uploadUrl;

};

struct SpeedTestConfig {

    std::vector<SpeedTestServer> servers{ { "Cloudflare", "https://speed.cloudflare.com/__down", "https://speed.cloudflare.com/__up"}};

    std::string downloadUrl{"https://speed.cloudflare.com/__down"};

    std::string uploadUrl{"https://speed.cloudflare.com/__up"};

    std::uint64_t downloadBytes{10ULL * 1024ULL * 1024ULL};

    std::uint64_t uploadBytes{5ULL * 1024ULL * 1024ULL};

    long timeoutSeconds{30};

    long connectTimeoutSeconds{10};
};

struct SpeedTestProgress {

    SpeedTestStage stage{SpeedTestStage::Idle};

    double progressPercent{0.0};

    std::uint64_t transferredBytes{0};

    std::uint64_t totalBytes{0};

    std::string serverName;
};

struct SpeedTestResult {

    bool success{false};

    double durationMs{0.0};

    double downloadMbps{0.0};

    double uploadMbps{0.0};

    double latencyMs{0.0};

    double jitterMs{0.0};

    double packetLossPercent{0.0};

    std::string error;

    std::string server;

    std::chrono::system_clock::time_point timestamp{};

    std::uint64_t downloadedBytes{0};

    std::uint64_t uploadedBytes{0};

};

using SpeedTestProgressCallback =
    std::function<void(const SpeedTestProgress&)>;

class SpeedTest final {

public:

    explicit SpeedTest(NetworkProbe& networkProbe) noexcept;

    [[nodiscard]]
    SpeedTestResult run(const SpeedTestConfig& config = {}, SpeedTestProgressCallback progressCallback = {}) const;

private:

    NetworkProbe& networkProbe_;
};

}
