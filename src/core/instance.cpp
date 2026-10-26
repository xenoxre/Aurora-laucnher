#include "instance.h"
#include "json.h"
#include "log.h"
#include "meta.h"
#include "util.h"

#include <algorithm>

namespace aurora {

static fs::path metaPath(const Paths& paths, const std::wstring& id) {
    return paths.instanceDir(id) / L"instance.json";
}

bool loadInstance(const Paths& paths, const std::wstring& id, Instance& out) {
    fs::path file = metaPath(paths, id);
    std::string text;
    if (!readFileText(file, text)) return false;
    Json j;
    std::string err;
    if (!Json::parse(text, j, &err)) return false;
    out.id = j["id"].asWString(id);
    out.name = j["name"].asWString(id);
    out.mcVersion = j["mcVersion"].asWString(id);
    out.loader = j["loader"].asWString(L"none");
    out.loaderVersion = j["loaderVersion"].asWString();
    out.versionId = j["versionId"].asWString(id);
    out.dir = paths.instanceDir(id);
    if (out.versionId.empty()) out.versionId = out.id;
    if (out.mcVersion.empty()) out.mcVersion = out.versionId;
    if (out.loader.empty()) out.loader = L"none";
    return true;
}

bool scanInstances(const Paths& paths, std::vector<Instance>& out) {
    out.clear();
    for (auto& d : listDirectory(paths.instances, true)) {
        std::wstring id = d.filename().wstring();
        Instance inst;
        if (loadInstance(paths, id, inst)) {
            // Instances stay visible even when their Minecraft version is not
            // installed yet: launching them downloads what is missing.
            out.push_back(inst);
            continue;
        }
        // Adopt versions installed without an instance record.
        if (isVersionInstalled(paths, id)) {
            inst.id = id;
            inst.name = id;
            inst.mcVersion = id;
            inst.versionId = id;
            inst.loader = L"none";
            inst.dir = d;
            std::wstring err;
            if (createInstance(paths, inst, err)) out.push_back(inst);
        }
    }
    std::sort(out.begin(), out.end(), [](const Instance& a, const Instance& b) {
        return toLower(a.name) < toLower(b.name);
    });
    return true;
}

bool createInstance(const Paths& paths, const Instance& inst, std::wstring& error) {
    if (inst.id.empty() || inst.versionId.empty()) {
        error = L"Instance invalide";
        return false;
    }
    fs::path dir = paths.instanceDir(inst.id);
    if (!ensureDir(dir)) { error = L"Création du dossier impossible"; return false; }
    ensureDir(dir / L"mods");
    ensureDir(dir / L"saves");
    ensureDir(dir / L"resourcepacks");
    ensureDir(dir / L"screenshots");
    ensureDir(dir / L"logs");
    Json j = Json::object();
    j.set("id", inst.id);
    j.set("name", inst.name.empty() ? inst.id : inst.name);
    j.set("mcVersion", inst.mcVersion);
    j.set("loader", inst.loader.empty() ? L"none" : inst.loader);
    j.set("loaderVersion", inst.loaderVersion);
    j.set("versionId", inst.versionId);
    j.set("createdAt", nowStampUtf8());
    if (!writeFileTextAtomic(metaPath(paths, inst.id), j.dump(2))) {
        error = L"Écriture instance.json impossible";
        return false;
    }
    return true;
}

bool deleteInstance(const Paths& paths, const std::wstring& id, std::wstring& error) {
    if (id.empty()) { error = L"Instance inconnue"; return false; }
    if (!removeAll(paths.instanceDir(id))) {
        error = L"Suppression impossible (jeu ouvert ?)";
        return false;
    }
    return true;
}

Instance* findInstance(std::vector<Instance>& list, const std::wstring& id) {
    for (auto& i : list) if (i.id == id) return &i;
    return nullptr;
}

const Instance* findInstance(const std::vector<Instance>& list, const std::wstring& id) {
    for (auto& i : list) if (i.id == id) return &i;
    return nullptr;
}

} // namespace aurora
