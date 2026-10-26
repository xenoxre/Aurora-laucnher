#include "ui/imgui_bridge.h"

#include "app.h"
#include "core/log.h"
#include "core/util.h"
#include "imgui.h"
#include "update/updater.h"

using namespace aurora;

namespace aurora::ui {

Bridge::Bridge(App* app) : app_(app) {
    refresh();
}

Bridge::~Bridge() = default;

Config& Bridge::config() {
    return app_->config_;
}

const VersionEntry* Bridge::findManifestVersion(const std::wstring& id) const {
    for (auto& v : state_.manifest.versions) {
        if (v.id == id) return &v;
    }
    return nullptr;
}

bool Bridge::isInstalled(const std::wstring& versionId) const {
    if (!app_) return false;
    return isVersionInstalled(app_->paths_, versionId);
}

bool Bridge::isBusyVersion(const std::wstring& versionId) const {
    if (!app_) return false;
    return app_->busyVersions_.count(versionId) > 0;
}

std::filesystem::path Bridge::dataRoot() const {
    if (!app_) return {};
    return app_->paths_.root;
}

void Bridge::refresh() {
    if (!app_) return;
    std::lock_guard<std::mutex> lock(mutex_);

    state_.instances = app_->instances_;
    state_.selectedInstanceId = app_->config_.selectedInstance;

    state_.manifest = app_->manifest_;
    state_.manifestLoaded = app_->manifestLoaded_;
    state_.manifestLoading = app_->manifestLoading_;
    state_.manifestError = app_->manifestError_;

    state_.modsQuery = app_->modsQuery_;
    state_.modsInstanceId = app_->modsInstanceId();
    state_.modHits = app_->modHits_;
    state_.modTotal = app_->modTotal_;
    state_.modsLoading = app_->modsLoading_;
    state_.modsError = app_->modsError_;

    state_.autoScroll = app_->autoScroll_;
    state_.logFilter = app_->logFilter_;
    state_.logs = app_->logCache_;

    state_.pingAddress = app_->pingAddress_;
    state_.pingStatus = app_->pingStatus_;
    state_.pingLoading = app_->pingLoading_;
    state_.pingError = app_->pingError_;

    state_.javaList = app_->javaList_;
    state_.javaLoading = app_->javaLoading_;

    state_.gameRunning = app_->game_ && app_->game_->running();
    state_.gameInstance = app_->gameInstance_;

    state_.localServerRunning = app_->localServer_ && app_->localServer_->running();
    state_.localServerName = app_->localServerName_;

    state_.updateConfigured = update::IsConfigured();
    state_.updateChecking = app_->updateChecking_;
    state_.updateAvailable = app_->updateAvailable_;
    state_.updateDownloading = app_->updateDownloading_;
    state_.updateReady = app_->updateReady_;
    state_.updatePromptOpen = app_->updatePromptOpen_;
    state_.updateSkipped = app_->updateSkipped();
    state_.updateVersion = app_->updateInfo_.version;
    state_.updateNotes = app_->updateInfo_.notes;
    state_.updateStatus = app_->updateStatus_;
    state_.updateError = app_->updateError_;
    state_.updateProgress = app_->updateProgress_.load();

    state_.jobStatuses.clear();
    for (auto& j : app_->jobs_.infos()) state_.jobStatuses.push_back(j.status);

    state_.toasts.clear();
    state_.toastError = false;
    for (auto& t : app_->toasts_) {
        state_.toasts.push_back(t.text);
        if (t.error) state_.toastError = true;
    }

    {
        std::lock_guard<std::mutex> ms(app_->ms_.mutex);
        state_.msRunning = app_->ms_.running;
        state_.msStatus = app_->ms_.status;
        if (state_.msStatus.empty()) state_.msStatus = app_->ms_.error;
        if (state_.msStatus.empty() && !app_->ms_.userCode.empty()) {
            state_.msStatus = L"Code : " + app_->ms_.userCode + L" - " + app_->ms_.verificationUri;
        }
    }
}

void Bridge::tick() {
    if (!app_) return;
    app_->needsFrames();
    double now = ImGui::GetTime();
    if (now - lastRefresh_ > 0.2) {
        refresh();
        lastRefresh_ = now;
    }
}

void Bridge::navigate(const wchar_t* page) {
    if (!app_) return;
    if (wcscmp(page, L"console") == 0) app_->page_ = Page::Console;
    else if (wcscmp(page, L"versions") == 0) app_->page_ = Page::Versions;
    else if (wcscmp(page, L"mods") == 0) app_->page_ = Page::Mods;
    else if (wcscmp(page, L"servers") == 0) app_->page_ = Page::Servers;
    else if (wcscmp(page, L"settings") == 0) app_->page_ = Page::Settings;
    else app_->page_ = Page::Play;
    app_->navigate(app_->page_);
    refresh();
}

void Bridge::selectInstance(const std::wstring& id) {
    if (!app_) return;
    app_->config_.selectedInstance = id;
    state_.selectedInstanceId = id;
    app_->saveConfigNow();
}

void Bridge::launchSelected() {
    if (!app_) return;
    Instance* inst = app_->currentInstance();
    if (!inst) {
        app_->notify(L"Aucune instance sÃ©lectionnÃ©e", true);
        return;
    }
    app_->launchInstance(*inst, L"", 0);
}

void Bridge::createInstance(const std::wstring& name, const std::wstring& version, int loader) {
    if (!app_) return;
    app_->newName_ = name.empty() ? L"Nouvelle instance" : name;
    // App::createInstanceFromDialog() indexes the full manifest, not the
    // release-only list, so resolve the index against manifest_.versions.
    int idx = 0;
    for (size_t i = 0; i < app_->manifest_.versions.size(); ++i) {
        if (app_->manifest_.versions[i].id == version) { idx = (int)i; break; }
    }
    app_->newVersionIndex_ = idx;
    app_->newLoader_ = loader;
    app_->createInstanceFromDialog();
    refresh();
}

void Bridge::installVersion(const std::wstring& version) {
    if (!app_) return;
    app_->installVersion(version);
}

void Bridge::openDataFolder() {
    if (app_) openInExplorer(app_->paths_.root);
}

void Bridge::uninstallVersion(const std::wstring& version) {
    if (!app_) return;
    app_->uninstallVersion(version);
}

void Bridge::deleteInstance(const std::wstring& id) {
    if (!app_) return;
    std::wstring error;
    if (aurora::deleteInstance(app_->paths_, id, error)) {
        if (app_->config_.selectedInstance == id) app_->config_.selectedInstance.clear();
        app_->refreshInstances();
        app_->saveConfigNow();
        app_->notify(L"Instance supprimée", false);
    } else {
        app_->notify(error.empty() ? L"Suppression impossible" : error, true);
    }
    refresh();
}



void Bridge::playVersion(const std::wstring& version){ if(!app_)return; app_->playVersion(version); }
void Bridge::refreshManifest(){ if(!app_)return; app_->refreshManifest(true); }
void Bridge::searchMods(const std::wstring& query,bool reset){ if(!app_)return; app_->modsQuery_=query; app_->searchMods(reset); }
void Bridge::installMod(const ModHit& hit){ if(!app_)return; app_->installMod(hit); }
void Bridge::pingServer(const std::wstring& addr){ if(!app_)return; app_->pingAddress_=addr; app_->pingServerAddress(addr); }
void Bridge::joinServer(const std::wstring& addr){ if(!app_)return; app_->joinServer(addr); }
void Bridge::startLocalServer(const LocalServerConfig& cfg){ if(!app_)return; app_->startLocalServer(cfg); }
void Bridge::stopLocalServer(){ if(!app_)return; app_->stopLocalServer(); }
void Bridge::addOfflineAccount(const std::wstring& name){ if(!app_)return; app_->offlineName_=name; app_->addOfflineAccount(); refresh(); }
void Bridge::removeAccount(int index){ if(!app_)return; app_->removeAccount(index); refresh(); }
void Bridge::startMicrosoftLogin(){ if(!app_)return; app_->startMicrosoftLogin(); }
void Bridge::cancelMicrosoftLogin(){ if(!app_)return; app_->cancelMicrosoftLogin(); }
void Bridge::detectJava(){ if(!app_)return; app_->detectJava(); }
void Bridge::clearLogs(){ Logger::instance().clearUi(); refresh(); }
void Bridge::setAutoScroll(bool on){ if(!app_)return; app_->autoScroll_=on; state_.autoScroll=on; }
void Bridge::setLogFilter(int filter){ if(!app_)return; app_->logFilter_=filter; state_.logFilter=filter; }
void Bridge::setModsInstance(const std::wstring& id){ if(!app_)return; app_->modsInstanceId_=id; state_.modsInstanceId=id; }
void Bridge::saveConfig(){ if(!app_)return; app_->saveConfigNow(); refresh(); }
void Bridge::openFolder(const std::filesystem::path& p){ openInExplorer(p); }
void Bridge::createLocalServer(const std::wstring& name,const std::wstring& version,int port,int ram){ if(!app_)return; std::wstring nn=name; if(nn.empty()) nn=L"Serveur"; std::wstring nv=version; if(nv.empty()) nv=L"1.21.1"; app_->serverName_=nn; app_->serverVersion_=nv; app_->serverPort_=std::to_wstring(port); app_->serverRam_=ram; app_->createServerFromDialog(); }

void Bridge::checkUpdates(bool manual) { if (!app_) return; app_->checkForUpdates(manual); refresh(); }
void Bridge::downloadUpdate() { if (!app_) return; app_->downloadUpdate(); refresh(); }
void Bridge::cancelUpdate() { if (!app_) return; app_->cancelUpdate(); refresh(); }
void Bridge::installUpdate() { if (!app_) return; app_->installUpdate(); }
void Bridge::dismissUpdatePrompt() { if (!app_) return; app_->dismissUpdatePrompt(); refresh(); }
void Bridge::skipUpdateVersion() { if (!app_) return; app_->skipUpdateVersion(); refresh(); }
void Bridge::setCheckUpdates(bool on) { if (!app_) return; app_->config_.checkUpdates = on; app_->saveConfigNow(); refresh(); }
} // namespace aurora::ui
