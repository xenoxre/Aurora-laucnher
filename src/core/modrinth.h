#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace aurora {

struct ModHit {
    std::wstring id;
    std::wstring slug;
    std::wstring title;
    std::wstring description;
    std::wstring author;
    std::wstring iconUrl;
    std::wstring categories;
    uint64_t downloads = 0;
};

struct ModFile {
    std::wstring url;
    std::wstring filename;
    std::wstring versionName;
    uint64_t size = 0;
};

bool modrinthSearch(const std::wstring& query, const std::wstring& loader,
                    const std::wstring& gameVersion, int offset,
                    std::vector<ModHit>& hits, int& total, std::wstring& error);

bool modrinthGetFile(const std::wstring& projectId, const std::wstring& loader,
                     const std::wstring& gameVersion, ModFile& file, std::wstring& error);

} // namespace aurora
