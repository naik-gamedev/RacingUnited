#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "DynamicSurfaceTypes.hpp"

namespace heritage::physics::dynamicsurface {

// Persistent software-virtual-texture address. The key contains world/chunk
// identity and sheet identity; it never contains camera-relative information.
struct VirtualPageAddress
{
    ChunkAddress chunk{};
    PageAddress page{};

    constexpr bool operator==(const VirtualPageAddress&) const = default;
    constexpr bool operator<(const VirtualPageAddress& other) const
    {
        if (chunk < other.chunk)
            return true;
        if (other.chunk < chunk)
            return false;
        if (page.sheet != other.page.sheet)
            return page.sheet < other.page.sheet;
        if (page.z != other.page.z)
            return page.z < other.page.z;
        return page.x < other.page.x;
    }
};

// Stable 0..1 phase derived only from persistent world/sheet page identity.
// Hydro, Track and later Dynamic Surface planes can share this to stagger the
// same fixed-2Hz tile cadence without introducing camera or midpoint state.
inline double deterministicPagePhase01(const VirtualPageAddress& address)
{
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&hash](std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    mix(static_cast<std::uint64_t>(address.chunk.x));
    mix(static_cast<std::uint64_t>(address.chunk.z));
    mix(static_cast<std::uint64_t>(address.page.sheet));
    mix(static_cast<std::uint64_t>(address.page.x));
    mix(static_cast<std::uint64_t>(address.page.z));
    return static_cast<double>(hash & 0xffffu) / 65536.0;
}

enum class PagePlaneMask : std::uint8_t
{
    None = 0,
    Hydro = 1u << 0,
    Track = 1u << 1,
    Contamination = 1u << 2,
    All = (1u << 0) | (1u << 1) | (1u << 2)
};

constexpr PagePlaneMask operator|(PagePlaneMask a, PagePlaneMask b)
{
    return static_cast<PagePlaneMask>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

constexpr PagePlaneMask operator&(PagePlaneMask a, PagePlaneMask b)
{
    return static_cast<PagePlaneMask>(
        static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

constexpr PagePlaneMask operator~(PagePlaneMask value)
{
    return static_cast<PagePlaneMask>(~static_cast<std::uint8_t>(value));
}

constexpr bool any(PagePlaneMask value)
{
    return static_cast<std::uint8_t>(value) != 0u;
}

struct PhysicalPageAssignment
{
    VirtualPageAddress virtualAddress{};
    std::uint32_t physicalSlot = 0;
    std::uint32_t generation = 0;
    PagePlaneMask dirtyPlanes = PagePlaneMask::None;
    bool pinned = false;
    std::uint64_t lastUseSerial = 0;
};

struct DynamicSurfacePagePoolTelemetry
{
    std::size_t capacityPages = 0;
    std::size_t residentPages = 0;
    std::size_t dirtyPages = 0;
    std::size_t pinnedPages = 0;
    std::size_t byteBudget = 0;
    std::size_t committedBytes = 0;
    std::uint64_t allocationCount = 0;
    std::uint64_t evictionCount = 0;
    std::uint64_t failedAllocationCount = 0;
    std::uint64_t tableGeneration = 0;
};

// CPU authority for DSURF02 page residency. It deliberately knows nothing
// about OpenGL. Graphics mirrors these assignments into a texture-array pool.
class DynamicSurfacePagePool
{
public:
    static constexpr std::size_t kDefaultBudgetBytes = 96ull * 1024ull * 1024ull;

    DynamicSurfacePagePool();

    void clear();
    void configure(std::size_t byteBudget, std::size_t maximumPhysicalPages = 0);

    std::optional<PhysicalPageAssignment> ensureResident(
        const VirtualPageAddress& address,
        bool pin = false);
    bool setPinned(const VirtualPageAddress& address, bool pinned);
    bool markDirty(
        const VirtualPageAddress& address,
        PagePlaneMask planes = PagePlaneMask::All);
    bool acknowledgeClean(
        const VirtualPageAddress& address,
        PagePlaneMask planes = PagePlaneMask::All);

    const PhysicalPageAssignment* find(const VirtualPageAddress& address) const;
    std::vector<PhysicalPageAssignment> residentAssignments() const;
    std::vector<PhysicalPageAssignment> dirtyAssignments(
        std::size_t maximumCount = 0) const;

    DynamicSurfacePagePoolTelemetry telemetry() const;
    std::size_t capacityPages() const { return m_capacityPages; }
    std::size_t byteBudget() const { return m_byteBudget; }
    std::uint64_t tableGeneration() const { return m_tableGeneration; }

private:
    struct Slot
    {
        bool occupied = false;
        PhysicalPageAssignment assignment{};
    };

    std::optional<std::uint32_t> acquireSlot();
    std::optional<std::uint32_t> findEvictionCandidate() const;
    void eraseSlot(std::uint32_t slot);
    void touch(PhysicalPageAssignment& assignment);

    std::size_t m_byteBudget = kDefaultBudgetBytes;
    std::size_t m_capacityPages = 0;
    std::vector<Slot> m_slots;
    std::map<VirtualPageAddress, std::uint32_t> m_lookup;
    std::uint64_t m_useSerial = 0;
    std::uint64_t m_nextGeneration = 1;
    std::uint64_t m_tableGeneration = 1;
    std::uint64_t m_allocationCount = 0;
    std::uint64_t m_evictionCount = 0;
    std::uint64_t m_failedAllocationCount = 0;
};

} // namespace heritage::physics::dynamicsurface
