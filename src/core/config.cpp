#include "config.h"
#include "json.h"
#include "log.h"
#include "util.h"

#include <algorithm>

namespace aurora {

void Paths::init(const fs::path& dataRoot) {
    root = dataRoot;
    versions = root / L"versions";
    libraries = root / L"libraries";
    assets = root / L"assets";
    assetIndexes = assets / L"indexes";
    assetObjects = assets / L"objects";
    instances = root / L"instances";
    runtime = root / L"runtime";
    meta = root / L"meta";
    logs = root / L"logs";
    cache = root / L"cache";
    serversDir = root / L"servers";

    ensureDir(root);
    ensureDir(versions);
    ensureDir(libraries);
    ensureDir(assets);
    ensureDir(assetIndexes);
    ensureDir(assetObjects);
    ensureDir(instances);
    ensureDir(runtime);
    ensureDir(meta);
    ensureDir(logs);
    ensureDir(cache);
    ensureDir(serversDir);
}

fs::path Paths::versionDir(const std::wstring& id) const { return versions / id; }
fs::path Paths::instanceDir(const std::wstring& id) const { return instances / id; }
fs::path Paths::modsDir(const std::wstring& instanceId) const { return instanceDir(instanceId) / L"mods"; }
fs::path Paths::configFile() const { return root / L"config.json"; }

void Config::ensureAccount() {
    if (accounts.empty()) {
        Account a;
        a.type = L"offline";
        a.name = L"Joueur";
        a.uuid = offlineUuid(a.name);
        accounts.push_back(a);
    }
    if (accountIndex < 0 || accountIndex >= (int)accounts.size()) accountIndex = 0;
}

Account& Config::activeAccount() {
    ensureAccount();
    return accounts[(size_t)accountIndex];
}

const Account& Config::activeAccount() const {
    static Account fallback;
    if (accounts.empty()) return fallback;
    int idx = accountIndex;
    if (idx < 0 || idx >= (int)accounts.size()) idx = 0;
    return accounts[(size_t)idx];
}

static Json accountToJson(const Account& a) {
    Json j = Json::object();
    j.set("type", a.type);
    j.set("name", a.name);
    j.set("uuid", a.uuid);
    j.set("accessToken", a.accessToken);
    j.set("refreshToken", a.refreshToken);
    j.set("xuid", a.xuid);
    j.set("expiresAt", (long long)a.expiresAt);
    return j;
}

static Account accountFromJson(const Json& j) {
    Account a;
    a.type = j["type"].asWString(L"offline");
    a.name = j["name"].asWString(L"Joueur");
    a.uuid = j["uuid"].asWString();
    a.accessToken = j["accessToken"].asWString();
    a.refreshToken = j["refreshToken"].asWString();
    a.xuid = j["xuid"].asWString();
    a.expiresAt = j["expiresAt"].asInt(0);
    if (a.uuid.empty()) a.uuid = offlineUuid(a.name);
    return a;
}

static Json configToJson(const Config& c) {
    Json j = Json::object();
    Json acc = Json::array();
    for (auto& a : c.accounts) acc.push(accountToJson(a));
    j.set("accounts", acc);
    j.set("accountIndex", c.accountIndex);
    j.set("selectedInstance", c.selectedInstance);
    j.set("ramMb", c.ramMb);
    j.set("javaPath", c.javaPath);
    j.set("jvmArgs", c.jvmArgs);
    j.set("width", c.width);
    j.set("height", c.height);
    j.set("fullscreen", c.fullscreen);
    j.set("closeOnLaunch", c.closeOnLaunch);
    j.set("verbose", c.verbose);
    j.set("msClientId", c.msClientId);
    j.set("windowW", c.windowW);
    j.set("windowH", c.windowH);
    j.set("windowX", c.windowX);
    j.set("windowY", c.windowY);
    j.set("windowMaximized", c.windowMaximized);
    j.set("lastVersionFilter", c.lastVersionFilter);
    j.set("modsQuery", c.modsQuery);
    j.set("autoScrollConsole", c.autoScrollConsole);
    j.set("showSnapshots", c.showSnapshots);
    j.set("checkUpdates", c.checkUpdates);
    j.set("skippedUpdateVersion", c.skippedUpdateVersion);
    Json servers = Json::array();
    for (auto& s : c.localServers) {
        Json o = Json::object();
        o.set("name", s.name);
        o.set("version", s.version);
        o.set("dir", s.dir);
        o.set("port", s.port);
        o.set("ramMb", s.ramMb);
        o.set("onlineMode", s.onlineMode);
        servers.push(o);
    }
    j.set("localServers", servers);
    return j;
}

static void configFromJson(const Json& j, Config& c) {
    c.accounts.clear();
    for (auto& a : j["accounts"].items()) c.accounts.push_back(accountFromJson(a));
    c.accountIndex = (int)j["accountIndex"].asInt(0);
    c.selectedInstance = j["selectedInstance"].asWString();
    c.ramMb = (int)j["ramMb"].asInt(4096);
    c.javaPath = j["javaPath"].asWString();
    c.jvmArgs = j["jvmArgs"].asWString();
    c.width = (int)j["width"].asInt(1280);
    c.height = (int)j["height"].asInt(720);
    c.fullscreen = j["fullscreen"].asBool(false);
    c.closeOnLaunch = j["closeOnLaunch"].asBool(false);
    c.verbose = j["verbose"].asBool(false);
    c.msClientId = j["msClientId"].asWString();
    c.windowW = (int)j["windowW"].asInt(1220);
    c.windowH = (int)j["windowH"].asInt(800);
    c.windowX = (int)j["windowX"].asInt(-1);
    c.windowY = (int)j["windowY"].asInt(-1);
    c.windowMaximized = j["windowMaximized"].asBool(false);
    c.lastVersionFilter = j["lastVersionFilter"].asWString(L"release");
    c.modsQuery = j["modsQuery"].asWString();
    c.autoScrollConsole = j["autoScrollConsole"].asBool(true);
    c.showSnapshots = j["showSnapshots"].asBool(false);
    c.checkUpdates = j["checkUpdates"].asBool(true);
    c.skippedUpdateVersion = j["skippedUpdateVersion"].asWString();
    c.localServers.clear();
    for (auto& s : j["localServers"].items()) {
        LocalServerConfig l;
        l.name = s["name"].asWString();
        l.version = s["version"].asWString();
        l.dir = s["dir"].asWString();
        l.port = (int)s["port"].asInt(25565);
        l.ramMb = (int)s["ramMb"].asInt(2048);
        l.onlineMode = s["onlineMode"].asBool(false);
        if (l.ramMb < 512) l.ramMb = 512;
        if (l.ramMb > 65536) l.ramMb = 65536;
        if (!l.name.empty() && !l.dir.empty()) c.localServers.push_back(l);
    }
    if (c.ramMb < 512) c.ramMb = 512;
    if (c.ramMb > 131072) c.ramMb = 131072;
    if (c.width < 320) c.width = 1280;
    if (c.height < 240) c.height = 720;
    if (c.windowW < 1024) c.windowW = 1220;
    if (c.windowH < 640) c.windowH = 800;
    c.ensureAccount();
}

bool loadConfig(const Paths& paths, Config& config, std::wstring& error) {
    fs::path file = paths.configFile();
    Config def;
    def.ensureAccount();
    if (!pathExists(file)) {
        config = def;
        saveConfig(paths, config, error);
        return true;
    }
    std::string text;
    if (!readFileText(file, text)) {
        error = L"Lecture de config.json impossible";
        config = def;
        return false;
    }
    Json j;
    std::string perr;
    if (!Json::parse(text, j, &perr)) {
        error = L"config.json corrompu : " + utf8ToWide(perr);
        fs::path backup = file;
        backup += L".broken";
        std::error_code ec;
        fs::copy_file(file, backup, fs::copy_options::overwrite_existing, ec);
        config = def;
        return false;
    }
    configFromJson(j, config);
    return true;
}

bool saveConfig(const Paths& paths, const Config& config, std::wstring& error) {
    Json j = configToJson(config);
    if (!writeFileTextAtomic(paths.configFile(), j.dump(2))) {
        error = L"Écriture de config.json impossible";
        return false;
    }
    return true;
}

} // namespace aurora
