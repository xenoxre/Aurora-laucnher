#include "sha1.h"
#include <cstdio>
#include <cstring>

namespace aurora {

static inline uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

Sha1::Sha1() : bits_(0), used_(0) {
    h_[0] = 0x67452301; h_[1] = 0xEFCDAB89; h_[2] = 0x98BADCFE;
    h_[3] = 0x10325476; h_[4] = 0xC3D2E1F0;
}

void Sha1::block(const uint8_t* p) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t(p[i * 4]) << 24) | (uint32_t(p[i * 4 + 1]) << 16) |
               (uint32_t(p[i * 4 + 2]) << 8) | uint32_t(p[i * 4 + 3]);
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6; }
        uint32_t t = rol(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d; h_[4] += e;
}

void Sha1::update(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    bits_ += uint64_t(len) * 8;
    while (len) {
        size_t n = 64 - used_;
        if (n > len) n = len;
        memcpy(buf_ + used_, p, n);
        used_ += n; p += n; len -= n;
        if (used_ == 64) { block(buf_); used_ = 0; }
    }
}

void Sha1::finish(uint8_t out[20]) {
    uint64_t bits = bits_;
    uint8_t pad = 0x80;
    update(&pad, 1);
    uint8_t zero = 0;
    while (used_ != 56) update(&zero, 1);
    uint8_t lenBuf[8];
    for (int i = 0; i < 8; i++) lenBuf[i] = uint8_t(bits >> (56 - i * 8));
    update(lenBuf, 8);
    for (int i = 0; i < 5; i++) {
        out[i * 4] = uint8_t(h_[i] >> 24);
        out[i * 4 + 1] = uint8_t(h_[i] >> 16);
        out[i * 4 + 2] = uint8_t(h_[i] >> 8);
        out[i * 4 + 3] = uint8_t(h_[i]);
    }
}

std::vector<uint8_t> Sha1::digest(const void* data, size_t len) {
    Sha1 s; s.update(data, len);
    uint8_t out[20]; s.finish(out);
    return std::vector<uint8_t>(out, out + 20);
}

std::string Sha1::hexDigest(const void* data, size_t len) {
    std::vector<uint8_t> d = digest(data, len);
    static const char* hx = "0123456789abcdef";
    std::string out;
    out.reserve(40);
    for (uint8_t b : d) { out.push_back(hx[b >> 4]); out.push_back(hx[b & 15]); }
    return out;
}

bool Sha1::fileDigest(const std::wstring& path, std::string& hexOut) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;
    Sha1 s;
    std::vector<uint8_t> buf(1 << 20);
    size_t n;
    while ((n = fread(buf.data(), 1, buf.size(), f)) > 0) s.update(buf.data(), n);
    bool ok = ferror(f) == 0;
    fclose(f);
    if (!ok) return false;
    uint8_t d[20]; s.finish(d);
    static const char* hx = "0123456789abcdef";
    hexOut.clear();
    hexOut.reserve(40);
    for (uint8_t b : d) { hexOut.push_back(hx[b >> 4]); hexOut.push_back(hx[b & 15]); }
    return true;
}

} // namespace aurora
