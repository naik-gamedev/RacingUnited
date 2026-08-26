#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <glad/glad.h>

namespace heritage::graphics {

// OPT00: non-blocking GPU timestamp ring for durable pass profiling.
//
// This deliberately uses GL_TIMESTAMP pairs instead of nested GL_TIME_ELAPSED
// queries. Heritage already owns a frame-wide GL_TIME_ELAPSED query; timestamp
// pairs can live inside that query and inside one another without changing the
// command stream or forcing a synchronization point.
class AsyncGpuTimer final
{
public:
    static constexpr std::size_t kSlotCount = 4;

    bool initialize()
    {
        shutdown();
        glGenQueries(static_cast<GLsizei>(m_startQueries.size()), m_startQueries.data());
        glGenQueries(static_cast<GLsizei>(m_endQueries.size()), m_endQueries.data());
        return m_startQueries[0] != 0 && m_endQueries[0] != 0;
    }

    void shutdown()
    {
        if (m_startQueries[0] != 0)
            glDeleteQueries(static_cast<GLsizei>(m_startQueries.size()), m_startQueries.data());
        if (m_endQueries[0] != 0)
            glDeleteQueries(static_cast<GLsizei>(m_endQueries.size()), m_endQueries.data());
        m_startQueries.fill(0);
        m_endQueries.fill(0);
        m_issued.fill(false);
        m_cursor = 0;
        m_active = false;
        m_activeSlot = 0;
        m_latestMs = 0.0;
        m_sampleCount = 0;
    }

    // Resolves any completed older samples. GL_QUERY_RESULT is read only after
    // GL_QUERY_RESULT_AVAILABLE reports true, so this path never waits for the
    // current frame's GPU work.
    bool poll(double& latestMilliseconds)
    {
        bool resolvedAny = false;
        GLuint64 newestEndNs = 0;
        double newestMilliseconds = m_latestMs;
        for (std::size_t slot = 0; slot < kSlotCount; ++slot)
        {
            if (!m_issued[slot] || m_endQueries[slot] == 0)
                continue;

            GLint available = GL_FALSE;
            glGetQueryObjectiv(m_endQueries[slot], GL_QUERY_RESULT_AVAILABLE, &available);
            if (available != GL_TRUE)
                continue;

            GLuint64 startNs = 0;
            GLuint64 endNs = 0;
            glGetQueryObjectui64v(m_startQueries[slot], GL_QUERY_RESULT, &startNs);
            glGetQueryObjectui64v(m_endQueries[slot], GL_QUERY_RESULT, &endNs);
            if (endNs >= startNs)
            {
                const double milliseconds =
                    static_cast<double>(endNs - startNs) / 1000000.0;
                if (!resolvedAny || endNs > newestEndNs)
                {
                    newestEndNs = endNs;
                    newestMilliseconds = milliseconds;
                }
                ++m_sampleCount;
                resolvedAny = true;
            }
            m_issued[slot] = false;
        }
        if (resolvedAny)
        {
            m_latestMs = newestMilliseconds;
            latestMilliseconds = newestMilliseconds;
        }
        return resolvedAny;
    }

    bool begin()
    {
        if (m_active || m_startQueries[0] == 0)
            return false;

        const std::size_t slot = m_cursor;
        if (m_issued[slot])
            return false; // GPU is more than the ring depth behind; skip safely.

        glQueryCounter(m_startQueries[slot], GL_TIMESTAMP);
        m_active = true;
        m_activeSlot = slot;
        return true;
    }

    void end(bool began)
    {
        if (!began || !m_active)
            return;

        glQueryCounter(m_endQueries[m_activeSlot], GL_TIMESTAMP);
        m_issued[m_activeSlot] = true;
        m_cursor = (m_activeSlot + 1) % kSlotCount;
        m_active = false;
    }

    double latestMilliseconds() const { return m_latestMs; }
    std::uint64_t sampleCount() const { return m_sampleCount; }

private:
    std::array<GLuint, kSlotCount> m_startQueries{};
    std::array<GLuint, kSlotCount> m_endQueries{};
    std::array<bool, kSlotCount> m_issued{};
    std::size_t m_cursor = 0;
    bool m_active = false;
    std::size_t m_activeSlot = 0;
    double m_latestMs = 0.0;
    std::uint64_t m_sampleCount = 0;
};

} // namespace heritage::graphics
