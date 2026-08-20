#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <vector>

#include "DynamicSurfaceChunk.hpp"
#include "DynamicSurfacePagePool.hpp"
#include "DynamicSurfaceHydrology.hpp"
#include "DynamicSurfaceThermal.hpp"
#include "../../CollisionSystem.hpp"

namespace heritage::physics::dynamicsurface {


struct DynamicSurfaceHydroMigrationStats
{
    std::size_t candidatePages = 0;
    std::size_t requestedResidentPages = 0;
    std::size_t residentHydroPages = 0;
    std::size_t failedResidencyRequests = 0;
};

// World-owned Dynamic Surface service. DSURF01 provides persistent static
// surface sheets; DSURF02 adds software virtual page residency. Simulation
// migration into those pages remains intentionally staged in DSURF03+.
class DynamicSurfaceSystem
{
public:
    void clear();

    static ChunkAddress chunkAddressFor(
        const heritage::math::DVec3& globalPosition);

    DynamicSurfaceChunk& acquireChunk(ChunkAddress address);
    DynamicSurfaceChunk* findChunk(ChunkAddress address);
    const DynamicSurfaceChunk* findChunk(ChunkAddress address) const;

    std::size_t chunkCount() const { return m_chunks.size(); }
    std::vector<ChunkAddress> chunkAddresses() const;

    bool loadOrBakeStaticScene(
        const std::vector<StaticSceneTriangle>& localTriangles,
        const heritage::math::DVec3& globalOrigin,
        const std::filesystem::path& cachePath,
        DynamicSurfaceStaticBakeReport& report);
    bool bakeStaticScene(
        const std::vector<StaticSceneTriangle>& localTriangles,
        const heritage::math::DVec3& globalOrigin,
        DynamicSurfaceStaticBakeReport& report);

    const std::vector<StaticSurfaceSheetLink>& sheetLinks() const
    {
        return m_sheetLinks;
    }
    const DynamicSurfaceStaticBakeReport& lastStaticBakeReport() const
    {
        return m_lastStaticBakeReport;
    }

    void setInterestSources(
        const std::vector<heritage::math::DVec3>& sources);
    const std::vector<heritage::math::DVec3>& interestSources() const
    {
        return m_interestSources;
    }

    // Highest cadence requested by any interest source. There is no synthetic
    // midpoint between local players.
    double requestedUpdateHz(ChunkAddress address) const;

    // DSURF02: software virtual-texture residency is world owned and camera
    // independent. Graphics mirrors these stable assignments into physical
    // texture-array layers; DSURF03+ will request/dirty pages as simulation
    // state migrates into Dynamic Surface.
    DynamicSurfacePagePool& pagePool() { return m_pagePool; }
    const DynamicSurfacePagePool& pagePool() const { return m_pagePool; }

    std::optional<PhysicalPageAssignment> ensurePageResident(
        const VirtualPageAddress& address,
        bool pin = false);
    bool markPageDirty(
        const VirtualPageAddress& address,
        PagePlaneMask planes = PagePlaneMask::All);

    // DSURF03: keep the nearest real surface pages resident for hydrology.
    // Residency is selected from DSURF01 surface coverage, never from a
    // camera-centred square texture. State remains world/sheet anchored.
    void refreshHydroResidency();
    const DynamicSurfaceHydroMigrationStats& hydroMigrationStats() const
    {
        return m_hydroMigrationStats;
    }

    // DSURF03B: authoritative water/moisture/flow now lives in persistent
    // Dynamic Surface page addresses. SurfaceHydrology remains only as a
    // temporary static-cover/debug compatibility service in SurfaceWorld.
    void advanceHydro(
        const SurfaceWeatherDescription& weather,
        const SurfaceWeatherOutput& weatherOutput,
        double deltaTimeSeconds);
    void clearHydroState();
    void resetHydroWater();
    DynamicSurfaceHydroSample sampleHydro(
        const heritage::math::DVec3& globalPosition) const;
    DynamicSurfaceHydroTireResult applyHydroTireContact(
        const heritage::math::DVec3& globalPosition,
        const DynamicSurfaceHydroTireInput& input);
    bool rasterHydroPage(
        const VirtualPageAddress& address,
        std::uint32_t outputResolution,
        std::vector<std::uint16_t>& hydroRgba4) const;
    std::uint64_t hydroPageRevision(const VirtualPageAddress& address) const;
    bool setUniformHydroDepthForLab(double waterDepthM);
    const DynamicSurfaceHydroStats& hydroStats() const
    {
        return m_hydrology.stats();
    }

    // DSURF04: Track-plane surface temperature is now persistent Dynamic
    // Surface authority. It shares the same world/chunk/sheet/page identity
    // and nearest-real-interest-source cadence as Hydro.
    void advanceThermal(
        const SurfaceWeatherDescription& weather,
        const SurfaceWeatherOutput& weatherOutput,
        double ambientTemperatureC,
        bool surfaceTemperatureOverrideEnabled,
        double surfaceTemperatureOverrideC,
        double deltaTimeSeconds);
    void resetThermalState();
    DynamicSurfaceThermalSample sampleThermal(
        const heritage::math::DVec3& globalPosition) const;
    DynamicSurfaceThermalTireResult applyThermalTireContact(
        const heritage::math::DVec3& globalPosition,
        const DynamicSurfaceThermalTireInput& input);
    bool rasterTrackPage(
        const VirtualPageAddress& address,
        std::uint32_t outputResolution,
        std::vector<float>& trackRgba) const;
    std::uint64_t trackPageRevision(const VirtualPageAddress& address) const;
    const DynamicSurfaceThermalStats& thermalStats() const
    {
        return m_thermal.stats();
    }

private:
    friend class DynamicSurfaceHydrology;
    friend class DynamicSurfaceThermal;
    std::uint64_t staticSceneFingerprint(
        const std::vector<StaticSceneTriangle>& localTriangles,
        const heritage::math::DVec3& globalOrigin) const;
    bool loadStaticBakeCache(
        const std::filesystem::path& cachePath,
        std::uint64_t expectedFingerprint,
        DynamicSurfaceStaticBakeReport& report);
    bool writeStaticBakeCache(
        const std::filesystem::path& cachePath,
        std::uint64_t fingerprint) const;

    std::map<ChunkAddress, DynamicSurfaceChunk> m_chunks;
    std::vector<StaticSurfaceSheetLink> m_sheetLinks;
    DynamicSurfaceStaticBakeReport m_lastStaticBakeReport{};
    std::vector<heritage::math::DVec3> m_interestSources;
    DynamicSurfacePagePool m_pagePool;
    DynamicSurfaceHydrology m_hydrology;
    DynamicSurfaceThermal m_thermal;
    DynamicSurfaceHydroMigrationStats m_hydroMigrationStats{};
};

} // namespace heritage::physics::dynamicsurface
