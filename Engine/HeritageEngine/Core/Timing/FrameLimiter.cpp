#include "FrameLimiter.hpp"

#include <thread>

namespace heritage::timing {

void FrameLimiter::beginFrame() noexcept
{
    m_frameStart = Clock::now();
    m_frameStarted = true;
}

void FrameLimiter::endFrame(int targetFramesPerSecond)
{
    if (!m_frameStarted)
        return;

    m_frameStarted = false;

    if (targetFramesPerSecond <= 0)
        return;

    const auto targetDuration = std::chrono::duration<double>(
        1.0 / static_cast<double>(targetFramesPerSecond));
    const auto targetDeadline = m_frameStart
        + std::chrono::duration_cast<Clock::duration>(targetDuration);

    auto now = Clock::now();
    if (now >= targetDeadline)
        return;

    // Sleep through most of the remaining frame, then use a short yield loop
    // near the deadline. This avoids both a full busy-wait and the large
    // overshoot that can come from sleeping for the entire remainder.
    constexpr auto finalWaitReserve = std::chrono::microseconds(500);
    const auto remaining = targetDeadline - now;

    if (remaining > finalWaitReserve)
        std::this_thread::sleep_until(targetDeadline - finalWaitReserve);

    while (Clock::now() < targetDeadline)
        std::this_thread::yield();
}

void FrameLimiter::reset() noexcept
{
    m_frameStarted = false;
}

} // namespace heritage::timing
