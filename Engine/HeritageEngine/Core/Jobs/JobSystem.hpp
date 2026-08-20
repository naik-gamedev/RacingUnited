#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace heritage::jobs {

struct JobSystemDescription
{
    // 0 = automatic. The automatic policy reserves logical slots from worker
    // creation: the caller participates in every batch and the remaining headroom
    // is available to renderer/driver/audio/OS work. With the default reserve,
    // 16 logical processors create 14 workers + caller, 4 create 2 + caller, and
    // 2 create 1 + caller.
    std::uint32_t requestedWorkerCount = 0;
    std::uint32_t reservedLogicalProcessors = 2;
};

struct JobSystemStats
{
    std::uint32_t hardwareThreadCount = 1;
    std::uint32_t workerThreadCount = 0;
    std::uint64_t parallelBatchCount = 0;
    std::uint64_t parallelRangeCount = 0;
    std::uint64_t workerRangeCount = 0;
    std::uint64_t callerRangeCount = 0;
};

// Heritage's process-wide bounded worker pool. JOB01 intentionally starts with
// a synchronous parallel-for primitive rather than a fire-and-forget task graph:
// callers retain explicit phase barriers and ownership, which makes it much
// harder to introduce data races into deterministic physics by accident.
//
// The caller thread participates in every parallel batch. Nested parallel-for
// calls on the same JobSystem execute serially to avoid pool starvation and
// accidental recursive waits.
class JobSystem final
{
public:
    explicit JobSystem(const JobSystemDescription& description = {});
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;
    JobSystem(JobSystem&&) = delete;
    JobSystem& operator=(JobSystem&&) = delete;

    std::uint32_t hardwareThreadCount() const { return m_hardwareThreadCount; }
    std::uint32_t workerThreadCount() const
    {
        return static_cast<std::uint32_t>(m_workers.size());
    }

    JobSystemStats stats() const;

    // Execute [0,itemCount) in bounded contiguous ranges. The callback receives
    // (beginIndex, endIndex, rangeIndex). rangeIndex is stable for the batch,
    // so deterministic per-range reductions can be stored without locks and
    // merged in ascending range order after this call returns.
    template <typename Function>
    void parallelFor(
        std::size_t itemCount,
        std::size_t minimumGrainSize,
        Function&& function)
    {
        if (itemCount == 0)
            return;

        const std::size_t grain = (std::max)(minimumGrainSize, std::size_t{ 1 });
        const std::size_t rangeCount = (itemCount + grain - 1u) / grain;

        // Small jobs, single-core systems and recursive use remain inline. The
        // same callback contract is preserved, which keeps subsystem code free
        // from separate threaded/non-threaded implementations.
        if (rangeCount <= 1u || m_workers.empty() || executingOnThisSystem())
        {
            function(std::size_t{ 0 }, itemCount, std::size_t{ 0 });
            m_callerRangeCount.fetch_add(1u, std::memory_order_relaxed);
            m_parallelRangeCount.fetch_add(1u, std::memory_order_relaxed);
            return;
        }

        using FunctionType = std::remove_reference_t<Function>;
        using MutableFunctionType = std::remove_const_t<FunctionType>;
        MutableFunctionType* functionPointer = const_cast<MutableFunctionType*>(
            std::addressof(function));

        ParallelBatch batch;
        batch.itemCount = itemCount;
        batch.grainSize = grain;
        batch.rangeCount = rangeCount;
        batch.context = functionPointer;
        batch.execute = [](void* context,
                           std::size_t begin,
                           std::size_t end,
                           std::size_t rangeIndex) {
            auto* callback = static_cast<MutableFunctionType*>(context);
            (*callback)(begin, end, rangeIndex);
        };

        m_parallelBatchCount.fetch_add(1u, std::memory_order_relaxed);
        runParallelBatch(batch);

        if (batch.exception)
            std::rethrow_exception(batch.exception);
    }

    // With JOB01 all public batches are synchronous, so this is primarily a
    // shutdown/diagnostic seam and a future-compatible contract for later
    // asynchronous jobs. It never spins; workers notify when the queue drains.
    void waitIdle();

private:
    struct ParallelBatch
    {
        std::size_t itemCount = 0;
        std::size_t grainSize = 1;
        std::size_t rangeCount = 0;
        std::atomic<std::size_t> nextRange{ 0 };

        void* context = nullptr;
        void (*execute)(void*, std::size_t, std::size_t, std::size_t) = nullptr;

        std::atomic<std::uint32_t> workerParticipantsRemaining{ 0 };
        std::mutex completionMutex;
        std::condition_variable completionCondition;

        std::atomic<bool> failed{ false };
        std::mutex exceptionMutex;
        std::exception_ptr exception;
    };

    static std::uint32_t chooseWorkerCount(
        std::uint32_t hardwareThreads,
        const JobSystemDescription& description);

    bool executingOnThisSystem() const;
    void runParallelBatch(ParallelBatch& batch);
    void executeRanges(ParallelBatch& batch, bool callerThread);
    void workerLoop(std::uint32_t workerIndex);
    void recordBatchException(ParallelBatch& batch, std::exception_ptr exception);

    std::uint32_t m_hardwareThreadCount = 1;
    std::vector<std::thread> m_workers;

    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    std::condition_variable m_idleCondition;
    std::vector<ParallelBatch*> m_queue;
    std::size_t m_queueReadIndex = 0;
    std::uint32_t m_activeWorkerTasks = 0;
    bool m_stopping = false;

    std::atomic<std::uint64_t> m_parallelBatchCount{ 0 };
    std::atomic<std::uint64_t> m_parallelRangeCount{ 0 };
    std::atomic<std::uint64_t> m_workerRangeCount{ 0 };
    std::atomic<std::uint64_t> m_callerRangeCount{ 0 };
};

} // namespace heritage::jobs
