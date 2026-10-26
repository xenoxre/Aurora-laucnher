// Aurora Launcher - application logic and composition
#include "app.h"
#include "core/java.h"
#include "core/launch.h"
#include "core/log.h"
#include "core/modrinth.h"
#include "core/server.h"
#include "core/util.h"
#include "version.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <shellapi.h>
#include <shlobj.h>
#include <thread>


namespace aurora {

static double nowSeconds() {
    using clock = std::chrono::steady_clock;
    static const clock::time_point start = clock::now();
    return std::chrono::duration<double>(clock::now() - start).count();
}


std::wstring slugify(const std::wstring& text) {
    std::wstring out;
    for (wchar_t c : toLower(trim(text))) {
        if (iswalnum(c)) out += c;
        else if (c == L' ' || c == L'-' || c == L'_') out += L'-';
    }
    while (out.find(L"--") != std::wstring::npos) out = replaceAll(out, L"--", L"-");
    while (!out.empty() && out.front() == L'-') out.erase(out.begin());
    while (!out.empty() && out.back() == L'-') out.pop_back();
    if (out.empty()) return L"instance";
    return out;
}

std::wstring uuidNoDashes(const std::wstring& uuid) {
    std::wstring out;
    for (wchar_t c : uuid) if (c != L'-') out += c;
    return out;
}

App::App() = default;
App::~App() { shutdown(); }

bool App::init(HINSTANCE instance, const AppOptions& options, std::wstring& error) {
    instance_ = instance;
    options_ = options;
    paths_.init(options_.dataDir);
    installCrashHandler(paths_.logs);

    Logger::instance().init(paths_.logs / L"aurora.log", options_.verbose);
    logInfo(L"Aurora Launcher " + std::wstring(AURORA_VERSION_W) + L" démarrage", L"Lanceur");
    logInfo(L"Répertoire de données : " + paths_.root.wstring(), L"Lanceur");

    if (!loadConfig(paths_, config_, error)) {
        logWarn(L"Configuration non chargée, valeurs par défaut utilisées : " + error, L"Lanceur");
        error.clear();
    }
    config_.ensureAccount();

    syncSettingsFromConfig();
    autoScroll_ = config_.autoScrollConsole;
    modsQuery_ = config_.modsQuery;
    if (config_.lastVersionFilter == L"all") versionFilter_ = 0;
    else if (config_.lastVersionFilter == L"snapshot") versionFilter_ = 2;
    else if (config_.lastVersionFilter == L"old") versionFilter_ = 3;
    else versionFilter_ = 1;
    refreshInstances();
    refreshManifest(false);

    if (!options_.startPage.empty()) {
        std::wstring p = toLower(options_.startPage);
        if (p == L"versions") navigate(Page::Versions);
        else if (p == L"mods") navigate(Page::Mods);
        else if (p == L"servers") navigate(Page::Servers);
        else if (p == L"console") navigate(Page::Console);
        else if (p == L"settings") navigate(Page::Settings);
        else navigate(Page::Play);
    }

    msClientDraft_ = config_.msClientId;
    offlineName_ = L"Steve";
    serverVersion_ = L"1.21.1";
    serverPort_ = L"25565";
    serverName_ = L"Mon serveur";
    serverRam_ = 4096;
    return true;
}

void App::shutdown() {
    if (shuttingDown_) return;
    shuttingDown_ = true;
    if (ms_.cancel) ms_.cancel->store(true);
    if (msThread_.joinable()) msThread_.join();
    processMicrosoftResult();
    saveConfigNow();
    if (localServer_) {
        localServer_->requestStop();
        delete localServer_;
        localServer_ = nullptr;
    }
    if (game_) {
        game_->abandon();
        delete game_;
        game_ = nullptr;
    }
    jobs_.shutdown();
    Logger::instance().shutdown();
}

void App::saveConfigNow() {
    // copy UI state back to config
    config_.msClientId = msClientDraft_;
    config_.windowW = (int)std::round(lastW_);
    config_.windowH = (int)std::round(lastH_);
    config_.windowMaximized = false;
    config_.autoScrollConsole = autoScroll_;
    config_.modsQuery = modsQuery_;
    const wchar_t* filterNames[] = { L"all", L"release", L"snapshot", L"old" };
    if (versionFilter_ >= 0 && versionFilter_ < 4) config_.lastVersionFilter = filterNames[versionFilter_];
    std::wstring err;
    saveConfig(paths_, config_, err);
}

void App::navigate(Page page) {
    page_ = page;
    pageOpenedAt_ = nowSeconds();
    if (page_ == Page::Console) refreshLogs(true);
    if (page_ == Page::Mods && modsInstanceId_.empty()) {
        Instance* inst = currentInstance();
        if (inst) modsInstanceId_ = inst->id;
    }
    if (page_ == Page::Settings && javaList_.empty()) detectJava();
}

void App::openDialog(Dialog dialog) {
    dialog_ = dialog;
    if (dialog_ == Dialog::Microsoft) {
        {
            std::lock_guard<std::mutex> lock(ms_.mutex);
            ms_.running = false;
            ms_.cancel = std::make_shared<std::atomic<bool>>(false);
            ms_.userCode.clear();
            ms_.verificationUri.clear();
            ms_.status.clear();
            ms_.error.clear();
            ms_.account = Account();
            ms_.ready = false;
            ms_.done = false;
            ms_.ok = false;
        }
        msClientDraft_ = config_.msClientId;
    }
    if (dialog_ == Dialog::Accounts) {
        offlineName_ = config_.activeAccount().name;
        if (offlineName_.empty()) offlineName_ = L"Steve";
    }
}

void App::closeDialog() { dialog_ = Dialog::None; }

void App::notify(const std::wstring& text, bool error) {
    Toast t;
    t.text = text;
    t.error = error;
    t.born = nowSeconds();
    toasts_.push_back(t);
    if (toasts_.size() > 4) toasts_.erase(toasts_.begin());
}

Instance* App::currentInstance() {
    if (!config_.selectedInstance.empty()) {
        if (auto* inst = findInstance(instances_, config_.selectedInstance)) return inst;
    }
    if (!instances_.empty()) return &instances_[0];
    return nullptr;
}

const Instance* App::currentInstance() const {
    if (!config_.selectedInstance.empty()) {
        if (auto* inst = findInstance(const_cast<std::vector<Instance>&>(instances_), config_.selectedInstance)) return inst;
    }
    if (!instances_.empty()) return &instances_[0];
    return nullptr;
}

Instance* App::instanceForVersion(const std::wstring& versionId) {
    for (auto& inst : instances_) if (inst.versionId == versionId) return &inst;
    return nullptr;
}

void App::refreshInstances() {
    instances_.clear();
    std::vector<Instance> list;
    if (scanInstances(paths_, list)) instances_ = std::move(list);
    ensureSelection();
}

void App::ensureSelection() {
    if (instances_.empty()) { config_.selectedInstance.clear(); return; }
    if (config_.selectedInstance.empty() || !findInstance(instances_, config_.selectedInstance)) {
        config_.selectedInstance = instances_[0].id;
    }
}

void App::refreshManifest(bool force) {
    if (manifestLoading_) return;
    manifestLoading_ = true;
    std::wstring err;
    auto manifest = std::make_shared<Manifest>();
    jobs_.submit(L"Actualisation du manifeste", [this, force, manifest](JobContext& ctx) {
        std::wstring e;
        if (!fetchManifest(paths_, *manifest, force, e)) {
            ctx.fail(e.empty() ? L"Manifeste indisponible" : e, 1.0f);
            return;
        }
        ctx.report(L"Manifeste à jour", 1.0f);
    }, [this, manifest](bool ok, const std::wstring& msg) {
        if (ok) {
            manifest_ = *manifest;
            manifestLoaded_ = true;
            manifestError_.clear();
            manifestFetchedAt_ = nowSeconds();
        } else {
            manifestError_ = msg;
        }
        manifestLoading_ = false;
    });
}

std::wstring App::effectiveClientId() const { return config_.msClientId.empty() ? msClientDraft_ : config_.msClientId; }

std::vector<std::wstring> App::versionIds() const {
    std::vector<std::wstring> ids;
    for (auto& v : manifest_.versions) ids.push_back(v.id);
    return ids;
}

std::vector<std::wstring> App::releaseIds() const {
    std::vector<std::wstring> ids;
    for (auto& v : manifest_.versions) if (v.type == L"release") ids.push_back(v.id);
    return ids;
}

std::wstring App::modsInstanceId() const {
    if (!modsInstanceId_.empty()) return modsInstanceId_;
    if (Instance* inst = const_cast<App*>(this)->currentInstance()) return inst->id;
    return L"";
}

void App::syncSettingsFromConfig() {
    javaChoice_ = -1;
    resolutionChoice_ = 2;
    const int sizes[][2] = { { 854, 480 }, { 1024, 576 }, { 1280, 720 },
                             { 1366, 768 }, { 1600, 900 }, { 1920, 1080 } };
    for (int i = 0; i < 6; i++) {
        if (config_.width == sizes[i][0] && config_.height == sizes[i][1]) {
            resolutionChoice_ = i;
            break;
        }
    }
}

void App::installVersion(const std::wstring& versionId) {
    if (busyVersions_.count(versionId)) return;
    busyVersions_.insert(versionId);
    jobs_.submit(L"Installer " + versionId, [this, versionId](JobContext& ctx) {
        std::wstring err;
        if (!aurora::installVersion(paths_, versionId, ctx, err)) {
            ctx.fail(err.empty() ? L"Installation impossible" : err);
            return;
        }
        ctx.report(L"Installé", 1.0f);
    }, [this, versionId](bool ok, const std::wstring& msg) {
        busyVersions_.erase(versionId);
        if (ok) notify(L"Version installée : " + versionId, false);
        else notify(L"Échec installation : " + msg, true);
        refreshInstances();
    });
}

void App::uninstallVersion(const std::wstring& versionId) {
    jobs_.submit(L"Désinstaller " + versionId, [this, versionId](JobContext& ctx) {
        std::wstring err;
        if (!aurora::uninstallVersion(paths_, versionId, err)) {
            ctx.fail(err.empty() ? L"Suppression impossible" : err);
            return;
        }
        ctx.report(L"Terminé", 1.0f);
    }, [this, versionId](bool ok, const std::wstring& msg) {
        if (ok) notify(L"Version supprimée : " + versionId, false);
        else notify(L"Suppression impossible : " + msg, true);
        refreshInstances();
    });
}

void App::playVersion(const std::wstring& versionId) {
    Instance* inst = instanceForVersion(versionId);
    if (!inst) {
        // create instance
        Instance ni;
        ni.name = versionId;
        ni.id = slugify(L"Instance " + versionId);
        ni.mcVersion = versionId;
        ni.loader = L"none";
        ni.versionId = versionId;
        ni.dir = paths_.instanceDir(ni.id);
        std::wstring err;
        if (createInstance(paths_, ni, err)) {
            refreshInstances();
            inst = findInstance(instances_, ni.id);
            if (inst) config_.selectedInstance = inst->id;
        }
    }
    if (inst) launchInstance(*inst, L"", 0);
    else notify(L"Aucune instance pour cette version", true);
}

void App::createInstanceFromDialog() {
    Instance ni;
    ni.name = newName_.empty() ? L"Nouvelle instance" : newName_;
    std::wstring baseId = slugify(ni.name);
    ni.id = baseId;
    int suffix = 2;
    while (findInstance(instances_, ni.id)) ni.id = baseId + L"-" + std::to_wstring(suffix++);
    ni.mcVersion = L"1.21.1";
    if (!manifest_.versions.empty() && newVersionIndex_ >= 0 && (size_t)newVersionIndex_ < manifest_.versions.size()) {
        ni.mcVersion = manifest_.versions[newVersionIndex_].id;
    }
    ni.loader = newLoader_ == 1 ? L"fabric" : (newLoader_ == 2 ? L"quilt" : L"none");
    ni.versionId = ni.mcVersion;
    ni.dir = paths_.instanceDir(ni.id);
    std::wstring err;
    if (!createInstance(paths_, ni, err)) {
        notify(err.empty() ? L"Création impossible" : err, true);
        return;
    }
    notify(L"Instance créée", false);
    refreshInstances();
    config_.selectedInstance = ni.id;
    closeDialog();
    navigate(Page::Play);

    if (ni.loader == L"none") return;
    busyInstances_.insert(ni.id);
    auto result = std::make_shared<InstallResult>();
    jobs_.submit(L"Installation de " + ni.loader, [this, ni, result](JobContext& ctx) {
        std::wstring ierr;
        if (!installLoader(paths_, ni.mcVersion, ni.loader, L"", ctx, *result, ierr)) {
            ctx.fail(ierr.empty() ? L"Installation du chargeur impossible" : ierr);
            return;
        }
        ctx.report(L"Chargeur installé", 1.0f);
    }, [this, ni, result](bool ok, const std::wstring& msg) {
        busyInstances_.erase(ni.id);
        if (!ok) {
            notify(L"Échec installation du chargeur : " + msg, true);
            return;
        }
        Instance* inst = findInstance(instances_, ni.id);
        if (inst) {
            inst->versionId = result->versionId;
            inst->loaderVersion = result->loaderVersion;
            std::wstring serr;
            createInstance(paths_, *inst, serr);
            refreshInstances();
            config_.selectedInstance = ni.id;
        }
        notify(ni.loader + L" installé pour " + ni.mcVersion, false);
    });
}

void App::addOfflineAccount() {
    std::wstring name = trim(offlineName_);
    if (!isValidPlayerName(name)) { notify(L"Nom de joueur invalide", true); return; }
    Account acc;
    acc.type = L"offline";
    acc.name = name;
    acc.uuid = offlineUuid(name);
    // check duplicate
    for (auto& a : config_.accounts) if (a.type == L"offline" && iequals(a.name, name)) { config_.accountIndex = (int)(&a - &config_.accounts[0]); saveConfigNow(); closeDialog(); notify(L"Compte déjà présent", false); return; }
    config_.accounts.push_back(acc);
    config_.accountIndex = (int)config_.accounts.size() - 1;
    saveConfigNow();
    closeDialog();
    notify(L"Compte hors ligne ajouté", false);
}

void App::removeAccount(int index) {
    if (index < 0 || index >= (int)config_.accounts.size()) return;
    config_.accounts.erase(config_.accounts.begin() + index);
    if (config_.accountIndex >= (int)config_.accounts.size()) config_.accountIndex = (int)config_.accounts.size() - 1;
    if (config_.accountIndex < 0) config_.ensureAccount();
    saveConfigNow();
}

void App::startMicrosoftLogin() {
    std::wstring cid = effectiveClientId();
    if (cid.empty()) { notify(L"Client ID Microsoft requis dans Paramètres > Comptes", true); openDialog(Dialog::Accounts); return; }
    config_.msClientId = cid;
    saveConfigNow();
    if (msThread_.joinable()) {
        if (ms_.cancel) ms_.cancel->store(true);
        msThread_.join();
    }
    {
        std::lock_guard<std::mutex> lock(ms_.mutex);
        ms_.running = false;
        ms_.cancel = std::make_shared<std::atomic<bool>>(false);
        ms_.userCode.clear();
        ms_.verificationUri.clear();
        ms_.status = L"Demande de code...";
        ms_.error.clear();
        ms_.account = Account();
        ms_.ready = false;
        ms_.done = false;
        ms_.ok = false;
    }
    ms_.running.store(true);
    auto cancel = ms_.cancel;
    msThread_ = std::thread([this, cid, cancel]() {
        DeviceCodeInfo info;
        std::wstring err;
        if (!msStartDeviceFlow(cid, info, err)) {
            std::lock_guard<std::mutex> lock(ms_.mutex);
            ms_.running = false;
            ms_.error = err;
            ms_.done = true;
            return;
        }
        {
            std::lock_guard<std::mutex> lock(ms_.mutex);
            ms_.userCode = info.userCode;
            ms_.verificationUri = info.verificationUri;
            ms_.status = L"Ouvrez " + info.verificationUri + L" et entrez le code";
        }
        openUrl(info.verificationUri);
        int interval = info.interval;
        while (!cancel->load()) {
            Account acc;
            MsPollResult r = msPollDeviceFlow(cid, info, acc, interval, err, cancel.get());
            if (r == MsPollResult::Success) {
                // The UI thread adds the account to the configuration.
                std::lock_guard<std::mutex> lock(ms_.mutex);
                ms_.running = false;
                ms_.account = acc;
                ms_.ready = true;
                ms_.done = true;
                ms_.ok = true;
                return;
            }
            if (r == MsPollResult::Error || r == MsPollResult::Expired) {
                std::lock_guard<std::mutex> lock(ms_.mutex);
                ms_.running = false;
                ms_.error = err;
                ms_.done = true;
                return;
            }
            if (r == MsPollResult::SlowDown) interval += 5;
            int waitMs = std::max(5, interval) * 1000;
            for (int slept = 0; slept < waitMs && !cancel->load(); slept += 100)
                Sleep(100);
        }
        std::lock_guard<std::mutex> lock(ms_.mutex);
        ms_.running = false;
        ms_.error = L"Connexion annulée";
        ms_.done = true;
    });
}

void App::processMicrosoftResult() {
    Account pending;
    bool hasPending = false;
    {
        std::lock_guard<std::mutex> lock(ms_.mutex);
        if (ms_.ready && ms_.ok) {
            pending = ms_.account;
            hasPending = true;
            ms_.ready = false;
        }
    }
    if (!hasPending) return;
    bool exists = false;
    for (auto& a : config_.accounts) {
        if (a.type == L"microsoft" && a.uuid == pending.uuid) {
            a = pending;
            exists = true;
            break;
        }
    }
    if (!exists) config_.accounts.push_back(pending);
    for (size_t i = 0; i < config_.accounts.size(); i++)
        if (config_.accounts[i].uuid == pending.uuid) config_.accountIndex = (int)i;
    saveConfigNow();
    notify(L"Compte Microsoft connecté : " + pending.name, false);
}

void App::cancelMicrosoftLogin() {
    if (ms_.cancel) ms_.cancel->store(true);
}

void App::searchMods(bool reset) {
    if (modsLoading_) return;
    if (reset) { modsOffset_ = 0; modHits_.clear(); }
    modsLoading_ = true;
    modsSearched_ = true;
    modsError_.clear();
    std::wstring loader = L"fabric";
    std::wstring mv = L"1.21.1";
    Instance* inst = findInstance(instances_, modsInstanceId());
    if (!inst) inst = currentInstance();
    if (inst) {
        mv = inst->mcVersion;
        if (inst->loader == L"quilt" || inst->loader == L"fabric") loader = inst->loader;
    }
    std::wstring query = modsQuery_;
    int offset = modsOffset_;
    auto hits = std::make_shared<std::vector<ModHit>>();
    auto total = std::make_shared<int>(0);
    auto err = std::make_shared<std::wstring>();
    jobs_.submit(L"Recherche Modrinth", [query, loader, mv, offset, hits, total, err](JobContext& ctx) {
        if (!modrinthSearch(query, loader, mv, offset, *hits, *total, *err)) {
            ctx.fail(err->empty() ? L"Recherche impossible" : *err);
            return;
        }
        ctx.report(L"Résultats trouvés", 1.0f);
    }, [this, hits, total, reset](bool ok, const std::wstring& msg) {
        if (ok) {
            if (reset) modHits_ = *hits;
            else modHits_.insert(modHits_.end(), hits->begin(), hits->end());
            modTotal_ = *total;
            // Advance the pagination cursor so "Charger plus" fetches the next page.
            // Without this the same page was appended over and over.
            if (!hits->empty()) modsOffset_ += (int)hits->size();
            modsError_.clear();
        } else {
            modsError_ = msg;
        }
        modsLoading_ = false;
    });
}

void App::installMod(const ModHit& hit) {
    std::wstring iid = modsInstanceId();
    if (iid.empty()) { notify(L"Sélectionnez une instance", true); return; }
    Instance* inst = findInstance(instances_, iid);
    if (!inst) { notify(L"Instance introuvable", true); return; }
    std::wstring loader = inst->loader == L"quilt" ? L"quilt" : L"fabric";
    std::wstring mcVersion = inst->mcVersion;
    installingMod_ = hit.title;
    jobs_.submit(L"Installer mod " + hit.title, [this, hit, iid, loader, mcVersion](JobContext& ctx) {
        ModFile mf;
        std::wstring err;
        if (!modrinthGetFile(hit.id, loader, mcVersion, mf, err)) {
            ctx.fail(err.empty() ? L"Fichier introuvable pour cette version" : err);
            return;
        }
        ensureDir(paths_.modsDir(iid));
        fs::path dest = paths_.modsDir(iid) / mf.filename;
        ctx.report(L"Téléchargement...", 0.3f);
        std::wstring derr;
        if (!downloadFile(mf.url, dest, L"", mf.size, [&](uint64_t r, uint64_t t) { float p = t>0?(0.3f+0.7f*(float)r/(float)t):0.5f; ctx.report(L"Téléchargement...", p); }, [&](){return ctx.cancelled();}, derr, false)) {
            ctx.fail(derr.empty() ? L"Téléchargement impossible" : derr);
            return;
        }
        ctx.report(L"Installé", 1.0f);
    }, [this](bool ok, const std::wstring& msg) {
        installingMod_.clear();
        if (ok) notify(L"Mod installé dans le dossier mods", false);
        else notify(L"Échec installation mod : " + msg, true);
    });
}

void App::pingServerAddress(const std::wstring& address) {
    if (pingLoading_) return;
    pingLoading_ = true;
    pingError_.clear();
    pingAddress_ = address;
    auto status = std::make_shared<ServerStatus>();
    jobs_.submit(L"Ping serveur", [status, address](JobContext& ctx) {
        *status = pingServer(address);
        ctx.report(status->ok ? L"OK" : L"Échec", 1.0f);
    }, [this, status](bool ok, const std::wstring& msg) {
        (void)ok; (void)msg;
        pingStatus_ = *status;
        pingLoading_ = false;
        pingAt_ = nowSeconds();
        if (!pingStatus_.ok) pingError_ = pingStatus_.error.empty() ? L"Impossible de joindre le serveur" : pingStatus_.error;
    });
}

void App::joinServer(const std::wstring& address, const std::wstring& preferredVersion) {
    std::wstring host;
    int port = 0;
    if (!parseServerAddress(address, host, port) || host.empty()) {
        notify(L"Adresse de serveur invalide", true);
        return;
    }
    Instance* chosen = nullptr;
    if (!preferredVersion.empty()) {
        for (auto& inst : instances_)
            if (inst.mcVersion == preferredVersion) { chosen = &inst; break; }
    }
    if (!chosen) chosen = currentInstance();
    if (!chosen) {
        notify(L"Créez d'abord une instance pour rejoindre un serveur", true);
        navigate(Page::Play);
        return;
    }
    if (!preferredVersion.empty() && chosen->mcVersion != preferredVersion)
        notify(L"Attention : le serveur utilise Minecraft " + preferredVersion, true);
    config_.selectedInstance = chosen->id;
    launchInstance(*chosen, host, port);
}

void App::createServerFromDialog() {
    LocalServerConfig sc;
    sc.name = serverName_.empty() ? L"Mon serveur" : serverName_;
    sc.version = serverVersion_.empty() ? L"1.21.1" : serverVersion_;
    sc.port = _wtoi(serverPort_.c_str());
    if (sc.port < 1) sc.port = 25565;
    sc.ramMb = std::max(512, serverRam_);
    sc.onlineMode = false;
    sc.dir = slugify(sc.name);
    jobs_.submit(L"Création serveur local", [this, sc](JobContext& ctx) {
        LocalServerConfig work = sc;
        std::wstring err;
        if (!createLocalServer(paths_, work, ctx, err)) {
            ctx.fail(err.empty() ? L"Création impossible" : err);
            return;
        }
        ctx.report(L"Serveur prêt", 1.0f);
    }, [this, sc](bool ok, const std::wstring& msg) {
        if (!ok) {
            notify(L"Échec création serveur : " + msg, true);
            return;
        }
        bool exists = false;
        for (auto& s : config_.localServers) if (s.dir == sc.dir) { s = sc; exists = true; break; }
        if (!exists) config_.localServers.push_back(sc);
        saveConfigNow();
        notify(L"Serveur local créé", false);
        closeDialog();
        navigate(Page::Servers);
    });
}

void App::startLocalServer(const LocalServerConfig& server) {
    if (localServer_ && localServer_->running()) { notify(L"Un serveur est déjà en cours", true); return; }
    struct LocalStart {
        Process* process = nullptr;
        std::wstring error;
    };
    auto result = std::make_shared<LocalStart>();
    jobs_.submit(L"Démarrage du serveur local", [this, server, result](JobContext& ctx) {
        fs::path dir = paths_.serversDir / server.dir;
        fs::path jar = dir / L"server.jar";
        if (!pathExists(jar)) {
            ctx.fail(L"server.jar manquant dans le dossier du serveur");
            return;
        }
        std::wstring java = config_.javaPath;
        if (java.empty()) {
            ResolvedVersion rv;
            std::wstring err;
            if (!resolveVersion(paths_, server.version, rv, false, err)) {
                ctx.fail(L"Version du serveur introuvable : " + err);
                return;
            }
            java = chooseJava(paths_, config_, rv, ctx, err, true);
            if (java.empty()) {
                ctx.fail(err.empty() ? L"Java introuvable pour ce serveur" : err);
                return;
            }
        }
        std::wstring cmd = quoteWinArg(java) + L" -Xmx" + std::to_wstring(server.ramMb) +
                           L"M -Xms" + std::to_wstring(std::max(512, server.ramMb / 2)) +
                           L"M -jar " + quoteWinArg(jar.wstring()) + L" nogui";
        auto* process = new Process();
        process->server = true;
        process->setOutput([this](const std::wstring& line) { logGame(line, L"Serveur"); });
        std::wstring err;
        if (!process->start(cmd, dir, err)) {
            delete process;
            ctx.fail(L"Démarrage serveur impossible : " + err);
            return;
        }
        result->process = process;
        ctx.report(L"Serveur démarré", 1.0f);
    }, [this, server, result](bool ok, const std::wstring& msg) {
        if (!ok || !result->process) {
            delete result->process;
            result->process = nullptr;
            notify(L"Serveur non démarré : " + msg, true);
            return;
        }
        delete localServer_;
        localServer_ = result->process;
        result->process = nullptr;
        localServerName_ = server.name;
        localServerDir_ = server.dir;
        localServerStartedAt_ = nowSeconds();
        notify(L"Serveur local démarré", false);
        navigate(Page::Servers);
    });
}

void App::stopLocalServer() {
    if (!localServer_) return;
    localServer_->requestStop();
    notify(L"Arrêt du serveur demandé", false);
}

void App::detectJava() {
    if (javaLoading_) return;
    javaLoading_ = true;
    auto list = std::make_shared<std::vector<JavaInfo>>();
    Config cfg = config_;
    jobs_.submit(L"Détection Java", [this, list, cfg](JobContext& ctx) {
        *list = detectJavaInstallations(paths_, cfg);
        ctx.report(L"Terminé", 1.0f);
    }, [this, list](bool ok, const std::wstring& msg) {
        (void)ok; (void)msg;
        javaList_ = *list;
        javaLoading_ = false;
    });
}

void App::launchInstance(const Instance& instance, std::wstring host, int port) {
    if (game_ && game_->running()) { notify(L"Le jeu est déjà en cours d'exécution", true); return; }
    if (busyInstances_.count(instance.id)) { notify(L"Cette instance est déjà en préparation", true); return; }
    busyInstances_.insert(instance.id);
    auto cfg = std::make_shared<Config>(config_);
    auto acc = std::make_shared<Account>(config_.activeAccount());
    std::wstring clientId = effectiveClientId();
    struct LaunchResult {
        Process* process = nullptr;
        bool refreshed = false;
    };
    auto result = std::make_shared<LaunchResult>();
    jobs_.submit(L"Lancement " + instance.name, [this, instance, host, port, cfg, acc, clientId, result](JobContext& ctx) {
        std::wstring err;
        if (acc->isMicrosoft() && !accountTokenValid(*acc)) {
            ctx.report(L"Renouvellement de la session Microsoft…", 0.01f);
            std::wstring rerr;
            if (!msRefreshAccount(clientId, *acc, rerr)) {
                ctx.fail(rerr.empty() ? L"Session Microsoft expirée" : rerr);
                return;
            }
            result->refreshed = true;
        }
        ResolvedVersion ver;
        if (!resolveVersion(paths_, instance.versionId, ver, true, err)) {
            ctx.fail(err.empty() ? L"Profil de version introuvable" : err);
            return;
        }
        if (!isVersionInstalled(paths_, instance.versionId) && !ver.clientUrl.empty()) {
            ctx.report(L"Installation...", 0.1f);
            if (!aurora::installVersion(paths_, instance.versionId, ctx, err)) {
                ctx.fail(err.empty() ? L"Installation impossible" : err);
                return;
            }
        }
        std::wstring javaPath = chooseJava(paths_, *cfg, ver, ctx, err, true);
        if (javaPath.empty()) {
            ctx.fail(err.empty() ? L"Java introuvable" : err);
            return;
        }
        std::wstring uuid = acc->uuid;
        if (uuid.empty() && acc->isOffline()) uuid = offlineUuid(acc->name);
        if (uuid.empty()) uuid = L"00000000-0000-0000-0000-000000000000";
        LaunchRequest req;
        req.paths = &paths_;
        req.config = cfg.get();
        req.version = &ver;
        req.instance = &instance;
        req.javaPath = javaPath;
        req.accountName = acc->name.empty() ? L"Joueur" : acc->name;
        req.accountUuid = uuidNoDashes(uuid);
        req.accessToken = acc->accessToken.empty() ? L"0" : acc->accessToken;
        req.userType = acc->isMicrosoft() ? L"msa" : L"legacy";
        req.xuid = acc->xuid;
        req.clientId = clientId;
        req.serverHost = host;
        req.serverPort = port;
        std::vector<std::wstring> args;
        if (!buildLaunchCommand(req, args, err)) {
            ctx.fail(err.empty() ? L"Commande de lancement invalide" : err);
            return;
        }
        std::wstring cmd = buildCommandLine(args);
        auto* process = new Process();
        process->setOutput([this](const std::wstring& line) { logGame(line); });
        if (!process->start(cmd, instance.dir, err)) {
            delete process;
            ctx.fail(err.empty() ? L"Démarrage du jeu impossible" : err);
            return;
        }
        result->process = process;
        ctx.report(L"Jeu lancé", 1.0f);
    }, [this, instance, acc, result](bool ok, const std::wstring& msg) {
        busyInstances_.erase(instance.id);
        if (result->refreshed && acc->isMicrosoft()) {
            for (auto& a : config_.accounts) {
                if (a.type == L"microsoft" && a.uuid == acc->uuid) { a = *acc; break; }
            }
            saveConfigNow();
        }
        if (ok && result->process) {
            delete game_;
            game_ = result->process;
            result->process = nullptr;
            gameInstance_ = instance.id;
            gameStartedAt_ = nowSeconds();
            gameExitNotified_ = false;
            notify(L"Jeu lancé", false);
            if (config_.closeOnLaunch) PostQuitMessage(0);
        } else {
            delete result->process;
            result->process = nullptr;
            notify(L"Échec lancement : " + msg, true);
        }
    });
}

bool App::needsFrames() {
    jobs_.poll();
    processMicrosoftResult();
    // check game exit
    if (game_ && !game_->running() && !gameExitNotified_) {
        gameExitNotified_ = true;
        int code = game_->exitCode();
        if (code == 0) logSuccess(L"Jeu fermé proprement", L"Jeu");
        else logWarn(L"Jeu fermé avec le code " + std::to_wstring(code), L"Jeu");
    }
    // check local server exit
    if (localServer_ && !localServer_->running()) {
        // keep reference for info
    }
    // toasts fade
    double now = nowSeconds();
    while (!toasts_.empty() && now - toasts_[0].born > 3.8) toasts_.erase(toasts_.begin());
    if (!toasts_.empty()) return true;
    if (ms_.running) return true;
    if (!jobs_.infos().empty()) return true;
    if (game_ && game_->running()) return true;
    if (localServer_ && localServer_->running()) return true;
    return false;
}

void App::tick() {}
void App::refreshLogs(bool force) {
    double now = nowSeconds();
    if (!force && now - logsRefreshedAt_ < 0.5) return;
    logsRefreshedAt_ = now;
    logCache_ = Logger::instance().snapshot();
    if (autoScroll_ && (int)logCache_.size() != lastLogCount_) {
        lastLogCount_ = (int)logCache_.size();
    }
}

} // namespace aurora
