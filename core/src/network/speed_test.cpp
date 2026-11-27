#include "deadnet/network/speed_test.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace deadnet::network {

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::uint64_t MAX_TEST_SIZE = 100ULL * 1024ULL * 1024ULL;

constexpr std::uint64_t MAX_RESPONSE_SIZE = 64ULL * 1024ULL;

constexpr std::size_t MAX_SERVER_COUNT = 8;

constexpr const char* USER_AGENT = "DEAD.NET/1.0.1";

struct CurlDeleter {

    void operator()(CURL* handle) const noexcept {

        if (handle != nullptr) {

            curl_easy_cleanup(handle);

        }

    }
};

using CurlHandle = std::unique_ptr<CURL, CurlDeleter>;

struct WriteContext {

    std::uint64_t transferredBytes{0};

    std::uint64_t maxBytes{0};

};

struct ProgressContext {

    SpeedTestStage stage{SpeedTestStage::Idle};

    std::uint64_t totalBytes{0};

    std::string serverName;

    SpeedTestProgressCallback* callback{nullptr};
};

struct ServerCandidate {

    SpeedTestServer server;

    double responseTimeMs{0.0};

};

bool initializeCurl() {

    static std::once_flag initializationFlag;

    static CURLcode initializationResult = CURLE_FAILED_INIT;

    std::call_once(initializationFlag, [] { initializationResult = curl_global_init(CURL_GLOBAL_DEFAULT);});

    return initializationResult == CURLE_OK;
}

bool isHttpsUrl(const std::string& url) {

    return url.rfind("https://", 0) == 0;

}

std::string makeDownloadUrl( const std::string& baseUrl, std::uint64_t bytes) {

    std::string result = baseUrl;

    const std::size_t fragmentPosition = result.find('#');

    const std::string fragment = fragmentPosition == std::string::npos ? "" : result.substr(fragmentPosition);

    if (fragmentPosition != std::string::npos) {

        result.erase(fragmentPosition);

    }

    result += result.find('?') == std::string::npos ? "?" : "&";

    result += "bytes=";

    result += std::to_string(bytes);

    result += fragment;

    return result;
}

void notifyProgress(ProgressContext& context, std::uint64_t transferredBytes) noexcept {

    if (context.callback == nullptr || !static_cast<bool>(*context.callback)) {
        return;
    }

    SpeedTestProgress progress;

    progress.stage = context.stage;
    progress.transferredBytes = transferredBytes;
    progress.totalBytes = context.totalBytes;
    progress.serverName = context.serverName;

    if (context.totalBytes > 0) {

        const double percentage = static_cast<double>(transferredBytes) / static_cast<double>(context.totalBytes) * 100.0;

        progress.progressPercent = std::clamp(percentage, 0.0, 100.0);
    }

    try {

        (*context.callback)(progress);

    } catch (...) {
        
    }
}

std::size_t writeCallback(char* data, std::size_t size, std::size_t count, void* userData) noexcept {

    if (userData == nullptr) {

        return 0;

    }

    if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size) {

        return 0;

    }

    const std::size_t receivedBytes = size * count;

    auto* context = static_cast<WriteContext*>(userData);

    if (receivedBytes > context->maxBytes - context->transferredBytes) {

        return 0;

    }

    context->transferredBytes += receivedBytes;

    return receivedBytes;
}

int progressCallback(

    void* userData,

    curl_off_t downloadTotal,
    curl_off_t downloadNow,
    curl_off_t uploadTotal,
    curl_off_t uploadNow

) noexcept {
    
    if (userData == nullptr) {

        return 0;

    }

    auto* context =static_cast<ProgressContext*>(userData);

    curl_off_t transferred = 0;

    if (context->stage == SpeedTestStage::Download) {
        
        transferred = downloadNow;
    
    } else if (context->stage == SpeedTestStage::Upload) {

        transferred = uploadNow;

    }

    if (transferred < 0) {
        transferred = 0;
    }

    notifyProgress(*context, static_cast<std::uint64_t>(transferred));

    return 0;
}

bool configureCommonOptions(CURL* handle, const std::string& url, const SpeedTestConfig& config, WriteContext& writeContext) {

    return

        curl_easy_setopt(handle, CURLOPT_URL, url.c_str()) == CURLE_OK &&

        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L) == CURLE_OK &&

        curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 3L) == CURLE_OK &&

        curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS) == CURLE_OK &&

        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, config.connectTimeoutSeconds) == CURLE_OK &&

        curl_easy_setopt(handle, CURLOPT_TIMEOUT, config.timeoutSeconds) == CURLE_OK &&

        curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L) == CURLE_OK &&

        curl_easy_setopt(handle, CURLOPT_USERAGENT, USER_AGENT) == CURLE_OK &&

        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L) == CURLE_OK &&

        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L) == CURLE_OK &&

        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeCallback) == CURLE_OK &&

        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &writeContext) == CURLE_OK;
}

bool configureProgressOptions(CURL* handle, ProgressContext& progressContext) {

    return

        curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L) == CURLE_OK &&

        curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, progressCallback) == CURLE_OK &&

        curl_easy_setopt(handle, CURLOPT_XFERINFODATA, &progressContext) == CURLE_OK;
}

bool isSuccessfulHttpResponse(CURL* handle, std::string& error) {

    long responseCode = 0;

    const CURLcode infoResult = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &responseCode);

    if (infoResult != CURLE_OK) {

        error = curl_easy_strerror(infoResult);

        return false;

    }

    if (responseCode < 200 || responseCode >= 300) {

        error = "HTTP server returned status " + std::to_string(responseCode);

        return false;

    }

    return true;
}

bool probeServer(const SpeedTestServer& server, const SpeedTestConfig& config, double& responseTimeMs, std::string& error) {

    CurlHandle handle{curl_easy_init()};

    if (!handle) {

        error = "Could not create CURL handle";

        return false;
        
    }

    WriteContext writeContext;

    writeContext.maxBytes = MAX_RESPONSE_SIZE;

    const std::string probeUrl = makeDownloadUrl(server.downloadUrl, 0);

    if (!configureCommonOptions(handle.get(), probeUrl, config, writeContext)) {

        error = "Could not configure CURL options";

        return false;

    }

    const long probeTimeout = std::min(config.timeoutSeconds, 10L);

    const long probeConnectTimeout = std::min(config.connectTimeoutSeconds, 5L);

    curl_easy_setopt( handle.get(), CURLOPT_TIMEOUT, probeTimeout);

    curl_easy_setopt(handle.get(), CURLOPT_CONNECTTIMEOUT, probeConnectTimeout);

    curl_easy_setopt( handle.get(), CURLOPT_HTTPGET, 1L);

    const CURLcode result = curl_easy_perform(handle.get());

    if (result != CURLE_OK) {

        error = curl_easy_strerror(result);

        return false;

    }

    if (!isSuccessfulHttpResponse(handle.get(), error)) {

        return false;

    }

    double startTransferSeconds = 0.0;

    const CURLcode infoResult = curl_easy_getinfo( handle.get(), CURLINFO_STARTTRANSFER_TIME, &startTransferSeconds);

    if (infoResult != CURLE_OK) {

        error = curl_easy_strerror(infoResult);

        return false;
    }

    responseTimeMs = startTransferSeconds * 1000.0;

    return true;
}

bool performDownload(const SpeedTestServer& server, const SpeedTestConfig& config, SpeedTestResult& result, SpeedTestProgressCallback& callback, std::string& error) {
    CurlHandle handle{curl_easy_init()};

    if (!handle) {

        error = "Could not create CURL handle";

        return false;

    }

    WriteContext writeContext;

    writeContext.maxBytes = config.downloadBytes;

    ProgressContext progressContext;

    progressContext.stage = SpeedTestStage::Download;
    progressContext.totalBytes = config.downloadBytes;
    progressContext.serverName = server.name;
    progressContext.callback = &callback;

    const std::string downloadUrl = makeDownloadUrl(server.downloadUrl, config.downloadBytes);

    if (!configureCommonOptions(handle.get(), downloadUrl, config, writeContext)) {

        error = "Could not configure download request";

        return false;
    }

    if (!configureProgressOptions(handle.get(), progressContext)) {

        error = "Could not configure download progress";

        return false;
    }

    curl_easy_setopt(handle.get(), CURLOPT_HTTPGET, 1L);

    const auto startTime = Clock::now();

    const CURLcode transferResult = curl_easy_perform(handle.get());

    const auto endTime = Clock::now();

    if (transferResult != CURLE_OK) {

        error = curl_easy_strerror(transferResult);

        return false;
    }

    if (!isSuccessfulHttpResponse(handle.get(), error)) {

        return false;
    }

    const std::uint64_t receivedBytes = writeContext.transferredBytes;

    if (receivedBytes != config.downloadBytes) {

        error ="Incomplete download: expected " + std::to_string(config.downloadBytes) + " bytes, received " + std::to_string(receivedBytes);

        return false;
    }

    const double durationSeconds = std::chrono::duration<double>(endTime - startTime).count();

    if (durationSeconds <= 0.0) {
        error = "Invalid download duration";
        return false;
    }

    result.downloadedBytes = receivedBytes;

    result.downloadMbps = (static_cast<double>(receivedBytes) * 8.0) / durationSeconds / 1'000'000.0;

    notifyProgress(progressContext, receivedBytes);

    return true;
}

bool performUpload(const SpeedTestServer& server,const SpeedTestConfig& config, SpeedTestResult& result, SpeedTestProgressCallback& callback, std::string& error) {

    CurlHandle handle{curl_easy_init()};

    if (!handle) {

        error = "Could not create CURL handle";

        return false;

    }

    std::vector<unsigned char> payload(static_cast<std::size_t>(config.uploadBytes));

    for (std::size_t i = 0; i < payload.size(); ++i) {

        payload[i] = static_cast<unsigned char>((i * 31U + 17U) & 0xFFU);

    }

    WriteContext writeContext;

    writeContext.maxBytes = MAX_RESPONSE_SIZE;

    ProgressContext progressContext;

    progressContext.stage = SpeedTestStage::Upload;
    progressContext.totalBytes = config.uploadBytes;
    progressContext.serverName = server.name;
    progressContext.callback = &callback;

    if (!configureCommonOptions(handle.get(), server.uploadUrl, config, writeContext)) {

        error = "Could not configure upload request";

        return false;
    }

    if (!configureProgressOptions(handle.get(), progressContext)) {

        error = "Could not configure upload progress";
        return false;
    }

    curl_slist* headers = nullptr;

    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");

    if (headers == nullptr) {

        error = "Could not allocate HTTP headers";

        return false;
    }

    curl_easy_setopt(handle.get(), CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(handle.get(), CURLOPT_POST, 1L);

    curl_easy_setopt( handle.get(), CURLOPT_POSTFIELDS, reinterpret_cast<const char*>(payload.data()));

    curl_easy_setopt(handle.get(), CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(payload.size()));

    const auto startTime = Clock::now();

    const CURLcode transferResult = curl_easy_perform(handle.get());

    const auto endTime = Clock::now();

    curl_slist_free_all(headers);

    if (transferResult != CURLE_OK) {
        error = curl_easy_strerror(transferResult);
        return false;
    }

    if (!isSuccessfulHttpResponse(handle.get(), error)) {
        return false;
    }

    const double durationSeconds = std::chrono::duration<double>(endTime - startTime ).count();

    if (durationSeconds <= 0.0) {
        error = "Invalid upload duration";
        return false;
    }

    result.uploadedBytes = config.uploadBytes;

    result.uploadMbps = (static_cast<double>(config.uploadBytes) * 8.0) / durationSeconds / 1'000'000.0;

    notifyProgress(progressContext, config.uploadBytes);

    return true;
}

} 

SpeedTest::SpeedTest(NetworkProbe& networkProbe) noexcept
    : networkProbe_(networkProbe) {
}

SpeedTestResult SpeedTest::run(

    const SpeedTestConfig& config,
    SpeedTestProgressCallback progressCallback

) const {

    SpeedTestResult result;

    result.timestamp = std::chrono::system_clock::now();

    const auto testStartTime = Clock::now();

    auto finish = [&]() {

        result.durationMs = std::chrono::duration<double, std::milli>(Clock::now() - testStartTime).count();

    };

    auto fail = [&](const std::string& message) {

        result.success = false;
        result.error = message;

        SpeedTestProgress progress;

        progress.stage = SpeedTestStage::Failed;

        try {
            
            if (progressCallback) {
                
                progressCallback(progress);
            
            }
        } catch (...) {
            
        }

        finish();

        return result;
    };

    if (config.servers.empty()) {
        
        return fail("No speed-test servers configured");
   
    }

    if (config.servers.size() > MAX_SERVER_COUNT) {
        
        return fail("Too many speed-test servers configured");
    
    }

    if (config.downloadBytes == 0 || config.uploadBytes == 0 || config.downloadBytes > MAX_TEST_SIZE || config.uploadBytes > MAX_TEST_SIZE) {
       
        return fail("Invalid transfer size");
    
    }

    if (config.timeoutSeconds <= 0 || config.connectTimeoutSeconds <= 0) {
        
        return fail("Invalid timeout configuration");
   
    }

    for (const auto& server : config.servers) {
        
        if (server.name.empty() || !isHttpsUrl(server.downloadUrl) || !isHttpsUrl(server.uploadUrl)) {

            return fail("Invalid server configuration: " + server.name);
        
        }

    }

    if (!initializeCurl()) {
        return fail("Could not initialize libcurl");
    }

    std::string probeWarning;

    SpeedTestProgress latencyProgress;

    latencyProgress.stage = SpeedTestStage::Latency;

    try {
    
        if (progressCallback) {

            progressCallback(latencyProgress);

        }

    } catch (...) {
    }

        try {
            
            const NetworkMeasurement measurement =
            networkProbe_.measure();

            result.latencyMs =
                measurement.latencyMs;

            result.jitterMs =
                measurement.jitterMs;

            result.packetLossPercent =
                measurement.packetLossPercent;

        } catch (const std::exception& exception) {
            probeWarning = std::string("Network probe failed: ") +exception.what();

    } catch (...) {
        probeWarning = "Network probe failed with an unknown error";
    }

    try {

        const NetworkMeasurement measurement = networkProbe_.measure();

        result.latencyMs = measurement.latencyMs;

        result.jitterMs = measurement.jitterMs;

        result.packetLossPercent = measurement.packetLossPercent;

    } catch (const std::exception& exception) {
        
        probeWarning =std::string("Network probe failed: ") +exception.what();
    
    } catch (...) {
        
        probeWarning = "Network probe failed with an unknown error";
    }

    std::vector<ServerCandidate> candidates;

    for (const auto& server : config.servers) {
        
        double responseTimeMs = 0.0;

        std::string error;

        if (probeServer(server, config, responseTimeMs, error)) {

            candidates.push_back({server, responseTimeMs});

        }
    }

    if (candidates.empty()) {

        return fail( "All configured speed-test servers are unavailable" );

    }

    std::sort(

        candidates.begin(),
        candidates.end(),

        [](const ServerCandidate& left,
           const ServerCandidate& right) {
            return left.responseTimeMs <
                   right.responseTimeMs;

        }
    );


    std::string lastError;

    bool transferCompleted = false;

    for (const auto& candidate : candidates) {

        result.server = candidate.server.name;

        result.downloadedBytes = 0;
        result.uploadedBytes = 0;

        result.downloadMbps = 0.0;
        result.uploadMbps = 0.0;

        std::string error;

        const bool downloadSuccess = performDownload( candidate.server, config, result, progressCallback, error );

        if (!downloadSuccess) {
            
            lastError = "Download failed on " + candidate.server.name + ": " + error;

            continue;
        }

        error.clear();

        const bool uploadSuccess = performUpload( candidate.server, config, result, progressCallback, error );

        if (!uploadSuccess) {

            lastError = "Upload failed on " + candidate.server.name + ": " + error;

            continue;
        }

        transferCompleted = true;

        break;
    }

    if (!transferCompleted) {

        return fail(

            lastError.empty()
                ? "Speed test failed on all available servers"
                : lastError
        );
    }

    result.success = true;

    if (!probeWarning.empty()) {
        result.error = probeWarning;
    }

    SpeedTestProgress completedProgress;

    completedProgress.stage = SpeedTestStage::Completed;

    completedProgress.progressPercent = 100.0;

    completedProgress.transferredBytes = result.downloadedBytes + result.uploadedBytes;

    completedProgress.totalBytes = config.downloadBytes + config.uploadBytes;

    completedProgress.serverName = result.server;

    try {

        if (progressCallback) {

            progressCallback(completedProgress);

        }
    } catch (...) {
    }

    finish();

    return result;
}

} 