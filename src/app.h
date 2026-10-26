#pragma once
// Aurora Launcher - application state, actions and page composition.
#include <windows.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "core/auth.h"
#include "core/config.h"
#include "core/http.h"
#include "core/instance.h"
#include "core/java.h"
#include "core/launch.h"
#include "core/log.h"
#include "core/meta.h"
#include "core/modrinth.h"
#include "core/process.h"
#include "core/server.h"
#include "core/tasks.h"

namespace aurora {

struct AppOptions {
    fs::path dataDir;
    bool verbose = false;
    std::wstring screenshotPath;
    std::wstring startPage;
    int screenshotDelayMs = 2200;
    bool demo = false;
    bool uiTest = false;
};

enum class Page { Play = 0, Versions, Mods, Servers, Console, Settings };

enum class Dialog {
    None,
    NewInstance,
    Accounts,
    Microsoft,
    CreateServer,
    ConfirmDeleteInstance,
    ConfirmDeleteVersion,
    About
};

struct Toast {
    std::wstring text;
    bool error = false;
    double born = 0;
};

struct MsLoginState {
    std::mutex mutex;
    std::atomic<bool> running{ false };
    std::shared_ptr<std::atomic<bool>> cancel = std::make_shared<std::atomic<bool>>(false);
    std::wstring userCode;
    std::wstring verificationUri;
    std::wstring status;
    std::wstring error;
    Account account;
    bool ready = false;
    bool done = false;
    bool ok = false;
    MsLoginState() = default;
    MsLoginState(const MsLoginState&) = delete;
    MsLoginState& operator=(const MsLoginState&) = delete;
    MsLoginState(MsLoginState&&) = delete;
    MsLoginState& operator=(MsLoginState&&) = delete;
};

class App {
public:
    App();
    ~App();

    bool init(HINSTANCE instance, const AppOptions& options, std::wstring& error);
    void shutdown();
    void tick();
    bool needsFrames();

    // Navigation / notifications ------------------------------------------------
    void navigate(Page page);
    void openDialog(Dialog dialog);
    void closeDialog();
    void notify(const std::wstring& text, bool error = false);
    void saveConfigNow();

    // Data helpers --------------------------------------------------------------
    Instance* currentInstance();
    const Instance* currentInstance() const;
    Instance* instanceForVersion(const std::wstring& versionId);
    void refreshInstances();
    void refreshManifest(bool force = false);
    std::wstring effectiveClientId() const;
    std::vector<std::wstring> versionIds() const;
    std::vector<std::wstring> releaseIds() const;
    std::wstring modsInstanceId() const;

    // Actions -------------------------------------------------------------------
    void launchInstance(const Instance& instance, std::wstring host = L"", int port = 0);
    void playVersion(const std::wstring& versionId);
    void installVersion(const std::wstring& versionId);
    void uninstallVersion(const std::wstring& versionId);
    void createInstanceFromDialog();
    void addOfflineAccount();
    void removeAccount(int index);
    void startMicrosoftLogin();
    void cancelMicrosoftLogin();
    void searchMods(bool reset);
    void installMod(const ModHit& hit);
    void pingServerAddress(const std::wstring& address);
    void joinServer(const std::wstring& address, const std::wstring& preferredVersion = L"");
    void createServerFromDialog();
    void startLocalServer(const LocalServerConfig& server);
    void stopLocalServer();
    void detectJava();

    // Pages (implemented in pages_*.cpp) ----------------------------------------


public:
    HINSTANCE instance_ = nullptr;
    AppOptions options_;
    Paths paths_;
    Config config_;
    std::vector<Instance> instances_;
    JobQueue jobs_;

    Manifest manifest_;
    bool manifestLoading_ = false;
    bool manifestLoaded_ = false;
    std::wstring manifestError_;
    double manifestFetchedAt_ = 0;

    Page page_ = Page::Play;
    Dialog dialog_ = Dialog::None;
    std::wstring lastPageName_;
    double pageOpenedAt_ = 0;
    float lastW_ = 1220, lastH_ = 800;
    double now_ = 0;
    bool shuttingDown_ = false;

    std::vector<Toast> toasts_;
    std::set<std::wstring> busyVersions_;
    std::set<std::wstring> busyInstances_;

    // Versions page
    std::wstring versionQuery_;
    int versionFilter_ = 0;
    double versionsFetchedAt_ = 0;

    // Mods page
    std::wstring modsQuery_;
    std::wstring modsInstanceId_;
    std::vector<ModHit> modHits_;
    int modTotal_ = 0;
    int modsOffset_ = 0;
    bool modsLoading_ = false;
    bool modsSearched_ = false;
    std::wstring modsError_;
    std::wstring installingMod_;

    // Servers page
    std::wstring pingAddress_;
    ServerStatus pingStatus_;
    bool pingLoading_ = false;
    std::wstring pingError_;
    double pingAt_ = 0;
    std::wstring serverName_ = L"Mon serveur";
    std::wstring serverVersion_;
    std::wstring serverPort_ = L"25565";
    int serverRam_ = 3072;
    bool serverOnline_ = false;
    Process* localServer_ = nullptr;
    std::wstring localServerName_;
    std::wstring localServerDir_;
    double localServerStartedAt_ = 0;

    // Console page
    std::vector<LogEntry> logCache_;
    double logsRefreshedAt_ = 0;
    int logFilter_ = 0;
    bool autoScroll_ = true;
    int lastLogCount_ = 0;

    // Settings page
    std::vector<JavaInfo> javaList_;
    bool javaLoading_ = false;
    bool settingsDirty_ = false;
    double settingsChangedAt_ = 0;
    int javaChoice_ = 0;
    int resolutionChoice_ = 0;
    int settingsSection_ = -1;

    // Dialog state
    std::wstring newName_ = L"Nouvelle instance";
    int newVersionIndex_ = 0;
    int newLoader_ = 0;
    std::wstring offlineName_;
    std::wstring msClientDraft_;
    MsLoginState ms_;
    std::thread msThread_;
    std::wstring pendingDeleteId_;
    std::wstring pendingDeleteName_;

    // Game process
    Process* game_ = nullptr;
    std::wstring gameInstance_;
    double gameStartedAt_ = 0;
    bool gameExitNotified_ = false;

private:
    void refreshLogs(bool force);
    void ensureSelection();
    void syncSettingsFromConfig();
    void processMicrosoftResult();
    void applyLaunch(const Instance& instance, std::wstring host, int port,
                     std::shared_ptr<ResolvedVersion> version, std::wstring javaPath,
                     bool needInstall);
};

std::wstring slugify(const std::wstring& text);
std::wstring uuidNoDashes(const std::wstring& uuid);

} // namespace aurora
