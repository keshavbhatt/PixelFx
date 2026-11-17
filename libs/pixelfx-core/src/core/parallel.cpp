// SPDX-License-Identifier: Apache-2.0
#include "core/parallel.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace pfx::detail {

namespace {

// More chunks than threads, so a thread slowed by other work on the machine does not hold the
// rest up.
constexpr int kChunksPerThread = 4;

struct Job
{
    const std::function<void(int, int)>* body = nullptr;
    int count = 0;
    int chunk = 1;
    int chunks = 0;
    int maxHelpers = 0; // pool threads that may join the caller
    std::atomic<int> next{0};
    std::atomic<int> helpers{0};
    std::atomic<int> done{0};
    std::mutex mutex;
    std::condition_variable finished;
    std::exception_ptr error; // the first chunk's that threw, raised in the caller

    void fail()
    {
        const std::scoped_lock lock(mutex);
        if (!error) {
            error = std::current_exception();
        }
    }

    // Runs chunks until none are left to take.
    void work()
    {
        for (;;) {
            const int c = next.fetch_add(1);
            if (c >= chunks) {
                return;
            }
            const int begin = c * chunk;
            try {
                (*body)(begin, std::min(count, begin + chunk));
            } catch (...) {
                fail();
            }
            if (done.fetch_add(1) + 1 == chunks) {
                const std::scoped_lock lock(mutex);
                finished.notify_all();
            }
        }
    }
};

// Threads started once and shared by every parallelFor, from any thread. The caller works on
// its own job too, so a parallelFor inside another never waits for a thread that is busy.
class Pool
{
public:
    Pool()
    {
        const auto n = std::max(1U, std::thread::hardware_concurrency()) - 1;
        m_threads.reserve(n);
        for (unsigned i = 0; i < n; ++i) {
            m_threads.emplace_back([this] { run(); });
        }
    }

    ~Pool()
    {
        {
            const std::scoped_lock lock(m_mutex);
            m_stopping = true;
        }
        m_wake.notify_all();
        for (std::thread& t : m_threads) {
            t.join();
        }
    }

    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;
    Pool(Pool&&) = delete;
    Pool& operator=(Pool&&) = delete;

    [[nodiscard]] int size() const { return static_cast<int>(m_threads.size()); }

    void execute(const std::shared_ptr<Job>& job)
    {
        if (job->maxHelpers > 0) {
            {
                const std::scoped_lock lock(m_mutex);
                m_jobs.push_back(job);
            }
            m_wake.notify_all();
        }
        job->work();
        {
            std::unique_lock lock(job->mutex);
            job->finished.wait(lock, [&job] { return job->done.load() == job->chunks; });
        }
        {
            const std::scoped_lock lock(m_mutex);
            std::erase(m_jobs, job);
        }
        if (job->error) {
            std::rethrow_exception(job->error);
        }
    }

private:
    void run()
    {
        for (;;) {
            std::shared_ptr<Job> job;
            {
                std::unique_lock lock(m_mutex);
                m_wake.wait(lock, [this] { return m_stopping || !m_jobs.empty(); });
                if (m_stopping) {
                    return;
                }
                job = m_jobs.front();
                // Enough helpers, or nothing left to take: the job leaves the queue.
                if (job->helpers.fetch_add(1) + 1 >= job->maxHelpers || job->next.load() >= job->chunks) {
                    m_jobs.pop_front();
                }
            }
            job->work();
        }
    }

    std::vector<std::thread> m_threads;
    std::deque<std::shared_ptr<Job>> m_jobs;
    std::mutex m_mutex;
    std::condition_variable m_wake;
    bool m_stopping = false;
};

Pool& pool()
{
    static Pool p;
    return p;
}

} // namespace

void parallelFor(int count, int threads, const std::function<void(int, int)>& body)
{
    if (count <= 0) {
        return;
    }
    Pool& shared = pool();
    int workers = threads > 0 ? threads : shared.size() + 1;
    workers = std::clamp(workers, 1, std::min(count, shared.size() + 1));
    if (workers == 1) {
        body(0, count);
        return;
    }
    const auto job = std::make_shared<Job>();
    job->body = &body;
    job->count = count;
    job->chunks = std::min(count, workers * kChunksPerThread);
    job->chunk = (count + job->chunks - 1) / job->chunks;
    job->chunks = (count + job->chunk - 1) / job->chunk;
    job->maxHelpers = workers - 1;
    shared.execute(job);
}

} // namespace pfx::detail
