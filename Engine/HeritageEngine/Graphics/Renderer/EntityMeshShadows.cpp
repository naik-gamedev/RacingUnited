#include "EntityMeshRenderer.hpp"
#include "EntityMeshRendererInternal.hpp"
#include "EntityMeshShadowConfig.hpp"
#include "EntityMeshShaders.hpp"
#include "../ShaderProgram.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

namespace heritage::graphics {
using namespace entity_mesh_internal;
using namespace entity_mesh_shadow_config;
using namespace entity_mesh_shaders;

static_assert(kCascadeCount == 4, "SHADOW02 layered geometry shader expects four cascades");

bool EntityMeshRenderer::initializeShadowResources()
{
    shutdownShadowResources();

    m_shadowProgram = buildShaderProgram(
        kShadowVertexShader,
        kShadowGeometryShader,
        kShadowFragmentShader);
    if (!m_shadowProgram)
        return false;

    GLint linked = GL_FALSE;
    glGetProgramiv(m_shadowProgram, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE)
    {
        shutdownShadowResources();
        return false;
    }

    m_shadowUniforms.model = glGetUniformLocation(m_shadowProgram, "uModel");
    m_shadowUniforms.lightViewProjection =
        glGetUniformLocation(m_shadowProgram, "uLightViewProjection[0]");
    m_shadowUniforms.cascadeMask =
        glGetUniformLocation(m_shadowProgram, "uCascadeMask");
    m_shadowUniforms.useSkinning =
        glGetUniformLocation(m_shadowProgram, "uUseSkinning");
    m_shadowUniforms.jointMatrices =
        glGetUniformLocation(m_shadowProgram, "uJointMatrices");
    m_shadowUniforms.tireVisualEnabled =
        glGetUniformLocation(m_shadowProgram, "uTireVisualEnabled");
    m_shadowUniforms.tireVisualGrounded =
        glGetUniformLocation(m_shadowProgram, "uTireVisualGrounded");
    m_shadowUniforms.tireVisualCenter =
        glGetUniformLocation(m_shadowProgram, "uTireVisualCenter");
    m_shadowUniforms.tireVisualAxleAxis =
        glGetUniformLocation(m_shadowProgram, "uTireVisualAxleAxis");
    m_shadowUniforms.tireVisualHalfWidth =
        glGetUniformLocation(m_shadowProgram, "uTireVisualHalfWidth");
    m_shadowUniforms.tireVisualInnerRadius =
        glGetUniformLocation(m_shadowProgram, "uTireVisualInnerRadius");
    m_shadowUniforms.tireVisualOuterRadius =
        glGetUniformLocation(m_shadowProgram, "uTireVisualOuterRadius");
    m_shadowUniforms.tireReferenceRadiusM =
        glGetUniformLocation(m_shadowProgram, "uTireReferenceRadiusM");
    m_shadowUniforms.tireRadialDeflectionM =
        glGetUniformLocation(m_shadowProgram, "uTireRadialDeflectionM");
    m_shadowUniforms.tireContactPatchLengthM =
        glGetUniformLocation(m_shadowProgram, "uTireContactPatchLengthM");
    m_shadowUniforms.tireContactPatchWidthM =
        glGetUniformLocation(m_shadowProgram, "uTireContactPatchWidthM");
    m_shadowUniforms.tireRingRadialOffsetM =
        glGetUniformLocation(m_shadowProgram, "uTireRingRadialOffsetM");
    m_shadowUniforms.tireRingLongitudinalOffsetM =
        glGetUniformLocation(m_shadowProgram, "uTireRingLongitudinalOffsetM");
    m_shadowUniforms.tireRingLateralOffsetM =
        glGetUniformLocation(m_shadowProgram, "uTireRingLateralOffsetM");
    m_shadowUniforms.tireRingYawDegrees =
        glGetUniformLocation(m_shadowProgram, "uTireRingYawDegrees");
    m_shadowUniforms.tireRingWindupDegrees =
        glGetUniformLocation(m_shadowProgram, "uTireRingWindupDegrees");
    m_shadowUniforms.tireFlatSpotDepthM =
        glGetUniformLocation(m_shadowProgram, "uTireFlatSpotDepthM");
    m_shadowUniforms.tireFlatSpotSector =
        glGetUniformLocation(m_shadowProgram, "uTireFlatSpotSector");
    m_shadowUniforms.tireContactNormalWorld =
        glGetUniformLocation(m_shadowProgram, "uTireContactNormalWorld");
    m_shadowUniforms.tireWheelForwardWorld =
        glGetUniformLocation(m_shadowProgram, "uTireWheelForwardWorld");
    m_shadowUniforms.tireWheelRightWorld =
        glGetUniformLocation(m_shadowProgram, "uTireWheelRightWorld");
    m_shadowUniforms.tireNormalForceN =
        glGetUniformLocation(m_shadowProgram, "uTireNormalForceN");
    m_shadowUniforms.tireLongitudinalForceN =
        glGetUniformLocation(m_shadowProgram, "uTireLongitudinalForceN");
    m_shadowUniforms.tireLateralForceN =
        glGetUniformLocation(m_shadowProgram, "uTireLateralForceN");
    m_shadowUniforms.tireVisualMotionSpeedMps =
        glGetUniformLocation(m_shadowProgram, "uTireVisualMotionSpeedMps");
    m_shadowUniforms.tireContactPlaneDistanceM =
        glGetUniformLocation(m_shadowProgram, "uTireContactPlaneDistanceM");
    m_shadowUniforms.tireVisualSupportGridValid =
        glGetUniformLocation(m_shadowProgram, "uTireVisualSupportGridValid");
    m_shadowUniforms.tireVisualSupportHalfLengthM =
        glGetUniformLocation(m_shadowProgram, "uTireVisualSupportHalfLengthM");
    m_shadowUniforms.tireVisualSupportHalfWidthM =
        glGetUniformLocation(m_shadowProgram, "uTireVisualSupportHalfWidthM");
    m_shadowUniforms.tireVisualSupportHeightResidualM =
        glGetUniformLocation(m_shadowProgram, "uTireVisualSupportHeightResidualM");
    m_shadowUniforms.tireVisualProbeGridValid =
        glGetUniformLocation(m_shadowProgram, "uTireVisualProbeGridValid");
    m_shadowUniforms.tireVisualProbeCompressionM =
        glGetUniformLocation(m_shadowProgram, "uTireVisualProbeCompressionM[0]");
    m_shadowUniforms.tireVisualColliderTriangleCount =
        glGetUniformLocation(m_shadowProgram, "uTireVisualColliderTriangleCount");
    m_shadowUniforms.tireVisualColliderTriangleA =
        glGetUniformLocation(m_shadowProgram, "uTireVisualColliderTriangleA[0]");
    m_shadowUniforms.tireVisualColliderTriangleB =
        glGetUniformLocation(m_shadowProgram, "uTireVisualColliderTriangleB[0]");
    m_shadowUniforms.tireVisualColliderTriangleC =
        glGetUniformLocation(m_shadowProgram, "uTireVisualColliderTriangleC[0]");
    m_shadowUniforms.tireVisualColliderTriangleNormal =
        glGetUniformLocation(m_shadowProgram, "uTireVisualColliderTriangleNormal[0]");

    GLint maximumTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    const int usableMaximumTextureSize = maximumTextureSize > 0
        ? static_cast<int>(maximumTextureSize)
        : kDefaultMapResolution;
    m_shadowResolution = std::max(
        1,
        std::min(kDefaultMapResolution, usableMaximumTextureSize));

    glGenTextures(1, &m_shadowTextureArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_shadowTextureArray);
    glTexImage3D(
        GL_TEXTURE_2D_ARRAY,
        0,
        GL_DEPTH_COMPONENT32F,
        m_shadowResolution,
        m_shadowResolution,
        kCascadeCount,
        0,
        GL_DEPTH_COMPONENT,
        GL_FLOAT,
        nullptr);
    // SHADOW04: the depth array itself stays raw. Two sampler objects view the
    // same storage differently: unit 10 performs linear hardware depth compares
    // for Poisson PCF/PCSS, while unit 11 exposes nearest raw depth for PCSS
    // blocker search and the diagnostic Nearest mode.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float borderDepth[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderDepth);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    glGenSamplers(1, &m_shadowCompareSampler);
    glSamplerParameteri(m_shadowCompareSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(m_shadowCompareSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(
        m_shadowCompareSampler,
        GL_TEXTURE_COMPARE_MODE,
        GL_COMPARE_REF_TO_TEXTURE);
    glSamplerParameteri(m_shadowCompareSampler, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glSamplerParameteri(m_shadowCompareSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(m_shadowCompareSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glSamplerParameterfv(
        m_shadowCompareSampler,
        GL_TEXTURE_BORDER_COLOR,
        borderDepth);

    glGenSamplers(1, &m_shadowRawSampler);
    glSamplerParameteri(m_shadowRawSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(m_shadowRawSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(m_shadowRawSampler, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glSamplerParameteri(m_shadowRawSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(m_shadowRawSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glSamplerParameterfv(m_shadowRawSampler, GL_TEXTURE_BORDER_COLOR, borderDepth);
    m_shadowFilterIndex = 2;

    glGenFramebuffers(1, &m_shadowFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFramebuffer);
    // SHADOW02 layered CSM: attach the entire depth-array texture once. The
    // geometry shader selects gl_Layer per emitted cascade, so one indexed
    // draw can populate every cascade it intersects.
    glFramebufferTexture(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        m_shadowTextureArray,
        0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr
            << "Shadow framebuffer incomplete: 0x"
            << std::hex << static_cast<unsigned int>(status) << std::dec << '\n';
        shutdownShadowResources();
        return false;
    }

    m_shadowResourcesValid = true;
    return true;
}

bool EntityMeshRenderer::synchronizeShadowSettings(
    const heritage::settings::VideoSettings& videoSettings)
{
    if (!m_shadowResourcesValid || !m_shadowTextureArray || !m_shadowFramebuffer)
        return false;

    const int qualityIndex = std::clamp(videoSettings.shadowQualityIndex, 0, 3);
    const Quality quality = static_cast<Quality>(qualityIndex);

    GLint maximumTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    const int usableMaximumTextureSize = maximumTextureSize > 0
        ? static_cast<int>(maximumTextureSize)
        : kDefaultMapResolution;
    const int desiredResolution = std::max(
        1,
        std::min(resolutionFor(quality), usableMaximumTextureSize));

    const int desiredFilterIndex = std::clamp(videoSettings.shadowFilterIndex, 0, 2);

    glBindTexture(GL_TEXTURE_2D_ARRAY, m_shadowTextureArray);

    if (m_shadowResolution != desiredResolution)
    {
        glTexImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            GL_DEPTH_COMPONENT32F,
            desiredResolution,
            desiredResolution,
            kCascadeCount,
            0,
            GL_DEPTH_COMPONENT,
            GL_FLOAT,
            nullptr);
        m_shadowResolution = desiredResolution;

        GLint previousFramebuffer = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFramebuffer);
        glFramebufferTexture(
            GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT,
            m_shadowTextureArray,
            0);
        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
            m_shadowResourcesValid = false;
            return false;
        }
    }

    // Filter selection is a shader algorithm choice. The compare sampler always
    // stays GL_LINEAR for Poisson PCF/PCSS and the raw sampler always stays
    // GL_NEAREST for blocker search / diagnostic nearest comparison.
    m_shadowFilterIndex = desiredFilterIndex;
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    return true;
}

void EntityMeshRenderer::shutdownShadowResources()
{
    m_shadowResourcesValid = false;
    m_shadowsActive = false;
    if (m_shadowFramebuffer)
    {
        glDeleteFramebuffers(1, &m_shadowFramebuffer);
        m_shadowFramebuffer = 0;
    }
    if (m_shadowTextureArray)
    {
        glDeleteTextures(1, &m_shadowTextureArray);
        m_shadowTextureArray = 0;
    }
    if (m_shadowCompareSampler)
    {
        glDeleteSamplers(1, &m_shadowCompareSampler);
        m_shadowCompareSampler = 0;
    }
    if (m_shadowRawSampler)
    {
        glDeleteSamplers(1, &m_shadowRawSampler);
        m_shadowRawSampler = 0;
    }
    if (m_shadowProgram)
    {
        glDeleteProgram(m_shadowProgram);
        m_shadowProgram = 0;
    }
    m_shadowUniforms = {};
    m_shadowResolution = 0;
    m_shadowFilterIndex = 2;
    for (heritage::math::Mat4& matrix : m_shadowMatrices)
        matrix = heritage::math::identity();
}
bool EntityMeshRenderer::buildShadowCascades(
    const heritage::math::Mat4& projection,
    const heritage::math::Mat4& view,
    const heritage::math::Vec3& sunDirection)
{
    if (!m_shadowResourcesValid)
        return false;

    const heritage::math::Vec3 lightDirection = normalize(sunDirection);
    if (length(lightDirection) <= 0.0001f)
        return false;

    const heritage::math::Mat4 inverseClip = inverseMatrix(multiply(projection, view));

    std::array<heritage::math::Vec3, 4> farRays{};
    int rayIndex = 0;
    for (int y = 0; y < 2; ++y)
    {
        for (int x = 0; x < 2; ++x)
        {
            const float ndcX = x == 0 ? -1.0f : 1.0f;
            const float ndcY = y == 0 ? -1.0f : 1.0f;
            // Sample a mid-depth point rather than the 100 km far plane. The
            // point lies on the same perspective ray but avoids magnifying
            // FP32 inversion error from the enormous main-view far distance.
            const heritage::math::Vec3 rayPoint =
                unprojectNdc(inverseClip, ndcX, ndcY, 0.0f);
            const heritage::math::Vec3 rayView = transformPoint(view, rayPoint);
            const float forwardDepth = std::max(-rayView.z, 0.0001f);
            farRays[static_cast<std::size_t>(rayIndex++)] =
                multiplyVector(rayPoint, 1.0f / forwardDepth);
        }
    }

    float sliceNear = kNearDistance;
    for (int cascade = 0; cascade < kCascadeCount; ++cascade)
    {
        const float sliceFar = m_shadowSplits[static_cast<std::size_t>(cascade)];
        std::array<heritage::math::Vec3, 8> corners{};
        for (int corner = 0; corner < 4; ++corner)
        {
            corners[static_cast<std::size_t>(corner)] =
                multiplyVector(farRays[static_cast<std::size_t>(corner)], sliceNear);
            corners[static_cast<std::size_t>(corner + 4)] =
                multiplyVector(farRays[static_cast<std::size_t>(corner)], sliceFar);
        }

        heritage::math::Vec3 center{ 0.0f, 0.0f, 0.0f };
        for (const heritage::math::Vec3& corner : corners)
            center = add(center, corner);
        center = multiplyVector(center, 1.0f / 8.0f);

        float radius = 0.0f;
        for (const heritage::math::Vec3& corner : corners)
            radius = std::max(radius, length(subtract(corner, center)));
        // Quantizing the extent keeps the projection from changing by tiny
        // floating-point amounts as the chase camera moves. Camera-relative
        // world space already removes the largest source of shadow shimmer.
        radius = std::ceil(std::max(radius, 1.0f) * 16.0f) / 16.0f;
        radius *= 1.04f;

        heritage::math::Vec3 lightUp{ 0.0f, 1.0f, 0.0f };
        if (std::abs(dot(lightDirection, lightUp)) > 0.94f)
            lightUp = { 0.0f, 0.0f, 1.0f };

        const float lightDistance = radius + kDepthPaddingMeters;
        const heritage::math::Vec3 lightEye =
            add(center, multiplyVector(lightDirection, lightDistance));
        const heritage::math::Mat4 lightView = lookAt(lightEye, center, lightUp);

        float minZ = std::numeric_limits<float>::max();
        float maxZ = -std::numeric_limits<float>::max();
        for (const heritage::math::Vec3& corner : corners)
        {
            const heritage::math::Vec3 lightCorner = transformPoint(lightView, corner);
            minZ = std::min(minZ, lightCorner.z);
            maxZ = std::max(maxZ, lightCorner.z);
        }

        const float nearPlane = std::max(0.10f, -maxZ - kDepthPaddingMeters);
        const float farPlane = std::max(
            nearPlane + 1.0f,
            -minZ + kDepthPaddingMeters);
        const heritage::math::Mat4 lightProjection = orthographic(
            -radius,
            radius,
            -radius,
            radius,
            nearPlane,
            farPlane);
        m_shadowMatrices[static_cast<std::size_t>(cascade)] =
            multiply(lightProjection, lightView);
        sliceNear = sliceFar;
    }

    return true;
}
void EntityMeshRenderer::drawShadowMaps(
    const std::vector<heritage::entities::MeshInstance>& instances,
    const heritage::math::Vec3& eye,
    float elapsedSeconds)
{
    if (!m_shadowResourcesValid || !m_shadowsActive || instances.empty())
        return;

    struct PreparedShadowInstance
    {
        const heritage::entities::MeshInstance* instance = nullptr;
        const Mesh* mesh = nullptr;
        heritage::math::Mat4 instanceModel = heritage::math::identity();
        std::vector<heritage::math::Mat4> nodeGlobals;
        std::vector<const heritage::entities::MeshNodeOverride*> tireVisualOverrides;
    };

    // Evaluate animation/node overrides once per mesh instance. SHADOW02 then
    // submits each accepted range once and lets the GPU fan the triangle out to
    // all intersecting cascades. The old path repeated this range work and its
    // OpenGL draw submission independently for all four cascades.
    std::vector<PreparedShadowInstance> prepared;
    prepared.reserve(instances.size());
    for (const auto& instance : instances)
    {
        const Mesh* mesh = acquireMesh(
            instance.assetPath,
            instance.normalize,
            instance.blenderCoordinates);
        if (!mesh)
            continue;

        heritage::entities::MeshInstance cameraRelativeInstance = instance;
        cameraRelativeInstance.position = {
            instance.position.x - eye.x,
            instance.position.y - eye.y,
            instance.position.z - eye.z
        };

        PreparedShadowInstance value;
        value.instance = &instance;
        value.mesh = mesh;
        value.instanceModel = modelMatrix(cameraRelativeInstance);
        value.nodeGlobals = animationTransformsForInstance(
            *mesh,
            instance,
            elapsedSeconds);
        applyMeshNodeOverrides(
            *mesh,
            instance,
            value.instanceModel,
            eye,
            value.nodeGlobals);

        // Tire deformation overrides are sparse, but the old cascade loop
        // searched the instance override list for every draw range. Resolve the
        // node-index lookup once while preparing the instance.
        value.tireVisualOverrides.resize(mesh->nodes.size(), nullptr);
        for (const auto& overrideValue : instance.nodeOverrides)
        {
            if (!overrideValue.hasTireVisualDeformation)
                continue;
            for (std::size_t nodeIndex = 0; nodeIndex < mesh->nodes.size(); ++nodeIndex)
            {
                if (mesh->nodes[nodeIndex].name == overrideValue.nodeName)
                {
                    value.tireVisualOverrides[nodeIndex] = &overrideValue;
                    break;
                }
            }
        }
        prepared.push_back(std::move(value));
    }

    if (prepared.empty())
        return;

    GLint previousFramebuffer = 0;
    GLint previousViewport[4] = {};
    GLint previousScissor[4] = {};
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    glGetIntegerv(GL_SCISSOR_BOX, previousScissor);

    const GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);

    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFramebuffer);
    glViewport(0, 0, m_shadowResolution, m_shadowResolution);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glClearDepth(1.0);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);
    glUseProgram(m_shadowProgram);

    // The entire array is a layered depth attachment. One clear resets all four
    // cascades and the geometry shader writes each primitive to gl_Layer.
    glClear(GL_DEPTH_BUFFER_BIT);
    glUniformMatrix4fv(
        m_shadowUniforms.lightViewProjection,
        kCascadeCount,
        GL_FALSE,
        m_shadowMatrices[0].m);

    std::array<ViewFrustum, kCascadeCount> shadowFrustums{};
    for (int cascade = 0; cascade < kCascadeCount; ++cascade)
    {
        shadowFrustums[static_cast<std::size_t>(cascade)] = extractViewFrustum(
            m_shadowMatrices[static_cast<std::size_t>(cascade)],
            heritage::math::identity());
    }

    const int allCascadeMask = (1 << kCascadeCount) - 1;
    GLenum activeFrontFace = GL_CCW;
    glFrontFace(activeFrontFace);

    for (const PreparedShadowInstance& preparedInstance : prepared)
    {
        const auto& instance = *preparedInstance.instance;
        const Mesh& mesh = *preparedInstance.mesh;
        const heritage::math::Mat4& instanceModel = preparedInstance.instanceModel;
        const auto& nodeGlobals = preparedInstance.nodeGlobals;

        if (instance.doubleSided)
            glDisable(GL_CULL_FACE);
        else
            glEnable(GL_CULL_FACE);

        glBindVertexArray(mesh.vao);

        const auto nodeTireVisualState = [&](int nodeIndex)
            -> std::pair<const MeshNode*, const heritage::entities::MeshNodeOverride*>
        {
            if (nodeIndex < 0
                || static_cast<std::size_t>(nodeIndex) >= mesh.nodes.size())
            {
                return { nullptr, nullptr };
            }

            const std::size_t index = static_cast<std::size_t>(nodeIndex);
            const MeshNode& candidate = mesh.nodes[index];
            if (!candidate.hasTireVisualGeometry)
                return { nullptr, nullptr };

            const heritage::entities::MeshNodeOverride* state =
                index < preparedInstance.tireVisualOverrides.size()
                ? preparedInstance.tireVisualOverrides[index]
                : nullptr;
            return { &candidate, state };
        };

        const auto rangeModelForNode = [&](int nodeIndex)
        {
            heritage::math::Mat4 rangeModel = instanceModel;
            if (nodeIndex >= 0
                && static_cast<std::size_t>(nodeIndex) < nodeGlobals.size())
            {
                rangeModel = multiply(
                    instanceModel,
                    nodeGlobals[static_cast<std::size_t>(nodeIndex)]);
            }
            return rangeModel;
        };

        const auto cascadeMaskForRange = [&]
        (
            const MeshDrawRange& range,
            const heritage::math::Mat4& rangeModel,
            const heritage::entities::MeshNodeOverride* tireVisualState)
        {
            if (!range.hasBounds || range.skinIndex >= 0)
                return allCascadeMask;

            const heritage::math::Vec3 boundsCenter =
                transformPoint(rangeModel, range.boundsCenter);
            const float tireBoundsInflation = tireVisualState ? 1.12f : 1.0f;
            const float boundsRadius =
                range.boundsRadius * tireBoundsInflation
                * maximumLinearScale(rangeModel);

            int cascadeMask = 0;
            for (int cascade = 0; cascade < kCascadeCount; ++cascade)
            {
                if (sphereOutsideFrustum(
                        shadowFrustums[static_cast<std::size_t>(cascade)],
                        boundsCenter,
                        boundsRadius))
                {
                    ++m_frameStats.shadowCulledRanges;
                }
                else
                {
                    cascadeMask |= (1 << cascade);
                }
            }
            return cascadeMask;
        };

        const auto drawShadowBatch = [&]
        (
            std::size_t firstIndex,
            std::size_t indexCount,
            int nodeIndex,
            int skinIndex,
            int cascadeMask)
        {
            if (indexCount == 0 || cascadeMask == 0)
                return;

            const heritage::math::Mat4 rangeModel = rangeModelForNode(nodeIndex);
            const auto [tireVisualNode, tireVisualState] =
                nodeTireVisualState(nodeIndex);

            const bool reflectedRange = linearDeterminant3x3(rangeModel) < 0.0f;
            const GLenum requestedFrontFace = reflectedRange ? GL_CW : GL_CCW;
            if (requestedFrontFace != activeFrontFace)
            {
                glFrontFace(requestedFrontFace);
                activeFrontFace = requestedFrontFace;
            }

            glUniform1i(m_shadowUniforms.cascadeMask, cascadeMask);
            glUniformMatrix4fv(
                m_shadowUniforms.model,
                1,
                GL_FALSE,
                rangeModel.m);

            const bool useTireVisual =
                tireVisualNode != nullptr && tireVisualState != nullptr;
            glUniform1i(
                m_shadowUniforms.tireVisualEnabled,
                useTireVisual ? 1 : 0);
            if (useTireVisual)
            {
                glUniform1i(
                    m_shadowUniforms.tireVisualGrounded,
                    tireVisualState->tireGrounded ? 1 : 0);
                glUniform3f(
                    m_shadowUniforms.tireVisualCenter,
                    tireVisualNode->tireVisualCenter[0],
                    tireVisualNode->tireVisualCenter[1],
                    tireVisualNode->tireVisualCenter[2]);
                glUniform1i(
                    m_shadowUniforms.tireVisualAxleAxis,
                    tireVisualNode->tireVisualAxleAxis);
                glUniform1f(
                    m_shadowUniforms.tireVisualHalfWidth,
                    tireVisualNode->tireVisualHalfWidth);
                glUniform1f(
                    m_shadowUniforms.tireVisualInnerRadius,
                    tireVisualNode->tireVisualInnerRadius);
                glUniform1f(
                    m_shadowUniforms.tireVisualOuterRadius,
                    tireVisualNode->tireVisualOuterRadius);
                glUniform1f(
                    m_shadowUniforms.tireReferenceRadiusM,
                    tireVisualState->tireReferenceRadiusM);
                glUniform1f(
                    m_shadowUniforms.tireRadialDeflectionM,
                    tireVisualState->tireRadialDeflectionM);
                glUniform1f(
                    m_shadowUniforms.tireContactPatchLengthM,
                    tireVisualState->tireContactPatchLengthM);
                glUniform1f(
                    m_shadowUniforms.tireContactPatchWidthM,
                    tireVisualState->tireContactPatchWidthM);
                glUniform1f(
                    m_shadowUniforms.tireRingRadialOffsetM,
                    tireVisualState->tireRingRadialOffsetM);
                glUniform1f(
                    m_shadowUniforms.tireRingLongitudinalOffsetM,
                    tireVisualState->tireRingLongitudinalOffsetM);
                glUniform1f(
                    m_shadowUniforms.tireRingLateralOffsetM,
                    tireVisualState->tireRingLateralOffsetM);
                glUniform1f(
                    m_shadowUniforms.tireRingYawDegrees,
                    tireVisualState->tireRingYawDegrees);
                glUniform1f(
                    m_shadowUniforms.tireRingWindupDegrees,
                    tireVisualState->tireRingWindupDegrees);
                glUniform1f(
                    m_shadowUniforms.tireFlatSpotDepthM,
                    tireVisualState->tireFlatSpotDepthM);
                glUniform1f(
                    m_shadowUniforms.tireFlatSpotSector,
                    tireVisualState->tireFlatSpotSector);
                glUniform3f(
                    m_shadowUniforms.tireContactNormalWorld,
                    tireVisualState->tireContactNormalWorld.x,
                    tireVisualState->tireContactNormalWorld.y,
                    tireVisualState->tireContactNormalWorld.z);
                glUniform3f(
                    m_shadowUniforms.tireWheelForwardWorld,
                    tireVisualState->tireWheelForwardWorld.x,
                    tireVisualState->tireWheelForwardWorld.y,
                    tireVisualState->tireWheelForwardWorld.z);
                glUniform3f(
                    m_shadowUniforms.tireWheelRightWorld,
                    tireVisualState->tireWheelRightWorld.x,
                    tireVisualState->tireWheelRightWorld.y,
                    tireVisualState->tireWheelRightWorld.z);
                glUniform1f(m_shadowUniforms.tireNormalForceN, tireVisualState->tireNormalForceN);
                glUniform1f(m_shadowUniforms.tireLongitudinalForceN, tireVisualState->tireLongitudinalForceN);
                glUniform1f(m_shadowUniforms.tireLateralForceN, tireVisualState->tireLateralForceN);
                glUniform1f(m_shadowUniforms.tireVisualMotionSpeedMps, tireVisualState->tireVisualMotionSpeedMps);
                glUniform1f(
                    m_shadowUniforms.tireContactPlaneDistanceM,
                    tireVisualState->tireContactPlaneDistanceM);
                glUniform1i(
                    m_shadowUniforms.tireVisualSupportGridValid,
                    tireVisualState->tireVisualSupportGridValid ? 1 : 0);
                glUniform1f(
                    m_shadowUniforms.tireVisualSupportHalfLengthM,
                    tireVisualState->tireVisualSupportHalfLengthM);
                glUniform1f(
                    m_shadowUniforms.tireVisualSupportHalfWidthM,
                    tireVisualState->tireVisualSupportHalfWidthM);
                glUniform1fv(
                    m_shadowUniforms.tireVisualSupportHeightResidualM,
                    9,
                    tireVisualState->tireVisualSupportHeightResidualM.data());
                glUniform1i(
                    m_shadowUniforms.tireVisualProbeGridValid,
                    tireVisualState->tireVisualProbeGridValid ? 1 : 0);
                glUniform1fv(
                    m_shadowUniforms.tireVisualProbeCompressionM,
                    static_cast<GLsizei>(heritage::entities::TireVisualProbeCount),
                    tireVisualState->tireVisualProbeCompressionM.data());
                const std::uint32_t colliderTriangleCount =
                    tireVisualState->tireVisualColliderTrianglesValid
                        ? (std::min)(
                            tireVisualState->tireVisualColliderTriangleCount,
                            static_cast<std::uint32_t>(heritage::entities::TireVisualColliderTriangleLimit))
                        : 0u;
                glUniform1i(
                    m_shadowUniforms.tireVisualColliderTriangleCount,
                    static_cast<GLint>(colliderTriangleCount));
                if (colliderTriangleCount > 0)
                {
                    // TIRE22/VIS14: convert absolute FP64/physics-world collider
                    // geometry into this spinning tire draw range's LOCAL space
                    // before it reaches GLSL. rangeModel is camera-relative, so
                    // feeding absolute world triangles directly to the shader
                    // compares unrelated coordinate systems and can make every
                    // contact appear inert. Local-space triangles also remain
                    // correct through wheel spin, steering and mirrored GLB nodes.
                    const heritage::math::Mat4 worldToTireLocal =
                        inverseMatrix(rangeModel);
                    const auto worldPointToTireLocal = [&](
                        const heritage::math::Vec3& point)
                    {
                        return transformPoint(
                            worldToTireLocal,
                            std::array<float, 3>{
                                point.x - eye.x,
                                point.y - eye.y,
                                point.z - eye.z });
                    };
                    const auto localTriangleNormal = [&](
                        const heritage::math::Vec3& localA,
                        const heritage::math::Vec3& localB,
                        const heritage::math::Vec3& localC)
                    {
                        const heritage::math::Vec3 ab{
                            localB.x - localA.x, localB.y - localA.y, localB.z - localA.z };
                        const heritage::math::Vec3 ac{
                            localC.x - localA.x, localC.y - localA.y, localC.z - localA.z };
                        heritage::math::Vec3 normal{
                            ab.y * ac.z - ab.z * ac.y,
                            ab.z * ac.x - ab.x * ac.z,
                            ab.x * ac.y - ab.y * ac.x };
                        const float lengthSquared = normal.x * normal.x
                            + normal.y * normal.y + normal.z * normal.z;
                        if (lengthSquared > 1.0e-12f)
                        {
                            const float inverseLength = 1.0f / std::sqrt(lengthSquared);
                            normal.x *= inverseLength;
                            normal.y *= inverseLength;
                            normal.z *= inverseLength;
                        }
                        else
                        {
                            normal = { 0.0f, 1.0f, 0.0f };
                        }
                        // Collision normals always point toward the tire interior.
                        // This makes road, kerb, wall and mirrored-side triangles
                        // share one stable "push rubber back toward the tire" rule.
                        const float towardCenter =
                            (tireVisualNode->tireVisualCenter[0] - localA.x) * normal.x
                            + (tireVisualNode->tireVisualCenter[1] - localA.y) * normal.y
                            + (tireVisualNode->tireVisualCenter[2] - localA.z) * normal.z;
                        if (towardCenter < 0.0f)
                        {
                            normal.x = -normal.x;
                            normal.y = -normal.y;
                            normal.z = -normal.z;
                        }
                        return normal;
                    };

                    std::array<float, heritage::entities::TireVisualColliderTriangleLimit * 4> a{};
                    std::array<float, heritage::entities::TireVisualColliderTriangleLimit * 4> b{};
                    std::array<float, heritage::entities::TireVisualColliderTriangleLimit * 4> c{};
                    std::array<float, heritage::entities::TireVisualColliderTriangleLimit * 4> n{};
                    for (std::uint32_t triangleIndex = 0;
                         triangleIndex < colliderTriangleCount; ++triangleIndex)
                    {
                        const auto& triangle =
                            tireVisualState->tireVisualColliderTriangles[triangleIndex];
                        const heritage::math::Vec3 localA =
                            worldPointToTireLocal(triangle.a);
                        const heritage::math::Vec3 localB =
                            worldPointToTireLocal(triangle.b);
                        const heritage::math::Vec3 localC =
                            worldPointToTireLocal(triangle.c);
                        const heritage::math::Vec3 localNormal =
                            localTriangleNormal(localA, localB, localC);
                        const std::size_t base = static_cast<std::size_t>(triangleIndex) * 4;
                        a[base + 0] = localA.x; a[base + 1] = localA.y;
                        a[base + 2] = localA.z; a[base + 3] = 1.0f;
                        b[base + 0] = localB.x; b[base + 1] = localB.y;
                        b[base + 2] = localB.z; b[base + 3] = 1.0f;
                        c[base + 0] = localC.x; c[base + 1] = localC.y;
                        c[base + 2] = localC.z; c[base + 3] = 1.0f;
                        n[base + 0] = localNormal.x; n[base + 1] = localNormal.y;
                        n[base + 2] = localNormal.z; n[base + 3] = 0.0f;
                    }
                    glUniform4fv(m_shadowUniforms.tireVisualColliderTriangleA,
                        static_cast<GLsizei>(colliderTriangleCount), a.data());
                    glUniform4fv(m_shadowUniforms.tireVisualColliderTriangleB,
                        static_cast<GLsizei>(colliderTriangleCount), b.data());
                    glUniform4fv(m_shadowUniforms.tireVisualColliderTriangleC,
                        static_cast<GLsizei>(colliderTriangleCount), c.data());
                    glUniform4fv(m_shadowUniforms.tireVisualColliderTriangleNormal,
                        static_cast<GLsizei>(colliderTriangleCount), n.data());
                }
            }

            MeshDrawRange paletteRange;
            paletteRange.nodeIndex = nodeIndex;
            paletteRange.skinIndex = skinIndex;
            std::vector<heritage::math::Mat4> palette =
                buildSkinPalette(mesh, paletteRange, nodeGlobals);
            const bool useSkinning = !palette.empty();
            glUniform1i(
                m_shadowUniforms.useSkinning,
                useSkinning ? 1 : 0);
            if (useSkinning)
            {
                std::array<float, 16 * kMaxSkinJoints> jointData{};
                const heritage::math::Mat4 identity = heritage::math::identity();
                for (int joint = 0; joint < kMaxSkinJoints; ++joint)
                {
                    const heritage::math::Mat4& source =
                        joint < static_cast<int>(palette.size())
                        ? palette[static_cast<std::size_t>(joint)]
                        : identity;
                    for (int value = 0; value < 16; ++value)
                    {
                        jointData[static_cast<std::size_t>(joint) * 16
                            + static_cast<std::size_t>(value)] = source.m[value];
                    }
                }
                glUniformMatrix4fv(
                    m_shadowUniforms.jointMatrices,
                    kMaxSkinJoints,
                    GL_FALSE,
                    jointData.data());
            }

            glDrawElements(
                GL_TRIANGLES,
                static_cast<GLsizei>(indexCount),
                GL_UNSIGNED_INT,
                reinterpret_cast<const void*>(
                    firstIndex * sizeof(unsigned int)));
            ++m_frameStats.shadowDrawCalls;

            int cascadeCopies = 0;
            for (int cascade = 0; cascade < kCascadeCount; ++cascade)
            {
                if ((cascadeMask & (1 << cascade)) != 0)
                    ++cascadeCopies;
            }
            m_frameStats.shadowTriangles +=
                static_cast<std::uint64_t>(indexCount / 3)
                * static_cast<std::uint64_t>(cascadeCopies);
        };

        if (mesh.drawRanges.empty())
        {
            MeshDrawRange complete;
            complete.indexCount = mesh.indices.size();
            const int cascadeMask = allCascadeMask;
            drawShadowBatch(
                0,
                complete.indexCount,
                complete.nodeIndex,
                complete.skinIndex,
                cascadeMask);
        }
        else
        {
            // SHADOW02 material-agnostic coalescing. Shadow depth does not care
            // which visible material split produced a triangle, so contiguous
            // ranges sharing the same node transform + skin can be submitted as
            // one indexed draw. Hidden/filtered ranges still break the batch.
            std::size_t rangeIndex = 0;
            while (rangeIndex < mesh.drawRanges.size())
            {
                const MeshDrawRange& firstRange = mesh.drawRanges[rangeIndex];
                if (firstRange.indexCount == 0
                    || firstRange.hiddenByAuthoring
                    || !nodeMatchesPrefixFilter(
                        mesh,
                        firstRange.nodeIndex,
                        instance.nodeNamePrefixFilter))
                {
                    ++rangeIndex;
                    continue;
                }

                const int nodeIndex = firstRange.nodeIndex;
                const int skinIndex = firstRange.skinIndex;
                const heritage::math::Mat4 rangeModel = rangeModelForNode(nodeIndex);
                const auto [unusedTireNode, tireVisualState] =
                    nodeTireVisualState(nodeIndex);
                (void)unusedTireNode;

                std::size_t batchFirstIndex = firstRange.firstIndex;
                std::size_t batchIndexCount = firstRange.indexCount;
                int batchCascadeMask = cascadeMaskForRange(
                    firstRange,
                    rangeModel,
                    tireVisualState);

                std::size_t nextIndex = rangeIndex + 1;
                while (nextIndex < mesh.drawRanges.size())
                {
                    const MeshDrawRange& nextRange = mesh.drawRanges[nextIndex];
                    const bool nextAccepted =
                        nextRange.indexCount > 0
                        && !nextRange.hiddenByAuthoring
                        && nodeMatchesPrefixFilter(
                            mesh,
                            nextRange.nodeIndex,
                            instance.nodeNamePrefixFilter);
                    const bool contiguous =
                        nextRange.firstIndex == batchFirstIndex + batchIndexCount;
                    const bool sameTransformState =
                        nextRange.nodeIndex == nodeIndex
                        && nextRange.skinIndex == skinIndex;
                    if (!nextAccepted || !contiguous || !sameTransformState)
                        break;

                    batchCascadeMask |= cascadeMaskForRange(
                        nextRange,
                        rangeModel,
                        tireVisualState);
                    batchIndexCount += nextRange.indexCount;
                    ++nextIndex;
                }

                drawShadowBatch(
                    batchFirstIndex,
                    batchIndexCount,
                    nodeIndex,
                    skinIndex,
                    batchCascadeMask);
                rangeIndex = nextIndex;
            }
        }
    }

    glBindVertexArray(0);
    glDisable(GL_POLYGON_OFFSET_FILL);
    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    // Heritage's main framebuffer uses reversed-Z globally. Shadow maps use
    // ordinary GL_LESS depth only inside this pass, then restore the engine
    // invariant before sky/material rendering resumes.
    glDepthFunc(GL_GREATER);
    glClearDepth(0.0);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
    glViewport(
        previousViewport[0],
        previousViewport[1],
        previousViewport[2],
        previousViewport[3]);
    glScissor(
        previousScissor[0],
        previousScissor[1],
        previousScissor[2],
        previousScissor[3]);
    if (scissorWasEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
}

} // namespace heritage::graphics
