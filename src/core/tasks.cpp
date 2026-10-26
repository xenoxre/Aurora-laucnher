#include "tasks.h"
#include "log.h"
#include "util.h"

#include <algorithm>
#include <exception>

namespace aurora {

struct JobQueue::Job {
    int id = 0;
    std::wstring title;
    std::wstring status;
    float progress = -1.0f;
    std::atomic<bool> cancel{false};
    std::atomic<bool> finished{false};
    bool ok = true;
    std::wstring message;
    Work work;
    Done done;
    std::thread thread;
    std::mutex m;
};

int JobQueue::submit(const std::wstring& title, Work work, Done done) {
    auto job = std::make_shared<Job>();
    job->id = nextId_.fetch_add(1);
    job->title = title;
    job->work = std::move(work);
    job->done = std::move(done);
    job->status = L"En attente…";
    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.push_back(job);
    }
    auto* cancelFlag = &job->cancel;
    job->thread = std::thread([job, cancelFlag]() {
        JobContext ctx;
        ctx.id = job->id;
        ctx.cancelFlag = cancelFlag;
        ctx.reporter = [job](const std::wstring& status, float progress) {
            std::lock_guard<std::mutex> lock(job->m);
            job->status = status;
            job->progress = progress;
        };
        try {
            job->work(ctx);
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(job->m);
            job->ok = false;
            job->message = L"Exception : " + utf8ToWide(e.what());
        } catch (...) {
            std::lock_guard<std::mutex> lock(job->m);
            job->ok = false;
            job->message = L"Exception inconnue";
        }
        if (ctx.failure) {
            std::lock_guard<std::mutex> lock(job->m);
            job->ok = false;
            job->message = ctx.error;
        }
        {
            std::lock_guard<std::mutex> lock(job->m);
            if (job->cancel.load() && job->ok) {
                job->ok = false;
                job->message = L"Tâche annulée";
            }
        }
        job->finished.store(true);
    });
    return job->id;
}

void JobQueue::poll() {
    std::vector<std::shared_ptr<Job>> finishedJobs;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = jobs_.begin(); it != jobs_.end();) {
            if ((*it)->finished.load()) {
                finishedJobs.push_back(*it);
                it = jobs_.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (auto& job : finishedJobs) {
        if (job->thread.joinable()) job->thread.join();
        bool ok;
        std::wstring message;
        {
            std::lock_guard<std::mutex> lock(job->m);
            ok = job->ok;
            message = job->message;
        }
        if (job->done) {
            try { job->done(ok, message); }
            catch (...) {}
        }
    }
}

void JobQueue::shutdown() {
    std::vector<std::shared_ptr<Job>> jobs;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        jobs = jobs_;
    }
    for (auto& j : jobs) j->cancel.store(true);
    for (auto& j : jobs) {
        if (j->thread.joinable()) j->thread.join();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    jobs_.clear();
}

void JobQueue::cancel(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& j : jobs_) if (j->id == id) j->cancel.store(true);
}

void JobQueue::cancelAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& j : jobs_) j->cancel.store(true);
}

std::vector<JobQueue::Info> JobQueue::infos() {
    std::vector<Info> out;
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& j : jobs_) {
        std::lock_guard<std::mutex> jl(j->m);
        out.push_back({ j->id, j->title, j->status, j->progress });
    }
    return out;
}

int JobQueue::activeCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    return (int)jobs_.size();
}

bool JobQueue::hasActive() {
    std::lock_guard<std::mutex> lock(mutex_);
    return !jobs_.empty();
}

std::wstring JobQueue::firstJobTitle() {
    std::lock_guard<std::mutex> lock(mutex_);
    return jobs_.empty() ? std::wstring() : jobs_.front()->title;
}

} // namespace aurora
