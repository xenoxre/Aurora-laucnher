#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace aurora {

class Sha1 {
public:
    Sha1();
    void update(const void* data, size_t len);
    void update(const std::string& s) { update(s.data(), s.size()); }
    void finish(uint8_t out[20]);
    static std::vector<uint8_t> digest(const void* data, size_t len);
    static std::string hexDigest(const void* data, size_t len);
    static bool fileDigest(const std::wstring& path, std::string& hexOut);
private:
    void block(const uint8_t* p);
    uint32_t h_[5];
    uint64_t bits_;
    uint8_t buf_[64];
    size_t used_;
};

} // namespace aurora
