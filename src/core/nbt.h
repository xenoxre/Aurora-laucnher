#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "util.h"

namespace aurora {

struct NbtTag {
    enum class Type : uint8_t {
        End = 0, Byte = 1, Short = 2, Int = 3, Long = 4, Float = 5, Double = 6,
        ByteArray = 7, String = 8, List = 9, Compound = 10, IntArray = 11, LongArray = 12
    };
    Type type = Type::Compound;
    Type listType = Type::End;
    std::string name;
    int64_t number = 0;
    std::string str;
    std::vector<NbtTag> children;
    Bytes raw;

    NbtTag* find(const std::string& key);
    const NbtTag* find(const std::string& key) const;
    std::string getString(const std::string& key, const std::string& def = "") const;
    int64_t getInt(const std::string& key, int64_t def = 0) const;
};

bool nbtParse(const Bytes& data, NbtTag& root, std::wstring& error);
Bytes nbtWrite(const NbtTag& root);
bool nbtParseFile(const std::filesystem::path& path, NbtTag& root, std::wstring& error);
bool nbtWriteFile(const std::filesystem::path& path, const NbtTag& root, std::wstring& error);
bool gzipDecompress(const Bytes& in, Bytes& out);

} // namespace aurora
