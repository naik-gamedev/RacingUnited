#include "JobSystem.hpp"

#include <limits>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace heritage::jobs {
namespace {

thread_local JobSystem* g_executingJobSystem = nullptr;

class ScopedJobExecution final
{
public:
    explicit ScopedJobExecution(JobSystem* system)
        : m_previous(g_executingJobSystem)
    {
        g_executingJobSystem = system;
    }

    ~ScopedJobExecution()
    {
        g_executingJobSystem = m_previous;
    }

private:
    JobSystem* m_previous = nullptr;
};

} // namespace

std::uint32_t JobSystem::chooseWorkerCount(
    std::uint32_t hardwareThreads,
    const JobSystemDescription& description)
{
    hardwareThreads = (std::max)(hardwareThreads, 1u);
    if (hardwareThreads <= 1u)
        return 0u;

    // The caller thread is an active participant, so never create as many
    // workers as there are logical processors. That would oversubscribe before
    // the renderer/driver/audio/OS are considered.
    const std::uint32_t maximumWorkers = hardwareThreads - 1u;
    if (description.requestedWorkerCount > 0u)
        return (std::min)(description.requestedWorkerCount, maximumWorkers);

    if (hardwareThreads <= 2u)
        return 1u;

    const std::uint32_t reserve = (std::min)(
        description.reservedLogicalProcessors,
        hardwareThreads - 1u);
    return (std::max)(hardwareThreads - reserve, 1u);
}

JobSystem::JobSystem(const JobSystemDescription& description)
{
    const unsigned reported = std::thread::hardware_concurrency();
    m_hardwareThreadCount = reported > 0u
        ? static_cast<std::uint32_t>(reported)
        : 1u;

    const std::uint32_t workerCount = chooseWorkerCount(
        m_hardwareThreadCount,
        description);
    m_workers.reserve(workerCount);
    m_queue.reserve((std::max)(workerCount * 4u, 16u));

    for (std::uint32_t workerIndex = 0; workerIndex < workerCount; ++workerIndex)
    {
        m_workers.emplace_back([this, workerIndex]() {
            workerLoop(workerIndex);
        });
    }
}

JobSystem::~JobSystem()
{
    waitIdle();
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_stopping = true;
    }
    m_queueCondition.notify_all();

    for (std::thread& worker : m_workers)
    {
        if (worker.joinable())
            worker.join();
    }
}

JobSystemStats JobSystem::stats() const
{
    JobSystemStats result;
    result.hardwareThreadCount = m_hardwareThreadCount;
    result.workerThreadCount = workerThreadCount();
    result.parallelBatchCount = m_parallelBatchCount.load(std::memory_order_relaxed);
    result.parallelRangeCount = m_parallelRangeCount.load(std::memory_order_relaxed);
    result.workerRangeCount = m_workerRangeCount.load(std::memory_order_relaxed);
    result.callerRangeCount = m_callerRangeCount.load(std::memory_order_relaxed);
    return result;
}

bool JobSystem::executingOnThisSystem() const
{
    return g_executingJobSystem == this;
}

void JobSystem::recordBatchException(
    ParallelBatch& batch,
    std::exception_ptr exception)
{
    bool expected = false;
    if (batch.failed.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel))
    {
        std::lock_guard<std::mutex> lock(batch.exceptionMutex);
        batch.exception = std::move(exception);
    }
}

void JobSystem::executeRanges(ParallelBatch& batch, bool callerThread)
{
    ScopedJobExecution executing(this);
    while (!batch.failed.load(std::memory_order_acquire))
    {
        const std::size_t rangeIndex = batch.nextRange.fetch_add(
            1u, std::memory_order_relaxed);
        if (rangeIndex >= batch.rangeCount)
            break;

        const std::size_t begin = rangeIndex * batch.grainSize;
        const std::size_t end = (std::min)(
            begin + batch.grainSize,
            batch.itemCount);
        try
        {
            batch.execute(batch.context, begin, end, rangeIndex);
        }
        catch (...)
        {
            recordBatchException(batch, std::current_exception());
            break;
        }

        m_parallelRangeCount.fetch_add(1u, std::memory_order_relaxed);
        if (callerThread)
            m_callerRangeCount.fetch_add(1u, std::memory_order_relaxed);
        else
            m_workerRangeCount.fetch_add(1u, std::memory_order_relaxed);
    }
}

void JobSystem::runParallelBatch(ParallelBatch& batch)
{
    if (batch.rangeCount <= 1u || m_workers.empty())
    {
        executeRanges(batch, true);
        return;
    }

    // The caller consumes one share of the work. Queue at most one participant
    // per remaining range; duplicate batch pointers are cheap and let workers
    // dynamically steal ranges from the batch's atomic cursor.
    const std::uint32_t participantCount = static_cast<std::uint32_t>((std::min)(
        static_cast<std::size_t>(m_workers.size()),
        batch.rangeCount - 1u));
    batch.workerParticipantsRemaining.store(
        participantCount,
        std::memory_order_release);

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        // Compact consumed entries before an occasional large batch grows the
        // queue indefinitely. This is outside hot per-range execution.
        if (m_queueReadIndex > 0u
            && (m_queueReadIndex == m_queue.size() || m_queueReadIndex > 1024u))
        {
            m_queue.erase(
                m_queue.begin(),
                m_queue.begin() + static_cast<std::ptrdiff_t>(m_queueReadIndex));
            m_queueReadIndex = 0u;
        }
        for (std::uint32_t index = 0; index < participantCount; ++index)
            m_queue.push_back(&batch);
    }
    m_queueCondition.notify_all();

    // Main/game thread helps rather than blocking while worker cores run.
    executeRanges(batch, true);

    if (participantCount > 0u)
    {
        std::unique_lock<std::mutex> completionLock(batch.completionMutex);
        batch.completionCondition.wait(completionLock, [&batch]() {
            return batch.workerParticipantsRemaining.load(
                std::memory_order_acquire) == 0u;
        });
    }
}

void JobSystem::workerLoop(std::uint32_t workerIndex)
{
#ifdef _WIN32
    // Names are diagnostic only; failure on older Windows versions is harmless.
    wchar_t name[32]{};
    swprintf_s(name, L"Heritage Worker %u", workerIndex);
    SetThreadDescription(GetCurrentThread(), name);
#else
    (void)workerIndex;
#endif

    for (;;)
    {
        ParallelBatch* batch = nullptr;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCondition.wait(lock, [this]() {
                return m_stopping || m_queueReadIndex < m_queue.size();
            });
            if (m_stopping && m_queueReadIndex >= m_queue.size())
                return;

            batch = m_queue[m_queueReadIndex++];
            ++m_activeWorkerTasks;
        }

        if (batch)
            executeRanges(*batch, false);

        if (batch)
        {
            // Keep completionMutex held across the final participant decrement
            // *and* notify. runParallelBatch() waits with this same mutex, so it
            // cannot return and destroy its stack-owned ParallelBatch while the
            // last worker is still inside notify_one(). Without this lifetime
            // handshake a fast/spurious wake could destroy completionCondition
            // concurrently with the worker signaling it, which manifested as an
            // intermittent hard hang when rain activated large hydrology batches.
            std::lock_guard<std::mutex> completionLock(batch->completionMutex);
            if (batch->workerParticipantsRemaining.fetch_sub(
                    1u, std::memory_order_acq_rel) == 1u)
            {
                batch->completionCondition.notify_one();
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            if (m_activeWorkerTasks > 0u)
                --m_activeWorkerTasks;
            if (m_activeWorkerTasks == 0u && m_queueReadIndex >= m_queue.size())
                m_idleCondition.notify_all();
        }
    }
}

void JobSystem::waitIdle()
{
    std::unique_lock<std::mutex> lock(m_queueMutex);
    m_idleCondition.wait(lock, [this]() {
        return m_activeWorkerTasks == 0u && m_queueReadIndex >= m_queue.size();
    });
}

} // namespace heritage::jobs
