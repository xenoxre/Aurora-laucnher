#include "log.h"
#include "util.h"
#include "../version.h"

#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <exception>

namespace aurora {

static std::wstring g_crashLogPath;

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::init(const fs::path& logFile, bool verbose) {
    std::lock_guard<std::mutex> lock(mutex_);
    verbose_ = verbose;
    if (fileHandle_) { fclose((FILE*)fileHandle_); fileHandle_ = nullptr; }
    file_ = logFile;
    ensureDir(file_.parent_path());
    // rotate if bigger than 4 MB
    std::error_code ec;
    if (fs::file_size(file_, ec) > 4 * 1024 * 1024) {
        fs::path old = file_;
        old += L".old";
        fs::remove(old, ec);
        fs::rename(file_, old, ec);
    }
    fileHandle_ = _wfopen(file_.wstring().c_str(), L"ab");
    inited_ = true;
    if (fileHandle_) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char line[256];
        int n = snprintf(line, sizeof(line),
                         "\r\n========== Aurora Launcher %s - %04d-%02d-%02d %02d:%02d:%02d ==========\r\n",
                         AURORA_VERSION, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        fwrite(line, 1, (size_t)n, (FILE*)fileHandle_);
        fflush((FILE*)fileHandle_);
    }
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (fileHandle_) { fclose((FILE*)fileHandle_); fileHandle_ = nullptr; }
    listener_ = nullptr;
}

void Logger::setListener(std::function<void(const LogEntry&)> listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    listener_ = std::move(listener);
}

void Logger::log(LogLevel level, const std::wstring& text, const std::wstring& source) {
    if (level == LogLevel::Debug && !verbose_) return;
    LogEntry e;
    e.time = nowTime();
    e.level = level;
    e.source = source;
    e.text = text;
    std::function<void(const LogEntry&)> cb;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.push_back(e);
        while (entries_.size() > 3000) entries_.pop_front();
        if (fileHandle_) {
            std::string line = wideToUtf8(formatW(L"[%s] [%s] [%s] %s\r\n", e.time.c_str(),
                logLevelName(level), source.c_str(), text.c_str()));
            fwrite(line.data(), 1, line.size(), (FILE*)fileHandle_);
            fflush((FILE*)fileHandle_);
        }
        cb = listener_;
    }
    if (cb) cb(e);
}

std::vector<LogEntry> Logger::snapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::vector<LogEntry>(entries_.begin(), entries_.end());
}

void Logger::clearUi() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

std::wstring Logger::logFilePath() const { return file_.wstring(); }

void logMsg(LogLevel level, const std::wstring& text, const std::wstring& source) {
    Logger::instance().log(level, text, source);
}
void logInfo(const std::wstring& text, const std::wstring& source) { logMsg(LogLevel::Info, text, source); }
void logSuccess(const std::wstring& text, const std::wstring& source) { logMsg(LogLevel::Success, text, source); }
void logWarn(const std::wstring& text, const std::wstring& source) { logMsg(LogLevel::Warn, text, source); }
void logError(const std::wstring& text, const std::wstring& source) { logMsg(LogLevel::Error, text, source); }
void logDebug(const std::wstring& text, const std::wstring& source) { logMsg(LogLevel::Debug, text, source); }
void logGame(const std::wstring& text, const std::wstring& source) { logMsg(LogLevel::Game, text, source); }

const wchar_t* logLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return L"DEBUG";
        case LogLevel::Info: return L"INFO";
        case LogLevel::Success: return L"OK";
        case LogLevel::Warn: return L"WARN";
        case LogLevel::Error: return L"ERR";
        case LogLevel::Game: return L"JEU";
    }
    return L"INFO";
}

static LONG WINAPI crashFilter(EXCEPTION_POINTERS* info) {
    wchar_t path[MAX_PATH * 2];
    wcscpy_s(path, g_crashLogPath.empty() ? L"crash.txt" : g_crashLogPath.c_str());
    FILE* f = _wfopen(path, L"w, ccs=UTF-8");
    if (f) {
        fwprintf(f, L"Aurora Launcher a rencontré une erreur fatale.\n" AURORA_VERSION_W L"\n");
        if (info && info->ExceptionRecord) {
            fwprintf(f, L"Code: 0x%08lX\nAdresse: %p\n",
                     (unsigned long)info->ExceptionRecord->ExceptionCode,
                     info->ExceptionRecord->ExceptionAddress);
        }
        fwprintf(f, L"Date: %s\n\nPile d'appels:\n", nowStamp().c_str());
        void* frames[48];
        USHORT count = CaptureStackBackTrace(0, 48, frames, nullptr);
        for (USHORT i = 0; i < count; i++) fwprintf(f, L"  [%02u] %p\n", i, frames[i]);
        fclose(f);
    }
    std::wstring dump = path;
    dump += L".dmp";
    HANDLE h = CreateFileW(dump.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = info;
        mei.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), h,
                          (MINIDUMP_TYPE)(MiniDumpWithDataSegs | MiniDumpWithHandleData),
                          info ? &mei : nullptr, nullptr, nullptr);
        CloseHandle(h);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

void installCrashHandler(const fs::path& logsDir) {
    ensureDir(logsDir);
    g_crashLogPath = (logsDir / L"crash.txt").wstring();
    SetUnhandledExceptionFilter(crashFilter);
}

} // namespace aurora
