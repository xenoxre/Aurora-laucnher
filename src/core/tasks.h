#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace aurora {

struct JobContext {
    int id = 0;
    std::atomic<bool>* cancelFlag = nullptr;
    std::function<void(const std::wstring& status, float progress)> reporter;
    bool failure = false;
    std::wstring error;

    void report(const std::wstring& status, float progress = -1.0f) {
        if (reporter) reporter(status, progress);
    }
    void fail(const std::wstring& message, float progress = 1.0f) {
        failure = true;
        error = message;
        if (reporter) reporter(message, progress);
    }
    bool cancelled() const { return cancelFlag && cancelFlag->load(); }
};

class JobQueue {
public:
    using Work = std::function<void(JobContext&)>;
    using Done = std::function<void(bool ok, const std::wstring& message)>;

    int submit(const std::wstring& title, Work work, Done done = {});
    void poll();          // UI thread only
    void shutdown();
    void cancel(int id);
    void cancelAll();

    struct Info {
        int id;
        std::wstring title;
        std::wstring status;
        float progress;
    };
    std::vector<Info> infos();
    int activeCount();
    bool hasActive();
    std::wstring firstJobTitle();

private:
    struct Job;
    std::mutex mutex_;
    std::vector<std::shared_ptr<Job>> jobs_;
    std::atomic<int> nextId_{1};
};

} // namespace aurora
