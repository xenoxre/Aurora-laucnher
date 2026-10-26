#pragma once

#include <mutex>
#include <string>
#include <vector>

#include "core/config.h"
#include "core/instance.h"
#include "core/java.h"
#include "core/log.h"
#include "core/meta.h"
#include "core/modrinth.h"
#include "core/server.h"

namespace aurora {
class App;
}

namespace aurora::ui {

struct BridgeState {
    std::vector<Instance> instances;
    std::wstring selectedInstanceId;

    Manifest manifest;
    bool manifestLoaded = false;
    bool manifestLoading = false;
    std::wstring manifestError;

    std::vector<ModHit> modHits;
    int modTotal = 0;
    bool modsLoading = false;
    std::wstring modsQuery;
    std::wstring modsInstanceId;
    std::wstring modsError;

    std::vector<LogEntry> logs;
    int logFilter = 0;
    bool autoScroll = true;

    ServerStatus pingStatus;
    bool pingLoading = false;
    std::wstring pingAddress;
    std::wstring pingError;

    std::vector<JavaInfo> javaList;
    bool javaLoading = false;

    bool gameRunning = false;
    std::wstring gameInstance;

    bool localServerRunning = false;
    std::wstring localServerName;

    // Mises a jour
    bool updateConfigured = false;
    bool updateChecking = false;
    bool updateAvailable = false;
    bool updateDownloading = false;
    bool updateReady = false;
    bool updatePromptOpen = false;
    bool updateSkipped = false;
    std::wstring updateVersion;
    std::wstring updateNotes;
    std::wstring updateStatus;
    std::wstring updateError;
    float updateProgress = 0.0f;

    std::vector<std::wstring> jobStatuses;
    std::vector<std::wstring> toasts;
    bool toastError = false;

    std::wstring msStatus;
    bool msRunning = false;
};

class Bridge {
public:
    explicit Bridge(App* app);
    ~Bridge();

    void tick();
    void refresh();

    BridgeState& state() { return state_; }
    Config& config();
    const VersionEntry* findManifestVersion(const std::wstring& id) const;
    bool isInstalled(const std::wstring& versionId) const;
    bool isBusyVersion(const std::wstring& versionId) const;
    std::filesystem::path dataRoot() const;

    void navigate(const wchar_t* page);
    void selectInstance(const std::wstring& id);
    void launchSelected();
    void createInstance(const std::wstring& name, const std::wstring& version, int loader);
    void installVersion(const std::wstring& version);
    void uninstallVersion(const std::wstring& version);
    void deleteInstance(const std::wstring& id);
    void playVersion(const std::wstring& version);
    void refreshManifest();
    void searchMods(const std::wstring& query, bool reset);
    void installMod(const ModHit& hit);
    void pingServer(const std::wstring& addr);
    void joinServer(const std::wstring& addr);
    void createLocalServer(const std::wstring& name, const std::wstring& version, int port, int ram);
    void startLocalServer(const LocalServerConfig& cfg);
    void stopLocalServer();
    void addOfflineAccount(const std::wstring& name);
    void removeAccount(int index);
    void startMicrosoftLogin();
    void cancelMicrosoftLogin();
    void detectJava();
    void checkUpdates(bool manual);
    void downloadUpdate();
    void cancelUpdate();
    void installUpdate();
    void dismissUpdatePrompt();
    void skipUpdateVersion();
    void setCheckUpdates(bool on);
    void clearLogs();
    void setAutoScroll(bool on);
    void setLogFilter(int filter);
    void setModsInstance(const std::wstring& id);
    void saveConfig();
    void openFolder(const std::filesystem::path& p);
    void openDataFolder();

private:
    App* app_ = nullptr;
    BridgeState state_;
    std::mutex mutex_;
    double lastRefresh_ = 0.0;
};

} // namespace aurora::ui
