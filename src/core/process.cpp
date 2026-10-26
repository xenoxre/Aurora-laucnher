#include "process.h"
#include "log.h"
#include "util.h"

#include <vector>

namespace aurora {

Process::~Process() {
    if (abandoned_) return;
    if (started_) {
        if (running_.load()) {
            TerminateProcess(pi_.hProcess, 0);
            WaitForSingleObject(pi_.hProcess, 3000);
        }
        if (reader_.joinable()) reader_.join();
        if (pi_.hThread) CloseHandle(pi_.hThread);
        if (pi_.hProcess) CloseHandle(pi_.hProcess);
        if (stdinWrite_) CloseHandle(stdinWrite_);
        if (stdoutRead_) CloseHandle(stdoutRead_);
    }
}

bool Process::start(const std::wstring& commandLine, const fs::path& workingDir,
                    std::wstring& error) {
    if (started_) {
        error = L"Processus déjà démarré";
        return false;
    }
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE stdinRead = nullptr, stdoutWrite = nullptr;
    if (!CreatePipe(&stdinRead, &stdinWrite_, &sa, 0) ||
        !CreatePipe(&stdoutRead_, &stdoutWrite, &sa, 0)) {
        error = L"Création des pipes impossible";
        return false;
    }
    SetHandleInformation(stdinWrite_, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdoutRead_, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdinRead;
    si.hStdOutput = stdoutWrite;
    si.hStdError = stdoutWrite;

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cmd(commandLine.begin(), commandLine.end());
    cmd.push_back(L'\0');
    std::wstring wd = workingDir.wstring();
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                             nullptr, wd.empty() ? nullptr : wd.c_str(), &si, &pi);
    DWORD lastError = GetLastError();
    CloseHandle(stdinRead);
    CloseHandle(stdoutWrite);
    if (!ok) {
        error = formatW(L"Lancement impossible (code %lu)", (unsigned long)lastError);
        CloseHandle(stdinWrite_);
        CloseHandle(stdoutRead_);
        stdinWrite_ = stdoutRead_ = nullptr;
        return false;
    }
    pi_ = pi;
    pid_ = pi.dwProcessId;
    started_ = true;
    running_.store(true);
    reader_ = std::thread([this]() { readLoop(); });
    return true;
}

void Process::readLoop() {
    std::string pending;
    std::vector<char> buf(8192);
    DWORD read = 0;
    while (ReadFile(stdoutRead_, buf.data(), (DWORD)buf.size(), &read, nullptr) && read > 0) {
        pending.append(buf.data(), read);
        size_t pos;
        while ((pos = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, pos);
            pending.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.size() > 16000) line.resize(16000);
            if (output_ && !line.empty()) output_(utf8ToWide(line));
        }
        if (pending.size() > 32000) {
            if (output_) output_(utf8ToWide(pending));
            pending.clear();
        }
    }
    if (!pending.empty() && output_) output_(utf8ToWide(pending));
    WaitForSingleObject(pi_.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi_.hProcess, &code);
    exitCode_.store((int)code);
    running_.store(false);
}

void Process::writeLine(const std::wstring& line) {
    if (!stdinWrite_) return;
    std::string data = wideToUtf8(line) + "\n";
    DWORD written = 0;
    WriteFile(stdinWrite_, data.data(), (DWORD)data.size(), &written, nullptr);
}

void Process::terminate() {
    if (started_ && running_.load()) TerminateProcess(pi_.hProcess, 0);
}

void Process::requestStop() {
    if (!started_ || !running_.load()) return;
    if (server) {
        logInfo(L"Arrêt du serveur en cours…", L"Serveur");
        writeLine(L"stop");
        // Give the server a moment, then force it.
        std::thread([this]() {
            for (int i = 0; i < 100 && running_.load(); i++) sleepMs(100);
            if (running_.load()) TerminateProcess(pi_.hProcess, 0);
        }).detach();
    } else {
        terminate();
    }
}

void Process::abandon() {
    if (!started_ || abandoned_) return;
    abandoned_ = true;
    running_.store(false);
    if (stdinWrite_) { CloseHandle(stdinWrite_); stdinWrite_ = nullptr; }
    if (stdoutRead_) { CloseHandle(stdoutRead_); stdoutRead_ = nullptr; }
    if (pi_.hThread) { CloseHandle(pi_.hThread); pi_.hThread = nullptr; }
    if (pi_.hProcess) { CloseHandle(pi_.hProcess); pi_.hProcess = nullptr; }
    if (reader_.joinable()) reader_.detach();
    started_ = false;
}

} // namespace aurora
