#include "earth_engine/providers/CurlBytesSource.h"

#include <algorithm>

#include <curl/curl.h>

namespace earth_engine {

namespace {

size_t writeCallback(char* data, size_t size, size_t nmemb, void* userData) {
    const size_t total = size * nmemb;
    auto* out = static_cast<std::vector<uint8_t>*>(userData);
    const auto* bytes = reinterpret_cast<const uint8_t*>(data);
    out->insert(out->end(), bytes, bytes + total);
    return total;
}

} // namespace

CurlBytesSource::CurlBytesSource(long timeoutMs) : timeoutMs_(timeoutMs) {}

std::optional<std::vector<uint8_t>> CurlBytesSource::requestTileBytes(
    const TileKey& /*key*/, const std::string& url) const {
    static const bool kCurlInited = [] {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        return true;
    }();
    (void)kCurlInited;

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        return std::nullopt;
    }
    std::vector<uint8_t> body;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeoutMs_);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, std::min(timeoutMs_, 5000L));
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    const CURLcode rc = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK || httpCode != 200 || body.empty()) {
        return std::nullopt;
    }
    return body;
}

} // namespace earth_engine
