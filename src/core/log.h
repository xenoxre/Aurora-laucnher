#pragma once
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace aurora {

enum class LogLevel { Debug = 0, Info = 1, Success = 2, Warn = 3, Error = 4, Game = 5 };

struct LogEntry {
    std::wstring time;
    LogLevel level = LogLevel::Info;
    std::wstring source;
    std::wstring text;
};

class Logger {
public:
    static Logger& instance();

    void init(const std::filesystem::path& logFile, bool verbose);
    void shutdown();
    void log(LogLevel level, const std::wstring& text, const std::wstring& source = L"Lanceur");
    void setListener(std::function<void(const LogEntry&)> listener);
    std::vector<LogEntry> snapshot();
    void clearUi();
    void setVerbose(bool v) { verbose_ = v; }
    bool verbose() const { return verbose_; }
    std::wstring logFilePath() const;

private:
    Logger() = default;
    std::mutex mutex_;
    std::deque<LogEntry> entries_;
    std::function<void(const LogEntry&)> listener_;
    std::filesystem::path file_;
    void* fileHandle_ = nullptr;
    bool verbose_ = false;
    bool inited_ = false;
};

void logMsg(LogLevel level, const std::wstring& text, const std::wstring& source = L"Lanceur");
void logInfo(const std::wstring& text, const std::wstring& source = L"Lanceur");
void logSuccess(const std::wstring& text, const std::wstring& source = L"Lanceur");
void logWarn(const std::wstring& text, const std::wstring& source = L"Lanceur");
void logError(const std::wstring& text, const std::wstring& source = L"Lanceur");
void logDebug(const std::wstring& text, const std::wstring& source = L"Lanceur");
void logGame(const std::wstring& text, const std::wstring& source = L"Jeu");

const wchar_t* logLevelName(LogLevel level);
void installCrashHandler(const std::filesystem::path& logsDir);

} // namespace aurora
