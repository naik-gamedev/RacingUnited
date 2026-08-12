// Simulation islands and sleep/wake propagation.

#include "../../CollisionSystem.hpp"
#include "../CollisionInternal.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>

namespace heritage::physics {
using namespace collision_detail;

void CollisionSystem::updateSimulationIslandsAndSleeping(
    RigidBodySystem& bodies,
    float fixedDeltaTime,
    bool finalizeSleep)
{
    const std::uint32_t invalidIndex =
        (std::numeric_limits<std::uint32_t>::max)();
    std::vector<std::uint32_t> parent(bodies.m_slots.size(), invalidIndex);
    std::vector<std::uint8_t> rank(bodies.m_slots.size(), 0u);

    for (std::uint32_t index = 0;
         index < static_cast<std::uint32_t>(bodies.m_slots.size());
         ++index)
    {
        const RigidBodySystem::Slot& slot = bodies.m_slots[index];
        if (slot.alive
            && slot.record.motionType == BodyMotionType::Dynamic)
        {
            parent[index] = index;
        }
    }

    const auto findRoot = [&parent, invalidIndex](std::uint32_t index) {
        if (index >= parent.size() || parent[index] == invalidIndex)
            return invalidIndex;

        std::uint32_t root = index;
        while (parent[root] != root)
            root = parent[root];

        while (parent[index] != index)
        {
            const std::uint32_t next = parent[index];
            parent[index] = root;
            index = next;
        }
        return root;
    };

    const auto unionBodies = [&parent, &rank, &findRoot, invalidIndex](
        std::uint32_t first,
        std::uint32_t second) {
        std::uint32_t rootA = findRoot(first);
        std::uint32_t rootB = findRoot(second);
        if (rootA == invalidIndex || rootB == invalidIndex || rootA == rootB)
            return;

        if (rank[rootA] < rank[rootB])
            std::swap(rootA, rootB);
        parent[rootB] = rootA;
        if (rank[rootA] == rank[rootB])
            ++rank[rootA];
    };

    for (const CollisionContact& contact : m_contacts)
    {
        if (contact.trigger)
            continue;

        std::uint32_t staticTriangleIndex = 0;
        if (staticTriangleIndexFromColliderHandle(
                contact.colliderB,
                staticTriangleIndex))
        {
            continue;
        }

        std::uint32_t bodyIndexA = 0;
        std::uint32_t bodyGenerationA = 0;
        std::uint32_t bodyIndexB = 0;
        std::uint32_t bodyGenerationB = 0;
        const bool validA = RigidBodySystem::decodeHandle(
            contact.bodyA, bodyIndexA, bodyGenerationA);
        const bool validB = RigidBodySystem::decodeHandle(
            contact.bodyB, bodyIndexB, bodyGenerationB);
        if (!validA || !validB
            || bodyIndexA >= bodies.m_slots.size()
            || bodyIndexB >= bodies.m_slots.size())
        {
            continue;
        }

        const RigidBodySystem::Slot& slotA = bodies.m_slots[bodyIndexA];
        const RigidBodySystem::Slot& slotB = bodies.m_slots[bodyIndexB];
        if (!slotA.alive || slotA.generation != bodyGenerationA
            || !slotB.alive || slotB.generation != bodyGenerationB)
        {
            continue;
        }

        if (slotA.record.motionType == BodyMotionType::Dynamic
            && slotB.record.motionType == BodyMotionType::Dynamic)
        {
            unionBodies(bodyIndexA, bodyIndexB);
        }
    }

    struct Island
    {
        std::vector<std::uint32_t> bodyIndices;
        bool wakeRequested = false;
        bool touchedMovingKinematic = false;
    };

    std::vector<Island> islands;
    std::unordered_map<std::uint32_t, std::size_t> islandByRoot;
    for (std::uint32_t index = 0;
         index < static_cast<std::uint32_t>(bodies.m_slots.size());
         ++index)
    {
        if (parent[index] == invalidIndex)
            continue;

        const std::uint32_t root = findRoot(index);
        auto [iterator, inserted] = islandByRoot.emplace(root, islands.size());
        if (inserted)
            islands.emplace_back();

        Island& island = islands[iterator->second];
        island.bodyIndices.push_back(index);
        if (!bodies.m_slots[index].record.sleeping)
            island.wakeRequested = true;
    }

    const auto movingKinematic = [](const RigidBodySystem::Record& body) {
        if (body.motionType != BodyMotionType::Kinematic)
            return false;
        return lengthSquared(body.linearVelocity)
                > kKinematicWakeSpeed * kKinematicWakeSpeed
            || lengthSquared(body.angularVelocityDegrees)
                > kKinematicWakeSpeed * kKinematicWakeSpeed;
    };

    for (const CollisionContact& contact : m_contacts)
    {
        if (contact.trigger)
            continue;

        std::uint32_t staticTriangleIndex = 0;
        if (staticTriangleIndexFromColliderHandle(
                contact.colliderB,
                staticTriangleIndex))
        {
            std::uint32_t bodyIndex = 0;
            std::uint32_t bodyGeneration = 0;
            if (RigidBodySystem::decodeHandle(
                    contact.bodyA,
                    bodyIndex,
                    bodyGeneration)
                && bodyIndex < bodies.m_slots.size()
                && bodies.m_slots[bodyIndex].alive
                && bodies.m_slots[bodyIndex].generation == bodyGeneration
                && contact.penetration > kPenetrationWakeThreshold)
            {
                const auto iterator = islandByRoot.find(
                    findRoot(bodyIndex));
                if (iterator != islandByRoot.end())
                    islands[iterator->second].wakeRequested = true;
            }
            continue;
        }

        std::uint32_t bodyIndexA = 0;
        std::uint32_t bodyGenerationA = 0;
        std::uint32_t bodyIndexB = 0;
        std::uint32_t bodyGenerationB = 0;
        if (!RigidBodySystem::decodeHandle(
                contact.bodyA, bodyIndexA, bodyGenerationA)
            || !RigidBodySystem::decodeHandle(
                contact.bodyB, bodyIndexB, bodyGenerationB)
            || bodyIndexA >= bodies.m_slots.size()
            || bodyIndexB >= bodies.m_slots.size())
        {
            continue;
        }

        const RigidBodySystem::Record& bodyA =
            bodies.m_slots[bodyIndexA].record;
        const RigidBodySystem::Record& bodyB =
            bodies.m_slots[bodyIndexB].record;

        if (bodyA.motionType == BodyMotionType::Dynamic
            && movingKinematic(bodyB))
        {
            const std::uint32_t root = findRoot(bodyIndexA);
            const auto iterator = islandByRoot.find(root);
            if (iterator != islandByRoot.end())
            {
                islands[iterator->second].wakeRequested = true;
                islands[iterator->second].touchedMovingKinematic = true;
            }
        }
        if (bodyB.motionType == BodyMotionType::Dynamic
            && movingKinematic(bodyA))
        {
            const std::uint32_t root = findRoot(bodyIndexB);
            const auto iterator = islandByRoot.find(root);
            if (iterator != islandByRoot.end())
            {
                islands[iterator->second].wakeRequested = true;
                islands[iterator->second].touchedMovingKinematic = true;
            }
        }

        if (contact.penetration > kPenetrationWakeThreshold)
        {
            if (bodyA.motionType == BodyMotionType::Dynamic)
            {
                const auto iterator = islandByRoot.find(findRoot(bodyIndexA));
                if (iterator != islandByRoot.end())
                    islands[iterator->second].wakeRequested = true;
            }
            if (bodyB.motionType == BodyMotionType::Dynamic)
            {
                const auto iterator = islandByRoot.find(findRoot(bodyIndexB));
                if (iterator != islandByRoot.end())
                    islands[iterator->second].wakeRequested = true;
            }
        }
    }

    if (!finalizeSleep)
    {
        for (Island& island : islands)
        {
            if (!island.wakeRequested)
                continue;
            for (const std::uint32_t bodyIndex : island.bodyIndices)
            {
                RigidBodySystem::Record& body =
                    bodies.m_slots[bodyIndex].record;
                if (body.sleeping)
                    RigidBodySystem::wakeRecord(body);
            }
        }
        return;
    }

    m_simulationIslandCount = islands.size();
    m_activeIslandCount = 0;
    m_sleepingIslandCount = 0;

    const float safeDeltaTime =
        finiteFloat(fixedDeltaTime) && fixedDeltaTime > 0.0f
        ? fixedDeltaTime
        : 0.0f;
    const float freeLinearThresholdSquared =
        kSleepFreeLinearSpeed * kSleepFreeLinearSpeed;
    const float contactTangentialThresholdSquared =
        kSleepContactTangentialSpeed * kSleepContactTangentialSpeed;
    const float angularThresholdSquared =
        kSleepAngularSpeedDegrees * kSleepAngularSpeedDegrees;

    std::vector<bool> hasContact(bodies.m_slots.size(), false);
    std::vector<float> maximumTangentialSpeedSquared(
        bodies.m_slots.size(), 0.0f);
    std::vector<float> maximumNormalSpeed(
        bodies.m_slots.size(), 0.0f);

    const auto recordContactVelocity =
        [&bodies,
         &hasContact,
         &maximumTangentialSpeedSquared,
         &maximumNormalSpeed](
            std::uint32_t bodyIndex,
            const heritage::math::Vec3& normal) {
            if (bodyIndex >= bodies.m_slots.size())
                return;
            const RigidBodySystem::Record& body =
                bodies.m_slots[bodyIndex].record;
            if (body.motionType != BodyMotionType::Dynamic)
                return;

            const float normalSpeed = dot(body.linearVelocity, normal);
            const heritage::math::Vec3 tangentVelocity = subtract(
                body.linearVelocity,
                scaleVector(normal, normalSpeed));
            hasContact[bodyIndex] = true;
            maximumTangentialSpeedSquared[bodyIndex] = (std::max)(
                maximumTangentialSpeedSquared[bodyIndex],
                lengthSquared(tangentVelocity));
            maximumNormalSpeed[bodyIndex] = (std::max)(
                maximumNormalSpeed[bodyIndex],
                std::abs(normalSpeed));
        };

    for (const CollisionContact& contact : m_contacts)
    {
        if (contact.trigger)
            continue;

        std::uint32_t staticTriangleIndex = 0;
        if (staticTriangleIndexFromColliderHandle(
                contact.colliderB,
                staticTriangleIndex))
        {
            std::uint32_t bodyIndex = 0;
            std::uint32_t bodyGeneration = 0;
            if (RigidBodySystem::decodeHandle(
                    contact.bodyA,
                    bodyIndex,
                    bodyGeneration))
            {
                recordContactVelocity(bodyIndex, contact.normal);
            }
            continue;
        }

        std::uint32_t bodyIndexA = 0;
        std::uint32_t bodyGenerationA = 0;
        std::uint32_t bodyIndexB = 0;
        std::uint32_t bodyGenerationB = 0;
        if (!RigidBodySystem::decodeHandle(
                contact.bodyA, bodyIndexA, bodyGenerationA)
            || !RigidBodySystem::decodeHandle(
                contact.bodyB, bodyIndexB, bodyGenerationB))
        {
            continue;
        }

        recordContactVelocity(bodyIndexA, contact.normal);
        recordContactVelocity(bodyIndexB, contact.normal);
    }

    for (Island& island : islands)
    {
        bool allSleeping = true;
        bool allQuiet = !island.touchedMovingKinematic;
        bool allAllowSleep = true;

        for (const std::uint32_t bodyIndex : island.bodyIndices)
        {
            const RigidBodySystem::Record& body =
                bodies.m_slots[bodyIndex].record;
            allSleeping = allSleeping && body.sleeping;
            allAllowSleep = allAllowSleep && body.allowSleep;

            const bool linearQuiet =
                lengthSquared(body.linearVelocity)
                    <= freeLinearThresholdSquared
                || (hasContact[bodyIndex]
                    && maximumTangentialSpeedSquared[bodyIndex]
                        <= contactTangentialThresholdSquared
                    && maximumNormalSpeed[bodyIndex]
                        <= kSleepContactNormalSpeed);
            if (!linearQuiet
                || lengthSquared(body.angularVelocityDegrees)
                    > angularThresholdSquared)
            {
                allQuiet = false;
            }
        }

        if (allSleeping)
        {
            ++m_sleepingIslandCount;
            continue;
        }

        if (!allQuiet || !allAllowSleep)
        {
            for (const std::uint32_t bodyIndex : island.bodyIndices)
                RigidBodySystem::wakeRecord(bodies.m_slots[bodyIndex].record);
            ++m_activeIslandCount;
            continue;
        }

        bool readyToSleep = true;
        for (const std::uint32_t bodyIndex : island.bodyIndices)
        {
            RigidBodySystem::Record& body =
                bodies.m_slots[bodyIndex].record;
            body.sleepTimer += safeDeltaTime;
            readyToSleep = readyToSleep
                && body.sleepTimer >= kSleepDelaySeconds;
        }

        if (readyToSleep)
        {
            for (const std::uint32_t bodyIndex : island.bodyIndices)
                RigidBodySystem::sleepRecord(bodies.m_slots[bodyIndex].record);
            ++m_sleepingIslandCount;
        }
        else
        {
            ++m_activeIslandCount;
        }
    }
}

} // namespace heritage::physics
