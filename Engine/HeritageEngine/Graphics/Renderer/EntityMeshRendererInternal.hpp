#pragma once

// CLEAN05 private implementation vocabulary for EntityMeshRenderer translation units.
// This is intentionally not a public rendering API. It exists so asset, animation,
// shadow and draw ownership can compile separately without duplicating math policy.

#include "EntityMeshRenderer.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace heritage::graphics::entity_mesh_internal {

inline constexpr int kMaxSkinJoints = 128;

struct FrustumPlane
{
    heritage::math::Vec3 normal{ 0.0f, 0.0f, 0.0f };
    float distance = 0.0f;
};

struct ViewFrustum
{
    std::array<FrustumPlane, 6> planes{};
};

heritage::math::Mat4 multiply(
    const heritage::math::Mat4& left,
    const heritage::math::Mat4& right);
float linearDeterminant3x3(const heritage::math::Mat4& matrix);
ViewFrustum extractViewFrustum(
    const heritage::math::Mat4& projection,
    const heritage::math::Mat4& view);
heritage::math::Vec3 transformPoint(
    const heritage::math::Mat4& matrix,
    const std::array<float, 3>& point);
float maximumLinearScale(const heritage::math::Mat4& matrix);
bool tireVisualDeformationWithinDistance(
    const heritage::math::Mat4& rangeModel,
    const MeshNode& tireVisualNode);
bool sphereOutsideFrustum(
    const ViewFrustum& frustum,
    const heritage::math::Vec3& center,
    float radius);
heritage::math::Mat4 translation(const heritage::math::Vec3& value);
heritage::math::Mat4 scaleMatrix(const heritage::math::Vec3& value);
heritage::math::Mat4 rotationX(float angle);
heritage::math::Mat4 rotationY(float angle);
heritage::math::Mat4 rotationZ(float angle);
heritage::math::Mat4 eulerRotationDegrees(
    const heritage::math::Vec3& rotationDegrees);
heritage::math::Mat4 worldPoseMatrix(
    const heritage::math::Vec3& position,
    const heritage::math::Vec3& rotationDegrees);
heritage::math::Mat4 modelMatrix(
    const heritage::entities::MeshInstance& instance);
heritage::math::Mat4 arrayMatrix(const std::array<float, 16>& value);
heritage::math::Mat4 quaternionMatrix(float x, float y, float z, float w);
std::array<float, 4> normalizedQuaternion(const std::array<float, 4>& q);
std::array<float, 4> lerpQuaternion(
    const std::array<float, 4>& a,
    const std::array<float, 4>& b,
    float alpha);
heritage::math::Mat4 trsMatrix(
    const std::array<float, 3>& translationValue,
    const std::array<float, 4>& rotationValue,
    const std::array<float, 3>& scaleValue);
heritage::math::Mat4 inverseMatrix(const heritage::math::Mat4& m);

// Animation/node helpers shared by the material and shadow passes.
bool nodeMatchesPrefixFilter(
    const Mesh& mesh,
    int nodeIndex,
    const std::string& prefix);
void applyMeshNodeOverrides(
    const Mesh& mesh,
    const heritage::entities::MeshInstance& instance,
    const heritage::math::Mat4& instanceModel,
    const heritage::math::Vec3& cameraOrigin,
    std::vector<heritage::math::Mat4>& globals);
std::vector<heritage::math::Mat4> buildSkinPalette(
    const Mesh& mesh,
    const MeshDrawRange& range,
    const std::vector<heritage::math::Mat4>& nodeGlobals);

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b);
float dot(const heritage::math::Vec3& a, const heritage::math::Vec3& b);
heritage::math::Vec3 cross(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b);
heritage::math::Vec3 normalize(const heritage::math::Vec3& value);
heritage::math::Vec3 add(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b);
heritage::math::Vec3 multiplyVector(
    const heritage::math::Vec3& value,
    float scalar);
float length(const heritage::math::Vec3& value);
heritage::math::Vec3 transformPoint(
    const heritage::math::Mat4& matrix,
    const heritage::math::Vec3& value);
heritage::math::Vec3 unprojectNdc(
    const heritage::math::Mat4& inverseClip,
    float x,
    float y,
    float z);
heritage::math::Mat4 lookAt(
    const heritage::math::Vec3& eye,
    const heritage::math::Vec3& target,
    const heritage::math::Vec3& up);
heritage::math::Mat4 orthographic(
    float left,
    float right,
    float bottom,
    float top,
    float nearPlane,
    float farPlane);

} // namespace heritage::graphics::entity_mesh_internal
