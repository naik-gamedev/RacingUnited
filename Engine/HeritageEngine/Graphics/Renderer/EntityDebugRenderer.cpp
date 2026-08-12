#include "EntityDebugRenderer.hpp"

#include "../ShaderProgram.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace heritage::graphics {
namespace {

#ifdef _WIN32
#define HERITAGE_ENTITY_GLSL_VERSION "#version 460 core\n"
#else
#define HERITAGE_ENTITY_GLSL_VERSION "#version 330 core\n"
#endif

constexpr float kPi = 3.14159265358979323846f;

const char* kVertexShader = HERITAGE_ENTITY_GLSL_VERSION R"glsl(
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

const char* kFragmentShader = HERITAGE_ENTITY_GLSL_VERSION R"glsl(
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
    vec3 lightDirection = normalize(vec3(5.0, 8.0, 6.0) - vWorldPosition);
    vec3 viewDirection = normalize(uEye - vWorldPosition);
    vec3 halfwayDirection = normalize(lightDirection + viewDirection);

    float diffuse = max(dot(normal, lightDirection), 0.0);
    float specular = pow(max(dot(normal, halfwayDirection), 0.0), 36.0) * 0.35;
    float rim = pow(1.0 - max(dot(normal, viewDirection), 0.0), 2.5) * 0.18;
    vec3 color = uColor * (0.20 + diffuse * 0.82) + vec3(specular) + uColor * rim;

    color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / max(uGamma, 0.01)));
    color = (color - 0.5) * uContrast + 0.5 + uBrightness;
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, uSaturation);
    FragColor = vec4(color, 1.0);
}
)glsl";

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

float dot(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

heritage::math::Vec3 cross(
    const heritage::math::Vec3& left,
    const heritage::math::Vec3& right)
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x
    };
}

heritage::math::Vec3 normalize(const heritage::math::Vec3& value)
{
    const float length = std::sqrt(dot(value, value));
    if (length <= 0.000001f)
        return { 0.0f, 0.0f, 0.0f };
    return { value.x / length, value.y / length, value.z / length };
}

heritage::math::Mat4 multiply(
    const heritage::math::Mat4& left,
    const heritage::math::Mat4& right)
{
    heritage::math::Mat4 result{};
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            for (int index = 0; index < 4; ++index)
            {
                result.m[column * 4 + row] +=
                    left.m[index * 4 + row] * right.m[column * 4 + index];
            }
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
    heritage::math::Mat4 result{};
    result.m[0] = value.x;
    result.m[5] = value.y;
    result.m[10] = value.z;
    result.m[15] = 1.0f;
    return result;
}

heritage::math::Mat4 rotationX(float radians)
{
    heritage::math::Mat4 result = heritage::math::identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.m[5] = cosine;
    result.m[6] = sine;
    result.m[9] = -sine;
    result.m[10] = cosine;
    return result;
}

heritage::math::Mat4 rotationY(float radians)
{
    heritage::math::Mat4 result = heritage::math::identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.m[0] = cosine;
    result.m[2] = -sine;
    result.m[8] = sine;
    result.m[10] = cosine;
    return result;
}

heritage::math::Mat4 rotationZ(float radians)
{
    heritage::math::Mat4 result = heritage::math::identity();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.m[0] = cosine;
    result.m[1] = sine;
    result.m[4] = -sine;
    result.m[5] = cosine;
    return result;
}

heritage::math::Mat4 modelMatrix(
    const heritage::entities::DebugPrimitiveInstance& instance)
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

void appendVertex(
    Mesh& mesh,
    float x, float y, float z,
    float nx, float ny, float nz)
{
    mesh.vertices.insert(mesh.vertices.end(), { x, y, z, nx, ny, nz });
}

Mesh makeBox()
{
    Mesh mesh;
    const float p = 0.5f;
    struct Face
    {
        heritage::math::Vec3 normal;
        heritage::math::Vec3 corners[4];
    };

    const Face faces[] = {
        {{ 1, 0, 0 }, {{ p,-p,-p },{ p,-p, p },{ p, p, p },{ p, p,-p }}},
        {{-1, 0, 0 }, {{-p,-p, p },{-p,-p,-p },{-p, p,-p },{-p, p, p }}},
        {{ 0, 1, 0 }, {{-p, p,-p },{ p, p,-p },{ p, p, p },{-p, p, p }}},
        {{ 0,-1, 0 }, {{-p,-p, p },{ p,-p, p },{ p,-p,-p },{-p,-p,-p }}},
        {{ 0, 0, 1 }, {{ p,-p, p },{-p,-p, p },{-p, p, p },{ p, p, p }}},
        {{ 0, 0,-1 }, {{-p,-p,-p },{ p,-p,-p },{ p, p,-p },{-p, p,-p }}}
    };

    for (const Face& face : faces)
    {
        const unsigned int base = static_cast<unsigned int>(mesh.vertices.size() / 6);
        for (const auto& corner : face.corners)
            appendVertex(mesh, corner.x, corner.y, corner.z,
                face.normal.x, face.normal.y, face.normal.z);
        mesh.indices.insert(mesh.indices.end(), {
            base, base + 1, base + 2,
            base, base + 2, base + 3
        });
    }
    return mesh;
}

Mesh makeCylinder(int segments)
{
    Mesh mesh;
    segments = std::max(segments, 8);

    // Cylinder axis is local X, matching a conventional vehicle wheel axle.
    for (int segment = 0; segment < segments; ++segment)
    {
        const float a0 = (2.0f * kPi * segment) / static_cast<float>(segments);
        const float a1 = (2.0f * kPi * (segment + 1)) / static_cast<float>(segments);
        const float y0 = std::cos(a0) * 0.5f;
        const float z0 = std::sin(a0) * 0.5f;
        const float y1 = std::cos(a1) * 0.5f;
        const float z1 = std::sin(a1) * 0.5f;

        unsigned int base = static_cast<unsigned int>(mesh.vertices.size() / 6);
        appendVertex(mesh, -0.5f, y0, z0, 0.0f, std::cos(a0), std::sin(a0));
        appendVertex(mesh,  0.5f, y0, z0, 0.0f, std::cos(a0), std::sin(a0));
        appendVertex(mesh,  0.5f, y1, z1, 0.0f, std::cos(a1), std::sin(a1));
        appendVertex(mesh, -0.5f, y1, z1, 0.0f, std::cos(a1), std::sin(a1));
        mesh.indices.insert(mesh.indices.end(), {
            base, base + 1, base + 2,
            base, base + 2, base + 3
        });

        base = static_cast<unsigned int>(mesh.vertices.size() / 6);
        appendVertex(mesh, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
        appendVertex(mesh, -0.5f, y1, z1, -1.0f, 0.0f, 0.0f);
        appendVertex(mesh, -0.5f, y0, z0, -1.0f, 0.0f, 0.0f);
        mesh.indices.insert(mesh.indices.end(), { base, base + 1, base + 2 });

        base = static_cast<unsigned int>(mesh.vertices.size() / 6);
        appendVertex(mesh, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        appendVertex(mesh, 0.5f, y0, z0, 1.0f, 0.0f, 0.0f);
        appendVertex(mesh, 0.5f, y1, z1, 1.0f, 0.0f, 0.0f);
        mesh.indices.insert(mesh.indices.end(), { base, base + 1, base + 2 });
    }
    return mesh;
}

Mesh makeSphere(int slices, int stacks)
{
    Mesh mesh;
    slices = std::max(slices, 8);
    stacks = std::max(stacks, 4);

    for (int stack = 0; stack <= stacks; ++stack)
    {
        const float v = static_cast<float>(stack) / static_cast<float>(stacks);
        const float phi = v * kPi;
        const float y = std::cos(phi) * 0.5f;
        const float radius = std::sin(phi) * 0.5f;
        for (int slice = 0; slice <= slices; ++slice)
        {
            const float u = static_cast<float>(slice) / static_cast<float>(slices);
            const float theta = u * 2.0f * kPi;
            const float x = std::cos(theta) * radius;
            const float z = std::sin(theta) * radius;
            const heritage::math::Vec3 normal = normalize({ x, y, z });
            appendVertex(mesh, x, y, z, normal.x, normal.y, normal.z);
        }
    }

    const int stride = slices + 1;
    for (int stack = 0; stack < stacks; ++stack)
    {
        for (int slice = 0; slice < slices; ++slice)
        {
            const unsigned int first = static_cast<unsigned int>(stack * stride + slice);
            const unsigned int second = first + static_cast<unsigned int>(stride);
            mesh.indices.insert(mesh.indices.end(), {
                first, second, first + 1,
                second, second + 1, first + 1
            });
        }
    }
    return mesh;
}

const Mesh& meshFor(
    heritage::entities::DebugPrimitiveType type,
    const Mesh& box,
    const Mesh& cylinder,
    const Mesh& sphere)
{
    if (type == heritage::entities::DebugPrimitiveType::Cylinder)
        return cylinder;
    if (type == heritage::entities::DebugPrimitiveType::Sphere)
        return sphere;
    return box;
}

} // namespace

bool EntityDebugRenderer::initialize()
{
    shutdown();

    m_program = buildShaderProgram(kVertexShader, kFragmentShader);
    if (!m_program)
        return false;

    m_box = makeBox();
    m_cylinder = makeCylinder(32);
    m_sphere = makeSphere(24, 16);
    uploadMesh(m_box);
    uploadMesh(m_cylinder);
    uploadMesh(m_sphere);

    return m_box.vao != 0 && m_cylinder.vao != 0 && m_sphere.vao != 0;
}

void EntityDebugRenderer::shutdown()
{
    destroyMesh(m_box);
    destroyMesh(m_cylinder);
    destroyMesh(m_sphere);
    if (m_program)
    {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

void EntityDebugRenderer::draw(
    const heritage::entities::EntityRegistry& registry,
    const heritage::math::Mat4& projection,
    const heritage::settings::VideoSettings& videoSettings,
    float elapsedSeconds,
    const heritage::camera::CameraFrame& cameraFrame) const
{
    (void)elapsedSeconds;
    if (!m_program)
        return;

    const std::vector<heritage::entities::DebugPrimitiveInstance> instances =
        registry.debugPrimitiveInstances();
    m_frameStats.instances += static_cast<std::uint64_t>(instances.size());
    if (instances.empty())
        return;

    const heritage::math::Vec3 eye = cameraFrame.valid
        ? cameraFrame.eyeLocal
        : heritage::math::Vec3{ 0.0f, 3.4f, 8.5f };
    const heritage::math::Vec3 cameraTarget = cameraFrame.valid
        ? cameraFrame.targetLocal
        : heritage::math::Vec3{ 0.0f, 1.0f, 0.0f };
    const heritage::math::Vec3 cameraUp = cameraFrame.valid
        ? cameraFrame.up
        : heritage::math::Vec3{ 0.0f, 1.0f, 0.0f };
    const heritage::math::Vec3 cameraRelativeTarget{
        cameraTarget.x - eye.x,
        cameraTarget.y - eye.y,
        cameraTarget.z - eye.z
    };
    const heritage::math::Mat4 view = lookAt(
        { 0.0f, 0.0f, 0.0f },
        cameraRelativeTarget,
        cameraUp);

    glUseProgram(m_program);
    glUniformMatrix4fv(
        glGetUniformLocation(m_program, "uView"),
        1, GL_FALSE, view.m);
    glUniformMatrix4fv(
        glGetUniformLocation(m_program, "uProjection"),
        1, GL_FALSE, projection.m);
    glUniform3f(glGetUniformLocation(m_program, "uEye"), 0.0f, 0.0f, 0.0f);
    glUniform1f(glGetUniformLocation(m_program, "uGamma"), videoSettings.gamma);
    glUniform1f(glGetUniformLocation(m_program, "uBrightness"), videoSettings.brightness);
    glUniform1f(glGetUniformLocation(m_program, "uContrast"), videoSettings.contrast);
    glUniform1f(glGetUniformLocation(m_program, "uSaturation"), videoSettings.saturation);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // A simple floor gives the prototype visual scale without creating a
    // permanent world/terrain system prematurely.
    heritage::entities::DebugPrimitiveInstance floor;
    floor.type = heritage::entities::DebugPrimitiveType::Box;
    floor.position = { -eye.x, 0.38f - eye.y, -eye.z };
    floor.scale = { 13.0f, 0.12f, 13.0f };
    floor.color = { 0.055f, 0.065f, 0.080f };
    const heritage::math::Mat4 floorModel = modelMatrix(floor);
    glUniformMatrix4fv(glGetUniformLocation(m_program, "uModel"), 1, GL_FALSE, floorModel.m);
    glUniform3f(glGetUniformLocation(m_program, "uColor"), floor.color.x, floor.color.y, floor.color.z);
    glBindVertexArray(m_box.vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_box.indices.size()), GL_UNSIGNED_INT, nullptr);
    ++m_frameStats.drawCalls;
    m_frameStats.triangles += static_cast<std::uint64_t>(m_box.indices.size() / 3);

    for (const auto& instance : instances)
    {
        const Mesh& mesh = meshFor(instance.type, m_box, m_cylinder, m_sphere);
        heritage::entities::DebugPrimitiveInstance cameraRelativeInstance = instance;
        cameraRelativeInstance.position = {
            instance.position.x - eye.x,
            instance.position.y - eye.y,
            instance.position.z - eye.z
        };
        const heritage::math::Mat4 model = modelMatrix(cameraRelativeInstance);
        glUniformMatrix4fv(
            glGetUniformLocation(m_program, "uModel"),
            1, GL_FALSE, model.m);
        glUniform3f(
            glGetUniformLocation(m_program, "uColor"),
            instance.color.x,
            instance.color.y,
            instance.color.z);
        glBindVertexArray(mesh.vao);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(mesh.indices.size()),
            GL_UNSIGNED_INT,
            nullptr);
        ++m_frameStats.drawCalls;
        m_frameStats.triangles += static_cast<std::uint64_t>(mesh.indices.size() / 3);
    }

    glBindVertexArray(0);
}

} // namespace heritage::graphics
