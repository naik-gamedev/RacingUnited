#pragma once

#include <chrono>

namespace heritage::timing {

class FrameLimiter
{
public:
    void beginFrame() noexcept;
    void endFrame(int targetFramesPerSecond);
    void reset() noexcept;

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point m_frameStart{};
    bool m_frameStarted = false;
};

} // namespace heritage::timing
