#include "meta.h"
#include "http.h"
#include "log.h"
#include "util.h"
#include "../version.h"

#include <algorithm>
#include <chrono>
#include <set>

namespace aurora {

static const wchar_t* kManifestUrl =
    L"https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";
static const wchar_t* kLibrariesUrl = L"https://libraries.minecraft.net/";
static const wchar_t* kResourcesUrl = L"https://resources.download.minecraft.net/";

bool parseMavenCoordinate(const std::wstring& coordinate, std::wstring& group,
                          std::wstring& artifact, std::wstring& version,
                          std::wstring& classifier, std::wstring& ext) {
    std::wstring c = coordinate;
    ext = L"jar";
    size_t at = c.find(L'@');
    if (at != std::wstring::npos) {
        ext = c.substr(at + 1);
        c = c.substr(0, at);
    }
    std::vector<std::wstring> parts = split(c, L':');
    if (parts.size() < 3) return false;
    group = parts[0];
    artifact = parts[1];
    version = parts[2];
    classifier = parts.size() >= 4 ? parts[3] : L"";
    return !group.empty() && !artifact.empty() && !version.empty();
}

std::wstring mavenPathForName(const std::wstring& coordinate) {
    std::wstring group, artifact, version, classifier, ext;
    if (!parseMavenCoordinate(coordinate, group, artifact, version, classifier, ext)) return L"";
    std::wstring path = group;
    std::replace(path.begin(), path.end(), L'.', L'/');
    path += L"/" + artifact + L"/" + version + L"/" + artifact + L"-" + version;
    if (!classifier.empty()) path += L"-" + classifier;
    path += L"." + ext;
    return path;
}

bool fetchManifest(const Paths& paths, Manifest& manifest, bool force, std::wstring& error) {
    fs::path cache = paths.meta / L"version_manifest_v2.json";
    bool fresh = false;
    std::error_code ec;
    if (pathExists(cache)) {
        auto ft = fs::last_write_time(cache, ec);
        if (!ec) {
            auto age = fs::file_time_type::clock::now() - ft;
            fresh = age >= std::chrono::hours(0) && age < std::chrono::hours(1);
        }
    }
    std::string text;
    bool loaded = false;
    if (!force && fresh) {
        if (readFileText(cache, text)) loaded = true;
    }
    if (!loaded) {
        std::wstring netError;
        std::string downloaded;
        if (httpGet(kManifestUrl, downloaded, netError)) {
            text = std::move(downloaded);
            writeFileTextAtomic(cache, text);
            loaded = true;
        } else if (pathExists(cache) && readFileText(cache, text)) {
            logWarn(L"Manifest hors ligne, utilisation du cache : " + netError, L"Mojang");
            loaded = true;
        } else {
            error = L"Impossible de joindre Mojang : " + netError;
            return false;
        }
    }
    Json root;
    std::string perr;
    if (!Json::parse(text, root, &perr)) {
        error = L"Manifest invalide : " + utf8ToWide(perr);
        return false;
    }
    manifest.latestRelease = root["latest"]["release"].asWString();
    manifest.latestSnapshot = root["latest"]["snapshot"].asWString();
    manifest.versions.clear();
    for (auto& v : root["versions"].items()) {
        VersionEntry e;
        e.id = v["id"].asWString();
        e.type = v["type"].asWString();
        e.url = v["url"].asWString();
        e.releaseTime = v["releaseTime"].asWString();
        if (!e.id.empty() && !e.url.empty()) manifest.versions.push_back(e);
    }
    if (manifest.versions.empty()) {
        error = L"Manifest vide";
        return false;
    }
    return true;
}

static bool parseJsonFile(const fs::path& path, Json& out, std::wstring& error) {
    std::string text;
    if (!readFileText(path, text)) {
        error = L"Lecture impossible : " + path.wstring();
        return false;
    }
    std::string perr;
    if (!Json::parse(text, out, &perr)) {
        error = L"JSON invalide (" + path.filename().wstring() + L") : " + utf8ToWide(perr);
        return false;
    }
    return true;
}

bool loadVersionJson(const Paths& paths, const std::wstring& id, Json& out,
                     bool allowDownload, std::wstring& error) {
    fs::path file = paths.versionDir(id) / (id + L".json");
    if (pathExists(file)) {
        if (parseJsonFile(file, out, error)) return true;
        logWarn(L"Profil corrompu, nouveau téléchargement : " + id, L"Mojang");
        removeFile(file);
    }
    if (!allowDownload) {
        error = L"Profil de version absent : " + id;
        return false;
    }
    Manifest manifest;
    std::wstring merr;
    if (!fetchManifest(paths, manifest, false, merr)) { error = merr; return false; }
    const VersionEntry* entry = nullptr;
    for (auto& v : manifest.versions) if (v.id == id) { entry = &v; break; }
    if (!entry) {
        error = L"Version inconnue chez Mojang : " + id;
        return false;
    }
    HttpResponse res;
    HttpRequest req;
    req.url = entry->url;
    req.headers.push_back({ L"User-Agent", L"AuroraLauncher/" AURORA_VERSION_W });
    if (!httpRequest(req, res) || res.status < 200 || res.status >= 300) {
        error = L"Téléchargement du profil impossible : " + (res.error.empty() ? entry->url : res.error);
        return false;
    }
    std::string perr;
    if (!Json::parse(res.body, out, &perr)) {
        error = L"Profil JSON invalide : " + utf8ToWide(perr);
        return false;
    }
    writeFileTextAtomic(file, res.body);
    logInfo(L"Profil téléchargé : " + id, L"Mojang");
    return true;
}

struct Features {
    bool customResolution = true;
};

static bool featureMatches(const Json& featureObj, const Features& f) {
    for (auto& kv : featureObj.fields()) {
        bool want = kv.second.asBool();
        bool have = false;
        if (kv.first == "has_custom_resolution") have = f.customResolution;
        else if (kv.first == "is_demo_user") have = false;
        else if (kv.first == "has_quick_plays_support") have = false;
        else if (startsWith(utf8ToWide(kv.first), L"is_quick_play")) have = false;
        if (have != want) return false;
    }
    return true;
}

static bool ruleMatches(const Json& rule, const Features& f) {
    if (rule.has("os")) {
        const Json& os = rule["os"];
        std::wstring name = os["name"].asWString();
        if (!name.empty() && name != L"windows") return false;
        std::wstring arch = os["arch"].asWString();
        if (!arch.empty() && arch != L"x86_64") return false;
    }
    if (rule.has("features") && !featureMatches(rule["features"], f)) return false;
    return true;
}

static bool rulesAllow(const Json& rules, const Features& f) {
    if (!rules.isArray() || rules.size() == 0) return true;
    bool allow = false;
    for (auto& rule : rules.items()) {
        if (!rule.isObject()) continue;
        if (ruleMatches(rule, f)) allow = rule["action"].asWString() == L"allow";
    }
    return allow;
}

static void appendArgValue(const Json& value, std::vector<std::wstring>& out) {
    if (value.isString()) out.push_back(value.asWString());
    else if (value.isArray()) {
        for (auto& v : value.items()) if (v.isString()) out.push_back(v.asWString());
    }
}

static void collectArguments(const Json& list, const Features& f, std::vector<std::wstring>& out) {
    if (!list.isArray()) return;
    for (auto& e : list.items()) {
        if (e.isString()) {
            out.push_back(e.asWString());
        } else if (e.isObject()) {
            if (!rulesAllow(e.get("rules"), f)) continue;
            appendArgValue(e.get("value"), out);
        }
    }
}

static std::vector<std::wstring> splitArgs(const std::wstring& line) {
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

static void addLibrary(const Json& lib, const Features& f, std::vector<LibraryFile>& out,
                       std::set<std::wstring>& seen) {
    if (!lib.isObject()) return;
    if (!rulesAllow(lib.get("rules"), f)) return;
    std::wstring name = lib["name"].asWString();
    if (name.empty()) return;
    std::vector<std::wstring> parts = split(name, L':');
    bool nameIsNative = parts.size() >= 4 && containsNoCase(parts[3], L"natives");
    std::wstring baseUrl = lib["url"].asWString(kLibrariesUrl);
    if (baseUrl.empty()) baseUrl = kLibrariesUrl;
    if (baseUrl.back() != L'/') baseUrl += L'/';
    std::vector<std::wstring> excludes;
    for (auto& ex : lib["extract"]["exclude"].items()) excludes.push_back(ex.asWString());

    auto pushRef = [&](const Json& d, bool native, const std::wstring& fallbackPath,
                       const std::wstring& fallbackUrl) {
        std::wstring path = d["path"].asWString();
        std::wstring url = d["url"].asWString();
        std::wstring sha1 = d["sha1"].asWString();
        uint64_t size = (uint64_t)d["size"].asInt(0);
        if (path.empty()) path = fallbackPath;
        if (path.empty()) return;
        if (url.empty()) url = fallbackUrl.empty() ? (baseUrl + path) : fallbackUrl;
        if (seen.count(path)) return;
        seen.insert(path);
        LibraryFile ref;
        ref.name = name;
        ref.path = path;
        ref.url = url;
        ref.sha1 = sha1;
        ref.size = size;
        ref.isNative = native;
        ref.extractExclude = excludes;
        out.push_back(ref);
    };

    std::wstring artifactPath = mavenPathForName(name);
    const Json& downloads = lib["downloads"];
    bool addedBase = false;
    if (downloads.isObject()) {
        const Json& artifact = downloads["artifact"];
        if (artifact.isObject()) {
            pushRef(artifact, nameIsNative, artifactPath, L"");
            addedBase = true;
        }
        const Json& artifacts = downloads["artifacts"];
        if (artifacts.isObject()) {
            for (auto& kv : artifacts.fields()) {
                bool native = containsNoCase(utf8ToWide(kv.first), L"natives");
                if (kv.first == "artifact" || kv.first.empty()) native = nameIsNative;
                pushRef(kv.second, native, mavenPathForName(name), L"");
                addedBase = true;
            }
        }
    }
    if (!addedBase && !nameIsNative && !artifactPath.empty()) {
        pushRef(Json::object(), false, artifactPath, baseUrl + artifactPath);
    }
    if (nameIsNative) {
        // The single artifact is the native bundle.
        return;
    }
    // Old-format natives (LWJGL 2 and friends) via "natives" + classifiers.
    const Json& natives = lib["natives"];
    if (natives.isObject()) {
        std::wstring classifier = natives["windows"].asWString();
        if (classifier.empty()) classifier = natives["windows-arm64"].asWString();
        if (!classifier.empty()) {
            classifier = replaceAll(classifier, L"${arch}", L"64");
            const Json& cls = downloads["classifiers"][wideToUtf8(classifier)];
            std::wstring coord = name;
            if (parts.size() == 3) coord += L":" + classifier;
            std::wstring path = mavenPathForName(coord);
            if (cls.isObject()) pushRef(cls, true, path, baseUrl + path);
            else if (!path.empty()) pushRef(Json::object(), true, path, baseUrl + path);
        }
    }
}

bool resolveVersion(const Paths& paths, const std::wstring& id, ResolvedVersion& out,
                    bool allowDownload, std::wstring& error, bool customResolution) {
    out = ResolvedVersion();
    std::vector<Json> chain;
    std::vector<std::wstring> ids;
    std::wstring cur = id;
    for (int depth = 0; depth < 10 && !cur.empty(); depth++) {
        Json j;
        if (!loadVersionJson(paths, cur, j, allowDownload, error)) return false;
        chain.push_back(j);
        ids.push_back(cur);
        std::wstring parent = j["inheritsFrom"].asWString();
        if (parent == cur) break;
        cur = parent;
    }
    if (chain.empty()) { error = L"Profil introuvable"; return false; }

    auto pick = [&](const char* key) -> const Json* {
        for (auto& j : chain) if (j.has(key)) return &j[key];
        return nullptr;
    };

    out.id = id;
    out.raw = chain[0];
    out.parentId = ids.back();
    out.jarId = ids.back();
    if (const Json* t = pick("type")) out.type = t->asWString(L"release");
    if (const Json* mc = pick("mainClass")) out.mainClass = mc->asWString();
    if (out.mainClass.empty()) {
        error = L"mainClass absente du profil " + id;
        return false;
    }

    for (size_t i = 0; i < chain.size(); i++) {
        const Json& d = chain[i]["downloads"];
        if (d.isObject() && d["client"].isObject()) {
            out.jarId = ids[i];
            out.clientUrl = d["client"]["url"].asWString();
            out.clientSha1 = d["client"]["sha1"].asWString();
            out.clientSize = (uint64_t)d["client"]["size"].asInt(0);
            break;
        }
    }
    for (size_t i = 0; i < chain.size(); i++) {
        const Json& d = chain[i]["downloads"];
        if (d.isObject() && d["server"].isObject()) {
            out.serverUrl = d["server"]["url"].asWString();
            out.serverSha1 = d["server"]["sha1"].asWString();
            out.serverSize = (uint64_t)d["server"]["size"].asInt(0);
            break;
        }
    }

    if (const Json* ai = pick("assetIndex")) {
        if (ai->isObject()) {
            out.assetsId = (*ai)["id"].asWString(L"legacy");
            out.assetsUrl = (*ai)["url"].asWString();
            out.assetsSha1 = (*ai)["sha1"].asWString();
            out.assetsSize = (uint64_t)(*ai)["size"].asInt(0);
        }
    }
    if (out.assetsUrl.empty()) {
        out.assetsId = L"legacy";
        out.assetsUrl = L"https://launchermeta.mojang.com/mc/assets/legacy/legacy.json";
    }

    if (const Json* jv = pick("javaVersion")) {
        out.javaMajor = (int)jv->get("majorVersion").asInt(8);
        out.javaComponent = jv->get("component").asWString(L"jre-legacy");
    }
    if (out.javaMajor <= 0) out.javaMajor = 8;
    if (out.javaComponent.empty()) {
        if (out.javaMajor >= 21) out.javaComponent = L"java-runtime-delta";
        else if (out.javaMajor >= 17) out.javaComponent = L"java-runtime-gamma";
        else out.javaComponent = L"jre-legacy";
    }

    for (size_t i = 0; i < chain.size(); i++) {
        const Json& log = chain[i]["logging"]["client"];
        if (log.isObject()) {
            out.logArg = log["argument"].asWString();
            const Json& file = log["file"];
            out.logFileId = file["id"].asWString();
            out.logFileUrl = file["url"].asWString();
            out.logFileSha1 = file["sha1"].asWString();
            break;
        }
    }

    Features features;
    features.customResolution = customResolution;
    std::set<std::wstring> seen;
    for (int i = (int)chain.size() - 1; i >= 0; i--) {
        for (auto& lib : chain[(size_t)i]["libraries"].items()) {
            addLibrary(lib, features, out.libraries, seen);
        }
    }

    bool anyModern = false;
    for (auto& j : chain) {
        if (j["arguments"].isObject() && j["arguments"]["game"].isArray()) anyModern = true;
    }
    if (anyModern) {
        for (int i = (int)chain.size() - 1; i >= 0; i--) {
            collectArguments(chain[(size_t)i]["arguments"]["game"], features, out.gameArgs);
        }
    } else {
        const Json* legacy = pick("minecraftArguments");
        if (legacy && legacy->isString()) {
            out.gameArgs = splitArgs(legacy->asWString());
            out.legacyArgs = true;
        }
    }
    for (int i = (int)chain.size() - 1; i >= 0; i--) {
        collectArguments(chain[(size_t)i]["arguments"]["jvm"], features, out.jvmArgs);
    }
    out.supportsQuickPlay = false;
    for (auto& j : chain) {
        std::string dump = j.dump();
        if (dump.find("quickPlayMultiplayer") != std::string::npos) {
            out.supportsQuickPlay = true;
            break;
        }
    }
    if (out.gameArgs.empty()) {
        error = L"Arguments de jeu absents du profil " + id;
        return false;
    }
    return true;
}

bool isVersionInstalled(const Paths& paths, const std::wstring& id) {
    return pathExists(paths.versionDir(id) / L".installed") &&
           pathExists(paths.versionDir(id) / (id + L".json"));
}

std::vector<std::wstring> installedVersionIds(const Paths& paths) {
    std::vector<std::wstring> out;
    for (auto& d : listDirectory(paths.versions, true)) {
        if (pathExists(d / L".installed")) out.push_back(d.filename().wstring());
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool versionSupportsQuickPlay(const ResolvedVersion& version) {
    return version.supportsQuickPlay;
}

std::wstring assetsDirectory(const Paths& paths, const ResolvedVersion& version) {
    fs::path indexFile = paths.assetIndexes / (version.assetsId + L".json");
    Json index;
    std::wstring err;
    if (pathExists(indexFile) && parseJsonFile(indexFile, index, err)) {
        bool virtualAssets = index["virtual"].asBool(false) || index["map_to_resources"].asBool(false);
        if (virtualAssets) {
            fs::path v = paths.assets / L"virtual" / version.assetsId;
            if (isDirectory(v)) return v.wstring();
        }
    }
    return paths.assets.wstring();
}

} // namespace aurora
