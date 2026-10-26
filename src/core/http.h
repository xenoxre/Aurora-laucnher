#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace aurora {

class Json;

struct HttpRequest {
    std::wstring url;
    std::wstring method = L"GET";
    std::vector<std::pair<std::wstring, std::wstring>> headers;
    std::string body;
    std::wstring contentType;
    int timeoutMs = 30000;
};

struct HttpResponse {
    bool ok = false;
    int status = 0;
    std::string body;
    std::wstring error;
    uint64_t size = 0;
    std::wstring finalUrl;
};

using HttpProgress = std::function<void(uint64_t received, uint64_t total)>;
using HttpCancel = std::function<bool()>;
using HttpSink = std::function<bool(const uint8_t* data, size_t len)>;

bool httpRequest(const HttpRequest& request, HttpResponse& response,
                 HttpProgress progress = {}, HttpCancel cancel = {}, HttpSink sink = {});

bool httpGet(const std::wstring& url, std::string& out, std::wstring& error,
             const std::vector<std::pair<std::wstring, std::wstring>>& headers = {});

bool httpGetJson(const std::wstring& url, Json& out, std::wstring& error,
                 const std::vector<std::pair<std::wstring, std::wstring>>& headers = {});

bool httpPostJson(const std::wstring& url, const std::string& jsonBody, Json& out,
                  std::wstring& error,
                  const std::vector<std::pair<std::wstring, std::wstring>>& headers = {});

bool httpPostForm(const std::wstring& url, const std::string& formBody, Json& out,
                  std::wstring& error);

// Streams to a temporary .part file, verifies sha256/sha1/size when provided, then renames.
bool downloadFile(const std::wstring& url, const std::filesystem::path& destination,
                  const std::wstring& expectedSha1, uint64_t expectedSize,
                  HttpProgress progress, HttpCancel cancel, std::wstring& error,
                  bool verifyHash = true, const std::wstring& expectedSha256 = L"");

struct DownloadItem {
    std::wstring url;
    std::filesystem::path path;
    std::wstring sha1;
    uint64_t size = 0;
    bool verifySha = true;
};

// Parallel, progress-aggregated download used by installers.
bool downloadBatch(const std::vector<DownloadItem>& items, int threads, HttpCancel cancel,
                   const std::function<void(int done, int total, uint64_t bytesDone,
                                            uint64_t bytesTotal)>& progress,
                   std::wstring& error);

bool isHttpUrl(const std::wstring& url);

} // namespace aurora
