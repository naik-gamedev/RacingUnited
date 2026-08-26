#include "DynamicSurfaceGpuRuntime.hpp"

#include "../../Physics/Surfaces/DynamicSurface/DynamicSurfaceSystem.hpp"
#include "../../Physics/Surfaces/Water/SurfaceHydrology.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace heritage::graphics::dynamicsurface {

void DynamicSurfaceGpuRuntime::updateGpuTimerResult()
{
    for (std::size_t i = 0; i < m_gpuTimerPending.size(); ++i)
    {
        if (!m_gpuTimerPending[i])
            continue;
        GLint available = GL_FALSE;
        glGetQueryObjectiv(m_gpuTimerEndQueries[i], GL_QUERY_RESULT_AVAILABLE, &available);
        if (available != GL_TRUE)
            continue;
        GLuint64 start = 0;
        GLuint64 end = 0;
        glGetQueryObjectui64v(m_gpuTimerStartQueries[i], GL_QUERY_RESULT, &start);
        glGetQueryObjectui64v(m_gpuTimerEndQueries[i], GL_QUERY_RESULT, &end);
        if (end >= start)
            m_stats.gpuComputeMs = static_cast<double>(end - start) / 1000000.0;
        m_gpuTimerPending[i] = false;
    }
}
bool DynamicSurfaceGpuRuntime::beginGpuTimer()
{
    const std::size_t index = m_gpuTimerWriteIndex;
    if (m_gpuTimerPending[index])
        return false;
    glQueryCounter(m_gpuTimerStartQueries[index], GL_TIMESTAMP);
    return true;
}
void DynamicSurfaceGpuRuntime::endGpuTimer(bool began)
{
    if (!began)
        return;
    const std::size_t index = m_gpuTimerWriteIndex;
    glQueryCounter(m_gpuTimerEndQueries[index], GL_TIMESTAMP);
    m_gpuTimerPending[index] = true;
    m_gpuTimerWriteIndex = (m_gpuTimerWriteIndex + 1u) % m_gpuTimerPending.size();
}

} // namespace heritage::graphics::dynamicsurface
