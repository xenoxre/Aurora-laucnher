#include "meta.h"
#include "http.h"
#include "log.h"
#include "util.h"
#include "zip.h"
#include "../version.h"

#include <algorithm>
#include <set>

namespace aurora {

static void reportBatch(JobContext& job, const std::wstring& label, float lo, float hi,
                        int done, int total, uint64_t bytes, uint64_t totalBytes) {
    std::wstring status = label + L" — " + std::to_wstring(done) + L"/" + std::to_wstring(total);
    if (totalBytes > 0) status += L" (" + humanBytes(bytes) + L" / " + humanBytes(totalBytes) + L")";
    float p = lo;
    if (total > 0) p = lo + (hi - lo) * (float)done / (float)total;
    job.report(status, p);
}

static std::wstring logConfigName(const ResolvedVersion& ver) {
    if (!ver.logFileId.empty()) {
        std::wstring id = ver.logFileId;
        for (auto& c : id) if (c == L'/' || c == L'\\' || c == L':') c = L'_';
        return id;
    }
    size_t slash = ver.logFileUrl.find_last_of(L'/');
    if (slash != std::wstring::npos) return ver.logFileUrl.substr(slash + 1);
    return L"log4j.xml";
}

bool installVersion(const Paths& paths, const std::wstring& id, JobContext& job,
                    std::wstring& error) {
    fs::path vdir = paths.versionDir(id);
    ensureDir(vdir);
    job.report(L"Résolution du profil " + id + L"…", 0.015f);
    ResolvedVersion ver;
    if (!resolveVersion(paths, id, ver, true, error)) return false;
    if (ver.clientUrl.empty()) {
        error = L"Jar client introuvable pour " + id;
        return false;
    }

    std::vector<DownloadItem> files;
    std::set<std::wstring> seenPaths;
    auto addFile = [&](const std::wstring& url, const fs::path& path, const std::wstring& sha1,
                       uint64_t size, bool verify) {
        std::wstring key = toLower(path.wstring());
        if (seenPaths.count(key)) return;
        seenPaths.insert(key);
        DownloadItem it;
        it.url = url;
        it.path = path;
        it.sha1 = sha1;
        it.size = size;
        it.verifySha = verify;
        files.push_back(it);
    };

    fs::path clientJar = paths.versionDir(ver.jarId) / (ver.jarId + L".jar");
    addFile(ver.clientUrl, clientJar, ver.clientSha1, ver.clientSize, true);
    for (auto& lib : ver.libraries) {
        addFile(lib.url, paths.libraries / lib.path, lib.sha1, lib.size, true);
    }
    if (!ver.logFileUrl.empty()) {
        fs::path logPath = paths.assets / L"log_configs" / logConfigName(ver);
        addFile(ver.logFileUrl, logPath, ver.logFileSha1, 0, true);
    }

    job.report(L"Téléchargement des fichiers de jeu…", 0.03f);
    std::wstring dlError;
    bool ok = downloadBatch(files, 8, [&]() { return job.cancelled(); },
        [&](int done, int total, uint64_t bytes, uint64_t totalBytes) {
            reportBatch(job, L"Fichiers de jeu", 0.03f, 0.38f, done, total, bytes, totalBytes);
        }, dlError);
    if (!ok) { error = dlError; return false; }

    // Natives -------------------------------------------------------------
    job.report(L"Extraction des bibliothèques natives…", 0.40f);
    fs::path nativesDir = vdir / L"natives";
    removeAll(nativesDir);
    ensureDir(nativesDir);
    for (auto& lib : ver.libraries) {
        if (!lib.isNative) continue;
        auto filter = [&](const std::string& entry) {
            std::wstring w = utf8ToWide(entry);
            if (w.empty() || w.back() == L'/') return false;
            if (startsWith(w, L"META-INF/")) return false;
            for (auto& ex : lib.extractExclude) {
                std::wstring e = ex;
                if (!e.empty() && (e.back() == L'/' || e.back() == L'\\')) {
                    if (startsWith(w, e)) return false;
                } else if (w == e) {
                    return false;
                }
            }
            return true;
        };
        std::wstring zerr;
        fs::path zip = paths.libraries / lib.path;
        if (!pathExists(zip)) { error = L"Natif manquant : " + lib.path; return false; }
        if (!zipExtract(zip, nativesDir, filter, zerr)) {
            error = L"Extraction native impossible : " + zerr;
            return false;
        }
    }

    // Assets --------------------------------------------------------------
    job.report(L"Préparation des ressources…", 0.42f);
    fs::path indexPath = paths.assetIndexes / (ver.assetsId + L".json");
    if (!pathExists(indexPath)) {
        if (ver.assetsUrl.empty()) {
            error = L"Index d'assets introuvable pour " + id;
            return false;
        }
        std::wstring aerr;
        if (!downloadFile(ver.assetsUrl, indexPath, ver.assetsSha1, ver.assetsSize, {}, {}, aerr)) {
            error = L"Index d'assets : " + aerr;
            return false;
        }
    }
    Json index;
    std::string perr;
    std::string indexText;
    if (!readFileText(indexPath, indexText) || !Json::parse(indexText, index, &perr)) {
        error = L"Index d'assets invalide";
        return false;
    }
    std::vector<DownloadItem> assets;
    uint64_t assetsTotal = 0;
    for (auto& kv : index["objects"].fields()) {
        std::string hash = kv.second["hash"].asString();
        uint64_t size = (uint64_t)kv.second["size"].asInt(0);
        if (hash.size() < 2) continue;
        fs::path objPath = paths.assetObjects / utf8ToWide(hash.substr(0, 2)) / utf8ToWide(hash);
        if (pathExists(objPath) && fileSize(objPath) == size) continue;
        DownloadItem it;
        it.url = std::wstring(L"https://resources.download.minecraft.net/") +
                 utf8ToWide(hash.substr(0, 2)) + L"/" + utf8ToWide(hash);
        it.path = objPath;
        it.size = size;
        it.verifySha = false;
        assets.push_back(it);
        assetsTotal += size;
    }
    if (!assets.empty()) {
        job.report(L"Téléchargement des ressources — " + humanBytes(assetsTotal), 0.44f);
        std::wstring aerr;
        bool aok = downloadBatch(assets, 10, [&]() { return job.cancelled(); },
            [&](int done, int total, uint64_t bytes, uint64_t totalBytes) {
                reportBatch(job, L"Ressources", 0.44f, 0.95f, done, total, bytes, totalBytes);
            }, aerr);
        if (!aok) { error = L"Ressources : " + aerr; return false; }
    }

    bool virtualAssets = index["virtual"].asBool(false) || index["map_to_resources"].asBool(false);
    if (virtualAssets) {
        job.report(L"Préparation des assets virtuels…", 0.96f);
        fs::path vdirRoot = paths.assets / L"virtual" / ver.assetsId;
        int i = 0;
        int total = (int)index["objects"].size();
        for (auto& kv : index["objects"].fields()) {
            std::string hash = kv.second["hash"].asString();
            uint64_t size = (uint64_t)kv.second["size"].asInt(0);
            if (hash.size() < 2) continue;
            fs::path src = paths.assetObjects / utf8ToWide(hash.substr(0, 2)) / utf8ToWide(hash);
            fs::path dst = vdirRoot / utf8ToWide(kv.first);
            if (!pathExists(dst) || fileSize(dst) != size) {
                ensureDir(dst.parent_path());
                if (!copyFile(src, dst)) {
                    error = L"Copie d'un asset virtuel impossible : " + utf8ToWide(kv.first);
                    return false;
                }
            }
            if ((++i % 250) == 0) {
                job.report(L"Assets virtuels — " + std::to_wstring(i) + L"/" + std::to_wstring(total),
                           0.96f + 0.03f * (float)i / (float)std::max(1, total));
            }
        }
    }

    Json marker = Json::object();
    marker.set("id", id);
    marker.set("mcVersion", ver.parentId);
    marker.set("installedAt", nowStampUtf8());
    writeFileTextAtomic(vdir / L".installed", marker.dump(2));
    job.report(L"Installation terminée", 1.0f);
    logSuccess(L"Version installée : " + id + L" (" + humanBytes(files.size() ? 0 : 0) + L")", L"Jeu");
    return true;
}

bool installLoader(const Paths& paths, const std::wstring& mcVersion,
                   const std::wstring& loader, const std::wstring& loaderVersion,
                   JobContext& job, InstallResult& result, std::wstring& error) {
    bool quilt = iequals(loader, L"quilt");
    std::wstring base = quilt ? L"https://meta.quiltmc.org/v3" : L"https://meta.fabricmc.net/v2";
    job.report(L"Récupération du chargeur " + loader + L"…", 0.02f);
    Json list;
    std::wstring listErr;
    std::wstring listUrl = base + L"/versions/loader/" + mcVersion;
    if (!httpGetJson(listUrl, list, listErr)) {
        error = L"Meta " + loader + L" : " + listErr;
        return false;
    }
    if (!list.isArray() || list.size() == 0) {
        error = L"Aucune version de " + loader + L" pour Minecraft " + mcVersion;
        return false;
    }
    std::wstring chosen = loaderVersion;
    if (chosen.empty()) {
        std::wstring fallback;
        for (auto& e : list.items()) {
            std::wstring lv = e["loader"]["version"].asWString();
            if (lv.empty()) lv = e["version"].asWString();
            bool stable = e["loader"]["stable"].asBool(e["stable"].asBool(true));
            if (fallback.empty()) fallback = lv;
            if (stable && !lv.empty()) { chosen = lv; break; }
        }
        if (chosen.empty()) chosen = fallback;
    }
    if (chosen.empty()) {
        error = L"Impossible de choisir une version de " + loader;
        return false;
    }
    job.report(L"Téléchargement du profil " + loader + L" " + chosen + L"…", 0.05f);
    std::wstring profileUrl = base + L"/versions/loader/" + mcVersion + L"/" + chosen + L"/profile/json";
    std::string profileText;
    std::wstring perr;
    if (!httpGet(profileUrl, profileText, perr,
                 { { L"User-Agent", L"AuroraLauncher/" AURORA_VERSION_W } })) {
        error = L"Profil " + loader + L" : " + perr;
        return false;
    }
    Json profile;
    std::string jerr;
    if (!Json::parse(profileText, profile, &jerr)) {
        error = L"Profil " + loader + L" invalide : " + utf8ToWide(jerr);
        return false;
    }
    std::wstring profileId = profile["id"].asWString();
    if (profileId.empty())
        profileId = loader + L"-loader-" + chosen + L"-" + mcVersion;
    fs::path profileDir = paths.versionDir(profileId);
    ensureDir(profileDir);
    if (!writeFileTextAtomic(profileDir / (profileId + L".json"), profileText)) {
        error = L"Écriture du profil " + loader + L" impossible";
        return false;
    }
    if (!installVersion(paths, profileId, job, error)) return false;
    result.ok = true;
    result.versionId = profileId;
    result.mcVersion = mcVersion;
    result.loader = quilt ? L"quilt" : L"fabric";
    result.loaderVersion = chosen;
    logSuccess(L"Chargeur installé : " + loader + L" " + chosen + L" pour " + mcVersion, L"Jeu");
    return true;
}

bool uninstallVersion(const Paths& paths, const std::wstring& id, std::wstring& error) {
    fs::path dir = paths.versionDir(id);
    if (!pathExists(dir)) return true;
    if (!removeAll(dir)) {
        error = L"Suppression impossible (fichiers verrouillés ?)";
        return false;
    }
    return true;
}

} // namespace aurora
