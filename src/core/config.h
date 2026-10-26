#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace aurora {

struct Paths {
    std::filesystem::path root;
    std::filesystem::path versions;
    std::filesystem::path libraries;
    std::filesystem::path assets;
    std::filesystem::path assetIndexes;
    std::filesystem::path assetObjects;
    std::filesystem::path instances;
    std::filesystem::path runtime;
    std::filesystem::path meta;
    std::filesystem::path logs;
    std::filesystem::path cache;
    std::filesystem::path serversDir;

    void init(const std::filesystem::path& dataRoot);
    std::filesystem::path versionDir(const std::wstring& id) const;
    std::filesystem::path instanceDir(const std::wstring& id) const;
    std::filesystem::path modsDir(const std::wstring& instanceId) const;
    std::filesystem::path configFile() const;
};

struct Account {
    std::wstring type = L"offline"; // offline | microsoft
    std::wstring name;
    std::wstring uuid;
    std::wstring accessToken;
    std::wstring refreshToken;
    std::wstring xuid;
    int64_t expiresAt = 0;

    bool isMicrosoft() const { return type == L"microsoft"; }
    bool isOffline() const { return !isMicrosoft(); }
};

struct LocalServerConfig {
    std::wstring name;
    std::wstring version;
    std::wstring dir; // folder name under <root>/servers
    int port = 25565;
    int ramMb = 2048;
    bool onlineMode = false;
};

struct Config {
    std::vector<Account> accounts;
    int accountIndex = 0;
    std::wstring selectedInstance;
    int ramMb = 4096;
    std::wstring javaPath;
    std::wstring jvmArgs;
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
    bool closeOnLaunch = false;
    bool verbose = false;
    std::wstring msClientId;
    int windowW = 1220;
    int windowH = 800;
    int windowX = -1;
    int windowY = -1;
    bool windowMaximized = false;
    std::wstring lastVersionFilter = L"release";
    std::wstring modsQuery;
    std::vector<LocalServerConfig> localServers;
    bool autoScrollConsole = true;
    bool showSnapshots = false;
    bool checkUpdates = true;
    std::wstring skippedUpdateVersion;

    Account& activeAccount();
    const Account& activeAccount() const;
    void ensureAccount();
};

bool loadConfig(const Paths& paths, Config& config, std::wstring& error);
bool saveConfig(const Paths& paths, const Config& config, std::wstring& error);

} // namespace aurora
