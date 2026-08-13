// CLEAN03B wheel-substep phase: 03_RoadEnvelopeAndFootprintSampling
// Run adaptive 2D road enveloping and cache footprint material/wetness/provider blends.
// This file is intentionally included inside VehicleSystem::simulateWheelSubstep().
// It preserves the validated lexical scope and statement order while making phase ownership explicit.

    // TIRE06 adaptive 2D road enveloping. The centre support ray remains the
    // 1 kHz authoritative suspension contact. Additional footprint rays form a
    // low-cost cross on smooth homogeneous road and expand to the requested 2D
    // lattice when a height, material, or wetness discontinuity is detected.
    if (wheel.tireModel.roadEnveloping.enabled && hitGround)
    {
        const auto& envelopeDescription = wheel.tireModel.roadEnveloping;
        wheel.roadEnvelopeQueryAccumulatorSeconds += substepDeltaTime;
        const VehicleScalar queryInterval = wheel.cachedRoadEnvelopeComplex
            ? envelopeDescription.queryIntervalSeconds
            : envelopeDescription.quietQueryIntervalSeconds;
        const bool refreshEnvelope = !wheel.roadEnvelopeInitialized
            || wheel.roadEnvelopeQueryAccumulatorSeconds >= queryInterval;
        if (refreshEnvelope)
        {
            wheel.roadEnvelopeQueryAccumulatorSeconds = 0.0;
            wheel.roadEnvelopeInitialized = true;

            const VehicleScalar contactLength = previousContactPatchLength > 0.01
                ? previousContactPatchLength
                : std::max(
                    static_cast<VehicleScalar>(description.radius) * 0.40,
                    VehicleScalar{0.08});
            const VehicleScalar contactWidth = state.tireContactPatchWidth > 0.03
                ? state.tireContactPatchWidth
                : (description.fitment.tireWidthMm > 30.0f
                    ? static_cast<VehicleScalar>(description.fitment.tireWidthMm)
                        * VehicleScalar{0.00075}
                    : VehicleScalar{0.15});

            heritage::math::Vec3 sampleForward = state.worldWheelForward;
            sampleForward = subtract(
                sampleForward,
                scale(suspensionDirection,
                    dot(sampleForward, suspensionDirection)));
            sampleForward = normalized(sampleForward,
                rotateVector(chassisRotation, { 0.0f, 0.0f, 1.0f }));
            heritage::math::Vec3 sampleRight = state.worldWheelRight;
            sampleRight = subtract(
                sampleRight,
                scale(suspensionDirection,
                    dot(sampleRight, suspensionDirection)));
            sampleRight = normalized(sampleRight,
                normalized(cross(suspensionDirection, sampleForward),
                    { 1.0f, 0.0f, 0.0f }));

            const heritage::math::Vec3 centerNormal = normalized(
                hit.normal, { 0.0f, 1.0f, 0.0f });
            const VehicleScalar normalAlongForward = dot(
                centerNormal, sampleForward);
            const VehicleScalar normalAlongRight = dot(
                centerNormal, sampleRight);
            const VehicleScalar normalAlongSupport = dot(
                centerNormal, suspensionDirection);
            const VehicleScalar centerHubLength =
                hit.distance - description.radius;

            struct QueriedFootprintSample
            {
                tires::TireRoadEnvelopeSample envelope;
                SurfaceProfile surface{};
                SurfaceProfile wetBaseSurface{};
                SurfaceProfile providerBaseSurface{};
                heritage::physics::SurfaceMaterial material =
                    heritage::physics::SurfaceMaterial::Default;
                heritage::physics::SurfaceDeformableProperties
                    deformableProperties{};
                VehicleScalar wetness = 0.0;
                VehicleScalar surfaceTemperatureC = 20.0;
            };
            std::vector<QueriedFootprintSample> queried;

            const auto alreadyQueried = [&](VehicleScalar x, VehicleScalar y) {
                return std::any_of(queried.begin(), queried.end(),
                    [&](const QueriedFootprintSample& sample) {
                        return std::abs(sample.envelope.longitudinalOffsetM - x)
                                <= VehicleScalar{1.0e-6}
                            && std::abs(sample.envelope.lateralOffsetM - y)
                                <= VehicleScalar{1.0e-6};
                    });
            };

            const auto queryOffset = [&](const tires::TireRoadEnvelopeOffset& offset) {
                if (alreadyQueried(offset.longitudinalOffsetM, offset.lateralOffsetM))
                    return;

                QueriedFootprintSample result;
                result.envelope.longitudinalOffsetM = offset.longitudinalOffsetM;
                result.envelope.lateralOffsetM = offset.lateralOffsetM;

                const bool isCenter = std::abs(offset.longitudinalOffsetM)
                        <= VehicleScalar{1.0e-6}
                    && std::abs(offset.lateralOffsetM) <= VehicleScalar{1.0e-6};
                if (isCenter)
                {
                    result.envelope.valid = true;
                    result.envelope.roadHeightRelativeToCenterM = 0.0;
                    result.material = hit.surfaceMaterial;
                    result.deformableProperties = hit.surfaceProperties.deformable;
                    result.wetness = hitSurfaceConditions.wetness;
                    result.surfaceTemperatureC = hitSurfaceConditions.surfaceTemperatureC;
                    result.surface = surfaceProfile(
                        hit.surfaceMaterial, static_cast<float>(result.wetness), vehicle.surface);
                    result.wetBaseSurface = hardWetSurfaceMaterial(hit.surfaceMaterial)
                        ? surfaceProfile(hit.surfaceMaterial, 0.0f, vehicle.surface)
                        : result.surface;
                    result.providerBaseSurface = providerBaseSurfaceProfile(
                        hit.surfaceMaterial, static_cast<float>(result.wetness), vehicle.surface,
                        wheel.tireModel.wetSurface.enabled,
                        wheel.tireModel.winterSurface.enabled,
                        wheel.tireModel.shallowGranularSurface.enabled,
                        wheel.tireModel.deformableTerrainSurface.enabled);
                    queried.push_back(result);
                    return;
                }

                const heritage::math::Vec3 sampleOrigin = add(
                    add(wheelRayOriginWorld,
                        scale(sampleForward, offset.longitudinalOffsetM)),
                    scale(sampleRight, offset.lateralOffsetM));
                heritage::physics::RaycastHit sampleHit;
                const VehicleScalar extraReach = std::max(
                    envelopeDescription.maximumRoadStepM,
                    VehicleScalar{0.02});
                const bool sampleSupported = collisions.raycast(
                    sampleOrigin,
                    suspensionDirection,
                    static_cast<float>(rayDistance + extraReach),
                    filter,
                    bodies,
                    sampleHit);
                result.envelope.valid = sampleSupported;
                if (sampleSupported)
                {
                    const VehicleScalar sampleHubLength =
                        sampleHit.distance - description.radius;
                    const VehicleScalar measuredRoadHeight =
                        centerHubLength - sampleHubLength;
                    const VehicleScalar expectedPlaneHeight =
                        tires::roadEnvelopeLocalPlaneHeightM(
                            offset.longitudinalOffsetM,
                            offset.lateralOffsetM,
                            normalAlongForward,
                            normalAlongRight,
                            normalAlongSupport);
                    result.envelope.roadHeightRelativeToCenterM =
                        measuredRoadHeight - expectedPlaneHeight;
                    result.material = sampleHit.surfaceMaterial;
                    result.deformableProperties = sampleHit.surfaceProperties.deformable;
                    const auto sampleConditions = surfaces.localConditions(
                        sampleHit.point,
                        sampleHit.surfaceMaterial,
                        sampleHit.surfaceWetness,
                        sampleHit.surfaceProperties);
                    result.wetness = sampleConditions.wetness;
                    result.surfaceTemperatureC = sampleConditions.surfaceTemperatureC;
                    result.surface = surfaceProfile(
                        sampleHit.surfaceMaterial,
                        static_cast<float>(result.wetness),
                        vehicle.surface);
                    result.wetBaseSurface = hardWetSurfaceMaterial(sampleHit.surfaceMaterial)
                        ? surfaceProfile(sampleHit.surfaceMaterial, 0.0f, vehicle.surface)
                        : result.surface;
                    result.providerBaseSurface = providerBaseSurfaceProfile(
                        sampleHit.surfaceMaterial, static_cast<float>(result.wetness), vehicle.surface,
                        wheel.tireModel.wetSurface.enabled,
                        wheel.tireModel.winterSurface.enabled,
                        wheel.tireModel.shallowGranularSurface.enabled,
                        wheel.tireModel.deformableTerrainSurface.enabled);
                }
                queried.push_back(result);
            };

            const bool startRefined = !envelopeDescription.adaptive2D
                || wheel.cachedRoadEnvelopeComplex;
            const auto initialPattern = tires::buildTireRoadEnvelopeSamplePattern(
                envelopeDescription, contactLength, contactWidth, startRefined);
            for (const auto& offset : initialPattern)
                queryOffset(offset);

            const auto envelopeSamples = [&]() {
                std::vector<tires::TireRoadEnvelopeSample> result;
                result.reserve(queried.size());
                for (const auto& sample : queried)
                    result.push_back(sample.envelope);
                return result;
            };

            auto samplesForComplexity = envelopeSamples();
            bool surfaceComplex = false;
            heritage::physics::SurfaceMaterial firstMaterial = hit.surfaceMaterial;
            VehicleScalar firstWetness = hitSurfaceConditions.wetness;
            heritage::physics::SurfaceDeformableProperties firstDeformableProperties{};
            bool haveFirst = false;
            const auto deformablePropertiesDiffer = [](
                const heritage::physics::SurfaceDeformableProperties& a,
                const heritage::physics::SurfaceDeformableProperties& b) {
                if (a.enabled != b.enabled || a.authored != b.authored)
                    return true;
                if (!a.enabled)
                    return false;
                const auto differs = [](double lhs, double rhs) {
                    const double scale = std::max({ 1.0, std::abs(lhs), std::abs(rhs) });
                    return std::abs(lhs - rhs) > scale * 1.0e-6;
                };
                return differs(a.densityKgM3, b.densityKgM3)
                    || differs(a.initialLooseDepthM, b.initialLooseDepthM)
                    || differs(a.initialMoisture, b.initialMoisture)
                    || differs(a.bekkerKc, b.bekkerKc)
                    || differs(a.bekkerKphi, b.bekkerKphi)
                    || differs(a.sinkageExponent, b.sinkageExponent)
                    || differs(a.cohesionPa, b.cohesionPa)
                    || differs(a.frictionAngleDegrees, b.frictionAngleDegrees)
                    || differs(a.shearDeformationModulusM, b.shearDeformationModulusM)
                    || differs(a.compactionStiffnessGain, b.compactionStiffnessGain)
                    || differs(a.compactionShearGain, b.compactionShearGain)
                    || differs(a.plasticRutFraction, b.plasticRutFraction)
                    || differs(a.compactionRateHz, b.compactionRateHz)
                    || differs(a.looseDepthLossPerCompactionM, b.looseDepthLossPerCompactionM)
                    || differs(a.mfBaseFrictionScale, b.mfBaseFrictionScale)
                    || differs(a.baseStiffnessScale, b.baseStiffnessScale)
                    || differs(a.rollingResistanceScale, b.rollingResistanceScale)
                    || differs(a.relaxationScale, b.relaxationScale);
            };
            for (const auto& sample : queried)
            {
                if (!sample.envelope.valid)
                    continue;
                if (!haveFirst)
                {
                    firstMaterial = sample.material;
                    firstWetness = sample.wetness;
                    firstDeformableProperties = sample.deformableProperties;
                    haveFirst = true;
                }
                else if (sample.material != firstMaterial
                    || std::abs(sample.wetness - firstWetness)
                        >= envelopeDescription.refinementWetnessThreshold
                    || deformablePropertiesDiffer(
                        sample.deformableProperties, firstDeformableProperties))
                {
                    surfaceComplex = true;
                    break;
                }
            }
            bool heightComplex = tires::tireRoadEnvelopeNeedsHeightRefinement(
                envelopeDescription, samplesForComplexity);
            bool supportComplex = tires::tireRoadEnvelopeHasPartialSupport(
                samplesForComplexity);
            bool refined = startRefined;
            if (envelopeDescription.adaptive2D
                && !startRefined
                && (heightComplex || supportComplex || surfaceComplex))
            {
                const auto refinedPattern = tires::buildTireRoadEnvelopeSamplePattern(
                    envelopeDescription, contactLength, contactWidth, true);
                for (const auto& offset : refinedPattern)
                    queryOffset(offset);
                refined = true;
                samplesForComplexity = envelopeSamples();
                heightComplex = tires::tireRoadEnvelopeNeedsHeightRefinement(
                    envelopeDescription, samplesForComplexity);
                supportComplex = tires::tireRoadEnvelopeHasPartialSupport(
                    samplesForComplexity);
            }

            const tires::TireRoadEnvelopeOutput envelope =
                tires::evaluateTireRoadEnvelope(
                    envelopeDescription, contactLength, samplesForComplexity);
            wheel.cachedFootprintRefined = refined;

            wheel.cachedRoadEnvelopeComplex = heightComplex || supportComplex || surfaceComplex;
            if (envelope.valid)
            {
                wheel.cachedRoadEnvelopeOffsetM = envelope.effectiveRoadHeightM;
                wheel.cachedRoadEnvelopeSlopeRadians =
                    envelope.effectiveRoadSlopeRadians;
                wheel.cachedRoadEnvelopeCrossSlopeRadians =
                    envelope.effectiveCrossSlopeRadians;
                wheel.cachedRoadEnvelopeRoughnessRangeM =
                    envelope.roughnessHeightRangeM;
                wheel.cachedRoadEnvelopeSupportedFraction =
                    envelope.supportedFraction;
                wheel.cachedRoadEnvelopeValidSamples = envelope.validSampleCount;
                wheel.cachedRoadEnvelopeTotalSamples = envelope.totalSampleCount;
            }
            else
            {
                wheel.cachedRoadEnvelopeOffsetM = 0.0;
                wheel.cachedRoadEnvelopeSlopeRadians = 0.0;
                wheel.cachedRoadEnvelopeCrossSlopeRadians = 0.0;
                wheel.cachedRoadEnvelopeRoughnessRangeM = 0.0;
                wheel.cachedRoadEnvelopeSupportedFraction = 0.0;
                wheel.cachedRoadEnvelopeValidSamples = 0;
                wheel.cachedRoadEnvelopeTotalSamples = queried.size();
            }

            VehicleScalar frictionSum = 0.0;
            VehicleScalar stiffnessSum = 0.0;
            VehicleScalar rollingSum = 0.0;
            VehicleScalar relaxationSum = 0.0;
            VehicleScalar wetBaseFrictionSum = 0.0;
            VehicleScalar wetBaseStiffnessSum = 0.0;
            VehicleScalar wetBaseRollingSum = 0.0;
            VehicleScalar wetBaseRelaxationSum = 0.0;
            VehicleScalar providerBaseFrictionSum = 0.0;
            VehicleScalar providerBaseStiffnessSum = 0.0;
            VehicleScalar providerBaseRollingSum = 0.0;
            VehicleScalar providerBaseRelaxationSum = 0.0;
            VehicleScalar frictionMinimum = std::numeric_limits<VehicleScalar>::infinity();
            VehicleScalar frictionMaximum = -std::numeric_limits<VehicleScalar>::infinity();
            VehicleScalar grassCount = 0.0;
            VehicleScalar dirtCount = 0.0;
            VehicleScalar gravelCount = 0.0;
            VehicleScalar snowCount = 0.0;
            VehicleScalar iceCount = 0.0;
            VehicleScalar mudCount = 0.0;
            VehicleScalar sandCount = 0.0;
            VehicleScalar softSoilCount = 0.0;
            VehicleScalar deepSnowCount = 0.0;
            VehicleScalar cleanHardCount = 0.0;
            VehicleScalar wetnessSum = 0.0;
            VehicleScalar surfaceTemperatureSum = 0.0;
            std::vector<heritage::physics::SurfaceDeformableProperties>
                deformablePropertySamples;
            std::vector<double> deformablePropertyWeights;
            std::size_t surfaceCount = 0;
            for (const auto& sample : queried)
            {
                if (!sample.envelope.valid)
                    continue;
                frictionSum += sample.surface.frictionMultiplier;
                stiffnessSum += sample.surface.stiffnessMultiplier;
                rollingSum += sample.surface.rollingResistanceMultiplier;
                relaxationSum += sample.surface.relaxationMultiplier;
                wetBaseFrictionSum += sample.wetBaseSurface.frictionMultiplier;
                wetBaseStiffnessSum += sample.wetBaseSurface.stiffnessMultiplier;
                wetBaseRollingSum += sample.wetBaseSurface.rollingResistanceMultiplier;
                wetBaseRelaxationSum += sample.wetBaseSurface.relaxationMultiplier;
                providerBaseFrictionSum += sample.providerBaseSurface.frictionMultiplier;
                providerBaseStiffnessSum += sample.providerBaseSurface.stiffnessMultiplier;
                providerBaseRollingSum += sample.providerBaseSurface.rollingResistanceMultiplier;
                providerBaseRelaxationSum += sample.providerBaseSurface.relaxationMultiplier;
                frictionMinimum = std::min(
                    frictionMinimum,
                    static_cast<VehicleScalar>(sample.surface.frictionMultiplier));
                frictionMaximum = std::max(
                    frictionMaximum,
                    static_cast<VehicleScalar>(sample.surface.frictionMultiplier));
                wetnessSum += std::clamp(sample.wetness, VehicleScalar{0.0}, VehicleScalar{1.0});
                surfaceTemperatureSum += sample.surfaceTemperatureC;
                if (sample.deformableProperties.enabled)
                {
                    deformablePropertySamples.push_back(sample.deformableProperties);
                    deformablePropertyWeights.push_back(1.0);
                }
                switch (sample.material)
                {
                case heritage::physics::SurfaceMaterial::Grass:
                    grassCount += 1.0;
                    break;
                case heritage::physics::SurfaceMaterial::Dirt:
                    dirtCount += 1.0;
                    break;
                case heritage::physics::SurfaceMaterial::Gravel:
                    gravelCount += 1.0;
                    break;
                case heritage::physics::SurfaceMaterial::Asphalt:
                case heritage::physics::SurfaceMaterial::Kerb:
                case heritage::physics::SurfaceMaterial::PaintedLine:
                case heritage::physics::SurfaceMaterial::Default:
                    cleanHardCount += 1.0;
                    break;
                case heritage::physics::SurfaceMaterial::Snow:
                    snowCount += 1.0;
                    break;
                case heritage::physics::SurfaceMaterial::Ice:
                    iceCount += 1.0;
                    break;
                case heritage::physics::SurfaceMaterial::Mud:
                    mudCount += 1.0;
                    break;
                case heritage::physics::SurfaceMaterial::Sand:
                    sandCount += 1.0;
                    break;
                case heritage::physics::SurfaceMaterial::SoftSoil:
                    softSoilCount += 1.0;
                    break;
                case heritage::physics::SurfaceMaterial::DeepSnow:
                    deepSnowCount += 1.0;
                    break;
                }
                ++surfaceCount;
            }
            if (surfaceCount > 0)
            {
                const VehicleScalar inverseCount = VehicleScalar{1.0}
                    / static_cast<VehicleScalar>(surfaceCount);
                wheel.cachedFootprintSurfaceValid = true;
                wheel.cachedFootprintFrictionMultiplier = frictionSum * inverseCount;
                wheel.cachedFootprintStiffnessMultiplier = stiffnessSum * inverseCount;
                wheel.cachedFootprintRollingResistanceMultiplier =
                    rollingSum * inverseCount;
                wheel.cachedFootprintRelaxationMultiplier =
                    relaxationSum * inverseCount;
                wheel.cachedFootprintWetBaseFrictionMultiplier =
                    wetBaseFrictionSum * inverseCount;
                wheel.cachedFootprintWetBaseStiffnessMultiplier =
                    wetBaseStiffnessSum * inverseCount;
                wheel.cachedFootprintWetBaseRollingResistanceMultiplier =
                    wetBaseRollingSum * inverseCount;
                wheel.cachedFootprintWetBaseRelaxationMultiplier =
                    wetBaseRelaxationSum * inverseCount;
                wheel.cachedFootprintProviderBaseFrictionMultiplier =
                    providerBaseFrictionSum * inverseCount;
                wheel.cachedFootprintProviderBaseStiffnessMultiplier =
                    providerBaseStiffnessSum * inverseCount;
                wheel.cachedFootprintProviderBaseRollingResistanceMultiplier =
                    providerBaseRollingSum * inverseCount;
                wheel.cachedFootprintProviderBaseRelaxationMultiplier =
                    providerBaseRelaxationSum * inverseCount;
                wheel.cachedFootprintFrictionSpread = std::max(
                    frictionMaximum - frictionMinimum, VehicleScalar{0.0});
                wheel.cachedFootprintMaterialBlendValid = true;
                wheel.cachedFootprintGrassFraction = grassCount * inverseCount;
                wheel.cachedFootprintDirtFraction = dirtCount * inverseCount;
                wheel.cachedFootprintGravelFraction = gravelCount * inverseCount;
                wheel.cachedFootprintSnowFraction =
                    (snowCount + deepSnowCount) * inverseCount;
                wheel.cachedFootprintIceFraction = iceCount * inverseCount;
                wheel.cachedFootprintMudFraction = mudCount * inverseCount;
                wheel.cachedFootprintSandFraction = sandCount * inverseCount;
                wheel.cachedFootprintSoftSoilFraction = softSoilCount * inverseCount;
                wheel.cachedFootprintDeepSnowFraction = deepSnowCount * inverseCount;
                wheel.cachedFootprintCleanHardFraction = cleanHardCount * inverseCount;
                wheel.cachedFootprintAverageWetness = wetnessSum * inverseCount;
                wheel.cachedFootprintAverageSurfaceTemperatureC =
                    surfaceTemperatureSum * inverseCount;
                if (!deformablePropertySamples.empty())
                {
                    wheel.cachedFootprintDeformableProperties =
                        heritage::physics::blendSurfaceDeformableProperties(
                            deformablePropertySamples.data(),
                            deformablePropertyWeights.data(),
                            deformablePropertySamples.size());
                    wheel.cachedFootprintDeformablePropertiesValid =
                        wheel.cachedFootprintDeformableProperties.enabled;
                }
                else
                {
                    wheel.cachedFootprintDeformableProperties = {};
                    wheel.cachedFootprintDeformablePropertiesValid = false;
                }
            }
            else
            {
                wheel.cachedFootprintSurfaceValid = false;
                wheel.cachedFootprintFrictionMultiplier = 1.0;
                wheel.cachedFootprintStiffnessMultiplier = 1.0;
                wheel.cachedFootprintRollingResistanceMultiplier = 1.0;
                wheel.cachedFootprintRelaxationMultiplier = 1.0;
                wheel.cachedFootprintWetBaseFrictionMultiplier = 1.0;
                wheel.cachedFootprintWetBaseStiffnessMultiplier = 1.0;
                wheel.cachedFootprintWetBaseRollingResistanceMultiplier = 1.0;
                wheel.cachedFootprintWetBaseRelaxationMultiplier = 1.0;
                wheel.cachedFootprintProviderBaseFrictionMultiplier = 1.0;
                wheel.cachedFootprintProviderBaseStiffnessMultiplier = 1.0;
                wheel.cachedFootprintProviderBaseRollingResistanceMultiplier = 1.0;
                wheel.cachedFootprintProviderBaseRelaxationMultiplier = 1.0;
                wheel.cachedFootprintFrictionSpread = 0.0;
                wheel.cachedFootprintMaterialBlendValid = false;
                wheel.cachedFootprintGrassFraction = 0.0;
                wheel.cachedFootprintDirtFraction = 0.0;
                wheel.cachedFootprintGravelFraction = 0.0;
                wheel.cachedFootprintSnowFraction = 0.0;
                wheel.cachedFootprintIceFraction = 0.0;
                wheel.cachedFootprintMudFraction = 0.0;
                wheel.cachedFootprintSandFraction = 0.0;
                wheel.cachedFootprintSoftSoilFraction = 0.0;
                wheel.cachedFootprintDeepSnowFraction = 0.0;
                wheel.cachedFootprintCleanHardFraction = 0.0;
                wheel.cachedFootprintAverageWetness = 0.0;
                wheel.cachedFootprintAverageSurfaceTemperatureC = 20.0;
                wheel.cachedFootprintDeformableProperties = {};
                wheel.cachedFootprintDeformablePropertiesValid = false;
            }
        }
    }
    else
    {
        wheel.roadEnvelopeQueryAccumulatorSeconds = 0.0;
        wheel.roadEnvelopeInitialized = false;
        wheel.cachedRoadEnvelopeOffsetM = 0.0;
        wheel.cachedRoadEnvelopeSlopeRadians = 0.0;
        wheel.cachedRoadEnvelopeCrossSlopeRadians = 0.0;
        wheel.cachedRoadEnvelopeRoughnessRangeM = 0.0;
        wheel.cachedRoadEnvelopeSupportedFraction = 0.0;
        wheel.cachedRoadEnvelopeValidSamples = 0;
        wheel.cachedRoadEnvelopeTotalSamples = 0;
        wheel.cachedRoadEnvelopeComplex = false;
        wheel.cachedFootprintRefined = false;
        wheel.cachedFootprintSurfaceValid = false;
        wheel.cachedFootprintFrictionMultiplier = 1.0;
        wheel.cachedFootprintStiffnessMultiplier = 1.0;
        wheel.cachedFootprintRollingResistanceMultiplier = 1.0;
        wheel.cachedFootprintRelaxationMultiplier = 1.0;
        wheel.cachedFootprintWetBaseFrictionMultiplier = 1.0;
        wheel.cachedFootprintWetBaseStiffnessMultiplier = 1.0;
        wheel.cachedFootprintWetBaseRollingResistanceMultiplier = 1.0;
        wheel.cachedFootprintWetBaseRelaxationMultiplier = 1.0;
        wheel.cachedFootprintProviderBaseFrictionMultiplier = 1.0;
        wheel.cachedFootprintProviderBaseStiffnessMultiplier = 1.0;
        wheel.cachedFootprintProviderBaseRollingResistanceMultiplier = 1.0;
        wheel.cachedFootprintProviderBaseRelaxationMultiplier = 1.0;
        wheel.cachedFootprintFrictionSpread = 0.0;
        wheel.cachedFootprintMaterialBlendValid = false;
        wheel.cachedFootprintGrassFraction = 0.0;
        wheel.cachedFootprintDirtFraction = 0.0;
        wheel.cachedFootprintGravelFraction = 0.0;
        wheel.cachedFootprintSnowFraction = 0.0;
        wheel.cachedFootprintIceFraction = 0.0;
        wheel.cachedFootprintMudFraction = 0.0;
        wheel.cachedFootprintSandFraction = 0.0;
        wheel.cachedFootprintSoftSoilFraction = 0.0;
        wheel.cachedFootprintDeepSnowFraction = 0.0;
        wheel.cachedFootprintCleanHardFraction = 0.0;
        wheel.cachedFootprintAverageWetness = 0.0;
        wheel.cachedFootprintAverageSurfaceTemperatureC = 20.0;
        wheel.cachedFootprintDeformableProperties = {};
        wheel.cachedFootprintDeformablePropertiesValid = false;
    }
