#include "modrinth.h"
#include "http.h"
#include "json.h"
#include "log.h"
#include "util.h"
#include "version.h"

namespace aurora {

static const wchar_t* kApi = L"https://api.modrinth.com/v2";

bool modrinthSearch(const std::wstring& query, const std::wstring& loader,
                    const std::wstring& gameVersion, int offset,
                    std::vector<ModHit>& hits, int& total, std::wstring& error) {
    hits.clear();
    total = 0;
    std::string facets = "[[\"project_type:mod\"]";
    if (!loader.empty() && loader != L"none" && loader != L"vanilla") {
        facets += ",[\"categories:" + wideToUtf8(loader) + "\"]";
    }
    if (!gameVersion.empty()) {
        facets += ",[\"versions:" + wideToUtf8(gameVersion) + "\"]";
    }
    facets += "]";
    std::wstring url = std::wstring(kApi) + L"/search?query=" + utf8ToWide(urlEncode(wideToUtf8(query))) +
                       L"&limit=20&offset=" + std::to_wstring(offset) +
                       L"&index=relevance&facets=" + utf8ToWide(urlEncode(facets));
    Json root;
    if (!httpGetJson(url, root, error,
                     { { L"User-Agent", L"AuroraLauncher/" AURORA_VERSION_W L" (Windows)" } }))
        return false;
    total = (int)root["total_hits"].asInt(0);
    for (auto& h : root["hits"].items()) {
        ModHit hit;
        hit.id = h["project_id"].asWString();
        hit.slug = h["slug"].asWString();
        hit.title = h["title"].asWString();
        hit.description = h["description"].asWString();
        hit.author = h["author"].asWString();
        hit.iconUrl = h["icon_url"].asWString();
        hit.downloads = (uint64_t)h["downloads"].asInt(0);
        std::wstring cats;
        for (auto& c : h["categories"].items()) {
            if (!cats.empty()) cats += L", ";
            cats += c.asWString();
        }
        hit.categories = cats;
        if (!hit.id.empty()) hits.push_back(hit);
    }
    logInfo(L"Modrinth : " + std::to_wstring(hits.size()) + L" résultat(s) pour « " +
            (query.empty() ? L"populaires" : query) + L" »", L"Mods");
    return true;
}

bool modrinthGetFile(const std::wstring& projectId, const std::wstring& loader,
                     const std::wstring& gameVersion, ModFile& file, std::wstring& error) {
    std::wstring url = std::wstring(kApi) + L"/project/" + projectId + L"/version?loaders=%5B%22" +
                       loader + L"%22%5D&game_versions=%5B%22" + gameVersion + L"%22%5D";
    Json list;
    if (!httpGetJson(url, list, error,
                     { { L"User-Agent", L"AuroraLauncher/" AURORA_VERSION_W L" (Windows)" } }))
        return false;
    if (!list.isArray() || list.size() == 0) {
        error = L"Aucune version compatible avec " + gameVersion + L" / " + loader;
        return false;
    }
    const Json& first = list.at(0);
    file.versionName = first["name"].asWString();
    for (auto& f : first["files"].items()) {
        if (f["primary"].asBool(false) || file.url.empty()) {
            file.url = f["url"].asWString();
            file.filename = f["filename"].asWString();
            file.size = (uint64_t)f["size"].asInt(0);
            if (f["primary"].asBool(false)) break;
        }
    }
    if (file.url.empty()) { error = L"Fichier de mod introuvable"; return false; }
    return true;
}

} // namespace aurora
