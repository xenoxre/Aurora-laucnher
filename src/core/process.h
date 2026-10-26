#pragma once
#include <windows.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>

namespace aurora {

class Process {
public:
    Process() = default;
    ~Process();

    bool start(const std::wstring& commandLine, const std::filesystem::path& workingDir,
               std::wstring& error);
    void setOutput(std::function<void(const std::wstring&)> cb) { output_ = std::move(cb); }
    bool running() const { return running_.load(); }
    int exitCode() const { return exitCode_.load(); }
    void writeLine(const std::wstring& line);
    void terminate();
    void requestStop();
    void abandon();          // leave the child running when the launcher exits
    unsigned long pid() const { return pid_; }
    bool server = false;

private:
    void readLoop();

    PROCESS_INFORMATION pi_{};
    HANDLE stdinWrite_ = nullptr;
    HANDLE stdoutRead_ = nullptr;
    std::thread reader_;
    std::function<void(const std::wstring&)> output_;
    std::atomic<bool> running_{ false };
    std::atomic<int> exitCode_{ -1 };
    unsigned long pid_ = 0;
    bool started_ = false;
    bool abandoned_ = false;
};

} // namespace aurora
