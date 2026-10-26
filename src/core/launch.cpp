#include "launch.h"
#include "log.h"
#include "util.h"
#include "version.h"

#include <algorithm>
#include <map>
#include <set>

namespace aurora {

static std::vector<std::wstring> splitQuoted(const std::wstring& line) {
    std::vector<std::wstring> out;
    std::wstring cur;
    bool quote = false;
    for (size_t i = 0; i < line.size(); i++) {
        wchar_t c = line[i];
        if (c == L'"') { quote = !quote; continue; }
        if (iswspace(c) && !quote) {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

static std::wstring substitute(std::wstring text,
                               const std::map<std::wstring, std::wstring>& vars,
                               std::set<std::wstring>* unknown) {
    for (auto& kv : vars) text = replaceAll(text, L"${" + kv.first + L"}", kv.second);
    size_t pos = 0;
    while ((pos = text.find(L"${", pos)) != std::wstring::npos) {
        size_t end = text.find(L'}', pos + 2);
        if (end == std::wstring::npos) break;
        if (unknown) unknown->insert(text.substr(pos + 2, end - pos - 2));
        text.erase(pos, end - pos + 1);
    }
    return text;
}

static bool containsToken(const std::vector<std::wstring>& args, const std::wstring& token) {
    return std::find(args.begin(), args.end(), token) != args.end();
}

bool buildLaunchCommand(const LaunchRequest& req, std::vector<std::wstring>& args,
                        std::wstring& error) {
    args.clear();
    if (!req.paths || !req.version || !req.config || !req.instance) {
        error = L"Paramètres de lancement incomplets";
        return false;
    }
    const ResolvedVersion& ver = *req.version;
    const Config& cfg = *req.config;
    const Paths& paths = *req.paths;

    fs::path nativesDir = paths.versionDir(ver.id) / L"natives";
    fs::path clientJar = paths.versionDir(ver.jarId) / (ver.jarId + L".jar");
    if (!pathExists(clientJar)) {
        error = L"Jar client manquant : " + clientJar.wstring();
        return false;
    }

    std::vector<std::wstring> missing;
    std::wstring classpath;
    for (auto& lib : ver.libraries) {
        if (lib.isNative) continue;
        fs::path p = paths.libraries / lib.path;
        if (!pathExists(p)) { if (missing.size() < 8) missing.push_back(lib.path); continue; }
        if (!classpath.empty()) classpath += L";";
        classpath += p.wstring();
    }
    if (!classpath.empty()) classpath += L";";
    classpath += clientJar.wstring();
    if (!missing.empty()) {
        error = L"Bibliothèques manquantes — réinstallez la version. Ex. : " + missing[0];
        return false;
    }
    if (!isDirectory(nativesDir)) {
        ensureDir(nativesDir);
    }
    fs::path assetsDir = assetsDirectory(paths, ver);
    if (!pathExists(paths.assetIndexes / (ver.assetsId + L".json"))) {
        error = L"Index d'assets manquant — réinstallez la version.";
        return false;
    }

    std::map<std::wstring, std::wstring> vars;
    vars[L"auth_player_name"] = req.accountName.empty() ? L"Joueur" : req.accountName;
    vars[L"auth_uuid"] = req.accountUuid;
    vars[L"auth_access_token"] = req.accessToken.empty() ? L"0" : req.accessToken;
    vars[L"auth_session"] = L"token:" + vars[L"auth_access_token"] + L":" + req.accountUuid;
    vars[L"user_type"] = req.userType;
    vars[L"user_properties"] = L"{}";
    vars[L"auth_xuid"] = req.xuid;
    vars[L"clientid"] = req.clientId;
    vars[L"version_name"] = ver.id;
    vars[L"version_type"] = ver.type;
    vars[L"game_directory"] = req.instance->dir.wstring();
    vars[L"assets_root"] = assetsDir.wstring();
    vars[L"assets_index_name"] = ver.assetsId;
    vars[L"game_assets"] = assetsDir.wstring();
    vars[L"natives_directory"] = nativesDir.wstring();
    vars[L"library_directory"] = paths.libraries.wstring();
    vars[L"classpath"] = classpath;
    vars[L"classpath_separator"] = L";";
    vars[L"launcher_name"] = L"AuroraLauncher";
    vars[L"launcher_version"] = AURORA_VERSION_W;
    vars[L"resolution_width"] = std::to_wstring(cfg.width);
    vars[L"resolution_height"] = std::to_wstring(cfg.height);
    vars[L"profile_name"] = req.accountName;
    vars[L"quickPlayPath"] = L"";
    vars[L"quickPlaySingleplayer"] = L"";
    vars[L"quickPlayMultiplayer"] = L"";
    vars[L"quickPlayRealms"] = L"";

    std::set<std::wstring> unknown;
    args.push_back(req.javaPath);
    int ram = std::max(512, cfg.ramMb);
    if (ram > 131072) ram = 131072;
    if (ram < 512) ram = 512;
    args.push_back(L"-Xmx" + std::to_wstring(ram) + L"M");
    int xms = std::max(512, ram / 4);
    if (xms > ram) xms = ram;
    args.push_back(L"-Xms" + std::to_wstring(xms) + L"M");
    args.push_back(L"-XX:+UseG1GC");
    args.push_back(L"-XX:+ParallelRefProcEnabled");
    args.push_back(L"-XX:MaxGCPauseMillis=200");
    args.push_back(L"-Djava.library.path=" + nativesDir.wstring());
    args.push_back(L"-Dminecraft.launcher.brand=AuroraLauncher");
    args.push_back(L"-Dminecraft.launcher.version=" AURORA_VERSION_W);
    args.push_back(L"-Dlog4j2.formatMsgNoLookups=true");
    if (ver.javaMajor >= 9) args.push_back(L"-Dfile.encoding=UTF-8");

    bool hasCp = false;
    for (auto& a : ver.jvmArgs) {
        std::wstring s = substitute(a, vars, &unknown);
        if (s.rfind(L"-Xmx", 0) == 0 || s.rfind(L"-Xms", 0) == 0) continue;
        if (s == L"-cp" || s == L"-classpath") hasCp = true;
        args.push_back(s);
    }
    if (!hasCp) {
        args.push_back(L"-cp");
        args.push_back(classpath);
    }
    if (!ver.logArg.empty()) {
        std::wstring name;
        if (!ver.logFileId.empty()) name = ver.logFileId;
        else {
            size_t slash = ver.logFileUrl.find_last_of(L'/');
            name = slash == std::wstring::npos ? L"log4j.xml" : ver.logFileUrl.substr(slash + 1);
        }
        for (auto& c : name) if (c == L'/' || c == L'\\' || c == L':') c = L'_';
        fs::path logPath = paths.assets / L"log_configs" / name;
        if (pathExists(logPath)) {
            std::wstring a = ver.logArg;
            a = replaceAll(a, L"${path}", logPath.wstring());
            args.push_back(substitute(a, vars, &unknown));
        }
    }
    // Append user JVM args last, but skip duplicates of Xmx/Xms we just set
    {
        std::vector<std::wstring> userArgs = splitQuoted(cfg.jvmArgs);
        for (auto& ua : userArgs) {
            if (ua.rfind(L"-Xmx", 0) == 0 || ua.rfind(L"-Xms", 0) == 0) continue;
            args.push_back(ua);
        }
    }

    args.push_back(ver.mainClass);

    std::vector<std::wstring> game;
    for (auto& a : ver.gameArgs) game.push_back(substitute(a, vars, &unknown));
    if (!containsToken(game, L"--width")) {
        game.push_back(L"--width");
        game.push_back(std::to_wstring(cfg.width));
    }
    if (!containsToken(game, L"--height")) {
        game.push_back(L"--height");
        game.push_back(std::to_wstring(cfg.height));
    }
    if (cfg.fullscreen && !containsToken(game, L"--fullscreen")) game.push_back(L"--fullscreen");
    if (!req.serverHost.empty() && req.serverPort > 0) {
        std::wstring addr = req.serverHost + L":" + std::to_wstring(req.serverPort);
        if (ver.supportsQuickPlay) {
            if (!containsToken(game, L"--quickPlayMultiplayer")) {
                game.push_back(L"--quickPlayMultiplayer");
                game.push_back(addr);
            }
        } else {
            if (!containsToken(game, L"--server")) {
                game.push_back(L"--server");
                game.push_back(req.serverHost);
                game.push_back(L"--port");
                game.push_back(std::to_wstring(req.serverPort));
            }
        }
    }
    for (auto& g : game) args.push_back(g);
    if (!unknown.empty()) {
        std::wstring list;
        for (auto& u : unknown) { if (!list.empty()) list += L", "; list += u; }
        logDebug(L"Variables non résolues : " + list, L"Lancement");
    }
    return true;
}

} // namespace aurora
