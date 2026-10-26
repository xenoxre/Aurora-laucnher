#include "java.h"
#include "http.h"
#include "log.h"
#include "util.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <set>

namespace aurora {

static bool captureOutput(const std::wstring& commandLine, const fs::path& wd,
                          std::wstring& output, DWORD timeoutMs) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE readH = nullptr, writeH = nullptr;
    if (!CreatePipe(&readH, &writeH, &sa, 0)) return false;
    SetHandleInformation(readH, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writeH;
    si.hStdError = writeH;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cmd(commandLine.begin(), commandLine.end());
    cmd.push_back(L'\0');
    std::wstring w = wd.wstring();
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                             nullptr, w.empty() ? nullptr : w.c_str(), &si, &pi);
    CloseHandle(writeH);
    if (!ok) {
        CloseHandle(readH);
        return false;
    }
    DWORD start = GetTickCount();
    std::string bytes;
    char buf[4096];
    for (;;) {
        DWORD avail = 0;
        if (PeekNamedPipe(readH, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            DWORD got = 0;
            if (ReadFile(readH, buf, sizeof(buf), &got, nullptr) && got > 0)
                bytes.append(buf, got);
        }
        DWORD wait = WaitForSingleObject(pi.hProcess, 30);
        if (wait == WAIT_OBJECT_0) {
            while (PeekNamedPipe(readH, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
                DWORD got = 0;
                if (ReadFile(readH, buf, sizeof(buf), &got, nullptr) && got > 0)
                    bytes.append(buf, got);
                else break;
            }
            break;
        }
        if (GetTickCount() - start > timeoutMs) {
            TerminateProcess(pi.hProcess, 1);
            break;
        }
    }
    CloseHandle(readH);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    output = utf8ToWide(bytes);
    return true;
}

bool inspectJava(const std::wstring& exePath, int& major, std::wstring& versionLine) {
    major = 0;
    versionLine.clear();
    if (!pathExists(exePath)) return false;
    std::wstring out;
    if (!captureOutput(L"\"" + exePath + L"\" -version", fs::path(exePath).parent_path(), out, 10000))
        return false;
    versionLine = trim(out);
    size_t pos = out.find(L"version \"");
    if (pos == std::wstring::npos) return false;
    pos += 9;
    size_t end = out.find(L'"', pos);
    if (end == std::wstring::npos) return false;
    std::wstring ver = out.substr(pos, end - pos);
    std::vector<std::wstring> parts = split(ver, L'.');
    if (parts.empty()) return false;
    int first = (int)wcstol(parts[0].c_str(), nullptr, 10);
    if (first == 1 && parts.size() > 1) major = (int)wcstol(parts[1].c_str(), nullptr, 10);
    else major = first;
    return major > 0;
}

static void addJavaCandidate(std::vector<JavaInfo>& out, std::set<std::wstring>& seen,
                             const std::wstring& exe) {
    if (exe.empty() || !pathExists(exe)) return;
    std::error_code ec;
    fs::path canon = fs::weakly_canonical(exe, ec);
    std::wstring key = toLower((ec ? fs::path(exe) : canon).wstring());
    if (!seen.insert(key).second) return;
    int major = 0;
    std::wstring line;
    if (!inspectJava(exe, major, line)) return;
    JavaInfo info;
    info.path = exe;
    info.major = major;
    info.version = line;
    out.push_back(info);
    logDebug(L"Java détecté : " + std::to_wstring(major) + L" — " + exe, L"Java");
}

static void scanJavaDir(std::vector<JavaInfo>& out, std::set<std::wstring>& seen,
                        const fs::path& base, int depth) {
    if (depth < 0 || !isDirectory(base)) return;
    fs::path direct = base / L"bin" / L"java.exe";
    if (pathExists(direct)) addJavaCandidate(out, seen, direct.wstring());
    for (auto& d : listDirectory(base, true)) {
        scanJavaDir(out, seen, d, depth - 1);
    }
}

std::vector<JavaInfo> detectJavaInstallations(const Paths& paths, const Config& config) {
    std::vector<JavaInfo> result;
    std::set<std::wstring> seen;
    if (!config.javaPath.empty()) addJavaCandidate(result, seen, config.javaPath);

    wchar_t env[32768];
    if (GetEnvironmentVariableW(L"JAVA_HOME", env, 32767) > 0)
        addJavaCandidate(result, seen, (fs::path(env) / L"bin" / L"java.exe").wstring());

    // Launcher-managed runtimes
    for (auto& d : listDirectory(paths.runtime, true)) {
        fs::path p = d / L"bin" / L"java.exe";
        if (pathExists(p)) addJavaCandidate(result, seen, p.wstring());
        else scanJavaDir(result, seen, d, 3);
    }

    // Official launcher runtimes
    fs::path appdata = knownFolder(CSIDL_APPDATA);
    if (!appdata.empty()) scanJavaDir(result, seen, appdata / L".minecraft" / L"runtime", 5);
    fs::path local = knownFolder(CSIDL_LOCAL_APPDATA);
    if (!local.empty()) {
        scanJavaDir(result, seen, local / L"Packages" / L"Microsoft.4297127D64EC6_8wekyb3d8bbwe" / L"LocalCache" / L"Local" / L"runtime", 5);
        scanJavaDir(result, seen, local / L"Programs" / L"Eclipse Adoptium", 3);
    }

    // Common vendors
    std::vector<fs::path> bases = {
        fs::path(L"C:\\Program Files\\Java"),
        fs::path(L"C:\\Program Files\\Eclipse Adoptium"),
        fs::path(L"C:\\Program Files\\Microsoft"),
        fs::path(L"C:\\Program Files\\Zulu"),
        fs::path(L"C:\\Program Files\\Amazon Corretto"),
        fs::path(L"C:\\Program Files\\BellSoft"),
        fs::path(L"C:\\Program Files (x86)\\Java"),
    };
    for (auto& b : bases) scanJavaDir(result, seen, b, 3);

    // PATH
    wchar_t pathEnv[32768];
    if (GetEnvironmentVariableW(L"PATH", pathEnv, 32767) > 0) {
        for (auto& part : split(pathEnv, L';')) {
            if (part.empty()) continue;
            fs::path p = fs::path(part) / L"java.exe";
            if (pathExists(p)) addJavaCandidate(result, seen, p.wstring());
        }
    }

    std::sort(result.begin(), result.end(), [](const JavaInfo& a, const JavaInfo& b) {
        return a.major > b.major;
    });
    return result;
}

bool javaSatisfies(int major, int required) {
    if (required <= 8) return major >= 8;
    return major >= required;
}

bool downloadJavaRuntime(const Paths& paths, const ResolvedVersion& version, JobContext& job,
                         std::wstring& javaPath, std::wstring& error) {
    const wchar_t* allUrl =
        L"https://launchermeta.mojang.com/v1/products/java-runtime/2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json";
    fs::path cache = paths.meta / L"java_runtime_all.json";
    Json all;
    std::string text;
    std::wstring netErr;
    bool ok = false;
    if (pathExists(cache) && readFileText(cache, text)) {
        std::string perr;
        if (Json::parse(text, all, &perr)) ok = true;
    }
    if (!ok) {
        job.report(L"Récupération des runtimes Java Mojang…", 0.02f);
        if (!httpGet(allUrl, text, netErr)) {
            error = L"Runtimes Java : " + netErr;
            return false;
        }
        writeFileTextAtomic(cache, text);
        std::string perr;
        if (!Json::parse(text, all, &perr)) {
            error = L"Runtimes Java : JSON invalide";
            return false;
        }
    }
    const Json& win = all["windows-x64"];
    if (!win.isObject()) { error = L"Runtimes Java indisponibles pour Windows x64"; return false; }

    std::wstring component = version.javaComponent;
    if (component.empty()) {
        if (version.javaMajor >= 21) component = L"java-runtime-delta";
        else if (version.javaMajor >= 17) component = L"java-runtime-gamma";
        else if (version.javaMajor >= 16) component = L"java-runtime-alpha";
        else component = L"jre-legacy";
    }
    const Json* chosen = nullptr;
    long long bestReleased = -1;
    for (auto& kv : win.fields()) {
        if (utf8ToWide(kv.first) != component) continue;
        for (auto& v : kv.second.fields()) {
            long long released = v.second["version"]["released"].asInt(0);
            if (released > bestReleased) {
                bestReleased = released;
                chosen = &v.second;
            }
        }
    }
    if (!chosen) {
        error = L"Runtime Java \"" + component + L"\" introuvable chez Mojang";
        return false;
    }
    std::wstring manifestUrl = (*chosen)["manifest"]["url"].asWString();
    if (manifestUrl.empty()) { error = L"Manifest de runtime invalide"; return false; }
    fs::path manifestPath = paths.meta / (L"runtime_" + component + L".json");
    std::string manifestText;
    if (!pathExists(manifestPath)) {
        if (!httpGet(manifestUrl, manifestText, netErr)) {
            error = L"Manifest Java : " + netErr;
            return false;
        }
        writeFileTextAtomic(manifestPath, manifestText);
    } else {
        readFileText(manifestPath, manifestText);
    }
    Json manifest;
    std::string perr;
    if (!Json::parse(manifestText, manifest, &perr)) {
        error = L"Manifest Java invalide";
        return false;
    }
    fs::path root = paths.runtime / component;
    fs::path exe = root / L"bin" / L"java.exe";
    if (pathExists(root / L".complete") && pathExists(exe)) {
        javaPath = exe.wstring();
        return true;
    }
    job.report(L"Téléchargement de Java " + std::to_wstring(version.javaMajor) + L"…", 0.05f);
    std::vector<DownloadItem> files;
    for (auto& kv : manifest["files"].fields()) {
        const Json& f = kv.second;
        if (f["type"].asWString() != L"file") continue;
        const Json& raw = f["downloads"]["raw"];
        std::wstring url = raw["url"].asWString();
        if (url.empty()) continue;
        DownloadItem it;
        it.url = url;
        it.path = root / utf8ToWide(kv.first);
        it.sha1 = raw["sha1"].asWString();
        it.size = (uint64_t)raw["size"].asInt(0);
        it.verifySha = true;
        files.push_back(it);
    }
    if (files.empty()) { error = L"Runtime Java vide"; return false; }
    std::wstring dlErr;
    bool dlOk = downloadBatch(files, 8, [&]() { return job.cancelled(); },
        [&](int done, int total, uint64_t bytes, uint64_t totalBytes) {
            std::wstring status = L"Java — " + std::to_wstring(done) + L"/" + std::to_wstring(total);
            if (totalBytes) status += L" (" + humanBytes(bytes) + L" / " + humanBytes(totalBytes) + L")";
            job.report(status, 0.05f + 0.9f * (float)done / (float)std::max(1, total));
        }, dlErr);
    if (!dlOk) { error = L"Java : " + dlErr; return false; }
    if (!pathExists(exe)) {
        for (auto& d : listDirectory(root, true)) {
            for (auto& d2 : listDirectory(d, true)) {
                fs::path p = d2 / L"bin" / L"java.exe";
                if (pathExists(p)) { exe = p; break; }
            }
            if (pathExists(exe)) break;
        }
    }
    if (!pathExists(exe)) { error = L"java.exe absent après installation"; return false; }
    writeFileText(root / L".complete", "ok");
    javaPath = exe.wstring();
    logSuccess(L"Runtime Java installé : " + component, L"Java");
    return true;
}

std::wstring chooseJava(const Paths& paths, const Config& config, const ResolvedVersion& version,
                        JobContext& job, std::wstring& error, bool allowDownload) {
    int required = std::max(8, version.javaMajor);
    if (!config.javaPath.empty() && pathExists(config.javaPath)) {
        int major = 0;
        std::wstring line;
        if (inspectJava(config.javaPath, major, line)) {
            if (!javaSatisfies(major, required))
                logWarn(L"Java configuré (" + std::to_wstring(major) + L") est plus ancien que requis (" +
                        std::to_wstring(required) + L")", L"Java");
            return config.javaPath;
        }
    }
    std::vector<JavaInfo> found = detectJavaInstallations(paths, config);
    for (auto& j : found) if (j.major == required) return j.path;
    for (auto& j : found) if (javaSatisfies(j.major, required)) return j.path;

    if (!allowDownload) {
        error = L"Java " + std::to_wstring(required) + L" requis, introuvable. Installez un JDK ou activez le téléchargement automatique.";
        return {};
    }
    job.report(L"Java " + std::to_wstring(required) + L" requis — téléchargement automatique…", 0.01f);
    std::wstring path;
    if (!downloadJavaRuntime(paths, version, job, path, error)) return {};
    int major = 0;
    std::wstring line;
    if (inspectJava(path, major, line)) {
        logInfo(L"Java " + std::to_wstring(major) + L" prêt : " + path, L"Java");
    }
    return path;
}

} // namespace aurora
