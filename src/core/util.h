#pragma once
// Aurora Launcher - small cross-cutting utilities (UTF, paths, files, formatting).
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace aurora {

using Bytes = std::vector<uint8_t>;
namespace fs = std::filesystem;

// ---------- text ----------
std::wstring utf8ToWide(const std::string& s);
std::string wideToUtf8(const std::wstring& s);
std::wstring toLower(std::wstring s);
std::wstring toUpper(std::wstring s);
std::wstring trim(const std::wstring& s);
std::string trim(const std::string& s);
bool iequals(const std::wstring& a, const std::wstring& b);
bool containsNoCase(const std::wstring& haystack, const std::wstring& needle);
bool startsWith(const std::wstring& s, const std::wstring& prefix);
std::wstring replaceAll(std::wstring s, const std::wstring& from, const std::wstring& to);
std::vector<std::wstring> split(const std::wstring& s, wchar_t sep);
std::wstring join(const std::vector<std::wstring>& v, const std::wstring& sep);
std::wstring formatW(const wchar_t* fmt, ...);
std::string formatA(const char* fmt, ...);
std::wstring stripMinecraftColors(const std::wstring& s);
std::wstring initials(const std::wstring& name);

// ---------- encoding ----------
std::string hexEncode(const uint8_t* data, size_t len);
std::string hexEncode(const Bytes& data);
std::vector<uint8_t> md5Digest(const void* data, size_t len);
std::wstring offlineUuid(const std::wstring& playerName);
Bytes hexDecode(const std::string& hex);
std::string base64Encode(const uint8_t* data, size_t len);
std::string base64Encode(const Bytes& data);
Bytes base64Decode(const std::string& text, bool* ok = nullptr);
std::string urlEncode(const std::string& s);
std::string urlEncodeComponent(const std::wstring& s);

// ---------- numbers / display ----------
std::wstring humanBytes(uint64_t bytes);
std::wstring humanDuration(double seconds);
std::wstring humanCount(uint64_t n);          // 1 234 / 12,4 k / 3,1 M
std::wstring formatDateFr(const std::wstring& iso);
std::wstring nowTime();                        // 12:04:32
std::wstring nowStamp();                       // 2026-10-23 12:04:32
std::string  nowStampUtf8();
int64_t nowUnix();
std::wstring unixToDate(int64_t unixTime);

// ---------- process / args ----------
std::wstring quoteWinArg(const std::wstring& arg);
std::wstring buildCommandLine(const std::vector<std::wstring>& args);
std::wstring executablePath();
fs::path executableDir();
fs::path defaultDataDir();
fs::path knownFolder(int csidl);
void openInExplorer(const fs::path& p);
void openUrl(const std::wstring& url);
void sleepMs(int ms);
std::wstring readRegistryString(const wchar_t* subkey, const wchar_t* value);

// ---------- system ----------
struct SystemMemoryInfo {
    uint64_t totalPhysical = 0;
    uint64_t availPhysical = 0;
    uint64_t totalPageFile = 0;
    uint64_t availPageFile = 0;
};
bool getSystemMemoryInfo(SystemMemoryInfo& info);

// ---------- filesystem ----------
bool ensureDir(const fs::path& p);
bool pathExists(const fs::path& p);
bool isDirectory(const fs::path& p);
uint64_t fileSize(const fs::path& p);
bool readFileBytes(const fs::path& p, Bytes& out);
bool readFileText(const fs::path& p, std::string& out);
bool writeFileBytes(const fs::path& p, const Bytes& data);
bool writeFileText(const fs::path& p, const std::string& text);
bool writeFileAtomic(const fs::path& p, const Bytes& data);
bool writeFileTextAtomic(const fs::path& p, const std::string& text);
bool sha256File(const fs::path& p, std::string& hexLower);
bool sha256Bytes(const void* data, size_t size, std::string& hexLower);
bool removeFile(const fs::path& p);
bool removeAll(const fs::path& p);
bool renamePath(const fs::path& from, const fs::path& to);
bool copyFile(const fs::path& from, const fs::path& to);
std::vector<fs::path> listDirectory(const fs::path& p, bool wantDirs);
std::vector<fs::path> listFiles(const fs::path& p, const std::wstring& lowerExt = L"");
std::wstring pathString(const fs::path& p);
fs::path child(const fs::path& base, const std::wstring& name);

// Clipboard (UTF-16)
std::wstring clipboardText();
void setClipboardText(const std::wstring& text);

// ---------- validation helpers ----------
bool isValidPlayerName(const std::wstring& name);
bool isValidHost(const std::wstring& host);

} // namespace aurora
