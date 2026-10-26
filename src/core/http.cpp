#include "http.h"
#include "json.h"
#include "log.h"
#include "sha1.h"
#include "util.h"
#include "../version.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>

namespace aurora {

static std::wstring winHttpError(DWORD code) {
    wchar_t* buf = nullptr;
    DWORD n = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                             FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr, code, MAKELANGID(LANG_FRENCH, SUBLANG_FRENCH),
                             (LPWSTR)&buf, 0, nullptr);
    std::wstring out;
    if (n && buf) {
        out.assign(buf, n);
        LocalFree(buf);
        while (!out.empty() && (out.back() == L'\r' || out.back() == L'\n' || out.back() == L' ')) out.pop_back();
    }
    return out.empty() ? formatW(L"erreur réseau %lu", (unsigned long)code) : out;
}

static bool crackUrl(const std::wstring& url, std::wstring& host, std::wstring& path,
                     INTERNET_PORT& port, bool& secure) {
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return false;
    host.assign(uc.lpszHostName, uc.dwHostNameLength);
    path.assign(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (uc.dwExtraInfoLength) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
    if (path.empty()) path = L"/";
    port = uc.nPort;
    secure = uc.nScheme == INTERNET_SCHEME_HTTPS;
    return !host.empty();
}

bool isHttpUrl(const std::wstring& url) {
    return startsWith(toLower(url), L"https://") || startsWith(toLower(url), L"http://");
}static bool httpOnce(const HttpRequest& req, HttpResponse& res, HttpProgress& progress,
                     HttpCancel& cancel, HttpSink& sink, DWORD& lastError) {
    std::wstring host, path;
    INTERNET_PORT port = 0;
    bool secure = false;
    if (!crackUrl(req.url, host, path, port, secure)) {
        res.error = L"URL invalide";
        return false;
    }
    HINTERNET session = WinHttpOpen(L"AuroraLauncher/" AURORA_VERSION_W L" (Windows NT 10.0; Win64; x64)",
                                    WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        lastError = GetLastError();
        res.error = winHttpError(lastError);
        return false;
    }
    int timeout = req.timeoutMs > 0 ? req.timeoutMs : 30000;
    WinHttpSetTimeouts(session, 15000, 15000, timeout, timeout);

    HINTERNET connect = WinHttpConnect(session, host.c_str(), port, 0);
    if (!connect) {
        lastError = GetLastError();
        res.error = winHttpError(lastError);
        WinHttpCloseHandle(session);
        return false;
    }
    DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, req.method.c_str(), path.c_str(),
                                           nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        lastError = GetLastError();
        res.error = winHttpError(lastError);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }
    // Allow modern TLS even on older systems where defaults can be conservative.
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    WinHttpSetOption(request, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof(protocols));

    std::wstring allHeaders;
    for (auto& h : req.headers) allHeaders += h.first + L": " + h.second + L"\r\n";
    if (!req.contentType.empty()) allHeaders += L"Content-Type: " + req.contentType + L"\r\n";

    BOOL sent = WinHttpSendRequest(request, allHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : allHeaders.c_str(),
                                   (DWORD)(allHeaders.empty() ? 0 : allHeaders.size()),
                                   req.body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)req.body.data(),
                                   (DWORD)req.body.size(), (DWORD)req.body.size(), 0);
    if (!sent) {
        lastError = GetLastError();
        res.error = winHttpError(lastError);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }
    if (!WinHttpReceiveResponse(request, nullptr)) {
        lastError = GetLastError();
        res.error = winHttpError(lastError);
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD status = 0, slen = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &slen, WINHTTP_NO_HEADER_INDEX);
    res.status = (int)status;
    uint64_t total = 0;
    wchar_t lenBuf[64];
    DWORD llen = sizeof(lenBuf);
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX,
                            lenBuf, &llen, WINHTTP_NO_HEADER_INDEX))
        total = _wcstoui64(lenBuf, nullptr, 10);
    res.finalUrl = req.url;

    if (status >= 400 && !sink) {
        // Still capture a bit of the body for diagnostics.
        std::string errBody;
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(request, &avail) && avail > 0 && errBody.size() < 4096) {
            std::vector<char> buf(std::min<DWORD>(avail, 2048));
            DWORD read = 0;
            if (!WinHttpReadData(request, buf.data(), (DWORD)buf.size(), &read) || read == 0) break;
            errBody.append(buf.data(), read);
        }
        res.body = errBody;
        res.ok = true;
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return true;
    }

    DWORD avail = 0;
    uint64_t received = 0;
    bool sinkOk = true;
    res.body.clear();
    while (WinHttpQueryDataAvailable(request, &avail)) {
        if (avail == 0) break;
        if (cancel && cancel()) {
            res.error = L"Téléchargement annulé";
            sinkOk = false;
            break;
        }
        std::vector<uint8_t> buf(std::min<DWORD>(avail, 65536));
        DWORD read = 0;
        if (!WinHttpReadData(request, buf.data(), (DWORD)buf.size(), &read)) {
            lastError = GetLastError();
            res.error = winHttpError(lastError);
            sinkOk = false;
            break;
        }
        if (read == 0) break;
        received += read;
        if (sink) {
            if (!sink(buf.data(), read)) {
                res.error = L"Écriture disque impossible";
                sinkOk = false;
                break;
            }
        } else {
            res.body.append((const char*)buf.data(), read);
        }
        if (progress) progress(received, total);
    }
    res.size = received;
    res.ok = sinkOk && res.error.empty();
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return res.ok;
}

bool httpRequest(const HttpRequest& request, HttpResponse& response, HttpProgress progress,
                 HttpCancel cancel, HttpSink sink) {
    response = HttpResponse();
    DWORD lastError = 0;
    for (int attempt = 0; attempt < 3; attempt++) {
        if (cancel && cancel()) { response.error = L"Annulé"; return false; }
        bool ok = httpOnce(request, response, progress, cancel, sink, lastError);
        if (ok && (response.status < 500 || response.status == 501 || attempt == 2)) return true;
        if (cancel && cancel()) return false;
        if (!ok || response.status >= 500 || response.status == 429) {
            response.error = response.error.empty() ? formatW(L"HTTP %d", response.status) : response.error;
            logWarn(formatW(L"Requête échouée (%s) - nouvel essai %d/2 : %s",
                            response.error.c_str(), attempt + 1, request.url.c_str()), L"Réseau");
            sleepMs(400 * (attempt + 1));
            continue;
        }
        return true;
    }
    return false;
}

bool httpGet(const std::wstring& url, std::string& out, std::wstring& error,
             const std::vector<std::pair<std::wstring, std::wstring>>& headers) {
    HttpRequest req;
    req.url = url;
    for (auto& h : headers) req.headers.push_back(h);
    if (headers.empty()) req.headers.push_back({ L"User-Agent", L"AuroraLauncher/" AURORA_VERSION_W });
    HttpResponse res;
    if (!httpRequest(req, res)) { error = res.error.empty() ? L"Échec réseau" : res.error; return false; }
    if (res.status < 200 || res.status >= 300) {
        error = formatW(L"HTTP %d", res.status);
        return false;
    }
    out = std::move(res.body);
    return true;
}

bool httpGetJson(const std::wstring& url, Json& out, std::wstring& error,
                 const std::vector<std::pair<std::wstring, std::wstring>>& headers) {
    std::string body;
    if (!httpGet(url, body, error, headers)) return false;
    std::string perr;
    if (!Json::parse(body, out, &perr)) {
        error = L"Réponse JSON invalide : " + utf8ToWide(perr);
        return false;
    }
    return true;
}

bool httpPostJson(const std::wstring& url, const std::string& jsonBody, Json& out,
                  std::wstring& error,
                  const std::vector<std::pair<std::wstring, std::wstring>>& headers) {
    HttpRequest req;
    req.url = url;
    req.method = L"POST";
    req.body = jsonBody;
    req.contentType = L"application/json";
    req.headers.push_back({ L"Accept", L"application/json" });
    req.headers.push_back({ L"User-Agent", L"AuroraLauncher/" AURORA_VERSION_W });
    for (auto& h : headers) req.headers.push_back(h);
    HttpResponse res;
    if (!httpRequest(req, res)) { error = res.error.empty() ? L"Échec réseau" : res.error; return false; }
    if (res.status < 200 || res.status >= 300) {
        out = Json::object();
        std::string perr;
        if (Json::parse(res.body, out, &perr)) {
            std::wstring msg = out["error_description"].asWString();
            if (msg.empty()) msg = out["error"].asWString();
            if (msg.empty()) msg = out["message"].asWString();
            if (!msg.empty()) error = formatW(L"HTTP %d - %s", res.status, msg.c_str());
            else error = formatW(L"HTTP %d", res.status);
        } else {
            error = formatW(L"HTTP %d", res.status);
        }
        return false;
    }
    std::string perr;
    if (!Json::parse(res.body, out, &perr)) {
        error = L"Réponse JSON invalide : " + utf8ToWide(perr);
        return false;
    }
    return true;
}

bool httpPostForm(const std::wstring& url, const std::string& formBody, Json& out,
                  std::wstring& error) {
    HttpRequest req;
    req.url = url;
    req.method = L"POST";
    req.body = formBody;
    req.contentType = L"application/x-www-form-urlencoded";
    req.headers.push_back({ L"Accept", L"application/json" });
    req.headers.push_back({ L"User-Agent", L"AuroraLauncher/" AURORA_VERSION_W });
    HttpResponse res;
    if (!httpRequest(req, res)) { error = res.error.empty() ? L"Échec réseau" : res.error; return false; }
    std::string perr;
    if (!Json::parse(res.body, out, &perr)) {
        error = formatW(L"HTTP %d - réponse invalide", res.status);
        return false;
    }
    if (res.status < 200 || res.status >= 300) {
        std::wstring msg = out["error_description"].asWString();
        if (msg.empty()) msg = out["error"].asWString();
        if (msg.empty()) msg = out["message"].asWString();
        error = msg.empty() ? formatW(L"HTTP %d", res.status) : msg;
        return false;
    }
    return true;
}

bool downloadFile(const std::wstring& url, const fs::path& destination,
                  const std::wstring& expectedSha1, uint64_t expectedSize,
                  HttpProgress progress, HttpCancel cancel, std::wstring& error,
                  bool verifyHash, const std::wstring& expectedSha256) {
    auto verifyFile = [&](const fs::path& file) -> bool {
        if (!expectedSha256.empty()) {
            std::string digest;
            if (!sha256File(file, digest) || !iequals(utf8ToWide(digest), toLower(expectedSha256))) {
                error = L"SHA-256 invalide pour " + destination.filename().wstring();
                return false;
            }
        } else if (!expectedSha1.empty() && verifyHash) {
            std::string digest;
            if (!Sha1::fileDigest(file.wstring(), digest) ||
                !iequals(utf8ToWide(digest), toLower(expectedSha1))) {
                error = L"SHA1 invalide pour " + destination.filename().wstring();
                return false;
            }
        }
        if (expectedSize > 0 && fileSize(file) != expectedSize) {
            error = L"Taille invalide pour " + destination.filename().wstring();
            return false;
        }
        return true;
    };

    if (pathExists(destination) && !isDirectory(destination)) {
        bool good = true;
        const bool hasDigest = !expectedSha256.empty() || (!expectedSha1.empty() && verifyHash);
        if (hasDigest || expectedSize > 0) {
            good = verifyFile(destination);
            if (!good) error.clear();
        }
        if (good) {
            if (progress) progress(expectedSize ? expectedSize : fileSize(destination),
                                   expectedSize ? expectedSize : fileSize(destination));
            return true;
        }
        removeFile(destination);
    }
    ensureDir(destination.parent_path());
    fs::path part = destination;
    part += L".part";
    removeFile(part);

    for (int attempt = 0; attempt < 3; attempt++) {
        if (cancel && cancel()) { error = L"Annulé"; removeFile(part); return false; }
        FILE* file = _wfopen(part.wstring().c_str(), L"wb");
        if (!file) {
            error = L"Impossible d'écrire : " + destination.wstring();
            return false;
        }
        bool writeOk = true;
        HttpRequest req;
        req.url = url;
        req.headers.push_back({ L"User-Agent", L"AuroraLauncher/" AURORA_VERSION_W });
        req.timeoutMs = 60000;
        HttpResponse res;
        bool ok = httpRequest(req, res, progress, cancel,
            [&](const uint8_t* data, size_t len) {
                if (fwrite(data, 1, len, file) != len) { writeOk = false; return false; }
                return true;
            });
        fflush(file);
        fclose(file);
        if (ok && writeOk && res.status >= 200 && res.status < 300) {
            if (!verifyFile(part)) {
                removeFile(part);
                if (attempt < 2) continue;
                return false;
            }
            removeFile(destination);
            if (!renamePath(part, destination)) {
                error = L"Impossible de finaliser " + destination.wstring();
                removeFile(part);
                return false;
            }
            return true;
        }
        removeFile(part);
        error = res.error.empty() ? formatW(L"HTTP %d", res.status) : res.error;
        if (res.status == 404) return false;
        if (cancel && cancel()) return false;
        sleepMs(350 * (attempt + 1));
    }
    if (error.empty()) error = L"Téléchargement impossible";
    return false;
}

bool downloadBatch(const std::vector<DownloadItem>& items, int threads, HttpCancel cancel,
                   const std::function<void(int done, int total, uint64_t bytesDone,
                                            uint64_t bytesTotal)>& progress,
                   std::wstring& error) {
    error.clear();
    if (items.empty()) return true;
    std::atomic<size_t> next{ 0 };
    std::atomic<bool> stop{ false };
    std::atomic<uint64_t> doneBytes{ 0 };
    std::atomic<int> doneItems{ 0 };
    std::atomic<long long> lastReport{ 0 };
    std::mutex errMutex;
    std::wstring firstError;
    uint64_t totalBytes = 0;
    for (auto& it : items) totalBytes += it.size;
    int totalItems = (int)items.size();
    int count = std::max(1, std::min(threads, 16));
    if (count > (int)items.size()) count = (int)items.size();
    auto begin = std::chrono::steady_clock::now();
    auto worker = [&]() {
        while (!stop.load()) {
            size_t i = next.fetch_add(1);
            if (i >= items.size()) break;
            if (cancel && cancel()) { stop.store(true); break; }
            std::wstring err;
            bool ok = downloadFile(items[i].url, items[i].path, items[i].sha1, items[i].size,
                                   [](uint64_t, uint64_t) {}, cancel, err, items[i].verifySha);
            if (!ok) {
                std::lock_guard<std::mutex> lock(errMutex);
                if (firstError.empty()) {
                    firstError = items[i].path.filename().wstring() + L" : " +
                                 (err.empty() ? L"échec du téléchargement" : err);
                }
                stop.store(true);
                break;
            }
            doneBytes.fetch_add(items[i].size);
            doneItems.fetch_add(1);
            if (progress) {
                long long now = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - begin).count();
                long long last = lastReport.load();
                if (now - last >= 150 && lastReport.compare_exchange_strong(last, now)) {
                    progress(doneItems.load(), totalItems, doneBytes.load(), totalBytes);
                }
            }
        }
    };
    std::vector<std::thread> pool;
    for (int i = 0; i < count; i++) pool.emplace_back(worker);
    for (auto& t : pool) t.join();
    if (progress) progress(doneItems.load(), totalItems, doneBytes.load(), totalBytes);
    if (cancel && cancel()) { error = L"Téléchargement annulé"; return false; }
    if (!firstError.empty()) { error = firstError; return false; }
    std::lock_guard<std::mutex> lock(errMutex);
    return firstError.empty();
}

} // namespace aurora
