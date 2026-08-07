#include "EntityMeshRenderer.hpp"

#include "../ShaderProgram.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace heritage::graphics {
namespace {

#ifdef _WIN32
#define HERITAGE_MESH_GLSL_VERSION "#version 460 core\n"
#else
#define HERITAGE_MESH_GLSL_VERSION "#version 330 core\n"
#endif

constexpr float kPi = 3.14159265358979323846f;

const char* kVertexShader = HERITAGE_MESH_GLSL_VERSION R"glsl(
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
out vec3 vNormal;
out vec3 vWorldPosition;
void main()
{
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPosition = world.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    gl_Position = uProjection * uView * world;
}
)glsl";

const char* kFragmentShader = HERITAGE_MESH_GLSL_VERSION R"glsl(
in vec3 vNormal;
in vec3 vWorldPosition;
uniform vec3 uColor;
uniform vec3 uEye;
uniform float uGamma;
uniform float uBrightness;
uniform float uContrast;
uniform float uSaturation;
out vec4 FragColor;
void main()
{
    vec3 normal = normalize(vNormal);
    // OBJ creator scenes can contain legacy/inverted face normals. When a
    // double-sided surface is viewed from its back face, orient its shading
    // normal toward the visible side. This makes the road top readable while
    // the physical underside remains substantially darker under an overhead sun.
    if (!gl_FrontFacing)
        normal = -normal;

    // Temporary readable-world lighting until the proper 29M lighting system.
    // A directional sun stays consistent across creator scenes hundreds of
    // metres wide; the old point light near world origin did not.
    vec3 lightDirection = normalize(vec3(-0.35, 0.86, 0.38));
    vec3 viewDirection = normalize(uEye - vWorldPosition);
    vec3 halfwayDirection = normalize(lightDirection + viewDirection);

    float upFacing = clamp(normal.y * 0.5 + 0.5, 0.0, 1.0);
    float hemisphere = mix(0.16, 0.34, upFacing);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float specular = pow(max(dot(normal, halfwayDirection), 0.0), 40.0) * 0.24;
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 2.8) * 0.10;
    float slopeReadability = mix(0.82, 1.08, clamp(normal.y, 0.0, 1.0));
    vec3 color = uColor * slopeReadability * (hemisphere + diffuse * 0.72)
        + vec3(specular) + uColor * rim;

    color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / max(uGamma, 0.01)));
    color = (color - 0.5) * uContrast + 0.5 + uBrightness;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, uSaturation);
    FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
)glsl";

heritage::math::Mat4 multiply(
    const heritage::math::Mat4& left,
    const heritage::math::Mat4& right)
{
    heritage::math::Mat4 result{};
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            result.m[column * 4 + row] =
                left.m[0 * 4 + row] * right.m[column * 4 + 0]
                + left.m[1 * 4 + row] * right.m[column * 4 + 1]
                + left.m[2 * 4 + row] * right.m[column * 4 + 2]
                + left.m[3 * 4 + row] * right.m[column * 4 + 3];
        }
    }
    return result;
}

heritage::math::Mat4 translation(const heritage::math::Vec3& value)
{
    heritage::math::Mat4 result = heritage::math::identity();
    result.m[12] = value.x;
    result.m[13] = value.y;
    result.m[14] = value.z;
    return result;
}

heritage::math::Mat4 scaleMatrix(const heritage::math::Vec3& value)
{
    heritage::math::Mat4 result = heritage::math::identity();
    result.m[0] = value.x;
    result.m[5] = value.y;
    result.m[10] = value.z;
    return result;
}

heritage::math::Mat4 rotationX(float angle)
{
    heritage::math::Mat4 result = heritage::math::identity();
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    result.m[5] = c;
    result.m[6] = s;
    result.m[9] = -s;
    result.m[10] = c;
    return result;
}

heritage::math::Mat4 rotationY(float angle)
{
    heritage::math::Mat4 result = heritage::math::identity();
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    result.m[0] = c;
    result.m[2] = -s;
    result.m[8] = s;
    result.m[10] = c;
    return result;
}

heritage::math::Mat4 rotationZ(float angle)
{
    heritage::math::Mat4 result = heritage::math::identity();
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    result.m[0] = c;
    result.m[1] = s;
    result.m[4] = -s;
    result.m[5] = c;
    return result;
}

heritage::math::Mat4 modelMatrix(
    const heritage::entities::MeshInstance& instance)
{
    const float toRadians = kPi / 180.0f;
    const heritage::math::Mat4 rotation = multiply(
        rotationZ(instance.rotationDegrees.z * toRadians),
        multiply(
            rotationY(instance.rotationDegrees.y * toRadians),
            rotationX(instance.rotationDegrees.x * toRadians)));
    return multiply(
        translation(instance.position),
        multiply(rotation, scaleMatrix(instance.scale)));
}

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

float dot(const heritage::math::Vec3& a, const heritage::math::Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

heritage::math::Vec3 cross(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

heritage::math::Vec3 normalize(const heritage::math::Vec3& value)
{
    const float length = std::sqrt(dot(value, value));
    if (length <= 0.000001f)
        return { 0.0f, 0.0f, 0.0f };
    return { value.x / length, value.y / length, value.z / length };
}

heritage::math::Mat4 lookAt(
    const heritage::math::Vec3& eye,
    const heritage::math::Vec3& target,
    const heritage::math::Vec3& up)
{
    const heritage::math::Vec3 forward = normalize(subtract(target, eye));
    const heritage::math::Vec3 side = normalize(cross(forward, up));
    const heritage::math::Vec3 correctedUp = cross(side, forward);

    heritage::math::Mat4 result = heritage::math::identity();
    result.m[0] = side.x;
    result.m[1] = correctedUp.x;
    result.m[2] = -forward.x;
    result.m[4] = side.y;
    result.m[5] = correctedUp.y;
    result.m[6] = -forward.y;
    result.m[8] = side.z;
    result.m[9] = correctedUp.z;
    result.m[10] = -forward.z;
    result.m[12] = -dot(side, eye);
    result.m[13] = -dot(correctedUp, eye);
    result.m[14] = dot(forward, eye);
    return result;
}

bool pathBeginsWith(
    const std::filesystem::path& candidate,
    const std::filesystem::path& root)
{
    auto candidatePart = candidate.begin();
    for (auto rootPart = root.begin(); rootPart != root.end(); ++rootPart, ++candidatePart)
    {
        if (candidatePart == candidate.end() || *candidatePart != *rootPart)
            return false;
    }
    return true;
}

} // namespace

bool EntityMeshRenderer::initialize(const std::filesystem::path& moduleAssetRoot)
{
    shutdown();

    std::error_code error;
    m_assetRoot = std::filesystem::weakly_canonical(moduleAssetRoot, error);
    if (error)
        m_assetRoot = std::filesystem::absolute(moduleAssetRoot).lexically_normal();

    m_program = buildShaderProgram(kVertexShader, kFragmentShader);
    if (!m_program)
    {
        m_lastError = "EntityMeshRenderer could not compile its shader.";
        return false;
    }

    m_lastError.clear();
    return true;
}

void EntityMeshRenderer::shutdown()
{
    clearCache();
    if (m_program)
    {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    m_assetRoot.clear();
    m_lastError.clear();
}

void EntityMeshRenderer::clearCache()
{
    for (auto& [key, asset] : m_cache)
        destroyMesh(asset.mesh);
    m_cache.clear();
}

bool EntityMeshRenderer::resolveAsset(
    const std::string& relativePath,
    std::filesystem::path& resolved,
    std::string& error) const
{
    const std::filesystem::path requested(relativePath);
    if (relativePath.empty() || requested.is_absolute() || requested.has_root_name())
    {
        error = "Mesh path must be relative to the active module Assets directory.";
        return false;
    }

    std::error_code canonicalError;
    resolved = std::filesystem::weakly_canonical(
        m_assetRoot / requested,
        canonicalError);
    if (canonicalError)
        resolved = std::filesystem::absolute(m_assetRoot / requested).lexically_normal();

    if (!pathBeginsWith(resolved, m_assetRoot))
    {
        error = "Mesh path escaped the active module Assets directory.";
        return false;
    }

    if (!std::filesystem::is_regular_file(resolved))
    {
        error = "Mesh asset was not found: " + resolved.string();
        return false;
    }

    error.clear();
    return true;
}

const Mesh* EntityMeshRenderer::acquireMesh(
    const std::string& relativePath,
    bool normalize,
    bool blenderCoordinates)
{
    std::filesystem::path resolved;
    std::string resolveError;
    const std::string cacheKey = relativePath
        + (normalize ? "|normalized" : "|authored")
        + (blenderCoordinates ? "|blender" : "|engine");
    CachedAsset& asset = m_cache[cacheKey];

    if (!resolveAsset(relativePath, resolved, resolveError))
    {
        if (!asset.attempted || asset.error != resolveError)
            std::cerr << "Entity mesh warning: " << resolveError << '\n';
        asset.attempted = true;
        asset.loaded = false;
        asset.error = resolveError;
        m_lastError = resolveError;
        return nullptr;
    }

    std::error_code timeError;
    const auto writeTime = std::filesystem::last_write_time(resolved, timeError);
    const bool changed = !asset.attempted
        || (!timeError && writeTime != asset.lastWriteTime);
    if (changed)
    {
        destroyMesh(asset.mesh);
        asset.mesh = loadObjMesh(
            resolved.string(), normalize, blenderCoordinates);
        asset.attempted = true;
        asset.lastWriteTime = writeTime;
        asset.loaded = !asset.mesh.indices.empty();
        if (asset.loaded)
        {
            uploadMesh(asset.mesh);
            asset.loaded = asset.mesh.vao != 0;
        }

        if (!asset.loaded)
        {
            asset.error = "OBJ contained no renderable triangles: " + resolved.string();
            std::cerr << "Entity mesh warning: " << asset.error << '\n';
            m_lastError = asset.error;
        }
        else
        {
            asset.error.clear();
            m_lastError.clear();
        }
    }

    return asset.loaded ? &asset.mesh : nullptr;
}

void EntityMeshRenderer::draw(
    const heritage::entities::EntityRegistry& registry,
    const heritage::math::Mat4& projection,
    const heritage::settings::VideoSettings& videoSettings,
    float elapsedSeconds)
{
    if (!m_program)
        return;

    const std::vector<heritage::entities::MeshInstance> instances =
        registry.meshInstances();
    if (instances.empty())
        return;

    heritage::math::Vec3 cameraTarget{ 0.0f, 1.0f, 0.0f };
    const heritage::entities::EntityHandle player =
        registry.findByName("Player Vehicle Root");
    heritage::math::Vec3 playerPosition{};
    if (player != heritage::entities::InvalidEntity
        && registry.worldPosition(player, playerPosition))
    {
        cameraTarget = {
            playerPosition.x,
            playerPosition.y + 0.9f,
            playerPosition.z
        };
    }

    heritage::math::Vec3 eye{};
    const bool playerWorldActive =
        registry.findByName("Player Scene Visual") != heritage::entities::InvalidEntity;
    if (player != heritage::entities::InvalidEntity && playerWorldActive)
    {
        heritage::math::Vec3 rotationDegrees{};
        registry.worldRotationDegrees(player, rotationDegrees);
        const float yawRadians = rotationDegrees.y * (kPi / 180.0f);
        const heritage::math::Vec3 forward{
            std::sin(yawRadians),
            0.0f,
            std::cos(yawRadians)
        };

        // Temporary drive camera for creator scenes. Keep the camera behind
        // the authoritative vehicle heading instead of orbiting continuously.
        cameraTarget = {
            playerPosition.x + forward.x * 2.0f,
            playerPosition.y + 0.85f,
            playerPosition.z + forward.z * 2.0f
        };
        eye = {
            playerPosition.x - forward.x * 6.6f,
            playerPosition.y + 2.7f,
            playerPosition.z - forward.z * 6.6f
        };
    }
    else
    {
        const float orbitAngle = elapsedSeconds * 0.18f;
        eye = {
            cameraTarget.x + std::sin(orbitAngle) * 8.5f,
            cameraTarget.y + 3.4f,
            cameraTarget.z + std::cos(orbitAngle) * 8.5f
        };
    }

    const heritage::math::Mat4 view = lookAt(
        eye,
        cameraTarget,
        { 0.0f, 1.0f, 0.0f });

    glUseProgram(m_program);
    glUniformMatrix4fv(glGetUniformLocation(m_program, "uView"), 1, GL_FALSE, view.m);
    glUniformMatrix4fv(glGetUniformLocation(m_program, "uProjection"), 1, GL_FALSE, projection.m);
    glUniform3f(glGetUniformLocation(m_program, "uEye"), eye.x, eye.y, eye.z);
    glUniform1f(glGetUniformLocation(m_program, "uGamma"), videoSettings.gamma);
    glUniform1f(glGetUniformLocation(m_program, "uBrightness"), videoSettings.brightness);
    glUniform1f(glGetUniformLocation(m_program, "uContrast"), videoSettings.contrast);
    glUniform1f(glGetUniformLocation(m_program, "uSaturation"), videoSettings.saturation);

    glEnable(GL_DEPTH_TEST);
    for (const auto& instance : instances)
    {
        const Mesh* mesh = acquireMesh(
            instance.assetPath,
            instance.normalize,
            instance.blenderCoordinates);
        if (!mesh)
            continue;

        if (instance.doubleSided)
            glDisable(GL_CULL_FACE);
        else
            glEnable(GL_CULL_FACE);

        const heritage::math::Mat4 model = modelMatrix(instance);
        glUniformMatrix4fv(glGetUniformLocation(m_program, "uModel"), 1, GL_FALSE, model.m);
        glUniform3f(
            glGetUniformLocation(m_program, "uColor"),
            instance.color.x,
            instance.color.y,
            instance.color.z);
        glBindVertexArray(mesh->vao);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(mesh->indices.size()),
            GL_UNSIGNED_INT,
            nullptr);
    }

    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
}

std::size_t EntityMeshRenderer::loadedAssetCount() const
{
    return static_cast<std::size_t>(std::count_if(
        m_cache.begin(),
        m_cache.end(),
        [](const auto& item) { return item.second.loaded; }));
}

} // namespace heritage::graphics
