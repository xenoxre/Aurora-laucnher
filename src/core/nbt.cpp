#include "nbt.h"
#include "log.h"

#include "miniz.h"

#include <cstring>

namespace aurora {

NbtTag* NbtTag::find(const std::string& key) {
    for (auto& c : children) if (c.name == key) return &c;
    return nullptr;
}
const NbtTag* NbtTag::find(const std::string& key) const {
    for (auto& c : children) if (c.name == key) return &c;
    return nullptr;
}
std::string NbtTag::getString(const std::string& key, const std::string& def) const {
    auto* t = find(key);
    return t && t->type == Type::String ? t->str : def;
}
int64_t NbtTag::getInt(const std::string& key, int64_t def) const {
    auto* t = find(key);
    if (!t) return def;
    if (t->type == Type::Byte || t->type == Type::Short || t->type == Type::Int ||
        t->type == Type::Long) return t->number;
    return def;
}

namespace {

struct Reader {
    const uint8_t* p;
    const uint8_t* end;
    std::wstring error;

    bool need(size_t n) {
        if ((size_t)(end - p) < n) { error = L"NBT tronqué"; return false; }
        return true;
    }
    bool u8(uint8_t& v) { if (!need(1)) return false; v = *p++; return true; }
    bool u16(uint16_t& v) {
        if (!need(2)) return false;
        v = (uint16_t)((p[0] << 8) | p[1]); p += 2; return true;
    }
    bool u32(uint32_t& v) {
        if (!need(4)) return false;
        v = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
        p += 4; return true;
    }
    bool u64(uint64_t& v) {
        if (!need(8)) return false;
        v = 0;
        for (int i = 0; i < 8; i++) v = (v << 8) | p[i];
        p += 8; return true;
    }
    bool str8(std::string& out) {
        uint16_t len = 0;
        if (!u16(len)) return false;
        if (!need(len)) return false;
        out.assign((const char*)p, len);
        p += len;
        return true;
    }
    bool payload(NbtTag& tag, uint8_t type, bool named);
};

bool Reader::payload(NbtTag& tag, uint8_t type, bool named) {
    tag.type = (NbtTag::Type)type;
    if (named && !str8(tag.name)) return false;
    switch ((NbtTag::Type)type) {
        case NbtTag::Type::End: return true;
        case NbtTag::Type::Byte: {
            uint8_t v; if (!u8(v)) return false; tag.number = (int8_t)v; return true;
        }
        case NbtTag::Type::Short: {
            uint16_t v; if (!u16(v)) return false; tag.number = (int16_t)v; return true;
        }
        case NbtTag::Type::Int: {
            uint32_t v; if (!u32(v)) return false; tag.number = (int32_t)v; return true;
        }
        case NbtTag::Type::Long: {
            uint64_t v; if (!u64(v)) return false; tag.number = (int64_t)v; return true;
        }
        case NbtTag::Type::Float: {
            uint32_t v; if (!u32(v)) return false; tag.number = v; return true;
        }
        case NbtTag::Type::Double: {
            uint64_t v; if (!u64(v)) return false; tag.number = (int64_t)v; return true;
        }
        case NbtTag::Type::ByteArray: {
            uint32_t n; if (!u32(n)) return false;
            if (!need(n)) { error = L"ByteArray NBT invalide"; return false; }
            tag.raw.assign(p, p + n); p += n; return true;
        }
        case NbtTag::Type::String: return str8(tag.str);
        case NbtTag::Type::List: {
            uint8_t elemType = 0;
            uint32_t count = 0;
            if (!u8(elemType) || !u32(count)) return false;
            tag.listType = (NbtTag::Type)elemType;
            tag.children.clear();
            for (uint32_t i = 0; i < count; i++) {
                NbtTag child;
                if (!payload(child, elemType, false)) return false;
                tag.children.push_back(std::move(child));
            }
            return true;
        }
        case NbtTag::Type::Compound: {
            for (;;) {
                uint8_t childType = 0;
                if (!u8(childType)) return false;
                if (childType == 0) break;
                NbtTag child;
                if (!payload(child, childType, true)) return false;
                tag.children.push_back(std::move(child));
            }
            return true;
        }
        case NbtTag::Type::IntArray: {
            uint32_t n; if (!u32(n)) return false;
            if (!need((size_t)n * 4)) return false;
            p += (size_t)n * 4; return true;
        }
        case NbtTag::Type::LongArray: {
            uint32_t n; if (!u32(n)) return false;
            if (!need((size_t)n * 8)) return false;
            p += (size_t)n * 8; return true;
        }
        default:
            error = L"Type NBT inconnu";
            return false;
    }
}

void writeU16(Bytes& out, uint16_t v) {
    out.push_back((uint8_t)(v >> 8));
    out.push_back((uint8_t)v);
}
void writeU32(Bytes& out, uint32_t v) {
    out.push_back((uint8_t)(v >> 24)); out.push_back((uint8_t)(v >> 16));
    out.push_back((uint8_t)(v >> 8)); out.push_back((uint8_t)v);
}
void writeU64(Bytes& out, uint64_t v) {
    for (int i = 7; i >= 0; i--) out.push_back((uint8_t)(v >> (i * 8)));
}
void writeStr(Bytes& out, const std::string& s) {
    writeU16(out, (uint16_t)s.size());
    out.insert(out.end(), s.begin(), s.end());
}

void writePayload(Bytes& out, const NbtTag& tag, bool named);

void writePayload(Bytes& out, const NbtTag& tag, bool named) {
    if (named) writeStr(out, tag.name);
    switch (tag.type) {
        case NbtTag::Type::Byte: out.push_back((uint8_t)tag.number); break;
        case NbtTag::Type::Short: writeU16(out, (uint16_t)tag.number); break;
        case NbtTag::Type::Int: writeU32(out, (uint32_t)tag.number); break;
        case NbtTag::Type::Long: writeU64(out, (uint64_t)tag.number); break;
        case NbtTag::Type::Float: writeU32(out, (uint32_t)tag.number); break;
        case NbtTag::Type::Double: writeU64(out, (uint64_t)tag.number); break;
        case NbtTag::Type::ByteArray:
            writeU32(out, (uint32_t)tag.raw.size());
            out.insert(out.end(), tag.raw.begin(), tag.raw.end());
            break;
        case NbtTag::Type::String: writeStr(out, tag.str); break;
        case NbtTag::Type::List: {
            NbtTag::Type elem = tag.listType;
            if (elem == NbtTag::Type::End && !tag.children.empty()) elem = tag.children[0].type;
            out.push_back((uint8_t)elem);
            writeU32(out, (uint32_t)tag.children.size());
            for (auto& c : tag.children) writePayload(out, c, false);
            break;
        }
        case NbtTag::Type::Compound:
            for (auto& c : tag.children) {
                out.push_back((uint8_t)c.type);
                writePayload(out, c, true);
            }
            out.push_back(0);
            break;
        case NbtTag::Type::IntArray:
        case NbtTag::Type::LongArray:
            writeU32(out, 0);
            break;
        case NbtTag::Type::End: break;
    }
}

} // namespace

bool nbtParse(const Bytes& data, NbtTag& root, std::wstring& error) {
    Reader r{ data.data(), data.data() + data.size(), L"" };
    uint8_t type = 0;
    if (!r.u8(type)) { error = L"NBT vide"; return false; }
    if (type == 0) { error = L"NBT vide"; return false; }
    if (!r.payload(root, type, true)) { error = r.error; return false; }
    return true;
}

Bytes nbtWrite(const NbtTag& root) {
    Bytes out;
    out.push_back((uint8_t)root.type);
    writePayload(out, root, true);
    return out;
}

bool gzipDecompress(const Bytes& in, Bytes& out) {
    if (in.size() < 18 || in[0] != 0x1F || in[1] != 0x8B) return false;
    size_t pos = 10;
    uint8_t flg = in[3];
    if (flg & 4) {
        if (pos + 2 > in.size()) return false;
        size_t xlen = (size_t)in[pos] | ((size_t)in[pos + 1] << 8);
        pos += 2 + xlen;
    }
    if (flg & 8) { while (pos < in.size() && in[pos] != 0) pos++; pos++; }
    if (flg & 16) { while (pos < in.size() && in[pos] != 0) pos++; pos++; }
    if (flg & 2) pos += 2;
    if (pos + 8 > in.size()) return false;
    size_t compSize = in.size() - pos - 8;
    size_t outLen = 0;
    void* p = tinfl_decompress_mem_to_heap(in.data() + pos, compSize, &outLen, 0);
    if (!p) return false;
    out.assign((uint8_t*)p, (uint8_t*)p + outLen);
    mz_free(p);
    return true;
}

bool nbtParseFile(const fs::path& path, NbtTag& root, std::wstring& error) {
    Bytes data;
    if (!readFileBytes(path, data)) { error = L"Fichier NBT introuvable"; return false; }
    if (data.size() > 2 && data[0] == 0x1F && data[1] == 0x8B) {
        Bytes plain;
        if (!gzipDecompress(data, plain)) { error = L"Décompression gzip impossible"; return false; }
        data = std::move(plain);
    }
    return nbtParse(data, root, error);
}

bool nbtWriteFile(const fs::path& path, const NbtTag& root, std::wstring& error) {
    if (!writeFileAtomic(path, nbtWrite(root))) {
        error = L"Écriture NBT impossible";
        return false;
    }
    return true;
}

} // namespace aurora
