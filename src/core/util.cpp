#include "util.h"
#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wincrypt.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cwctype>
#include <random>

namespace aurora {

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

std::string wideToUtf8(const std::wstring& s) {
    if (s.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring toLower(std::wstring s) {
    for (auto& c : s) c = (wchar_t)std::towlower(c);
    return s;
}

std::wstring toUpper(std::wstring s) {
    for (auto& c : s) c = (wchar_t)std::towupper(c);
    return s;
}

std::wstring trim(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) a++;
    while (b > a && iswspace(s[b - 1])) b--;
    return s.substr(a, b - a);
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (unsigned char)s[a] <= ' ') a++;
    while (b > a && (unsigned char)s[b - 1] <= ' ') b--;
    return s.substr(a, b - a);
}

bool iequals(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (std::towlower(a[i]) != std::towlower(b[i])) return false;
    return true;
}

bool containsNoCase(const std::wstring& haystack, const std::wstring& needle) {
    if (needle.empty()) return true;
    std::wstring h = toLower(haystack), n = toLower(needle);
    return h.find(n) != std::wstring::npos;
}

bool startsWith(const std::wstring& s, const std::wstring& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::wstring replaceAll(std::wstring s, const std::wstring& from, const std::wstring& to) {
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::wstring::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::vector<std::wstring> split(const std::wstring& s, wchar_t sep) {
    std::vector<std::wstring> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); i++) {
        if (i == s.size() || s[i] == sep) {
            out.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    return out;
}

std::wstring join(const std::vector<std::wstring>& v, const std::wstring& sep) {
    std::wstring out;
    for (size_t i = 0; i < v.size(); i++) {
        if (i) out += sep;
        out += v[i];
    }
    return out;
}

std::wstring formatW(const wchar_t* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    wchar_t buf[2048];
    int n = _vsnwprintf(buf, 2047, fmt, ap);
    va_end(ap);
    if (n < 0) {
        std::vector<wchar_t> big(8192);
        va_start(ap, fmt);
        n = _vsnwprintf(big.data(), big.size() - 1, fmt, ap);
        va_end(ap);
        if (n < 0) return L"?";
        return std::wstring(big.data(), (size_t)n);
    }
    return std::wstring(buf, (size_t)n);
}

std::string formatA(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    char buf[4096];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return "?";
    return std::string(buf, (size_t)std::min<size_t>((size_t)n, sizeof(buf) - 1));
}

std::wstring stripMinecraftColors(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == L'\u00a7' && i + 1 < s.size()) {
            // Skip §x§R§R§G§G§B§B hex sequences too.
            if (s[i + 1] == L'x' || s[i + 1] == L'X') {
                size_t j = i + 2;
                int count = 0;
                while (j + 1 < s.size() && s[j] == L'\u00a7' && count < 6) { j += 2; count++; }
                i = j - 1;
            } else {
                i++;
            }
            continue;
        }
        out.push_back(s[i]);
    }
    return out;
}

std::wstring initials(const std::wstring& name) {
    std::wstring out;
    bool start = true;
    for (wchar_t c : name) {
        if (start && !iswspace(c)) {
            out.push_back((wchar_t)std::towupper(c));
            if (out.size() >= 2) break;
            start = false;
        }
        if (iswspace(c)) start = true;
    }
    if (out.empty()) out = L"?";
    return out;
}

std::string hexEncode(const uint8_t* data, size_t len) {
    static const char* hx = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; i++) { out.push_back(hx[data[i] >> 4]); out.push_back(hx[data[i] & 15]); }
    return out;
}

std::string hexEncode(const Bytes& data) { return hexEncode(data.data(), data.size()); }

std::vector<uint8_t> md5Digest(const void* data, size_t len) {
    std::vector<uint8_t> out;
    HCRYPTPROV prov = 0;
    HCRYPTHASH hash = 0;
    if (!CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
        return out;
    if (!CryptCreateHash(prov, CALG_MD5, 0, 0, &hash)) {
        CryptReleaseContext(prov, 0);
        return out;
    }
    CryptHashData(hash, (const BYTE*)data, (DWORD)len, 0);
    DWORD size = 16;
    out.resize(16);
    if (!CryptGetHashParam(hash, HP_HASHVAL, out.data(), &size, 0)) out.clear();
    else out.resize(size);
    CryptDestroyHash(hash);
    CryptReleaseContext(prov, 0);
    return out;
}

std::wstring offlineUuid(const std::wstring& playerName) {
    std::string input = "OfflinePlayer:" + wideToUtf8(playerName);
    std::vector<uint8_t> d = md5Digest(input.data(), input.size());
    if (d.size() != 16) return L"00000000-0000-3000-8000-000000000000";
    d[6] = (uint8_t)((d[6] & 0x0F) | 0x30);
    d[8] = (uint8_t)((d[8] & 0x3F) | 0x80);
    std::wstring hex = utf8ToWide(hexEncode(d));
    return hex.substr(0, 8) + L"-" + hex.substr(8, 4) + L"-" + hex.substr(12, 4) + L"-" +
           hex.substr(16, 4) + L"-" + hex.substr(20, 12);
}

Bytes hexDecode(const std::string& hex) {
    Bytes out;
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    if (hex.size() % 2) return out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        int a = val(hex[i]), b = val(hex[i + 1]);
        if (a < 0 || b < 0) return {};
        out.push_back(uint8_t((a << 4) | b));
    }
    return out;
}

static const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    size_t i = 0;
    while (i + 2 < len) {
        uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) | data[i + 2];
        out.push_back(kB64[(v >> 18) & 63]); out.push_back(kB64[(v >> 12) & 63]);
        out.push_back(kB64[(v >> 6) & 63]); out.push_back(kB64[v & 63]);
        i += 3;
    }
    if (i + 1 == len) {
        uint32_t v = uint32_t(data[i]) << 16;
        out.push_back(kB64[(v >> 18) & 63]); out.push_back(kB64[(v >> 12) & 63]);
        out.push_back('='); out.push_back('=');
    } else if (i + 2 == len) {
        uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8);
        out.push_back(kB64[(v >> 18) & 63]); out.push_back(kB64[(v >> 12) & 63]);
        out.push_back(kB64[(v >> 6) & 63]); out.push_back('=');
    }
    return out;
}

std::string base64Encode(const Bytes& data) { return base64Encode(data.data(), data.size()); }

Bytes base64Decode(const std::string& text, bool* ok) {
    if (ok) *ok = true;
    int rev[256];
    for (int i = 0; i < 256; i++) rev[i] = -1;
    for (int i = 0; i < 64; i++) rev[(unsigned char)kB64[i]] = i;
    Bytes out;
    out.reserve(text.size() * 3 / 4);
    uint32_t acc = 0;
    int bits = 0;
    for (unsigned char c : text) {
        if (c == '=' || c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
        int v = rev[c];
        if (v < 0) { if (ok) *ok = false; return {}; }
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(uint8_t((acc >> bits) & 0xFF));
        }
    }
    return out;
}

std::string urlEncode(const std::string& s) {
    static const char* hx = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
            out.push_back((char)c);
        else {
            out.push_back('%');
            out.push_back(hx[c >> 4]);
            out.push_back(hx[c & 15]);
        }
    }
    return out;
}

std::string urlEncodeComponent(const std::wstring& s) { return urlEncode(wideToUtf8(s)); }

std::wstring humanBytes(uint64_t bytes) {
    const wchar_t* units[] = { L"o", L"Ko", L"Mo", L"Go", L"To" };
    double v = (double)bytes;
    int u = 0;
    while (v >= 1024.0 && u < 4) { v /= 1024.0; u++; }
    if (u == 0) return formatW(L"%llu o", (unsigned long long)bytes);
    std::wstring s = formatW(L"%.1f", v);
    if (s.find(L'.') != std::wstring::npos) {
        while (!s.empty() && s.back() == L'0') s.pop_back();
        if (!s.empty() && s.back() == L'.') s.pop_back();
    }
    return s + L" " + units[u];
}

std::wstring humanDuration(double seconds) {
    if (seconds < 1) return L"< 1 s";
    int s = (int)seconds;
    if (s < 60) return formatW(L"%d s", s);
    int m = s / 60;
    if (m < 60) return formatW(L"%d min %d s", m, s % 60);
    return formatW(L"%d h %02d min", m / 60, m % 60);
}

std::wstring humanCount(uint64_t n) {
    std::wstring digits = std::to_wstring(n);
    std::wstring out;
    int c = 0;
    for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
        if (c && c % 3 == 0) out.push_back(L' ');
        out.push_back(*it);
        c++;
    }
    std::reverse(out.begin(), out.end());
    return out;
}

std::wstring formatDateFr(const std::wstring& iso) {
    int y = 0, m = 0, d = 0;
    if (swscanf(iso.c_str(), L"%d-%d-%d", &y, &m, &d) != 3) return iso;
    static const wchar_t* months[] = { L"", L"janv.", L"févr.", L"mars", L"avr.", L"mai", L"juin",
                                       L"juil.", L"août", L"sept.", L"oct.", L"nov.", L"déc." };
    if (m < 1 || m > 12) return iso;
    return formatW(L"%d %s %d", d, months[m], y);
}

std::wstring nowTime() {
    SYSTEMTIME st; GetLocalTime(&st);
    return formatW(L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
}

std::wstring nowStamp() {
    SYSTEMTIME st; GetLocalTime(&st);
    return formatW(L"%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

std::string nowStampUtf8() { return wideToUtf8(nowStamp()); }

int64_t nowUnix() { return (int64_t)time(nullptr); }

std::wstring unixToDate(int64_t unixTime) {
    time_t t = (time_t)unixTime;
    tm local{};
    localtime_s(&local, &t);
    return formatW(L"%02d/%02d/%04d", local.tm_mday, local.tm_mon + 1, local.tm_year + 1900);
}

std::wstring quoteWinArg(const std::wstring& arg) {
    if (arg.empty()) return L"\"\"";
    bool need = false;
    for (wchar_t c : arg) {
        if (iswspace(c) || c == L'"') { need = true; break; }
    }
    if (!need) return arg;
    std::wstring out = L"\"";
    int backslashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') { backslashes++; continue; }
        if (c == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
            out.push_back(L'"');
        } else {
            out.append(backslashes, L'\\');
            out.push_back(c);
        }
        backslashes = 0;
    }
    out.append(backslashes * 2, L'\\');
    out.push_back(L'"');
    return out;
}

std::wstring buildCommandLine(const std::vector<std::wstring>& args) {
    std::wstring out;
    for (size_t i = 0; i < args.size(); i++) {
        if (i) out.push_back(L' ');
        out += quoteWinArg(args[i]);
    }
    return out;
}

std::wstring executablePath() {
    wchar_t buf[32768];
    DWORD n = GetModuleFileNameW(nullptr, buf, 32767);
    return std::wstring(buf, n);
}

fs::path executableDir() {
    fs::path p(executablePath());
    return p.parent_path();
}

fs::path knownFolder(int csidl) {
    wchar_t buf[MAX_PATH * 2];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, csidl, nullptr, SHGFP_TYPE_CURRENT, buf)))
        return fs::path(buf);
    return {};
}

fs::path defaultDataDir() {
    wchar_t env[32768];
    DWORD n = GetEnvironmentVariableW(L"AURORA_DATA_DIR", env, 32767);
    if (n > 0) return fs::path(env);
    fs::path exe = executableDir();
    if (fs::exists(exe / L"portable.txt")) return exe / L"data";
    fs::path appdata = knownFolder(CSIDL_APPDATA);
    if (appdata.empty()) appdata = exe;
    return appdata / L"AuroraLauncher";
}

void openInExplorer(const fs::path& p) {
    ShellExecuteW(nullptr, L"open", p.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void openUrl(const std::wstring& url) {
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void sleepMs(int ms) { Sleep((DWORD)ms); }

std::wstring readRegistryString(const wchar_t* subkey, const wchar_t* value) {
    wchar_t buf[4096];
    DWORD size = sizeof(buf) - sizeof(wchar_t);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, subkey, value, RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS) {
        return std::wstring(buf, size / sizeof(wchar_t) > 0 ? (size / sizeof(wchar_t)) - 1 : 0);
    }
    return {};
}

bool ensureDir(const fs::path& p) {
    std::error_code ec;
    if (fs::is_directory(p, ec)) return true;
    fs::create_directories(p, ec);
    return !ec;
}

bool pathExists(const fs::path& p) {
    std::error_code ec;
    return fs::exists(p, ec);
}

bool isDirectory(const fs::path& p) {
    std::error_code ec;
    return fs::is_directory(p, ec);
}

uint64_t fileSize(const fs::path& p) {
    std::error_code ec;
    auto s = fs::file_size(p, ec);
    return ec ? 0 : (uint64_t)s;
}

bool readFileBytes(const fs::path& p, Bytes& out) {
    FILE* f = _wfopen(p.wstring().c_str(), L"rb");
    if (!f) return false;
    _fseeki64(f, 0, SEEK_END);
    __int64 n = _ftelli64(f);
    _fseeki64(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return false; }
    out.resize((size_t)n);
    bool ok = n == 0 || fread(out.data(), 1, (size_t)n, f) == (size_t)n;
    fclose(f);
    return ok;
}

bool readFileText(const fs::path& p, std::string& out) {
    Bytes b;
    if (!readFileBytes(p, b)) return false;
    out.assign((const char*)b.data(), b.size());
    return true;
}

bool writeFileBytes(const fs::path& p, const Bytes& data) {
    ensureDir(p.parent_path());
    FILE* f = _wfopen(p.wstring().c_str(), L"wb");
    if (!f) return false;
    bool ok = data.empty() || fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = (fclose(f) == 0) && ok;
    return ok;
}

bool writeFileText(const fs::path& p, const std::string& text) {
    return writeFileBytes(p, Bytes(text.begin(), text.end()));
}

bool writeFileAtomic(const fs::path& p, const Bytes& data) {
    fs::path tmp = p;
    tmp += L".tmp";
    if (!writeFileBytes(tmp, data)) return false;
    std::error_code ec;
    fs::rename(tmp, p, ec);
    if (ec) {
        fs::remove(p, ec);
        ec.clear();
        fs::rename(tmp, p, ec);
    }
    return !ec;
}

bool writeFileTextAtomic(const fs::path& p, const std::string& text) {
    return writeFileAtomic(p, Bytes(text.begin(), text.end()));
}

bool removeFile(const fs::path& p) {
    std::error_code ec;
    fs::remove(p, ec);
    return !ec;
}

bool removeAll(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
    return !ec;
}

bool renamePath(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    fs::rename(from, to, ec);
    return !ec;
}

bool copyFile(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    ensureDir(to.parent_path());
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    return !ec;
}

static std::string sha256DigestToHex(const unsigned char* digest, unsigned len) {
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.resize(len * 2);
    for (unsigned i = 0; i < len; ++i) {
        out[i * 2] = hex[digest[i] >> 4];
        out[i * 2 + 1] = hex[digest[i] & 0x0F];
    }
    return out;
}

bool sha256Bytes(const void* data, size_t size, std::string& hexLower) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objLen = 0, cb = 0, hashLen = 0;
    std::vector<unsigned char> object, digest(32);
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
    if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &cb, 0) >= 0 &&
        BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &cb, 0) >= 0 &&
        hashLen == 32) {
        object.resize(objLen);
        if (BCryptCreateHash(alg, &hash, object.data(), objLen, nullptr, 0, 0) >= 0) {
            digest.resize(hashLen);
            if (BCryptHashData(hash, (PUCHAR)data, (ULONG)size, 0) >= 0 &&
                BCryptFinishHash(hash, digest.data(), hashLen, 0) >= 0) {
                hexLower = sha256DigestToHex(digest.data(), hashLen);
                ok = true;
            }
            BCryptDestroyHash(hash);
        }
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

bool sha256File(const fs::path& p, std::string& hexLower) {
    FILE* f = _wfopen(p.wstring().c_str(), L"rb");
    if (!f) return false;
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objLen = 0, cb = 0, hashLen = 0;
    std::vector<unsigned char> object, digest(32);
    bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) {
        fclose(f);
        return false;
    }
    if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &cb, 0) >= 0 &&
        BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &cb, 0) >= 0 &&
        hashLen == 32) {
        object.resize(objLen);
        if (BCryptCreateHash(alg, &hash, object.data(), objLen, nullptr, 0, 0) >= 0) {
            std::vector<unsigned char> buffer(64 * 1024);
            bool readOk = true;
            for (;;) {
                size_t n = fread(buffer.data(), 1, buffer.size(), f);
                if (n > 0 && BCryptHashData(hash, buffer.data(), (ULONG)n, 0) < 0) {
                    readOk = false;
                    break;
                }
                if (n < buffer.size()) {
                    if (ferror(f)) readOk = false;
                    break;
                }
            }
            digest.resize(hashLen);
            if (readOk && BCryptFinishHash(hash, digest.data(), hashLen, 0) >= 0) {
                hexLower = sha256DigestToHex(digest.data(), hashLen);
                ok = true;
            }
            BCryptDestroyHash(hash);
        }
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    fclose(f);
    return ok;
}

std::vector<fs::path> listDirectory(const fs::path& p, bool wantDirs) {
    std::vector<fs::path> out;
    std::error_code ec;
    if (!fs::is_directory(p, ec)) return out;
    for (auto& e : fs::directory_iterator(p, fs::directory_options::skip_permission_denied, ec)) {
        std::error_code ec2;
        bool d = e.is_directory(ec2);
        if (d == wantDirs) out.push_back(e.path());
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<fs::path> listFiles(const fs::path& p, const std::wstring& lowerExt) {
    std::vector<fs::path> out = listDirectory(p, false);
    if (lowerExt.empty()) return out;
    std::wstring ext = toLower(lowerExt);
    out.erase(std::remove_if(out.begin(), out.end(), [&](const fs::path& f) {
        return toLower(f.extension().wstring()) != ext;
    }), out.end());
    return out;
}

std::wstring pathString(const fs::path& p) { return p.wstring(); }

fs::path child(const fs::path& base, const std::wstring& name) {
    fs::path p = base / name;
    return p;
}

std::wstring clipboardText() {
    if (!OpenClipboard(nullptr)) return {};
    std::wstring out;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        auto* p = (const wchar_t*)GlobalLock(h);
        if (p) { out = p; GlobalUnlock(h); }
    }
    CloseClipboard();
    return out;
}

void setClipboardText(const std::wstring& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (h) {
        void* p = GlobalLock(h);
        if (p) {
            memcpy(p, text.c_str(), bytes);
            GlobalUnlock(h);
            SetClipboardData(CF_UNICODETEXT, h);
        }
    }
    CloseClipboard();
}

bool isValidPlayerName(const std::wstring& name) {
    if (name.size() < 3 || name.size() > 16) return false;
    for (wchar_t c : name) {
        if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
              (c >= L'0' && c <= L'9') || c == L'_')) return false;
    }
    return true;
}

bool isValidHost(const std::wstring& host) {
    if (host.empty() || host.size() > 253) return false;
    for (wchar_t c : host) {
        if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
            (c >= L'0' && c <= L'9') || c == L'.' || c == L'-' || c == L'_' ||
            c == L':' || c == L'[' || c == L']') continue;
        return false;
    }
    return true;
}

bool getSystemMemoryInfo(SystemMemoryInfo& info) {
    info = SystemMemoryInfo{};
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms)) return false;
    info.totalPhysical = ms.ullTotalPhys;
    info.availPhysical = ms.ullAvailPhys;
    info.totalPageFile = ms.ullTotalPageFile;
    info.availPageFile = ms.ullAvailPageFile;
    return true;
}

} // namespace aurora
