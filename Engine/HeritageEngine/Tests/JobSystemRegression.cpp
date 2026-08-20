#include "PhysicsRegressionCommon.hpp"

#include "../Core/Jobs/JobSystem.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace heritage::tests {

bool jobSystemParallelForIsBoundedAndDeterministic()
{
    heritage::jobs::JobSystemDescription description;
    description.requestedWorkerCount = 3u;
    heritage::jobs::JobSystem jobs(description);

    constexpr std::size_t kItemCount = 32768u;
    constexpr std::size_t kGrain = 257u;
    const std::size_t expectedRanges = (kItemCount + kGrain - 1u) / kGrain;

    std::vector<std::uint64_t> values(kItemCount, 0u);
    std::vector<std::uint64_t> rangeSums(expectedRanges, 0u);

    jobs.parallelFor(
        kItemCount,
        kGrain,
        [&](std::size_t begin, std::size_t end, std::size_t rangeIndex) {
            std::uint64_t local = 0u;
            for (std::size_t index = begin; index < end; ++index)
            {
                const std::uint64_t value =
                    static_cast<std::uint64_t>(index) * 17ull + 3ull;
                values[index] = value;
                local += value;
            }
            rangeSums[rangeIndex] = local;
        });

    std::uint64_t expectedTotal = 0u;
    for (std::size_t index = 0; index < kItemCount; ++index)
    {
        const std::uint64_t expected =
            static_cast<std::uint64_t>(index) * 17ull + 3ull;
        if (values[index] != expected)
            return false;
        expectedTotal += expected;
    }

    std::uint64_t reducedTotal = 0u;
    for (const std::uint64_t rangeSum : rangeSums)
        reducedTotal += rangeSum;
    if (reducedTotal != expectedTotal)
        return false;

    // Nested work on the same pool is deliberately executed inline. This
    // regression proves the no-deadlock policy and exact-once semantics.
    std::vector<std::uint32_t> nestedHits(4096u, 0u);
    jobs.parallelFor(
        nestedHits.size(),
        128u,
        [&](std::size_t begin, std::size_t end, std::size_t) {
            jobs.parallelFor(
                end - begin,
                16u,
                [&](std::size_t nestedBegin,
                    std::size_t nestedEnd,
                    std::size_t) {
                    for (std::size_t local = nestedBegin;
                         local < nestedEnd; ++local)
                    {
                        ++nestedHits[begin + local];
                    }
                });
        });
    for (const std::uint32_t hits : nestedHits)
    {
        if (hits != 1u)
            return false;
    }

    // JOB01A: hammer stack-owned ParallelBatch lifetime with many consecutive
    // synchronous batches. The original JOB01 completion path allowed the
    // caller to destroy a batch condition_variable while the last worker was
    // still inside notify_one(), producing intermittent rain-activation hangs
    // on larger hydrology fields.
    constexpr std::size_t kStressItemCount = 65536u;
    constexpr std::size_t kStressGrain = 1024u;
    std::atomic<std::uint64_t> stressVisits{ 0u };
    for (std::size_t pass = 0; pass < 96u; ++pass)
    {
        jobs.parallelFor(
            kStressItemCount,
            kStressGrain,
            [&](std::size_t begin, std::size_t end, std::size_t) {
                stressVisits.fetch_add(
                    static_cast<std::uint64_t>(end - begin),
                    std::memory_order_relaxed);
            });
    }
    if (stressVisits.load(std::memory_order_relaxed)
        != static_cast<std::uint64_t>(kStressItemCount) * 96ull)
    {
        return false;
    }

    jobs.waitIdle();
    const heritage::jobs::JobSystemStats stats = jobs.stats();
    if (stats.hardwareThreadCount == 0u
        || stats.parallelRangeCount < expectedRanges
        || stats.callerRangeCount == 0u)
    {
        return false;
    }

    // On any machine with more than one hardware thread, the requested pool
    // must contain at least one persistent worker. Do not require a worker to
    // win a particular tiny range race; scheduling is intentionally OS-owned.
    if (stats.hardwareThreadCount > 1u && stats.workerThreadCount == 0u)
        return false;

    return true;
}

} // namespace heritage::tests
