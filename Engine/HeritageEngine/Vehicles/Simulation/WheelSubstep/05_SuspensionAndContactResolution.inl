// CLEAN03B wheel-substep phase: 05_SuspensionAndContactResolution
// Solve massless or unsprung-mass suspension/contact support, apply normal/link forces, and finalize grounded state.
// This file is intentionally included inside VehicleSystem::simulateWheelSubstep().
// It preserves the validated lexical scope and statement order while making phase ownership explicit.

    VehicleScalar suspensionForce = 0.0;
    if (description.effectiveUnsprungMass <= 0.0f && !kartRigidSupport)
    {
        if (!hitGround)
        {
            state.suspensionLength = maximumLength;
            state.compression = description.restLength - maximumLength;
            state.compressionVelocity = (
                wheel.previousSuspensionLength - maximumLength)
                / substepDeltaTime;
            state.worldCenter = add(
                wheelRayOriginWorld,
                scale(suspensionDirection, maximumLength));
            state.contactPoint = add(
                state.worldCenter,
                scale(suspensionDirection, description.radius));
            wheel.previousSuspensionLength = maximumLength;
            updateUprightGeometry();
            advanceFreeWheelRotation();
            finalizeContactState();
            advanceAirborneThermal();
            return;
        }

        VehicleScalar suspensionLength = hit.distance - physicalSupportRadiusM
            - structuralRoadOffset + surfaceSupportSinkageM;
        if (suspensionLength > maximumLength)
        {
            state.contactStatus =
                WheelContactStatus::BeyondSuspensionReach;
            state.suspensionLength = maximumLength;
            state.compression = description.restLength - maximumLength;
            state.worldCenter = add(
                wheelRayOriginWorld,
                scale(suspensionDirection, maximumLength));
            state.contactPoint = add(
                hit.point, scale(suspensionDirection, surfaceSupportSinkageM));
            wheel.previousSuspensionLength = maximumLength;
            updateUprightGeometry();
            finalizeContactState();
            advanceAirborneThermal();
            return;
        }

        if (suspensionLength < minimumLength)
        {
            state.suspensionBottomed = true;
            state.bottomOutPenetration =
                minimumLength - suspensionLength;
            state.contactStatus =
                WheelContactStatus::SuspensionBottomed;
        }
        else
        {
            state.contactStatus = WheelContactStatus::Supported;
        }
        suspensionLength = std::clamp(
            suspensionLength,
            minimumLength,
            maximumLength);
        state.grounded = true;

        state.suspensionLength = suspensionLength;
        state.compression = description.restLength - suspensionLength;
        wheel.previousSuspensionLength = suspensionLength;
        state.worldCenter = add(
            wheelRayOriginWorld,
            scale(suspensionDirection, suspensionLength));
        state.contactPoint = add(
            hit.point, scale(suspensionDirection, surfaceSupportSinkageM));
        state.contactNormal = normalized(
            hit.normal,
            { 0.0f, 1.0f, 0.0f });
        state.contactCollider = hit.collider;
        state.surfaceMaterial = hit.surfaceMaterial;
        state.surfaceWetness = hitSurfaceConditions.wetness;
        state.surfaceTemperatureC = hitSurfaceConditions.surfaceTemperatureC;

        // The vehicle loop may run at 1000 Hz inside a 120 Hz rigid-body
        // world step. Attachment velocity remains authoritative across
        // those substeps for the legacy massless contact.
        bodies.linearVelocity(
            vehicle.description.chassisBody,
            chassisLinearVelocity);
        bodies.angularVelocityDegrees(
            vehicle.description.chassisBody,
            chassisAngularVelocityDegrees);
        const heritage::math::Vec3 mountVelocity = pointVelocityFromOffset(
            chassisLinearVelocity,
            chassisAngularVelocityDegrees,
            mountOffsetFromCenterOfMass);
        heritage::math::Vec3 supportVelocity{};
        if (hit.body != heritage::physics::InvalidBody)
        {
            heritage::physics::RigidBodyPose supportPose;
            heritage::math::Vec3 supportLinearVelocity{};
            heritage::math::Vec3 supportAngularVelocityDegrees{};
            if (bodies.pose(hit.body, supportPose)
                && bodies.linearVelocity(
                    hit.body,
                    supportLinearVelocity)
                && bodies.angularVelocityDegrees(
                    hit.body,
                    supportAngularVelocityDegrees))
            {
                heritage::math::Vec3 supportCenterOfMassWorld =
                    supportPose.position;
                bodies.centerOfMassWorld(
                    hit.body,
                    supportCenterOfMassWorld);
                supportVelocity = pointVelocity(
                    supportLinearVelocity,
                    supportAngularVelocityDegrees,
                    supportCenterOfMassWorld,
                    hit.point);
            }
        }
        state.compressionVelocity = dot(
            subtract(mountVelocity, supportVelocity),
            suspensionDirection);
        updateUprightGeometry();

        const SuspensionModelOutput suspensionOutput =
            evaluateSuspensionModel(
                currentSuspensionModelDescription(),
                { state.compression,
                  state.compressionVelocity,
                  currentGeometryOutput.springTwistRadians,
                  currentGeometryOutput.springAngularMotionRatioRadPerM,
                  currentGeometryOutput.referenceSpringAngularMotionRatioRadPerM,
                  currentGeometryOutput.springCompressionM,
                  currentGeometryOutput.springMotionRatio,
                  currentGeometryOutput.damperMotionRatio,
                  state.leafAxleWrapAngleRadians,
                  state.leafAxleWrapRateRadiansPerSecond,
                  previousLongitudinalTireForce,
                  previousEffectiveRollingRadius,
                  currentGeometryOutput.motorcycleChainDistanceMotionRatio,
                  currentGeometryOutput.twistBeamTwistRadians,
                  currentGeometryOutput.twistBeamTwistRateRadiansPerSecond,
                  currentGeometryOutput.twistBeamAngularMotionRatioRadPerM });
        suspensionForce = std::clamp(
            suspensionOutput.normalForceN + antiRollBarForceN,
            0.0,
            static_cast<VehicleScalar>(description.maximumSuspensionForce));
        state.suspensionSpringForce = suspensionOutput.springForceN;
        state.suspensionDampingForce = suspensionOutput.dampingForceN;
        state.suspensionBumpStopForce = suspensionOutput.bumpStopForceN;
        state.suspensionDroopStopForce = suspensionOutput.droopStopForceN;
        state.suspensionUnclampedForce = suspensionOutput.unclampedForceN;
        state.damperDissipationWatts =
            suspensionOutput.damperDissipationW;
        state.leafInterleafFrictionForceN = suspensionOutput.leafInterleafForceN;
        state.leafInterleafDissipationWatts = suspensionOutput.leafInterleafDissipationW;
        state.leafAxleWrapJackingForceN = suspensionOutput.leafAxleWrapJackingForceN;
        state.motorcycleChainJackingForceN = suspensionOutput.motorcycleChainJackingForceN;
        state.twistBeamCouplingForceN = suspensionOutput.twistBeamCouplingForceN;
        state.twistBeamDissipationWatts = suspensionOutput.twistBeamDissipationW;
        state.normalForce = suspensionForce;
        if (suspensionForce > 0.0f)
        {
            const heritage::math::Vec3 suspensionImpulse = scale(
                suspensionDirection,
                -suspensionForce * substepDeltaTime);
            bodies.applyImpulseAtPoint(
                vehicle.description.chassisBody,
                suspensionImpulse,
                mountWorld);
        }
    }
    else
    {
        bodies.linearVelocity(
            vehicle.description.chassisBody,
            chassisLinearVelocity);
        bodies.angularVelocityDegrees(
            vehicle.description.chassisBody,
            chassisAngularVelocityDegrees);
        const heritage::math::Vec3 mountVelocity = pointVelocityFromOffset(
            chassisLinearVelocity,
            chassisAngularVelocityDegrees,
            mountOffsetFromCenterOfMass);
        heritage::math::Vec3 supportVelocity{};
        if (hitGround
            && hit.body != heritage::physics::InvalidBody)
        {
            heritage::physics::RigidBodyPose supportPose;
            heritage::math::Vec3 supportLinearVelocity{};
            heritage::math::Vec3 supportAngularVelocityDegrees{};
            if (bodies.pose(hit.body, supportPose)
                && bodies.linearVelocity(
                    hit.body,
                    supportLinearVelocity)
                && bodies.angularVelocityDegrees(
                    hit.body,
                    supportAngularVelocityDegrees))
            {
                heritage::math::Vec3 supportCenterOfMassWorld =
                    supportPose.position;
                bodies.centerOfMassWorld(
                    hit.body,
                    supportCenterOfMassWorld);
                supportVelocity = pointVelocity(
                    supportLinearVelocity,
                    supportAngularVelocityDegrees,
                    supportCenterOfMassWorld,
                    hit.point);
            }
        }
        const VehicleScalar roadHubLength = hitGround
            ? hit.distance - physicalSupportRadiusM - structuralRoadOffset
                + surfaceSupportSinkageM
            : maximumLength;
        if (hitGround && roadHubLength < minimumLength)
        {
            state.suspensionBottomed = true;
            state.bottomOutPenetration =
                minimumLength - roadHubLength;
        }
        const VehicleScalar geometricCompressionVelocity = hitGround
            ? dot(
                subtract(mountVelocity, supportVelocity),
                suspensionDirection)
            : 0.0f;
        if (!wheel.unsprungMass.initialized)
        {
            wheel.unsprungMass.initialized = true;
            wheel.unsprungMass.suspensionLengthM = std::clamp(
                hitGround
                    ? roadHubLength
                    : static_cast<VehicleScalar>(description.restLength),
                minimumLength,
                maximumLength);
            wheel.unsprungMass.suspensionLengthVelocityMps =
                hitGround ? -geometricCompressionVelocity : 0.0f;
        }

        state.compression = description.restLength
            - wheel.unsprungMass.suspensionLengthM;
        state.compressionVelocity =
            -wheel.unsprungMass.suspensionLengthVelocityMps;
        updateUprightGeometry();
        const SuspensionModelOutput suspensionOutput =
            evaluateSuspensionModel(
                currentSuspensionModelDescription(),
                { state.compression,
                  state.compressionVelocity,
                  currentGeometryOutput.springTwistRadians,
                  currentGeometryOutput.springAngularMotionRatioRadPerM,
                  currentGeometryOutput.referenceSpringAngularMotionRatioRadPerM,
                  currentGeometryOutput.springCompressionM,
                  currentGeometryOutput.springMotionRatio,
                  currentGeometryOutput.damperMotionRatio,
                  state.leafAxleWrapAngleRadians,
                  state.leafAxleWrapRateRadiansPerSecond,
                  previousLongitudinalTireForce,
                  previousEffectiveRollingRadius,
                  currentGeometryOutput.motorcycleChainDistanceMotionRatio,
                  currentGeometryOutput.twistBeamTwistRadians,
                  currentGeometryOutput.twistBeamTwistRateRadiansPerSecond,
                  currentGeometryOutput.twistBeamAngularMotionRatioRadPerM });
        const VehicleScalar suspensionLinkForce = std::clamp(
            suspensionOutput.unclampedForceN + antiRollBarForceN,
            -static_cast<VehicleScalar>(description.maximumSuspensionForce),
            static_cast<VehicleScalar>(description.maximumSuspensionForce));

        UnsprungMassDescription unsprungDescription;
        // SUSP11 fixed kart hubs still use the tire radial model even when no
        // explicit unsprung mass is authored. A tiny numerical mass is enough
        // because minimumLength == maximumLength; the state cannot acquire a
        // hidden suspension DOF and tire deflection remains the force authority.
        unsprungDescription.effectiveMassKg = kartRigidSupport
            ? std::max(static_cast<VehicleScalar>(description.effectiveUnsprungMass), VehicleScalar{1.0})
            : static_cast<VehicleScalar>(description.effectiveUnsprungMass);
        // TIRE17C1: pressure must affect the pneumatic support rather than
        // being only a force-model/contact-area input. Keep the authored
        // vertical stiffness authoritative at nominal pressure, then apply a
        // deliberately conservative pneumatic contribution around it. The
        // 40% structural floor is a low-confidence Heritage development seed
        // until a tire supplies measured pressure-vs-stiffness data.
        const VehicleScalar authoredTireStiffness =
            wheel.tireModel.contactGeometry.verticalStiffnessNPerM >= VehicleScalar{1000.0}
                ? wheel.tireModel.contactGeometry.verticalStiffnessNPerM
                : static_cast<VehicleScalar>(description.tireRadialStiffness);
        const VehicleScalar nominalPressurePa = std::max(
            wheel.tireModel.referenceInflationPressurePa,
            VehicleScalar{50000.0});
        const VehicleScalar currentPressurePa = thermalBefore.valid
            ? thermalBefore.inflationPressurePa
            : wheel.tireModel.inflationPressurePa;
        const VehicleScalar pressureRatio = std::clamp(
            currentPressurePa / std::max(nominalPressurePa, VehicleScalar{50000.0}),
            VehicleScalar{0.0}, VehicleScalar{5.0});
        const VehicleScalar pressureStiffnessScale = std::clamp(
            VehicleScalar{0.40} + VehicleScalar{0.60} * std::sqrt(pressureRatio),
            VehicleScalar{0.40}, VehicleScalar{1.75});
        unsprungDescription.tireRadialStiffnessNPerM =
            authoredTireStiffness * pressureStiffnessScale
            * (failureBefore.valid
                ? failureBefore.carcassSupportScale : VehicleScalar{1.0});
        unsprungDescription.tireRadialDampingNsPerM =
            description.tireRadialDamping;
        unsprungDescription.maximumTireDeflectionM =
            description.maximumTireDeflection;
        unsprungDescription.maximumNormalForceN =
            description.maximumTireNormalForce;
        UnsprungMassInput unsprungInput;
        unsprungInput.deltaTimeSeconds = substepDeltaTime;
        unsprungInput.restLengthM = description.restLength;
        unsprungInput.minimumLengthM = minimumLength;
        unsprungInput.maximumLengthM = maximumLength;
        unsprungInput.suspensionForceN = suspensionLinkForce;
        unsprungInput.roadAvailable = hitGround;
        unsprungInput.roadHubLengthM = roadHubLength;
        unsprungInput.roadHubLengthVelocityMps =
            -geometricCompressionVelocity - structuralRoadVelocity;
        unsprungInput.roadNormalAlignment = hitGround
            ? dot(
                normalized(hit.normal, { 0.0f, 1.0f, 0.0f }),
                scale(suspensionDirection, -1.0f))
            : 1.0f;
        const UnsprungMassOutput unsprungOutput =
            advanceUnsprungMassModel(
                unsprungDescription,
                unsprungInput,
                wheel.unsprungMass);

        state.suspensionLength = unsprungOutput.suspensionLengthM;
        state.compression = description.restLength
            - state.suspensionLength;
        state.compressionVelocity =
            -unsprungOutput.suspensionLengthVelocityMps;
        state.unsprungVelocity =
            unsprungOutput.suspensionLengthVelocityMps;
        state.tireDeflection = unsprungOutput.tireDeflectionM;
        state.tireDeflectionVelocity =
            unsprungOutput.tireDeflectionVelocityMps;
        state.tireRadialDissipationWatts =
            unsprungOutput.tireRadialDissipationW;
        state.suspensionSpringForce = suspensionOutput.springForceN;
        state.suspensionDampingForce = suspensionOutput.dampingForceN;
        state.suspensionBumpStopForce = suspensionOutput.bumpStopForceN;
        state.suspensionDroopStopForce = suspensionOutput.droopStopForceN;
        state.suspensionUnclampedForce =
            suspensionOutput.unclampedForceN;
        state.damperDissipationWatts =
            suspensionOutput.damperDissipationW;
        state.leafInterleafFrictionForceN = suspensionOutput.leafInterleafForceN;
        state.leafInterleafDissipationWatts = suspensionOutput.leafInterleafDissipationW;
        state.leafAxleWrapJackingForceN = suspensionOutput.leafAxleWrapJackingForceN;
        state.motorcycleChainJackingForceN = suspensionOutput.motorcycleChainJackingForceN;
        state.twistBeamCouplingForceN = suspensionOutput.twistBeamCouplingForceN;
        state.twistBeamDissipationWatts = suspensionOutput.twistBeamDissipationW;
        state.normalForce = unsprungOutput.normalForceN;
        suspensionForce = state.normalForce;
        state.grounded = hitGround && unsprungOutput.grounded;
        if (state.grounded)
        {
            state.contactStatus = state.suspensionBottomed
                ? WheelContactStatus::SuspensionBottomed
                : WheelContactStatus::Supported;
        }
        else if (hitGround)
        {
            state.contactStatus = state.suspensionBottomed
                ? WheelContactStatus::SuspensionBottomed
                : WheelContactStatus::RoadDetectedNoLoad;
        }
        state.worldCenter = add(
            wheelRayOriginWorld,
            scale(suspensionDirection, state.suspensionLength));
        state.contactPoint = hitGround
            ? add(hit.point, scale(suspensionDirection, surfaceSupportSinkageM))
            : add(
                state.worldCenter,
                scale(suspensionDirection, description.radius));
        wheel.previousSuspensionLength = state.suspensionLength;

        if (hitGround)
        {
            state.contactNormal = normalized(
                hit.normal,
                { 0.0f, 1.0f, 0.0f });
        }
        if (state.grounded)
        {

            state.contactCollider = hit.collider;
            state.surfaceMaterial = hit.surfaceMaterial;
            state.surfaceWetness = hitSurfaceConditions.wetness;
            state.surfaceTemperatureC = hitSurfaceConditions.surfaceTemperatureC;
        }

        // SUSP11: with a rigid kart hub there is no spring link between tire
        // and frame. Transmit the pneumatic tire reaction directly into the
        // chassis at the physical wheel mount. Other suspension families keep
        // their existing spring/damper link-force transmission unchanged.
        const VehicleScalar chassisSupportForce = kartRigidSupport
            ? state.normalForce : suspensionLinkForce;
        if (std::abs(chassisSupportForce) > 0.0f)
        {
            const heritage::math::Vec3 suspensionImpulse = scale(
                suspensionDirection,
                -chassisSupportForce * substepDeltaTime);
            bodies.applyImpulseAtPoint(
                vehicle.description.chassisBody,
                suspensionImpulse,
                mountWorld);
        }
        if (!state.grounded)
        {
            updateUprightGeometry();
            advanceFreeWheelRotation();
            finalizeContactState();
            advanceAirborneThermal();
            return;
        }
    }

    finalizeContactState();
