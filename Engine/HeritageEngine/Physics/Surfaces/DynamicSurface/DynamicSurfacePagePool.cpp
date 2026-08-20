#include "DynamicSurfacePagePool.hpp"

#include <algorithm>
#include <limits>

namespace heritage::physics::dynamicsurface {

DynamicSurfacePagePool::DynamicSurfacePagePool()
{
    configure(kDefaultBudgetBytes);
}

void DynamicSurfacePagePool::clear()
{
    m_slots.assign(m_capacityPages, {});
    m_lookup.clear();
    m_useSerial = 0;
    // Physical-slot generations must remain monotonic across scene reloads.
    // Otherwise a recycled slot could accidentally match the renderer's old
    // generation and retain stale water/rubber/contamination texels.
    ++m_tableGeneration;
    m_allocationCount = 0;
    m_evictionCount = 0;
    m_failedAllocationCount = 0;
}

void DynamicSurfacePagePool::configure(
    std::size_t byteBudget,
    std::size_t maximumPhysicalPages)
{
    m_byteBudget = std::max<std::size_t>(
        byteBudget, kBytesPerPhysicalPageWithMipChain);
    std::size_t capacity = m_byteBudget / kBytesPerPhysicalPageWithMipChain;
    if (maximumPhysicalPages > 0)
        capacity = std::min(capacity, maximumPhysicalPages);
    capacity = std::max<std::size_t>(1, capacity);

    m_capacityPages = capacity;
    clear();
}

std::optional<PhysicalPageAssignment> DynamicSurfacePagePool::ensureResident(
    const VirtualPageAddress& address,
    bool pin)
{
    const auto found = m_lookup.find(address);
    if (found != m_lookup.end())
    {
        Slot& slot = m_slots[found->second];
        touch(slot.assignment);
        slot.assignment.pinned = slot.assignment.pinned || pin;
        return slot.assignment;
    }

    const std::optional<std::uint32_t> physicalSlot = acquireSlot();
    if (!physicalSlot)
    {
        ++m_failedAllocationCount;
        return std::nullopt;
    }

    Slot& slot = m_slots[*physicalSlot];
    slot.occupied = true;
    slot.assignment = {};
    slot.assignment.virtualAddress = address;
    slot.assignment.physicalSlot = *physicalSlot;
    slot.assignment.generation = static_cast<std::uint32_t>(m_nextGeneration++);
    slot.assignment.dirtyPlanes = PagePlaneMask::None;
    slot.assignment.pinned = pin;
    touch(slot.assignment);
    m_lookup[address] = *physicalSlot;
    ++m_allocationCount;
    ++m_tableGeneration;
    return slot.assignment;
}

bool DynamicSurfacePagePool::setPinned(
    const VirtualPageAddress& address,
    bool pinned)
{
    const auto found = m_lookup.find(address);
    if (found == m_lookup.end())
        return false;
    Slot& slot = m_slots[found->second];
    slot.assignment.pinned = pinned;
    touch(slot.assignment);
    return true;
}

bool DynamicSurfacePagePool::markDirty(
    const VirtualPageAddress& address,
    PagePlaneMask planes)
{
    const auto found = m_lookup.find(address);
    if (found == m_lookup.end())
        return false;
    Slot& slot = m_slots[found->second];
    slot.assignment.dirtyPlanes = slot.assignment.dirtyPlanes | planes;
    touch(slot.assignment);
    return true;
}

bool DynamicSurfacePagePool::acknowledgeClean(
    const VirtualPageAddress& address,
    PagePlaneMask planes)
{
    const auto found = m_lookup.find(address);
    if (found == m_lookup.end())
        return false;
    Slot& slot = m_slots[found->second];
    slot.assignment.dirtyPlanes = static_cast<PagePlaneMask>(
        static_cast<std::uint8_t>(slot.assignment.dirtyPlanes)
        & ~static_cast<std::uint8_t>(planes));
    return true;
}

const PhysicalPageAssignment* DynamicSurfacePagePool::find(
    const VirtualPageAddress& address) const
{
    const auto found = m_lookup.find(address);
    if (found == m_lookup.end())
        return nullptr;
    return &m_slots[found->second].assignment;
}

std::vector<PhysicalPageAssignment> DynamicSurfacePagePool::residentAssignments() const
{
    std::vector<PhysicalPageAssignment> result;
    result.reserve(m_lookup.size());
    for (const Slot& slot : m_slots)
    {
        if (slot.occupied)
            result.push_back(slot.assignment);
    }
    return result;
}

std::vector<PhysicalPageAssignment> DynamicSurfacePagePool::dirtyAssignments(
    std::size_t maximumCount) const
{
    std::vector<PhysicalPageAssignment> result;
    for (const Slot& slot : m_slots)
    {
        if (!slot.occupied || !any(slot.assignment.dirtyPlanes))
            continue;
        result.push_back(slot.assignment);
        if (maximumCount > 0 && result.size() >= maximumCount)
            break;
    }
    return result;
}

DynamicSurfacePagePoolTelemetry DynamicSurfacePagePool::telemetry() const
{
    DynamicSurfacePagePoolTelemetry result;
    result.capacityPages = m_capacityPages;
    result.byteBudget = m_byteBudget;
    result.committedBytes = m_capacityPages * kBytesPerPhysicalPageWithMipChain;
    result.allocationCount = m_allocationCount;
    result.evictionCount = m_evictionCount;
    result.failedAllocationCount = m_failedAllocationCount;
    result.tableGeneration = m_tableGeneration;

    for (const Slot& slot : m_slots)
    {
        if (!slot.occupied)
            continue;
        ++result.residentPages;
        if (slot.assignment.pinned)
            ++result.pinnedPages;
        if (any(slot.assignment.dirtyPlanes))
            ++result.dirtyPages;
    }
    return result;
}

std::optional<std::uint32_t> DynamicSurfacePagePool::acquireSlot()
{
    for (std::uint32_t index = 0; index < m_slots.size(); ++index)
    {
        if (!m_slots[index].occupied)
            return index;
    }

    const std::optional<std::uint32_t> candidate = findEvictionCandidate();
    if (!candidate)
        return std::nullopt;
    eraseSlot(*candidate);
    ++m_evictionCount;
    return candidate;
}

std::optional<std::uint32_t> DynamicSurfacePagePool::findEvictionCandidate() const
{
    std::optional<std::uint32_t> result;
    std::uint64_t oldestSerial = (std::numeric_limits<std::uint64_t>::max)();
    for (std::uint32_t index = 0; index < m_slots.size(); ++index)
    {
        const Slot& slot = m_slots[index];
        if (!slot.occupied || slot.assignment.pinned
            || any(slot.assignment.dirtyPlanes))
        {
            continue;
        }
        if (slot.assignment.lastUseSerial < oldestSerial)
        {
            oldestSerial = slot.assignment.lastUseSerial;
            result = index;
        }
    }
    return result;
}

void DynamicSurfacePagePool::eraseSlot(std::uint32_t slotIndex)
{
    if (slotIndex >= m_slots.size())
        return;
    Slot& slot = m_slots[slotIndex];
    if (!slot.occupied)
        return;
    m_lookup.erase(slot.assignment.virtualAddress);
    slot = {};
    ++m_tableGeneration;
}

void DynamicSurfacePagePool::touch(PhysicalPageAssignment& assignment)
{
    assignment.lastUseSerial = ++m_useSerial;
}

} // namespace heritage::physics::dynamicsurface
