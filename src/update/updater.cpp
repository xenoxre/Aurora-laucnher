#include "update/updater.h"
#include "update/update_config.h"

#include "core/json.h"
#include "core/log.h"
#include "core/util.h"
#include "version.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <vector>

namespace aurora::update {

namespace {

std::wstring trimCopy(const std::wstring& s) {
    size_t b = 0, e = s.size();
    while (b < e && iswspace(s[b])) ++b;
    while (e > b && iswspace(s[e - 1])) --e;
    return s.substr(b, e - b);
}

bool isHex64(const std::wstring& s) {
    if (s.size() != 64) return false;
    for (wchar_t c : s) {
        if (!iswxdigit(c)) return false;
    }
    return true;
}

bool isLocalSource(const std::wstring& source) {
    return source.rfind(L"file://", 0) == 0 || source.find(L"://") == std::wstring::npos;
}

std::wstring localPathFromSource(std::wstring source) {
    if (source.rfind(L"file://", 0) == 0) {
        source = source.substr(7);
        // file:///C:/... -> C:/...
        if (source.size() > 2 && source[0] == L'/' && iswalpha(source[1]) && source[2] == L':')
            source.erase(source.begin());
    }
    return source;
}

bool isAllowedRemoteUrl(const std::wstring& url) {
    std::wstring lower = toLower(url);
    if (lower.rfind(L"https://", 0) == 0) return true;
    if (lower.rfind(L"http://", 0) == 0) {
        const size_t hostStart = 7;
        const size_t slash = lower.find(L'/', hostStart);
        const std::wstring host =
            lower.substr(hostStart, slash == std::wstring::npos ? std::wstring::npos : slash - hostStart);
        return host == L"localhost" || host == L"127.0.0.1" || host == L"[::1]";
    }
    return false;
}

struct ParsedVersion {
    std::vector<long long> core;
    std::vector<std::wstring> pre;
    bool valid = false;
};

ParsedVersion parseVersion(std::wstring text) {
    ParsedVersion out;
    text = trimCopy(text);
    if (!text.empty() && (text[0] == L'v' || text[0] == L'V')) text.erase(text.begin());
    const size_t plus = text.find(L'+');
    if (plus != std::wstring::npos) text = text.substr(0, plus);
    const size_t dash = text.find(L'-');
    std::wstring core = dash == std::wstring::npos ? text : text.substr(0, dash);
    if (dash != std::wstring::npos) {
        const std::wstring pre = text.substr(dash + 1);
        if (pre.empty()) return out;
        size_t start = 0;
        while (start <= pre.size()) {
            const size_t dot = pre.find(L'.', start);
            std::wstring part =
                dot == std::wstring::npos ? pre.substr(start) : pre.substr(start, dot - start);
            if (part.empty()) return out;
            out.pre.push_back(part);
            if (dot == std::wstring::npos) break;
            start = dot + 1;
        }
    }
    if (core.empty()) return out;
    size_t start = 0;
    while (start <= core.size()) {
        const size_t dot = core.find(L'.', start);
        std::wstring part =
            dot == std::wstring::npos ? core.substr(start) : core.substr(start, dot - start);
        if (part.empty()) return out;
        for (wchar_t c : part) {
            if (!iswdigit(c)) return out;
        }
        // Avoid overflow on absurd manifests.
        if (part.size() > 9) return out;
        out.core.push_back(_wtoi64(part.c_str()));
        if (dot == std::wstring::npos) break;
        start = dot + 1;
    }
    out.valid = !out.core.empty();
    return out;
}

int comparePre(const std::vector<std::wstring>& a, const std::vector<std::wstring>& b) {
    if (a.empty() && b.empty()) return 0;
    if (a.empty()) return 1;  // release > prerelease
    if (b.empty()) return -1;
    const size_t n = std::max(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        if (i >= a.size()) return -1;
        if (i >= b.size()) return 1;
        const bool na = std::all_of(a[i].begin(), a[i].end(), [](wchar_t c) { return iswdigit(c); });
        const bool nb = std::all_of(b[i].begin(), b[i].end(), [](wchar_t c) { return iswdigit(c); });
        if (na && nb) {
            const long long va = _wtoi64(a[i].c_str());
            const long long vb = _wtoi64(b[i].c_str());
            if (va != vb) return va < vb ? -1 : 1;
        } else if (na != nb) {
            return na ? -1 : 1;  // numeric identifiers have lower precedence
        } else {
            const int cmp = a[i].compare(b[i]);
            if (cmp != 0) return cmp < 0 ? -1 : 1;
        }
    }
    return 0;
}

bool parseAsset(const Json& node, Asset& out, const wchar_t* name, std::wstring& error) {
    if (!node.isObject()) {
        error = std::wstring(L"Champ manquant : ") + name;
        return false;
    }
    if (node["url"].isString()) out.url = trimCopy(node["url"].asWString());
    if (node["sha256"].isString()) out.sha256 = toLower(trimCopy(node["sha256"].asWString()));
    if (node["size"].isNumber()) out.size = (uint64_t)node["size"].asInt(0);
    else if (node["size"].isString()) out.size = (uint64_t)_wtoi64(node["size"].asWString().c_str());
    if (out.url.empty()) {
        error = std::wstring(L"URL manquante pour ") + name;
        return false;
    }
    if (!isHex64(out.sha256)) {
        error = std::wstring(L"SHA-256 invalide pour ") + name;
        return false;
    }
    if (out.size == 0) {
        error = std::wstring(L"Taille invalide pour ") + name;
        return false;
    }
    return true;
}

bool verifyLocalFile(const fs::path& path, const Asset& asset, std::wstring& error) {
    if (fileSize(path) != asset.size) {
        error = L"Taille invalide pour " + path.filename().wstring();
        return false;
    }
    std::string digest;
    if (!sha256File(path, digest) || toLower(utf8ToWide(digest)) != asset.sha256) {
        error = L"SHA-256 invalide pour " + path.filename().wstring();
        return false;
    }
    return true;
}

std::wstring quoteArg(const std::wstring& arg) {
    if (arg.find_first_of(L" \"\t") == std::wstring::npos) return arg;
    std::wstring out = L"\"";
    for (wchar_t c : arg) {
        if (c == L'"') out += L'\\';
        out += c;
    }
    out += L'"';
    return out;
}

} // namespace

bool Asset::valid() const {
    return !url.empty() && size > 0 && isHex64(sha256);
}

bool IsValidVersion(const std::wstring& version) {
    return parseVersion(version).valid;
}

int CompareVersions(const std::wstring& a, const std::wstring& b) {
    const ParsedVersion va = parseVersion(a);
    const ParsedVersion vb = parseVersion(b);
    if (!va.valid || !vb.valid) return 0;
    const size_t n = std::max(va.core.size(), vb.core.size());
    for (size_t i = 0; i < n; ++i) {
        const long long x = i < va.core.size() ? va.core[i] : 0;
        const long long y = i < vb.core.size() ? vb.core[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return comparePre(va.pre, vb.pre);
}

bool ParseManifest(const std::string& text, Info& out, std::wstring& error) {
    Json root;
    std::string parseError;
    if (!Json::parse(text, root, &parseError)) {
        error = L"Manifeste JSON invalide";
        return false;
    }
    if (!root.isObject()) {
        error = L"Manifeste invalide : objet attendu";
        return false;
    }

    Info info;
    if (root["schema"].isNumber()) info.schema = root["schema"].asInt(1);
    if (info.schema != 1) {
        error = L"Version de manifeste non prise en charge";
        return false;
    }
    if (root["version"].isString()) info.version = trimCopy(root["version"].asWString());
    if (root["channel"].isString()) info.channel = trimCopy(root["channel"].asWString());
    if (root["notes"].isString()) info.notes = root["notes"].asWString();
    else if (root["changelog"].isString()) info.notes = root["changelog"].asWString();
    if (root["publishedAt"].isString()) info.publishedAt = root["publishedAt"].asWString();
    if (root["minVersion"].isString()) info.minVersion = trimCopy(root["minVersion"].asWString());

    if (!IsValidVersion(info.version)) {
        error = L"Version invalide dans le manifeste";
        return false;
    }
    if (info.channel.empty()) info.channel = L"stable";
    if (!parseAsset(root["launcher"], info.launcher, L"launcher", error)) return false;
    if (root["updater"].isObject()) {
        if (!parseAsset(root["updater"], info.updater, L"updater", error)) return false;
    }
    if (!info.launcher.valid()) {
        error = L"Asset lanceur invalide";
        return false;
    }
    out = std::move(info);
    return true;
}

std::wstring ManifestUrl() {
    if (const wchar_t* env = _wgetenv(L"AURORA_UPDATE_MANIFEST")) {
        const std::wstring value = trimCopy(env);
        if (!value.empty()) return value;
    }
    // Parametre anti-cache local. GitHub Raw peut toutefois conserver le
    // manifeste jusqu'a cinq minutes : la verification suivante le rafraichit.
    return std::wstring(AURORA_UPDATE_MANIFEST_URL) + L"?t=" + std::to_wstring(GetTickCount64());
}

bool IsConfigured() {
    const std::wstring url = ManifestUrl();
    return !url.empty() && url.find(L"OWNER/REPO") == std::wstring::npos;
}

bool FetchManifest(const std::wstring& source, Info& out, std::wstring& error) {
    if (source.empty()) {
        error = L"Source de mise à jour non configurée";
        return false;
    }
    if (!IsConfigured() && source == AURORA_UPDATE_MANIFEST_URL) {
        error = L"Source de mise à jour non configurée (OWNER/REPO)";
        return false;
    }

    std::string body;
    if (isLocalSource(source)) {
        if (!readFileText(localPathFromSource(source), body)) {
            error = L"Manifeste introuvable : " + source;
            return false;
        }
    } else {
        if (!isAllowedRemoteUrl(source)) {
            error = L"URL de manifeste refusée (HTTPS requis)";
            return false;
        }
        std::wstring httpError;
        if (!httpGet(source, body, httpError,
                     { { L"User-Agent", L"AuroraLauncher/" AURORA_VERSION_W L" (Windows)" } })) {
            error = httpError.empty() ? L"Impossible de contacter le serveur de mise à jour"
                                      : httpError;
            return false;
        }
    }
    return ParseManifest(body, out, error);
}

bool DownloadAsset(const Asset& asset, const fs::path& destination,
                   const HttpProgress& progress, const HttpCancel& cancel,
                   std::wstring& error) {
    if (!asset.valid()) {
        error = L"Asset de mise à jour invalide";
        return false;
    }
    ensureDir(destination.parent_path());

    if (isLocalSource(asset.url)) {
        const fs::path source = localPathFromSource(asset.url);
        if (!pathExists(source) || isDirectory(source)) {
            error = L"Fichier source introuvable : " + source.wstring();
            return false;
        }
        if (cancel && cancel()) {
            error = L"Annulé";
            return false;
        }
        const fs::path part = fs::path(destination.wstring() + L".part");
        removeFile(part);
        if (progress) progress(0, asset.size);
        if (!copyFile(source, part)) {
            error = L"Impossible de copier " + source.filename().wstring();
            return false;
        }
        if (!verifyLocalFile(part, asset, error)) {
            removeFile(part);
            return false;
        }
        removeFile(destination);
        if (!renamePath(part, destination)) {
            error = L"Impossible de finaliser " + destination.filename().wstring();
            removeFile(part);
            return false;
        }
        if (progress) progress(asset.size, asset.size);
        return true;
    }

    if (!isAllowedRemoteUrl(asset.url)) {
        error = L"URL de téléchargement refusée (HTTPS requis)";
        return false;
    }
    return downloadFile(asset.url, destination, L"", asset.size, progress, cancel, error, true,
                        asset.sha256);
}

bool LaunchUpdaterAndExit(const fs::path& targetExe, const fs::path& stagedExe,
                          const fs::path& installedUpdater, const std::wstring& oldVersion,
                          std::wstring& error) {
    if (!pathExists(targetExe)) {
        error = L"Lanceur introuvable : " + targetExe.wstring();
        return false;
    }
    if (!pathExists(stagedExe)) {
        error = L"Mise à jour non téléchargée";
        return false;
    }
    if (!pathExists(installedUpdater)) {
        error = L"AuroraUpdater.exe est absent du dossier du lanceur";
        return false;
    }

    std::wstring cmd = quoteArg(installedUpdater.wstring());
    cmd += L" --pid " + std::to_wstring((unsigned long)GetCurrentProcessId());
    cmd += L" --target " + quoteArg(targetExe.wstring());
    cmd += L" --source " + quoteArg(stagedExe.wstring());
    cmd += L" --updated-from " + quoteArg(oldVersion);
    cmd += L" --restart " + quoteArg(targetExe.wstring());

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');
    const std::wstring workDir = targetExe.parent_path().wstring();
    if (!CreateProcessW(installedUpdater.c_str(), mutableCmd.data(), nullptr, nullptr, FALSE,
                        CREATE_UNICODE_ENVIRONMENT, nullptr, workDir.empty() ? nullptr : workDir.c_str(),
                        &si, &pi)) {
        error = formatW(L"Impossible de lancer AuroraUpdater (erreur %lu)", GetLastError());
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

} // namespace aurora::update
