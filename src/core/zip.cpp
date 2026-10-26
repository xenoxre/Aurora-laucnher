#include "zip.h"
#include "log.h"

#include "miniz.h"

#include <algorithm>

namespace aurora {

static bool safeEntryPath(const std::string& name, fs::path& rel) {
    if (name.empty()) return false;
    if (name[0] == '/' || name[0] == '\\') return false;
    if (name.find(':') != std::string::npos) return false;
    std::wstring w = utf8ToWide(name);
    std::replace(w.begin(), w.end(), L'/', L'\\');
    rel.clear();
    size_t start = 0;
    while (start <= w.size()) {
        size_t pos = w.find(L'\\', start);
        std::wstring part = w.substr(start, pos == std::wstring::npos ? std::wstring::npos : pos - start);
        if (part.empty() || part == L".") { }
        else if (part == L"..") return false;
        else rel /= part;
        if (pos == std::wstring::npos) break;
        start = pos + 1;
    }
    return !rel.empty();
}

bool zipExtract(const fs::path& zipPath, const fs::path& dest,
                const std::function<bool(const std::string& name)>& filter,
                std::wstring& error) {
    Bytes data;
    if (!readFileBytes(zipPath, data)) {
        error = L"Archive introuvable : " + zipPath.wstring();
        return false;
    }
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, data.data(), data.size(), 0)) {
        error = L"Archive ZIP invalide : " + zipPath.filename().wstring();
        return false;
    }
    ensureDir(dest);
    bool ok = true;
    mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count; i++) {
        mz_zip_archive_file_stat st{};
        if (!mz_zip_reader_file_stat(&zip, i, &st)) continue;
        if (st.m_is_directory) continue;
        std::string name = st.m_filename;
        if (filter && !filter(name)) continue;
        fs::path rel;
        if (!safeEntryPath(name, rel)) {
            logWarn(L"Entrée ZIP ignorée (chemin non sûr) : " + utf8ToWide(name), L"ZIP");
            continue;
        }
        size_t size = 0;
        void* ptr = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
        if (!ptr) { ok = false; continue; }
        fs::path out = dest / rel;
        ensureDir(out.parent_path());
        if (!writeFileBytes(out, Bytes((uint8_t*)ptr, (uint8_t*)ptr + size))) ok = false;
        mz_free(ptr);
    }
    mz_zip_reader_end(&zip);
    if (!ok) error = L"Extraction incomplète : " + zipPath.filename().wstring();
    return ok;
}

bool zipExtractAll(const fs::path& zipPath, const fs::path& dest, std::wstring& error) {
    return zipExtract(zipPath, dest, {}, error);
}

bool zipReadEntry(const fs::path& zipPath, const std::string& entryName, Bytes& out,
                  std::wstring& error) {
    Bytes data;
    if (!readFileBytes(zipPath, data)) {
        error = L"Archive introuvable";
        return false;
    }
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, data.data(), data.size(), 0)) {
        error = L"Archive ZIP invalide";
        return false;
    }
    int index = mz_zip_reader_locate_file(&zip, entryName.c_str(), nullptr, 0);
    if (index < 0) {
        mz_zip_reader_end(&zip);
        error = L"Entrée absente : " + utf8ToWide(entryName);
        return false;
    }
    size_t size = 0;
    void* ptr = mz_zip_reader_extract_to_heap(&zip, (mz_uint)index, &size, 0);
    if (!ptr) {
        mz_zip_reader_end(&zip);
        error = L"Lecture impossible";
        return false;
    }
    out.assign((uint8_t*)ptr, (uint8_t*)ptr + size);
    mz_free(ptr);
    mz_zip_reader_end(&zip);
    return true;
}

bool zipList(const fs::path& zipPath, std::vector<std::string>& names, std::wstring& error) {
    Bytes data;
    if (!readFileBytes(zipPath, data)) {
        error = L"Archive introuvable";
        return false;
    }
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, data.data(), data.size(), 0)) {
        error = L"Archive ZIP invalide";
        return false;
    }
    mz_uint count = mz_zip_reader_get_num_files(&zip);
    names.clear();
    for (mz_uint i = 0; i < count; i++) {
        char name[512];
        if (mz_zip_reader_get_filename(&zip, i, name, sizeof(name)) > 0) names.push_back(name);
    }
    mz_zip_reader_end(&zip);
    return true;
}

bool zipCreate(const fs::path& zipPath,
               const std::vector<std::pair<std::string, Bytes>>& entries,
               std::wstring& error) {
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) {
        error = L"Création ZIP impossible";
        return false;
    }
    for (auto& e : entries) {
        if (!mz_zip_writer_add_mem(&zip, e.first.c_str(), e.second.data(), e.second.size(),
                                   MZ_BEST_COMPRESSION)) {
            mz_zip_writer_end(&zip);
            error = L"Ajout ZIP impossible";
            return false;
        }
    }
    void* buf = nullptr;
    size_t size = 0;
    if (!mz_zip_writer_finalize_heap_archive(&zip, &buf, &size)) {
        mz_zip_writer_end(&zip);
        error = L"Finalisation ZIP impossible";
        return false;
    }
    bool ok = writeFileBytes(zipPath, Bytes((uint8_t*)buf, (uint8_t*)buf + size));
    mz_free(buf);
    mz_zip_writer_end(&zip);
    if (!ok) error = L"Écriture ZIP impossible";
    return ok;
}

} // namespace aurora
