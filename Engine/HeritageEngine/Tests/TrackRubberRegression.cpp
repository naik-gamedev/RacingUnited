#include "PhysicsRegressionCommon.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace heritage::tests {

bool trackRubberBuildsMigratesAndWashes()
{
    using heritage::math::DVec3;
    using heritage::math::Vec3;
    using heritage::physics::SurfaceMaterial;
    using heritage::physics::SurfaceWorld;
    using heritage::physics::rubber::TrackRubberContactInput;
    using heritage::physics::rubber::TrackRubberVisualCell;
    using heritage::physics::rubber::TrackRubberWakeInput;
    using heritage::physics::rubber::TrackRubberTransientVisual;

    const auto settleMovingRubber = [](heritage::physics::rubber::TrackRubberState& state,
                                       float seconds = 2.5f) {
        const int steps = static_cast<int>(std::ceil(seconds * 60.0f));
        for (int step = 0; step < steps; ++step)
            state.advance(1.0f / 60.0f, 0.0f);
    };

    SurfaceWorld world;
    world.setGlobalOrigin({ 100000.0, 12.0, -50000.0 });
    const Vec3 local{ 8.25f, 0.10f, -4.25f };

    // TIRE15C1: gentle straight-line rolling may very slowly rubber-in the
    // pavement, but must not continuously manufacture loose visible marbles.
    {
        heritage::physics::rubber::TrackRubberState gentleState;
        TrackRubberContactInput gentle;
        gentle.material = SurfaceMaterial::Asphalt;
        gentle.deltaTimeSeconds = 0.001f;
        gentle.normalLoadN = 3500.0f;
        gentle.nominalLoadN = 3500.0f;
        gentle.tireWidthM = 0.205f;
        gentle.forwardSpeedMps = 6.0f;
        gentle.longitudinalSlipSpeedMps = 0.02f;
        gentle.lateralSlipSpeedMps = 0.01f;
        gentle.slipDissipationWatts = 20.0f;
        gentle.treadWearDepthDeltaM = 0.0f;
        gentle.treadTemperatureC = 45.0f;
        for (int step = 0; step < 10000; ++step)
            gentleState.applyContact({ 0.0, 0.0, 0.0 }, gentle);
        const auto gentleSample = gentleState.sample(
            { 0.0, 0.0, 0.0 }, SurfaceMaterial::Asphalt, 0.0f);
        if (!gentleSample.valid
            || gentleSample.looseRubber > 1.0e-5f
            || gentleSample.depositedRubber > 0.010f)
        {
            return false;
        }
    }

    // TIRE16G: production 1x is exactly 3x the TIRE16D calibration after user
    // testing showed 0.12 was still too sterile. A continuously worked road tire
    // must now create a clearly non-zero but still bounded marble population.
    {
        heritage::physics::rubber::TrackRubberState workedState;
        TrackRubberContactInput worked;
        worked.material = SurfaceMaterial::Asphalt;
        worked.deltaTimeSeconds = 0.004f;
        worked.normalLoadN = 3500.0f;
        worked.nominalLoadN = 3500.0f;
        worked.tireWidthM = 0.205f;
        worked.forwardSpeedMps = 12.0f;
        worked.longitudinalSlipSpeedMps = 0.25f;
        worked.lateralSlipSpeedMps = 0.75f;
        worked.slipDissipationWatts = 2200.0f;
        worked.treadWearDepthDeltaM = 1.0e-9f;
        worked.treadWearFraction = 0.20f;
        worked.treadTemperatureC = 78.0f;
        for (int step = 0; step < 30000; ++step)
            workedState.applyContact({ 0.0, 0.0, 0.0 }, worked);
        const auto workedStats = workedState.stats();
        if (workedStats.looseGeneration < 0.135
            || workedStats.looseGeneration > 0.36)
        {
            return false;
        }
    }

    // TIRE15C2: wear and compound are continuous susceptibility factors, not
    // a magic tread-life threshold. Equal stress on a worn/shedding-prone tire
    // must create more loose rubber than a fresh/low-shedding tire, while both
    // remain capable of shedding under sufficiently abusive conditions.
    const auto sheddingSample = [](float wearFraction, float compoundFactor) {
        heritage::physics::rubber::TrackRubberState state;
        TrackRubberContactInput shedding;
        shedding.material = SurfaceMaterial::Asphalt;
        shedding.deltaTimeSeconds = 0.001f;
        shedding.normalLoadN = 3900.0f;
        shedding.nominalLoadN = 3500.0f;
        shedding.tireWidthM = 0.205f;
        shedding.forwardSpeedMps = 26.0f;
        shedding.longitudinalSlipSpeedMps = 1.9f;
        shedding.lateralSlipSpeedMps = 0.8f;
        shedding.slipDissipationWatts = 4200.0f;
        shedding.treadWearDepthDeltaM = 1.5e-9f;
        shedding.treadWearFraction = wearFraction;
        shedding.treadTemperatureC = 82.0f;
        shedding.compoundSheddingFactor = compoundFactor;
        for (int step = 0; step < 1200; ++step)
            state.applyContact({ 0.0, 0.0, 0.0 }, shedding);
        for (int step = 0; step < 180; ++step)
            state.advance(1.0f / 60.0f, 0.0f);
        std::vector<TrackRubberVisualCell> cells;
        state.collectPresentationCells({ 0.0, 0.0, 0.0 }, 3.0, cells, 128);
        float looseTotal = 0.0f;
        for (const auto& cell : cells)
            looseTotal += cell.looseRubber;
        return looseTotal;
    };
    const float freshHardLoose = sheddingSample(0.05f, 0.65f);
    const float wornSoftLoose = sheddingSample(0.75f, 1.35f);
    if (freshHardLoose <= 0.0f || wornSoftLoose <= freshHardLoose * 1.45f)
        return false;

    TrackRubberContactInput input;
    input.material = SurfaceMaterial::Asphalt;
    input.normal = { 0.0f, 1.0f, 0.0f };
    input.forward = { 0.0f, 0.0f, 1.0f };
    input.deltaTimeSeconds = 0.001f;
    input.wetness = 0.0f;
    input.normalLoadN = 3600.0f;
    input.nominalLoadN = 3500.0f;
    input.tireWidthM = 0.205f;
    input.forwardSpeedMps = 22.0f;
    input.longitudinalSlipSpeedMps = 1.4f;
    input.lateralSlipSpeedMps = 0.55f;
    input.slipDissipationWatts = 2800.0f;
    input.treadWearDepthDeltaM = 1.0e-9f;
    input.treadTemperatureC = 72.0f;

    // C5A deliberately lowered production-world loose-rubber flux. Build a
    // dense field quickly here with the existing developer multiplier so this
    // regression remains about migration/persistence rather than waiting tens
    // of simulated seconds for presentation-scale debris.
    TrackRubberContactInput fieldBuildInput = input;
    fieldBuildInput.generationMultiplier = 25.0f;
    for (int step = 0; step < 2500; ++step)
        world.applyTrackRubberContact(local, fieldBuildInput);
    for (int step = 0; step < 180; ++step)
        world.advancePresentation(1.0f / 60.0f);

    const auto dryCenter = world.sampleTrackRubber(local, SurfaceMaterial::Asphalt, 0.0f);
    if (!dryCenter.valid
        || dryCenter.depositedRubber <= 0.030f
        || dryCenter.contactFrictionScale <= 1.0f
        || dryCenter.passCount == 0u)
    {
        return false;
    }

    const auto wetCenter = world.sampleTrackRubber(local, SurfaceMaterial::Asphalt, 1.0f);
    if (wetCenter.contactFrictionScale >= dryCenter.contactFrictionScale)
        return false;

    // Rubber evolution is a rate process, not a hidden fixed-step constant.
    // One physical second at 1000 Hz and 250 Hz should converge closely.
    const auto rateSample = [](float deltaTimeSeconds) {
        heritage::physics::rubber::TrackRubberState state;
        TrackRubberContactInput rateInput;
        rateInput.material = SurfaceMaterial::Asphalt;
        rateInput.deltaTimeSeconds = deltaTimeSeconds;
        rateInput.normalLoadN = 3600.0f;
        rateInput.nominalLoadN = 3500.0f;
        rateInput.tireWidthM = 0.205f;
        rateInput.forwardSpeedMps = 22.0f;
        rateInput.longitudinalSlipSpeedMps = 1.4f;
        rateInput.lateralSlipSpeedMps = 0.55f;
        rateInput.slipDissipationWatts = 2800.0f;
        rateInput.treadWearDepthDeltaM = 1.0e-6f * deltaTimeSeconds;
        rateInput.treadTemperatureC = 72.0f;
        const int steps = static_cast<int>(std::lround(1.0f / deltaTimeSeconds));
        for (int step = 0; step < steps; ++step)
            state.applyContact({ 0.0, 0.0, 0.0 }, rateInput);
        for (int step = 0; step < 180; ++step)
            state.advance(1.0f / 60.0f, 0.0f);
        return state.sample({ 0.0, 0.0, 0.0 }, SurfaceMaterial::Asphalt, 0.0f);
    };
    const auto rubber1000Hz = rateSample(0.001f);
    const auto rubber250Hz = rateSample(0.004f);
    if (std::abs(rubber1000Hz.depositedRubber - rubber250Hz.depositedRubber) > 0.0025f
        || std::abs(rubber1000Hz.looseRubber - rubber250Hz.looseRubber) > 0.00025f
        || std::abs(rubber1000Hz.contactFrictionScale - rubber250Hz.contactFrictionScale) > 0.0005f)
    {
        return false;
    }

    std::vector<TrackRubberVisualCell> visual;
    const DVec3 centerGlobal = world.localToGlobal(local);
    world.trackRubber().collectPresentationCells(centerGlobal, 3.0, visual, 128);
    const auto loose = std::max_element(
        visual.begin(), visual.end(),
        [](const TrackRubberVisualCell& a, const TrackRubberVisualCell& b) {
            return a.looseRubber < b.looseRubber;
        });
    if (loose == visual.end() || loose->looseRubber <= 0.002f)
        return false;

    const auto looseSample = world.trackRubber().sample(
        loose->globalPosition, loose->material, 0.0f);
    // A C5 cell may legitimately contain both rubbered-in deposit and loose
    // marbles, so its *net* friction scale need not be below bare asphalt.
    // What matters here is that the loose reservoir is physically available
    // for pickup and backed by a persistent logical piece population.
    if (looseSample.pickupAvailability <= 0.0f
        || looseSample.persistentPiecePopulation < 0.5f)
    {
        return false;
    }

    // Fresh loose rubber does not mature simply because wall/simulation time
    // passes. Traffic over the pile must do the work. The lab multiplier can
    // accelerate that physical process for visual inspection.
    const float maturityBeforeTraffic = looseSample.marbleMaturity;
    world.trackRubber().advance(120.0f, 0.0f);
    const auto maturityAfterWaiting = world.trackRubber().sample(
        loose->globalPosition, loose->material, 0.0f);
    if (std::abs(maturityAfterWaiting.marbleMaturity - maturityBeforeTraffic) > 1.0e-6f
        || std::abs(maturityAfterWaiting.looseRubber - looseSample.looseRubber) > 1.0e-6f
        || std::abs(
            maturityAfterWaiting.persistentPiecePopulation
                - looseSample.persistentPiecePopulation) > 1.0e-4f)
    {
        return false;
    }

    TrackRubberContactInput traffic = input;
    traffic.deltaTimeSeconds = 0.004f;
    traffic.forwardSpeedMps = 18.0f;
    traffic.longitudinalSlipSpeedMps = 0.05f;
    traffic.lateralSlipSpeedMps = 0.05f;
    traffic.slipDissipationWatts = 80.0f;
    traffic.treadWearDepthDeltaM = 0.0f;
    traffic.maturationMultiplier = 100.0f;
    for (int step = 0; step < 120; ++step)
        world.trackRubber().applyContact(loose->globalPosition, traffic);
    const auto maturityAfterTraffic = world.trackRubber().sample(
        loose->globalPosition, loose->material, 0.0f);
    if (maturityAfterTraffic.marbleMaturity <= maturityBeforeTraffic + 0.08f)
        return false;

    // TIRE15C4: a resting cell owns a stable support-plane anchor. Revisiting
    // the same X/Z cell with a different steering direction or slightly
    // different contact Y must not make already-resting marbles rise/sink.
    {
        heritage::physics::rubber::TrackRubberState anchoredState;
        TrackRubberContactInput anchoredInput = input;
        anchoredInput.generationMultiplier = 250.0f;
        anchoredInput.forward = { 0.0f, 0.0f, 1.0f };
        const DVec3 anchoredPosition{ 0.10, 0.25, 0.10 };
        for (int step = 0; step < 80; ++step)
            anchoredState.applyContact(anchoredPosition, anchoredInput);

        std::vector<TrackRubberVisualCell> anchoredCells;
        anchoredState.collectPresentationCells(
            { 0.25, 0.25, 0.25 }, 2.0, anchoredCells, 128);
        if (anchoredCells.empty())
            return false;
        const auto closestBefore = std::min_element(
            anchoredCells.begin(), anchoredCells.end(),
            [&](const TrackRubberVisualCell& a, const TrackRubberVisualCell& b) {
                const double adx = a.globalPosition.x - 0.25;
                const double adz = a.globalPosition.z - 0.25;
                const double bdx = b.globalPosition.x - 0.25;
                const double bdz = b.globalPosition.z - 0.25;
                return adx * adx + adz * adz < bdx * bdx + bdz * bdz;
            });
        const double anchoredHeight = closestBefore->globalPosition.y;

        anchoredInput.forward = { 1.0f, 0.0f, 0.0f };
        for (int step = 0; step < 20; ++step)
            anchoredState.applyContact({ 0.10, 0.40, 0.10 }, anchoredInput);

        anchoredCells.clear();
        anchoredState.collectPresentationCells(
            { 0.25, 0.25, 0.25 }, 2.0, anchoredCells, 128);
        if (anchoredCells.empty())
            return false;
        const auto closestAfter = std::min_element(
            anchoredCells.begin(), anchoredCells.end(),
            [&](const TrackRubberVisualCell& a, const TrackRubberVisualCell& b) {
                const double adx = a.globalPosition.x - 0.25;
                const double adz = a.globalPosition.z - 0.25;
                const double bdx = b.globalPosition.x - 0.25;
                const double bdz = b.globalPosition.z - 0.25;
                return adx * adx + adz * adz < bdx * bdx + bdz * bdz;
            });
        if (std::abs(closestAfter->globalPosition.y - anchoredHeight) > 1.0e-9)
            return false;
    }

    // TIRE15C5: fresh tear-off must exist as authoritative moving rubber before
    // settlement, and a high-speed vehicle wake must migrate an established
    // loose-rubber field without deleting the swept quantity.
    {
        heritage::physics::rubber::TrackRubberState dynamicState;
        TrackRubberContactInput dynamicInput = input;
        dynamicInput.generationMultiplier = 350.0f;
        dynamicInput.longitudinalSlipSpeedMps = 2.6f;
        dynamicInput.lateralSlipSpeedMps = 1.0f;
        dynamicInput.slipDissipationWatts = 9000.0f;
        dynamicInput.treadWearDepthDeltaM = 1.2e-8f;
        dynamicInput.treadWearFraction = 0.55f;
        dynamicInput.treadTemperatureC = 102.0f;
        for (int step = 0; step < 70; ++step)
            dynamicState.applyContact({ 0.0, 0.0, 0.0 }, dynamicInput);

        const auto transientBeforeSettle = dynamicState.stats();
        if (transientBeforeSettle.transientPackets == 0u
            || transientBeforeSettle.transientLoose <= 0.0)
        {
            return false;
        }
        std::vector<TrackRubberTransientVisual> movingVisuals;
        dynamicState.collectTransientPresentation(
            { 0.0, 0.0, 0.0 }, 10.0, movingVisuals, 128);
        if (movingVisuals.empty()
            || movingVisuals.front().lengthM <= 0.0f
            || movingVisuals.front().widthM <= 0.0f)
        {
            return false;
        }
        // TIRE16L: the GPU feed deliberately skips per-frame distance sorting.
        // It must still expose the same bounded authoritative moving packets.
        std::vector<TrackRubberTransientVisual> movingGpuFeed;
        dynamicState.collectTransientPresentationUnsorted(
            { 0.0, 0.0, 0.0 }, 10.0, movingGpuFeed);
        if (movingGpuFeed.empty() || movingGpuFeed.size() < movingVisuals.size())
            return false;

        settleMovingRubber(dynamicState, 3.5f);
        std::vector<TrackRubberVisualCell> settledCells;
        dynamicState.collectPresentationCells(
            { 0.0, 0.0, 0.0 }, 8.0, settledCells, 512);
        float looseBeforeWake = 0.0f;
        for (const auto& cell : settledCells)
            looseBeforeWake += cell.looseRubber;
        if (looseBeforeWake <= 0.002f)
            return false;

        std::vector<TrackRubberVisualCell> marbleGpuFeed;
        dynamicState.collectPresentationCellsUnsorted(
            { 0.0, 0.0, 0.0 }, 8.0, marbleGpuFeed, true);
        if (marbleGpuFeed.empty())
            return false;
        const auto activeGpuCell = std::find_if(
            marbleGpuFeed.begin(), marbleGpuFeed.end(),
            [](const TrackRubberVisualCell& cell) {
                return cell.looseRubber > 0.0012f
                    || cell.persistentPiecePopulation >= 0.45f;
            });
        if (activeGpuCell == marbleGpuFeed.end() || activeGpuCell->updateSerial == 0)
            return false;

        TrackRubberWakeInput wake;
        wake.forward = { 0.0f, 0.0f, 1.0f };
        wake.up = { 0.0f, 1.0f, 0.0f };
        wake.deltaTimeSeconds = 1.0f / 60.0f;
        wake.speedMps = 70.0f;
        wake.vehicleWidthM = 1.80f;
        wake.vehicleLengthM = 4.10f;
        wake.rideHeightM = 0.12f;
        wake.normalLoadN = 14000.0f;
        wake.referenceWeightN = 13000.0f;
        wake.aeroWakeFactor = 1.0f;
        float movedByWake = 0.0f;
        for (int step = 0; step < 90; ++step)
        {
            const auto wakeResult = dynamicState.applyWake(
                { 0.0, 0.0, 0.0 }, wake);
            movedByWake += wakeResult.groundMovedLoose + wakeResult.liftedLoose;
            dynamicState.advance(1.0f / 60.0f, 0.0f);
        }
        if (movedByWake <= 0.0f)
            return false;

        settledCells.clear();
        dynamicState.collectPresentationCells(
            { 0.0, 0.0, 0.0 }, 12.0, settledCells, 1024);
        float staticLooseAfterWake = 0.0f;
        for (const auto& cell : settledCells)
            staticLooseAfterWake += cell.looseRubber;
        const float totalLooseAfterWake = staticLooseAfterWake
            + static_cast<float>(dynamicState.stats().transientLoose);
        // Aggregate quantities are normalized cell reservoirs rather than grams,
        // but migration itself must remain materially conservative to tight
        // numerical tolerance when there is no pickup/rain/generation.
        if (totalLooseAfterWake < looseBeforeWake * 0.96f
            || totalLooseAfterWake > looseBeforeWake * 1.04f)
        {
            return false;
        }
    }

    // Logical piece state is aggregate/cell-based and bounded globally. The
    // default production budget is 500k; use the minimum accepted cap here so
    // the regression can saturate it cheaply.
    {
        heritage::physics::rubber::TrackRubberDescription cappedDescription;
        cappedDescription.maximumPersistentPieceCount = 1024;
        heritage::physics::rubber::TrackRubberState cappedState(cappedDescription);
        TrackRubberContactInput cappedInput = input;
        cappedInput.generationMultiplier = 1000.0f;
        cappedInput.longitudinalSlipSpeedMps = 2.8f;
        cappedInput.lateralSlipSpeedMps = 1.8f;
        cappedInput.slipDissipationWatts = 12000.0f;
        cappedInput.treadWearDepthDeltaM = 2.0e-8f;
        cappedInput.treadWearFraction = 0.65f;
        cappedInput.treadTemperatureC = 108.0f;
        for (int cellIndex = 0; cellIndex < 80; ++cellIndex)
        {
            const DVec3 position{
                static_cast<double>(cellIndex) * 0.55, 0.0, 0.0
            };
            for (int step = 0; step < 35; ++step)
                cappedState.applyContact(position, cappedInput);
        }
        const auto cappedStats = cappedState.stats();
        if (cappedStats.persistentPieces == 0u
            || cappedStats.persistentPieces > 1024u)
        {
            return false;
        }
    }

    heritage::physics::SurfaceWorldDevelopmentControls labControls;
    labControls.tireWearRateMultiplier = 1000.0;
    labControls.rubberGenerationMultiplier = 1000.0;
    labControls.marbleMaturationMultiplier = 1000.0;
    if (!world.setDevelopmentControls(labControls))
        return false;
    labControls.rubberGenerationMultiplier = 1000.1;
    if (world.setDevelopmentControls(labControls))
        return false;

    // Same absolute patch after a floating-origin rebase must resolve the same
    // rubbered racing line rather than creating a second local-coordinate map.
    const DVec3 shiftedOrigin{ 104096.0, 12.0, -54096.0 };
    const Vec3 shiftedLocal{
        local.x - 4096.0f,
        local.y,
        local.z + 4096.0f
    };
    // Traffic/maturation above can legitimately alter the centre cell after
    // dryCenter was captured, so compare the same absolute patch immediately
    // before and after rebasing rather than against stale pre-traffic state.
    const auto immediatelyBeforeRebase = world.sampleTrackRubber(
        local, SurfaceMaterial::Asphalt, 0.0f);
    world.setGlobalOrigin(shiftedOrigin);
    const auto afterRebase = world.sampleTrackRubber(
        shiftedLocal, SurfaceMaterial::Asphalt, 0.0f);
    if (std::abs(
            afterRebase.depositedRubber
                - immediatelyBeforeRebase.depositedRubber) > 1.0e-5f)
    {
        return false;
    }

    const auto looseImmediatelyBeforeRain = world.trackRubber().sample(
        loose->globalPosition, loose->material, 0.0f);
    const float looseBeforeRain = looseImmediatelyBeforeRain.looseRubber;
    const float piecesBeforeRain =
        looseImmediatelyBeforeRain.persistentPiecePopulation;
    const float depositBeforeRain = dryCenter.depositedRubber;
    heritage::physics::SurfaceWorldEnvironment environment = world.environment();
    environment.wetness = 1.0;
    if (!world.setEnvironment(environment))
        return false;
    for (int step = 0; step < 1200; ++step)
        world.advancePresentation(0.1f);

    const auto washedLoose = world.trackRubber().sample(
        loose->globalPosition, loose->material, 1.0f);
    const auto washedCenter = world.sampleTrackRubber(
        shiftedLocal, SurfaceMaterial::Asphalt, 1.0f);
    return washedLoose.looseRubber < looseBeforeRain * 0.45f
        && washedLoose.persistentPiecePopulation < piecesBeforeRain * 0.45f
        && washedCenter.depositedRubber < depositBeforeRain
        && washedCenter.depositedRubber > depositBeforeRain * 0.80f;
}

} // namespace heritage::tests
