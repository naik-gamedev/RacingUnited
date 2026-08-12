#include "../../CollisionSystem.hpp"
#include "../CollisionInternal.hpp"

#include <algorithm>
#include <vector>

namespace heritage::physics {
using namespace collision_detail;

// CLEAN13 broadphase owner. Builds dynamic primitive sweep-and-prune candidates
// and bounded static-scene manifolds, then hands exact pairs to the existing
// narrowphase/contact-cache machinery without changing solver order.
void CollisionSystem::collectBroadphaseContacts(RigidBodySystem& bodies)
{
    std::vector<BroadphaseProxy> proxies;
    proxies.reserve(m_aliveCount);
    for (std::uint32_t index = 0;
         index < static_cast<std::uint32_t>(m_slots.size());
         ++index)
    {
        const Slot& slot = m_slots[index];
        if (!slot.alive)
            continue;

        const RigidBodySystem::Slot* bodySlot = bodies.resolve(slot.record.body);
        if (!bodySlot)
            continue;

        BroadphaseProxy proxy;
        proxy.slotIndex = index;
        proxy.handle = makeHandle(index, slot.generation);
        proxy.bounds = worldAabb(slot.record, bodySlot->record);
        proxies.push_back(proxy);
    }

    std::stable_sort(
        proxies.begin(),
        proxies.end(),
        [](const BroadphaseProxy& left, const BroadphaseProxy& right) {
            if (left.bounds.minimum.x != right.bounds.minimum.x)
                return left.bounds.minimum.x < right.bounds.minimum.x;
            return left.handle < right.handle;
        });

    for (std::size_t firstIndex = 0; firstIndex < proxies.size(); ++firstIndex)
    {
        const BroadphaseProxy& proxyA = proxies[firstIndex];
        Slot& slotA = m_slots[proxyA.slotIndex];
        RigidBodySystem::Slot* bodySlotA = bodies.resolve(slotA.record.body);
        if (!bodySlotA)
            continue;

        for (std::size_t secondIndex = firstIndex + 1;
             secondIndex < proxies.size();
             ++secondIndex)
        {
            const BroadphaseProxy& proxyB = proxies[secondIndex];
            if (proxyB.bounds.minimum.x > proxyA.bounds.maximum.x)
                break;

            Slot& slotB = m_slots[proxyB.slotIndex];
            if (slotA.record.body == slotB.record.body)
                continue;
            if ((slotA.record.mask & slotB.record.layer) == 0u
                || (slotB.record.mask & slotA.record.layer) == 0u)
            {
                continue;
            }
            if (!aabbOverlap(proxyA.bounds, proxyB.bounds))
                continue;

            ++m_broadphaseCandidateCount;
            RigidBodySystem::Slot* bodySlotB = bodies.resolve(slotB.record.body);
            if (!bodySlotB)
                continue;

            ++m_narrowphaseTestCount;
            CollisionContact contact;
            if (!generateContact(
                    proxyA.handle,
                    slotA.record,
                    bodySlotA->record,
                    proxyB.handle,
                    slotB.record,
                    bodySlotB->record,
                    contact))
            {
                continue;
            }

            contact.trigger = slotA.record.trigger || slotB.record.trigger;
            canonicalizeContact(contact);
            if (!contact.trigger)
                restoreCachedImpulse(contact);
            m_contacts.push_back(contact);
        }
    }

    // Static creator triangles use the same immutable BVH as suspension rays.
    // Each dynamic primitive receives a bounded local manifold so dense lidar
    // terrain cannot feed an unbounded number of contacts into the solver.
    if (!m_staticTriangleBvh.empty())
    {
        const heritage::math::Vec3 contactExpansion{
            kStaticTriangleContactSkin,
            kStaticTriangleContactSkin,
            kStaticTriangleContactSkin
        };
        const float separationSquared =
            kStaticTriangleManifoldPointSeparation
            * kStaticTriangleManifoldPointSeparation;
        for (const BroadphaseProxy& proxy : proxies)
        {
            Slot& colliderSlot = m_slots[proxy.slotIndex];
            RigidBodySystem::Slot* bodySlot = bodies.resolve(
                colliderSlot.record.body);
            if (!bodySlot
                || bodySlot->record.motionType != BodyMotionType::Dynamic
                || colliderSlot.record.trigger
                || (colliderSlot.record.mask & 1u) == 0u)
            {
                continue;
            }

            const Aabb expandedBounds{
                subtract(proxy.bounds.minimum, contactExpansion),
                add(proxy.bounds.maximum, contactExpansion)
            };
            std::vector<std::uint32_t> triangleIndices;
            std::size_t nodeTestCount = 0;
            queryStaticSceneTriangles(
                expandedBounds,
                triangleIndices,
                nodeTestCount);
            m_staticBroadphaseNodeTestCount += nodeTestCount;
            m_staticTriangleCandidateCount += triangleIndices.size();
            m_broadphaseCandidateCount += triangleIndices.size();

            std::vector<CollisionContact> candidates;
            candidates.reserve(triangleIndices.size());
            for (const std::uint32_t triangleIndex : triangleIndices)
            {
                ++m_staticTriangleNarrowphaseTestCount;
                ++m_narrowphaseTestCount;
                CollisionContact contact;
                if (generateStaticTriangleContact(
                        proxy.handle,
                        colliderSlot.record,
                        bodySlot->record,
                        triangleIndex,
                        contact))
                {
                    candidates.push_back(contact);
                }
            }

            std::stable_sort(
                candidates.begin(),
                candidates.end(),
                [](const CollisionContact& left,
                   const CollisionContact& right) {
                    if (left.penetration != right.penetration)
                        return left.penetration > right.penetration;
                    return left.colliderB < right.colliderB;
                });

            std::vector<CollisionContact> manifold;
            manifold.reserve(kMaximumStaticContactsPerCollider);
            for (CollisionContact& candidate : candidates)
            {
                bool redundant = false;
                for (const CollisionContact& selected : manifold)
                {
                    if (lengthSquared(subtract(
                            candidate.point,
                            selected.point)) < separationSquared
                        && dot(candidate.normal, selected.normal) > 0.98f)
                    {
                        redundant = true;
                        break;
                    }
                }
                if (redundant)
                    continue;

                canonicalizeContact(candidate);
                restoreCachedImpulse(candidate);
                manifold.push_back(candidate);
                if (manifold.size() >= kMaximumStaticContactsPerCollider)
                    break;
            }

            m_staticTriangleContactCount += manifold.size();
            m_contacts.insert(
                m_contacts.end(),
                manifold.begin(),
                manifold.end());
        }
    }

}

} // namespace heritage::physics
