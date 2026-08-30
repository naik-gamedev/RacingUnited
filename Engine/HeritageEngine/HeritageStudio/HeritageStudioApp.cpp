#include "HeritageStudioApp.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#include <shellapi.h>
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#endif
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

namespace heritage::studio {
namespace {

constexpr int kWindowWidth = 1500;
constexpr int kWindowHeight = 920;
constexpr float kStudioTitlebarHeight = 31.0f;
constexpr int kStudioMinimumWidth = 980;
constexpr int kStudioMinimumHeight = 620;
ImFont* g_headingFont = nullptr;

constexpr const char* kStudioRuntimeSpawnSection = "entity:heritage_studio_vehicle_spawn";
constexpr const char* kStudioRuntimeSpawnName = "Heritage Studio Vehicle Spawn";

#ifdef _WIN32
void applyDarkNativeTitleBar(GLFWwindow* window)
{
    if (!window)
        return;

    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd)
        return;

    // Windows 10 1809+ understands the immersive-dark caption request. The
    // numeric fallback keeps this source compatible with older Windows SDKs.
    // Windows 11 additionally accepts explicit caption/text/border colours,
    // giving Heritage Studio a true black non-client title bar when supported.
    const BOOL dark = TRUE;
#ifdef DWMWA_USE_IMMERSIVE_DARK_MODE
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
#else
    constexpr DWORD kUseImmersiveDarkMode = 20;
    if (FAILED(DwmSetWindowAttribute(hwnd, kUseImmersiveDarkMode, &dark, sizeof(dark))))
    {
        constexpr DWORD kUseImmersiveDarkModeBefore20H1 = 19;
        DwmSetWindowAttribute(hwnd, kUseImmersiveDarkModeBefore20H1, &dark, sizeof(dark));
    }
#endif

    constexpr DWORD kCaptionColor = 35;
    constexpr DWORD kTextColor = 36;
    constexpr DWORD kBorderColor = 34;
    const COLORREF black = RGB(0, 0, 0);
    const COLORREF nearBlack = RGB(16, 16, 16);
    const COLORREF white = RGB(240, 240, 240);
    DwmSetWindowAttribute(hwnd, kCaptionColor, &black, sizeof(black));
    DwmSetWindowAttribute(hwnd, kTextColor, &white, sizeof(white));
    DwmSetWindowAttribute(hwnd, kBorderColor, &nearBlack, sizeof(nearBlack));
}
#endif

std::string trimCopy(const std::string& value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
    if (first == value.end())
        return {};
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    return std::string(first, last);
}

std::string lowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::filesystem::path moduleEntryScenePath(const std::filesystem::path& moduleRoot)
{
    std::string entryScene = "prototype";
    std::ifstream manifest(moduleRoot / "module.ini");
    std::string line;
    while (std::getline(manifest, line))
    {
        const std::size_t comment = line.find_first_of("#;");
        if (comment != std::string::npos)
            line.erase(comment);
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos)
            continue;
        const std::string key = lowerCopy(trimCopy(line.substr(0, equals)));
        if (key != "entry_scene")
            continue;
        std::string value = trimCopy(line.substr(equals + 1));
        if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\'')))
            value = value.substr(1, value.size() - 2);
        if (!value.empty())
            entryScene = value;
        break;
    }
    return moduleRoot / "Scenes" / (entryScene + ".hscene");
}

bool parseVec3Property(const std::string& text, authoring::Vec3& value)
{
    std::string normalized = text;
    std::replace(normalized.begin(), normalized.end(), ',', ' ');
    std::istringstream input(normalized);
    return static_cast<bool>(input >> value.x >> value.y >> value.z);
}

std::string formatVec3Property(const authoring::Vec3& value)
{
    char buffer[192]{};
    std::snprintf(buffer, sizeof(buffer), "%.9g, %.9g, %.9g", value.x, value.y, value.z);
    return buffer;
}

std::string luaQuote(const std::string& value)
{
    std::string out;
    out.reserve(value.size() + 8);
    out.push_back('"');
    for (const char ch : value)
    {
        switch (ch)
        {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out.push_back(ch); break;
        }
    }
    out.push_back('"');
    return out;
}

bool readStudioRuntimeSpawn(
    const std::filesystem::path& file,
    authoring::Vec3& position,
    authoring::Vec3& rotation,
    std::string& message)
{
    std::ifstream input(file);
    if (!input)
    {
        message = "Could not read runtime scene: " + file.string();
        return false;
    }

    bool inSpawnSection = false;
    bool foundSection = false;
    bool foundPosition = false;
    std::string line;
    while (std::getline(input, line))
    {
        const std::string trimmed = trimCopy(line);
        if (trimmed.size() >= 2 && trimmed.front() == '[' && trimmed.back() == ']')
        {
            const std::string section = lowerCopy(trimCopy(trimmed.substr(1, trimmed.size() - 2)));
            inSpawnSection = section == kStudioRuntimeSpawnSection;
            foundSection |= inSpawnSection;
            continue;
        }
        if (!inSpawnSection)
            continue;

        const std::size_t equals = trimmed.find('=');
        if (equals == std::string::npos)
            continue;
        const std::string key = lowerCopy(trimCopy(trimmed.substr(0, equals)));
        const std::string value = trimCopy(trimmed.substr(equals + 1));
        if (key == "position")
        {
            if (!parseVec3Property(value, position))
            {
                message = "Runtime vehicle spawn has an invalid position in " + file.string();
                return false;
            }
            foundPosition = true;
        }
        else if (key == "rotation" && !parseVec3Property(value, rotation))
        {
            message = "Runtime vehicle spawn has an invalid rotation in " + file.string();
            return false;
        }
    }

    if (!foundSection || !foundPosition)
    {
        message = "Runtime scene has no Heritage Studio vehicle spawn yet; GLB-authored spawn remains active.";
        return false;
    }

    message = "Loaded runtime vehicle spawn from " + file.string();
    return true;
}

bool writeStudioRuntimeSpawn(
    const std::filesystem::path& file,
    const authoring::Vec3& position,
    const authoring::Vec3& rotation,
    std::string& message)
{
    std::ifstream input(file);
    if (!input)
    {
        message = "Could not read runtime scene before saving: " + file.string();
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line))
        lines.push_back(line);

    std::size_t sectionBegin = lines.size();
    std::size_t sectionEnd = lines.size();
    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        const std::string trimmed = trimCopy(lines[i]);
        if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']')
            continue;
        const std::string section = lowerCopy(trimCopy(trimmed.substr(1, trimmed.size() - 2)));
        if (section == kStudioRuntimeSpawnSection)
        {
            sectionBegin = i;
            sectionEnd = lines.size();
            for (std::size_t j = i + 1; j < lines.size(); ++j)
            {
                const std::string candidate = trimCopy(lines[j]);
                if (candidate.size() >= 2 && candidate.front() == '[' && candidate.back() == ']')
                {
                    sectionEnd = j;
                    break;
                }
            }
            break;
        }
    }

    const std::vector<std::string> spawnBlock{
        "[entity:heritage_studio_vehicle_spawn]",
        std::string("name = ") + kStudioRuntimeSpawnName,
        "tags = VehicleSpawn, HeritageStudio",
        "position = " + formatVec3Property(position),
        "rotation = " + formatVec3Property(rotation),
        "scale = 1.0, 1.0, 1.0",
        ""
    };

    if (sectionBegin < lines.size())
    {
        lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(sectionBegin),
            lines.begin() + static_cast<std::ptrdiff_t>(sectionEnd));
        lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(sectionBegin), spawnBlock.begin(), spawnBlock.end());
    }
    else
    {
        if (!lines.empty() && !lines.back().empty())
            lines.emplace_back();
        lines.insert(lines.end(), spawnBlock.begin(), spawnBlock.end());
    }

    const std::filesystem::path temp = file.string() + ".heritage-studio.tmp";
    std::ofstream output(temp, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        message = "Could not create temporary runtime scene: " + temp.string();
        return false;
    }
    for (const auto& outputLine : lines)
        output << outputLine << '\n';
    output.close();
    if (!output)
    {
        std::error_code cleanup;
        std::filesystem::remove(temp, cleanup);
        message = "Could not finish writing runtime scene: " + temp.string();
        return false;
    }

    std::error_code copyError;
    std::filesystem::copy_file(temp, file, std::filesystem::copy_options::overwrite_existing, copyError);
    std::error_code cleanup;
    std::filesystem::remove(temp, cleanup);
    if (copyError)
    {
        message = "Could not replace runtime scene: " + copyError.message();
        return false;
    }

    message = "Saved runtime vehicle spawn to " + file.string();
    return true;
}


void headingText(const char* text)
{
    if (g_headingFont)
        ImGui::PushFont(g_headingFont);
    ImGui::TextUnformatted(text);
    if (g_headingFont)
        ImGui::PopFont();
}

bool inputString(const char* label, std::string& value, std::size_t capacity = 512)
{
    capacity = std::clamp<std::size_t>(capacity, 32, 2048);
    std::array<char, 2048> buffer{};
    std::snprintf(buffer.data(), capacity, "%s", value.c_str());
    if (!ImGui::InputText(label, buffer.data(), capacity))
        return false;
    value = buffer.data();
    return true;
}

const char* workspaceName(Workspace workspace)
{
    switch (workspace)
    {
    case Workspace::Scene: return "SCENE";
    case Workspace::Race: return "RACE";
    case Workspace::Traffic: return "TRAFFIC";
    case Workspace::Gameplay: return "GAMEPLAY";
    case Workspace::Weather: return "WEATHER";
    case Workspace::Vehicle: return "VEHICLE";
    case Workspace::Audio: return "AUDIO";
    case Workspace::Assets: return "ASSETS";
    }
    return "UNKNOWN";
}

bool workspaceButton(const char* label, Workspace value, Workspace& current)
{
    const bool selected = current == value;
    if (selected)
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    const bool clicked = ImGui::Button(label, ImVec2(-1.0f, 44.0f));
    if (selected)
        ImGui::PopStyleColor();
    if (clicked)
        current = value;
    return clicked;
}

bool slider(const char* label, float* value, float minimum, float maximum, const char* format = "%.2f")
{
    ImGui::SetNextItemWidth(-1.0f);
    return ImGui::SliderFloat(label, value, minimum, maximum, format);
}

void sectionTitle(const char* text)
{
    ImGui::Spacing();
    headingText(text);
    ImGui::Separator();
}

bool beginParameterTable(const char* id)
{
    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_SizingStretchProp;
    if (!ImGui::BeginTable(id, 3, flags))
        return false;

    ImGui::TableSetupColumn("PARAMETER", ImGuiTableColumnFlags_WidthFixed, 190.0f);
    ImGui::TableSetupColumn("VALUE", ImGuiTableColumnFlags_WidthStretch, 1.25f);
    ImGui::TableSetupColumn("DESCRIPTION", ImGuiTableColumnFlags_WidthStretch, 1.65f);
    ImGui::TableHeadersRow();
    return true;
}

bool describedSlider(const char* id, const char* label, const char* description,
    float* value, float minimum, float maximum, const char* format)
{
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    if (description && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", description);

    ImGui::TableSetColumnIndex(1);
    ImGui::PushID(id);
    ImGui::SetNextItemWidth(-1.0f);
    const bool changed = ImGui::SliderFloat("##value", value, minimum, maximum, format);
    if (description && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", description);
    ImGui::PopID();

    ImGui::TableSetColumnIndex(2);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", description ? description : "");
    ImGui::PopTextWrapPos();
    return changed;
}


constexpr float kPi = 3.14159265358979323846f;

float radians(float degrees)
{
    return degrees * (kPi / 180.0f);
}

authoring::Vec3 add3(const authoring::Vec3& a, const authoring::Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

authoring::Vec3 sub3(const authoring::Vec3& a, const authoring::Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

authoring::Vec3 mul3(const authoring::Vec3& v, float s)
{
    return { v.x * s, v.y * s, v.z * s };
}

float dot3(const authoring::Vec3& a, const authoring::Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

authoring::Vec3 cross3(const authoring::Vec3& a, const authoring::Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float length3(const authoring::Vec3& v)
{
    return std::sqrt(dot3(v, v));
}

authoring::Vec3 normalize3(const authoring::Vec3& v)
{
    const float length = length3(v);
    if (length < 1.0e-6f)
        return { 0.0f, 0.0f, 0.0f };
    return mul3(v, 1.0f / length);
}

struct StudioViewportProjection
{
    ImVec2 min{};
    ImVec2 max{};
    ImVec2 size{};
    authoring::Vec3 camera{};
    authoring::Vec3 target{};
    authoring::Vec3 forward{};
    authoring::Vec3 right{};
    authoring::Vec3 up{};
    float fovYRad = radians(52.0f);
    float aspect = 1.0f;
    bool orthographic = false;
    float orthoHalfHeight = 10.0f;
};

StudioViewportProjection makeViewportProjection(
    const ImVec2& min,
    const ImVec2& max,
    float yawDeg,
    float pitchDeg,
    float distance,
    const authoring::Vec3& target,
    bool orthographic)
{
    StudioViewportProjection view;
    view.min = min;
    view.max = max;
    view.size = { std::max(1.0f, max.x - min.x), std::max(1.0f, max.y - min.y) };
    view.aspect = view.size.x / view.size.y;
    view.target = target;
    view.orthographic = orthographic;
    view.orthoHalfHeight = std::max(0.5f, distance * 0.48f);

    const float yaw = radians(yawDeg);
    const float pitch = radians(pitchDeg);
    const float cp = std::cos(pitch);
    const authoring::Vec3 offset{
        cp * std::sin(yaw) * distance,
        std::sin(pitch) * distance,
        cp * std::cos(yaw) * distance
    };
    view.camera = add3(target, offset);
    view.forward = normalize3(sub3(target, view.camera));
    const authoring::Vec3 worldUp{ 0.0f, 1.0f, 0.0f };
    view.right = normalize3(cross3(view.forward, worldUp));
    if (length3(view.right) < 0.1f)
        view.right = { 1.0f, 0.0f, 0.0f };
    view.up = normalize3(cross3(view.right, view.forward));
    return view;
}

bool projectWorldPoint(const StudioViewportProjection& view, const authoring::Vec3& world, ImVec2& screen)
{
    const authoring::Vec3 rel = sub3(world, view.camera);
    const float z = dot3(rel, view.forward);
    if (z <= 0.05f)
        return false;

    const float x = dot3(rel, view.right);
    const float y = dot3(rel, view.up);
    const float tanHalf = std::tan(view.fovYRad * 0.5f);
    const float ndcX = view.orthographic
        ? x / (view.orthoHalfHeight * view.aspect)
        : x / (z * tanHalf * view.aspect);
    const float ndcY = view.orthographic
        ? y / view.orthoHalfHeight
        : y / (z * tanHalf);
    if (std::abs(ndcX) > 4.0f || std::abs(ndcY) > 4.0f)
        return false;

    screen.x = view.min.x + (ndcX * 0.5f + 0.5f) * view.size.x;
    screen.y = view.min.y + (-ndcY * 0.5f + 0.5f) * view.size.y;
    return true;
}

bool projectWorldPointUnbounded(const StudioViewportProjection& view, const authoring::Vec3& world, ImVec2& screen)
{
    const authoring::Vec3 rel = sub3(world, view.camera);
    const float z = dot3(rel, view.forward);
    if (z <= 0.05f)
        return false;

    const float x = dot3(rel, view.right);
    const float y = dot3(rel, view.up);
    const float tanHalf = std::tan(view.fovYRad * 0.5f);
    const float ndcX = view.orthographic
        ? x / (view.orthoHalfHeight * view.aspect)
        : x / (z * tanHalf * view.aspect);
    const float ndcY = view.orthographic
        ? y / view.orthoHalfHeight
        : y / (z * tanHalf);

    screen.x = view.min.x + (ndcX * 0.5f + 0.5f) * view.size.x;
    screen.y = view.min.y + (-ndcY * 0.5f + 0.5f) * view.size.y;
    return std::isfinite(screen.x) && std::isfinite(screen.y);
}

bool clipScreenLineToViewport(const StudioViewportProjection& view, ImVec2& a, ImVec2& b)
{
    float t0 = 0.0f;
    float t1 = 1.0f;
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const auto clip = [&](float p, float q)
    {
        if (std::abs(p) < 1.0e-8f)
            return q >= 0.0f;
        const float r = q / p;
        if (p < 0.0f)
        {
            if (r > t1) return false;
            if (r > t0) t0 = r;
        }
        else
        {
            if (r < t0) return false;
            if (r < t1) t1 = r;
        }
        return true;
    };

    if (!clip(-dx, a.x - view.min.x)
        || !clip(dx, view.max.x - a.x)
        || !clip(-dy, a.y - view.min.y)
        || !clip(dy, view.max.y - a.y))
        return false;

    const ImVec2 originalA = a;
    a = { originalA.x + dx * t0, originalA.y + dy * t0 };
    b = { originalA.x + dx * t1, originalA.y + dy * t1 };
    return true;
}

bool projectWorldSegmentToViewport(const StudioViewportProjection& view,
    authoring::Vec3 a, authoring::Vec3 b, ImVec2& screenA, ImVec2& screenB)
{
    constexpr float nearZ = 0.051f;
    float za = dot3(sub3(a, view.camera), view.forward);
    float zb = dot3(sub3(b, view.camera), view.forward);
    if (za <= nearZ && zb <= nearZ)
        return false;

    if (za <= nearZ || zb <= nearZ)
    {
        const float denominator = zb - za;
        if (std::abs(denominator) < 1.0e-8f)
            return false;
        const float t = std::clamp((nearZ - za) / denominator, 0.0f, 1.0f);
        const authoring::Vec3 clipped = add3(a, mul3(sub3(b, a), t));
        if (za <= nearZ)
        {
            a = clipped;
            za = nearZ;
        }
        else
        {
            b = clipped;
            zb = nearZ;
        }
    }

    if (!projectWorldPointUnbounded(view, a, screenA) || !projectWorldPointUnbounded(view, b, screenB))
        return false;
    return clipScreenLineToViewport(view, screenA, screenB);
}

void drawWorldSegmentClipped(ImDrawList* draw, const StudioViewportProjection& view,
    const authoring::Vec3& a, const authoring::Vec3& b, ImU32 color, float thickness)
{
    ImVec2 screenA{}, screenB{};
    if (projectWorldSegmentToViewport(view, a, b, screenA, screenB))
        draw->AddLine(screenA, screenB, color, thickness);
}

bool viewportGroundPoint(const StudioViewportProjection& view, const ImVec2& mouse, authoring::Vec3& world)
{
    const float ndcX = ((mouse.x - view.min.x) / view.size.x) * 2.0f - 1.0f;
    const float ndcY = 1.0f - ((mouse.y - view.min.y) / view.size.y) * 2.0f;
    const float tanHalf = std::tan(view.fovYRad * 0.5f);
    authoring::Vec3 rayOrigin = view.camera;
    authoring::Vec3 ray = view.forward;
    if (view.orthographic)
    {
        rayOrigin = add3(rayOrigin, mul3(view.right, ndcX * view.orthoHalfHeight * view.aspect));
        rayOrigin = add3(rayOrigin, mul3(view.up, ndcY * view.orthoHalfHeight));
    }
    else
    {
        ray = add3(ray, mul3(view.right, ndcX * view.aspect * tanHalf));
        ray = add3(ray, mul3(view.up, ndcY * tanHalf));
        ray = normalize3(ray);
    }
    if (std::abs(ray.y) < 1.0e-5f)
        return false;
    const float t = -rayOrigin.y / ray.y;
    if (t <= 0.0f)
        return false;
    world = add3(rayOrigin, mul3(ray, t));
    world.y = 0.0f;
    return true;
}

void drawViewportGrid(ImDrawList* draw, const StudioViewportProjection& view, float centerX, float centerZ);

bool viewportRay(const StudioViewportProjection& view, const ImVec2& mouse,
    authoring::Vec3& rayOrigin, authoring::Vec3& rayDirection)
{
    const float ndcX = ((mouse.x - view.min.x) / view.size.x) * 2.0f - 1.0f;
    const float ndcY = 1.0f - ((mouse.y - view.min.y) / view.size.y) * 2.0f;
    const float tanHalf = std::tan(view.fovYRad * 0.5f);
    rayOrigin = view.camera;
    rayDirection = view.forward;
    if (view.orthographic)
    {
        rayOrigin = add3(rayOrigin, mul3(view.right, ndcX * view.orthoHalfHeight * view.aspect));
        rayOrigin = add3(rayOrigin, mul3(view.up, ndcY * view.orthoHalfHeight));
    }
    else
    {
        rayDirection = add3(rayDirection, mul3(view.right, ndcX * view.aspect * tanHalf));
        rayDirection = add3(rayDirection, mul3(view.up, ndcY * tanHalf));
        rayDirection = normalize3(rayDirection);
    }
    return length3(rayDirection) > 0.5f;
}

bool viewportAuthoringPoint(const StudioViewportProjection& view, const ImVec2& mouse,
    const StudioScenePreview* preview, authoring::Vec3& world)
{
    if (preview && preview->loaded() && preview->visible())
    {
        authoring::Vec3 origin{};
        authoring::Vec3 direction{};
        if (viewportRay(view, mouse, origin, direction) && preview->raycast(origin, direction, world))
            return true;
    }
    return viewportGroundPoint(view, mouse, world);
}

StudioPreviewCamera previewCamera(const StudioViewportProjection& view)
{
    StudioPreviewCamera camera;
    camera.position = view.camera;
    camera.target = view.target;
    camera.right = view.right;
    camera.up = view.up;
    camera.forward = view.forward;
    camera.fovYRadians = view.fovYRad;
    camera.aspect = view.aspect;
    camera.orthographic = view.orthographic;
    camera.orthoHalfHeight = view.orthoHalfHeight;
    return camera;
}

void drawFlyNavigationOverlay(ImDrawList* draw, const StudioViewportProjection& view, const StudioViewportFlyState& fly)
{
    if (!draw || !fly.active)
        return;
    draw->PushClipRect(view.min, view.max, true);
    draw->AddRectFilled(ImVec2(view.min.x + 12.0f, view.min.y + 12.0f), ImVec2(view.min.x + 565.0f, view.min.y + 60.0f), IM_COL32(16, 20, 26, 235), 4.0f);
    char flyText[256]{};
    std::snprintf(flyText, sizeof(flyText), "FLY NAVIGATION  %.2f m/s  |  WASD move | Q/E down/up | wheel speed | Shift fast | Alt slow", fly.speedMps);
    draw->AddText(ImVec2(view.min.x + 20.0f, view.min.y + 20.0f), IM_COL32(235, 238, 242, 255), flyText);
    draw->AddText(ImVec2(view.min.x + 20.0f, view.min.y + 39.0f), IM_COL32(150, 165, 180, 255), "LMB/Enter confirm | RMB/Esc cancel and restore previous view");
    draw->PopClipRect();
}

void drawSceneGeometryBackdrop(ImDrawList* draw, const StudioViewportProjection& view,
    StudioScenePreview* preview, bool gridVisible)
{
    if (!preview || !preview->loaded() || !preview->visible())
        return;

    const GLuint texture = preview->render(
        static_cast<int>(std::max(1.0f, view.size.x)),
        static_cast<int>(std::max(1.0f, view.size.y)),
        previewCamera(view), gridVisible);
    if (!texture)
        return;

    draw->PushClipRect(view.min, view.max, true);
    draw->AddImage(
        static_cast<ImTextureID>(texture),
        view.min, view.max, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    // Grid + world axes are already rendered depth-aware inside the
    // Scene GLB framebuffer (STUDIO31). Do not draw a second always-on-top
    // ImGui lattice over the scene.
    draw->AddRect(view.min, view.max, IM_COL32(75, 88, 100, 255), 0.0f, 0, 1.0f);
    draw->AddText(ImVec2(view.min.x + 10.0f, view.max.y - 22.0f), IM_COL32(175, 185, 195, 240),
        "Scene GLB + PBR preview | Shift+` fly | Click placement snaps to visible scene geometry");
    draw->PopClipRect();
}

float pointSegmentDistance(const ImVec2& p, const ImVec2& a, const ImVec2& b)
{
    const float vx = b.x - a.x;
    const float vy = b.y - a.y;
    const float wx = p.x - a.x;
    const float wy = p.y - a.y;
    const float vv = vx * vx + vy * vy;
    if (vv <= 1.0e-6f)
        return std::sqrt(wx * wx + wy * wy);
    const float t = std::clamp((wx * vx + wy * vy) / vv, 0.0f, 1.0f);
    const float dx = p.x - (a.x + vx * t);
    const float dy = p.y - (a.y + vy * t);
    return std::sqrt(dx * dx + dy * dy);
}

float snapValue(float value, bool enabled, float step)
{
    if (!enabled || step <= 0.0001f)
        return value;
    return std::round(value / step) * step;
}

void drawViewportGrid(ImDrawList* draw, const StudioViewportProjection& view, float centerX, float centerZ)
{
    // STUDIO31 fallback for an empty/hidden Scene GLB viewport.  The real
    // scene-backed viewport uses StudioScenePreview's procedural depth-aware
    // floor shader.  Here we keep only one screen-appropriate world scale at a
    // time so an empty viewport remains useful without STUDIO30's overlapping
    // six-LOD perspective comb.
    constexpr float baseStepM = 5.0f;
    constexpr float infiniteHalfExtentM = 1000000.0f;
    constexpr int halfLines = 70;
    constexpr int majorEvery = 5;

    const float viewDistance = std::max(0.1f, length3(sub3(view.camera, view.target)));
    const float metresPerPixel = view.orthographic
        ? (2.0f * view.orthoHalfHeight) / std::max(1.0f, view.size.y)
        : (2.0f * viewDistance * std::tan(view.fovYRad * 0.5f)) / std::max(1.0f, view.size.y);
    const float desiredStep = std::max(baseStepM, metresPerPixel * 24.0f);
    int level = static_cast<int>(std::floor(std::log(desiredStep / baseStepM) / std::log(5.0f)));
    level = std::clamp(level, 0, 8);
    const float stepM = baseStepM * std::pow(5.0f, static_cast<float>(level));
    const float pixelsPerStep = stepM / std::max(metresPerPixel, 1.0e-5f);
    const float fineFade = std::clamp((pixelsPerStep - 5.0f) / 15.0f, 0.0f, 1.0f);

    const ImU32 minorColor = IM_COL32(48, 58, 66, static_cast<int>(55.0f + 95.0f * fineFade));
    const ImU32 majorColor = IM_COL32(72, 84, 94, 195);
    const long long baseCellX = static_cast<long long>(std::floor(static_cast<double>(centerX) / stepM));
    const long long baseCellZ = static_cast<long long>(std::floor(static_cast<double>(centerZ) / stepM));

    for (int i = -halfLines; i <= halfLines; ++i)
    {
        const long long cellX = baseCellX + i;
        const float x = static_cast<float>(static_cast<double>(cellX) * stepM);
        const bool majorX = cellX % majorEvery == 0;
        drawWorldSegmentClipped(draw, view,
            { x, 0.0f, centerZ - infiniteHalfExtentM },
            { x, 0.0f, centerZ + infiniteHalfExtentM },
            majorX ? majorColor : minorColor, majorX ? 1.2f : 1.0f);

        const long long cellZ = baseCellZ + i;
        const float z = static_cast<float>(static_cast<double>(cellZ) * stepM);
        const bool majorZ = cellZ % majorEvery == 0;
        drawWorldSegmentClipped(draw, view,
            { centerX - infiniteHalfExtentM, 0.0f, z },
            { centerX + infiniteHalfExtentM, 0.0f, z },
            majorZ ? majorColor : minorColor, majorZ ? 1.2f : 1.0f);
    }

    drawWorldSegmentClipped(draw, view,
        { -infiniteHalfExtentM, 0.01f, 0.0f }, { infiniteHalfExtentM, 0.01f, 0.0f },
        IM_COL32(170, 72, 72, 230), 2.0f);
    drawWorldSegmentClipped(draw, view,
        { 0.0f, 0.01f, -infiniteHalfExtentM }, { 0.0f, 0.01f, infiniteHalfExtentM },
        IM_COL32(72, 116, 185, 230), 2.0f);
}

void drawMarker(ImDrawList* draw, const ImVec2& screen, ImU32 color, bool selected, const char* label)
{
    const float radius = selected ? 7.0f : 5.0f;
    draw->AddCircleFilled(screen, radius, color, 16);
    draw->AddCircle(screen, radius + 2.0f, selected ? IM_COL32(255, 255, 255, 245) : IM_COL32(20, 20, 20, 220), 16, selected ? 2.0f : 1.0f);
    if (selected && label && *label)
        draw->AddText(ImVec2(screen.x + 10.0f, screen.y - 9.0f), IM_COL32(235, 238, 242, 255), label);
}

bool drawMoveGizmo(
    ImDrawList* draw,
    const StudioViewportProjection& view,
    authoring::Vec3& position,
    bool hovered,
    bool snapEnabled,
    float snapStep,
    int& activeAxis,
    float& dragAccumulator)
{
    ImVec2 origin{};
    if (!projectWorldPoint(view, position, origin))
        return false;

    const float axisLength = std::clamp(length3(sub3(view.camera, position)) * 0.13f, 1.5f, 12.0f);
    const authoring::Vec3 ends3[3] = {
        add3(position, { axisLength, 0.0f, 0.0f }),
        add3(position, { 0.0f, axisLength, 0.0f }),
        add3(position, { 0.0f, 0.0f, axisLength })
    };
    ImVec2 ends[3]{};
    const ImU32 colors[3] = {
        IM_COL32(230, 75, 70, 255),
        IM_COL32(88, 210, 105, 255),
        IM_COL32(75, 130, 235, 255)
    };
    bool projected[3]{};
    for (int axis = 0; axis < 3; ++axis)
    {
        projected[axis] = projectWorldPoint(view, ends3[axis], ends[axis]);
        if (projected[axis])
        {
            draw->AddLine(origin, ends[axis], colors[axis], activeAxis == axis ? 4.0f : 2.4f);
            draw->AddCircleFilled(ends[axis], activeAxis == axis ? 5.0f : 4.0f, colors[axis], 10);
        }
    }

    const ImGuiIO& io = ImGui::GetIO();
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && activeAxis < 0)
    {
        float best = 9.0f;
        for (int axis = 0; axis < 3; ++axis)
        {
            if (!projected[axis])
                continue;
            const float distance = pointSegmentDistance(io.MousePos, origin, ends[axis]);
            if (distance < best)
            {
                best = distance;
                activeAxis = axis;
            }
        }
        if (activeAxis >= 0)
            dragAccumulator = activeAxis == 0 ? position.x : (activeAxis == 1 ? position.y : position.z);
    }

    bool changed = false;
    if (activeAxis >= 0)
    {
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            activeAxis = -1;
        }
        else if (projected[activeAxis])
        {
            const ImVec2 axisScreen{ ends[activeAxis].x - origin.x, ends[activeAxis].y - origin.y };
            const float axisPixels = std::sqrt(axisScreen.x * axisScreen.x + axisScreen.y * axisScreen.y);
            if (axisPixels > 1.0f)
            {
                const float ux = axisScreen.x / axisPixels;
                const float uy = axisScreen.y / axisPixels;
                const float pixels = io.MouseDelta.x * ux + io.MouseDelta.y * uy;
                const float worldPerPixel = (2.0f * length3(sub3(view.camera, position)) * std::tan(view.fovYRad * 0.5f)) / std::max(1.0f, view.size.y);
                dragAccumulator += pixels * worldPerPixel;
                const float value = snapValue(dragAccumulator, snapEnabled, snapStep);
                if (activeAxis == 0) position.x = value;
                if (activeAxis == 1) position.y = value;
                if (activeAxis == 2) position.z = value;
                changed = true;
            }
        }
    }
    return changed;
}


StudioViewportProjection prepareInteractiveViewport(
    GLFWwindow* window,
    StudioViewportFlyState& fly,
    const char* id,
    const ImVec2& size,
    float& yawDeg,
    float& pitchDeg,
    float& distance,
    authoring::Vec3& target,
    bool gridVisible,
    bool& orthographic,
    bool& hovered)
{
    const ImVec2 min = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, size);
    const bool itemHovered = ImGui::IsItemHovered();
    const ImVec2 max{ min.x + size.x, min.y + size.y };

    ImGuiIO& io = ImGui::GetIO();
    bool flyCapturedThisFrame = fly.active;
    const bool flyHotkey = itemHovered && !io.WantTextInput && io.KeyShift
        && ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false);

    if (flyHotkey && !fly.active)
    {
        fly.active = true;
        flyCapturedThisFrame = true;
        fly.startYawDeg = yawDeg;
        fly.startPitchDeg = pitchDeg;
        fly.startDistanceM = distance;
        fly.startTarget = target;
        fly.startOrthographic = orthographic;
        orthographic = false;
        if (window)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
#ifdef GLFW_RAW_MOUSE_MOTION
            if (glfwRawMouseMotionSupported())
                glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
#endif
        }
    }

    if (fly.active)
    {
        flyCapturedThisFrame = true;

        // Blender-style modal Walk/Fly navigation: camera location stays fixed
        // while mouse-look rotates the view, then WASD/QE translates camera and
        // target together. The existing orbit representation is preserved so
        // leaving fly mode returns seamlessly to normal MMB navigation.
        StudioViewportProjection viewBefore = makeViewportProjection(min, max, yawDeg, pitchDeg, distance, target, false);
        const authoring::Vec3 fixedCamera = viewBefore.camera;
        yawDeg -= io.MouseDelta.x * 0.12f;
        pitchDeg = std::clamp(pitchDeg + io.MouseDelta.y * 0.12f, -89.0f, 89.0f);
        StudioViewportProjection rotated = makeViewportProjection(min, max, yawDeg, pitchDeg, distance, target, false);
        target = sub3(fixedCamera, sub3(rotated.camera, target));

        if (io.MouseWheel != 0.0f)
            fly.speedMps = std::clamp(fly.speedMps * std::pow(1.22f, io.MouseWheel), 0.05f, 5000.0f);

        StudioViewportProjection movementView = makeViewportProjection(min, max, yawDeg, pitchDeg, distance, target, false);
        authoring::Vec3 move{};
        if (ImGui::IsKeyDown(ImGuiKey_W)) move = add3(move, movementView.forward);
        if (ImGui::IsKeyDown(ImGuiKey_S)) move = sub3(move, movementView.forward);
        if (ImGui::IsKeyDown(ImGuiKey_D)) move = add3(move, movementView.right);
        if (ImGui::IsKeyDown(ImGuiKey_A)) move = sub3(move, movementView.right);
        const authoring::Vec3 worldUp{ 0.0f, 1.0f, 0.0f };
        if (ImGui::IsKeyDown(ImGuiKey_E)) move = add3(move, worldUp);
        if (ImGui::IsKeyDown(ImGuiKey_Q)) move = sub3(move, worldUp);
        if (length3(move) > 0.001f)
        {
            float speed = fly.speedMps;
            if (io.KeyShift) speed *= 3.0f;
            if (io.KeyAlt) speed *= 0.25f;
            const float dt = std::clamp(io.DeltaTime, 0.0f, 0.1f);
            target = add3(target, mul3(normalize3(move), speed * dt));
        }

        const bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false)
            || ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        const bool confirm = ImGui::IsKeyPressed(ImGuiKey_Enter, false)
            || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)
            || ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        if (cancel || confirm)
        {
            if (cancel)
            {
                yawDeg = fly.startYawDeg;
                pitchDeg = fly.startPitchDeg;
                distance = fly.startDistanceM;
                target = fly.startTarget;
                orthographic = fly.startOrthographic;
            }
            if (window)
            {
#ifdef GLFW_RAW_MOUSE_MOTION
                if (glfwRawMouseMotionSupported())
                    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
#endif
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            fly.active = false;
        }
    }
    else if (itemHovered)
    {
        // Blender 5.2 default navigation: MMB orbit, Shift+MMB pan,
        // Ctrl+MMB dolly/zoom, wheel zoom. RMB remains available for context/cancel.
        if (io.MouseWheel != 0.0f)
            distance = std::clamp(distance * std::pow(0.88f, io.MouseWheel), 2.0f, 2500.0f);

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) && io.KeyCtrl)
        {
            distance = std::clamp(distance * std::pow(1.008f, io.MouseDelta.y), 2.0f, 2500.0f);
        }
        else if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) && !io.KeyShift)
        {
            yawDeg -= io.MouseDelta.x * 0.28f;
            pitchDeg = std::clamp(pitchDeg - io.MouseDelta.y * 0.24f, -88.0f, 88.0f);
        }
    }

    StudioViewportProjection view = makeViewportProjection(min, max, yawDeg, pitchDeg, distance, target, orthographic);
    if (!fly.active && itemHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle) && io.KeyShift && !io.KeyCtrl)
    {
        const float worldPerPixel = view.orthographic
            ? (2.0f * view.orthoHalfHeight) / std::max(1.0f, view.size.y)
            : (2.0f * distance * std::tan(view.fovYRad * 0.5f)) / std::max(1.0f, view.size.y);
        target = add3(target, mul3(view.right, -io.MouseDelta.x * worldPerPixel));
        target = add3(target, mul3(view.up, io.MouseDelta.y * worldPerPixel));
        view = makeViewportProjection(min, max, yawDeg, pitchDeg, distance, target, orthographic);
    }

    if (!fly.active && itemHovered)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Keypad1, false)) { yawDeg = 180.0f; pitchDeg = 0.0f; orthographic = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_Keypad3, false)) { yawDeg = -90.0f; pitchDeg = 0.0f; orthographic = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_Keypad7, false)) { yawDeg = 0.0f; pitchDeg = 89.0f; orthographic = true; }
        if (ImGui::IsKeyPressed(ImGuiKey_Keypad5, false)) orthographic = !orthographic;
        view = makeViewportProjection(min, max, yawDeg, pitchDeg, distance, target, orthographic);
    }

    // Suppress selection/placement/transform actions while the modal fly tool
    // owns the mouse. The final confirm/cancel click is consumed for this frame
    // too, matching Blender instead of accidentally selecting an object.
    hovered = itemHovered && !flyCapturedThisFrame && !fly.active;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(min, max, true);
    draw->AddRectFilled(min, max, IM_COL32(12, 16, 21, 255));
    if (gridVisible)
        drawViewportGrid(draw, view, target.x, target.z);
    draw->AddRect(min, max, IM_COL32(75, 88, 100, 255), 0.0f, 0, 1.0f);
    draw->AddText(ImVec2(min.x + 10.0f, max.y - 22.0f), IM_COL32(150, 160, 170, 235),
        "Blender: Shift+` fly | MMB orbit | Shift+MMB pan | Ctrl+MMB/wheel zoom | Num1/3/7 views | Num5 perspective/ortho | G/R/S transform");
    draw->PopClipRect();
    return view;
}

void drawViewportToolbar(
    bool& gridVisible,
    bool& snapEnabled,
    float& snapStep,
    float& yawDeg,
    float& pitchDeg,
    float& distance,
    bool& orthographic)
{
    ImGui::TextDisabled("OBJECT MODE");
    ImGui::SameLine();
    ImGui::TextUnformatted("View");
    ImGui::SameLine();
    ImGui::TextUnformatted("Select");
    ImGui::SameLine();
    ImGui::TextUnformatted("Add");
    ImGui::SameLine();
    ImGui::TextUnformatted("Object");
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Checkbox("Overlays", &gridVisible);
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &snapEnabled);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::DragFloat("##SnapStep", &snapStep, 0.05f, 0.05f, 20.0f, "%.2f m");
    ImGui::SameLine();
    if (ImGui::Button("Front [1]")) { yawDeg = 180.0f; pitchDeg = 0.0f; orthographic = true; }
    ImGui::SameLine();
    if (ImGui::Button("Right [3]")) { yawDeg = -90.0f; pitchDeg = 0.0f; orthographic = true; }
    ImGui::SameLine();
    if (ImGui::Button("Top [7]")) { yawDeg = 0.0f; pitchDeg = 89.0f; orthographic = true; }
    ImGui::SameLine();
    if (ImGui::Button(orthographic ? "ORTHO [5]" : "PERSP [5]")) orthographic = !orthographic;
    ImGui::SameLine();
    ImGui::TextDisabled("%.1f m", distance);
}

} // namespace

HeritageStudioApp::HeritageStudioApp() = default;

HeritageStudioApp::~HeritageStudioApp()
{
    shutdown();
}

int HeritageStudioApp::run()
{
    if (!initialize())
        return 1;

    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();
        frame();
    }
    return 0;
}

bool HeritageStudioApp::initialize()
{
    m_repositoryRoot = findRepositoryRoot();
    m_moduleRoot = m_repositoryRoot / "Modules" / "RacingUnited";
    m_moduleUserRoot = m_repositoryRoot / "UserData" / "Modules" / "RacingUnited";
    m_engineScriptPath = m_moduleRoot / "Assets" / "Audio" / "Authoring" / "EngineSimulator" /
        "Peugeot206RC" / "Peugeot_206_RC_EW10J4S_FINAL_STOCK.mr";
    m_engineResearchPath = m_moduleRoot / "Assets" / "Audio" / "Authoring" / "EngineSimulator" /
        "Peugeot206RC" / "Peugeot_206_RC_EW10J4S_FINAL_RESEARCH.txt";
    m_studioProjectRoot = m_moduleUserRoot / "HeritageStudio" / "Projects" / "RacingUnited";
    m_assetBrowserPath = m_moduleRoot / "Assets";
    m_runtimeScenePath = moduleEntryScenePath(m_moduleRoot);

    std::error_code directoryError;
    std::filesystem::create_directories(m_moduleUserRoot, directoryError);
    std::filesystem::create_directories(m_studioProjectRoot, directoryError);
    m_authoring.resetDefaults();
    m_authoring.loadAll(m_studioProjectRoot, m_studioMessage);
    std::string runtimeSpawnMessage;
    if (loadRuntimeVehicleSpawn(runtimeSpawnMessage))
        m_studioMessage = runtimeSpawnMessage;

    if (!glfwInit())
        return false;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 0);
    // Heritage Engine owns its non-client chrome instead of relying on the OS
    // caption. Studio now follows that exact architectural path, so Windows
    // cannot paint a white title bar above the dark ImGui application.
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    m_window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Heritage Studio", nullptr, nullptr);
    if (!m_window)
    {
        glfwTerminate();
        return false;
    }
#ifdef _WIN32
    // Harmless on an undecorated window, but retained as a fallback for SDK /
    // compositor changes and to keep STUDIO25 compatibility intent explicit.
    applyDarkNativeTitleBar(m_window);
#endif
    glfwSetWindowSizeLimits(m_window, kStudioMinimumWidth, kStudioMinimumHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwGetWindowPos(m_window, &m_studioSavedX, &m_studioSavedY);
    glfwGetWindowSize(m_window, &m_studioSavedW, &m_studioSavedH);

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    const auto orbitronPath = m_repositoryRoot / "Assets" / "Fonts" / "Orbitron-SemiBold.ttf";
    std::error_code fontError;
    if (std::filesystem::exists(orbitronPath, fontError))
    {
        io.FontDefault = io.Fonts->AddFontFromFileTTF(orbitronPath.string().c_str(), 15.0f);
        g_headingFont = io.Fonts->AddFontFromFileTTF(orbitronPath.string().c_str(), 17.0f);
    }
    if (!io.FontDefault)
    {
        ImFontConfig bodyConfig{};
        bodyConfig.SizePixels = 15.0f;
        io.FontDefault = io.Fonts->AddFontDefault(&bodyConfig);
    }
    if (!g_headingFont)
        g_headingFont = io.FontDefault;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    std::string message;
    if (!m_audio.initialize(message))
        m_audioBackendMessage = "Audio unavailable: " + message;
    else
        m_audioBackendMessage = "Audio: " + m_audio.backendName();

    m_soundLab = std::make_unique<audio::lab::EngineSoundCaptureLab>(m_audio, m_moduleUserRoot);

    // Spatial GLB/PBR preview is intentionally lazy. Starting Studio in the
    // Audio workspace must not load the world merely to edit engine sound.
    m_scenePreview = std::make_unique<StudioScenePreview>();

    buildPeugeotCaptureGrid();
    m_initialized = true;
    return true;
}

void HeritageStudioApp::shutdown()
{
    if (!m_initialized && !m_window)
        return;

    if (m_soundLab)
    {
        m_soundLab->stopCapture();
        m_soundLab->stopPreview();
        m_soundLab.reset();
    }
    if (m_scenePreview)
    {
        m_scenePreview->shutdown();
        m_scenePreview.reset();
        m_scenePreviewInitialized = false;
    }
    m_audio.shutdown();

    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    if (m_window)
    {
        if (m_viewFly.active)
        {
#ifdef GLFW_RAW_MOUSE_MOTION
            if (glfwRawMouseMotionSupported()) glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
#endif
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            m_viewFly.active = false;
        }
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
    m_initialized = false;
}

void HeritageStudioApp::frame()
{
    m_audio.update(true);
    if (m_soundLab)
        m_soundLab->update();
    updateCaptureAutoAdvance();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    const ImGuiIO& frameIo = ImGui::GetIO();
    if (frameIo.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
        saveAllAuthoring();

    // Blender workspace convention: Ctrl+PageUp / Ctrl+PageDown steps through
    // workspaces. Heritage adds racing/free-roam workspaces to the same model.
    if (frameIo.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_PageUp, false) || ImGui::IsKeyPressed(ImGuiKey_PageDown, false)))
    {
        constexpr int workspaceCount = 8;
        int index = static_cast<int>(m_workspace);
        index += ImGui::IsKeyPressed(ImGuiKey_PageDown, false) ? 1 : -1;
        if (index < 0) index = workspaceCount - 1;
        if (index >= workspaceCount) index = 0;
        m_workspace = static_cast<Workspace>(index);
    }

    // Mirror Heritage Engine's undecorated-window restore path. GLFW may need
    // one event/frame to leave the maximized state before the saved client
    // rectangle can be applied reliably on Windows.
    if (m_studioPendingRestore && glfwGetWindowAttrib(m_window, GLFW_MAXIMIZED) == GLFW_FALSE)
    {
        glfwSetWindowPos(m_window, m_studioSavedX, m_studioSavedY);
        glfwSetWindowSize(m_window, m_studioSavedW, m_studioSavedH);
        m_studioWindowMaximized = false;
        m_studioPendingRestore = false;
    }

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(m_window, &framebufferWidth, &framebufferHeight);

    drawStudioWindowChrome(framebufferWidth, framebufferHeight);

    ImGui::SetNextWindowPos(ImVec2(0.0f, kStudioTitlebarHeight));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(framebufferWidth), std::max(1.0f, static_cast<float>(framebufferHeight) - kStudioTitlebarHeight)));
    ImGui::Begin("HeritageStudioRoot", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    drawMainMenu();

    const float statusHeight = 28.0f;
    const float workspaceTabsHeight = 36.0f;
    const ImVec2 available = ImGui::GetContentRegionAvail();

    // Blender-style workspace strip across the top instead of a permanent
    // left rail. The game-specific editors are first-class workspaces.
    ImGui::BeginChild("WorkspaceTabs", ImVec2(0.0f, workspaceTabsHeight), true);
    const std::array<Workspace, 8> workspaces{
        Workspace::Scene, Workspace::Race, Workspace::Traffic, Workspace::Gameplay, Workspace::Weather,
        Workspace::Vehicle, Workspace::Audio, Workspace::Assets };
    for (std::size_t i = 0; i < workspaces.size(); ++i)
    {
        if (i > 0) ImGui::SameLine();
        const bool selected = m_workspace == workspaces[i];
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(workspaceName(workspaces[i]), ImVec2(105.0f, 24.0f)))
            m_workspace = workspaces[i];
        if (selected) ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("  Ctrl+PgUp/PgDn workspaces");
    ImGui::EndChild();

    ImGui::BeginChild("WorkspaceContent", ImVec2(0.0f, available.y - statusHeight - workspaceTabsHeight), false);
    drawWorkspace();
    ImGui::EndChild();

    ImGui::BeginChild("StatusBar", ImVec2(0.0f, statusHeight), true);
    drawStatusBar();
    ImGui::EndChild();

    ImGui::End();
    drawStudioResizeHandles();
    ImGui::Render();

    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClearColor(0.055f, 0.060f, 0.070f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_window);
}


void HeritageStudioApp::setStudioMaximized(bool maximized)
{
    if (!m_window || maximized == m_studioWindowMaximized)
        return;

    if (maximized)
    {
        glfwGetWindowPos(m_window, &m_studioSavedX, &m_studioSavedY);
        glfwGetWindowSize(m_window, &m_studioSavedW, &m_studioSavedH);
        glfwMaximizeWindow(m_window);
        m_studioWindowMaximized = true;
        m_studioPendingRestore = false;
    }
    else
    {
        glfwRestoreWindow(m_window);
        m_studioPendingRestore = true;
    }
}

void HeritageStudioApp::drawStudioWindowChrome(int framebufferWidth, int framebufferHeight)
{
    (void)framebufferHeight;
    if (!m_window)
        return;

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(framebufferWidth), kStudioTitlebarHeight), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.018f, 0.020f, 0.024f, 1.0f));
    ImGui::Begin("##HeritageStudioWindowChrome", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    const float controlWidth = 42.0f;
    const float controlsTotal = controlWidth * 3.0f;
    const float dragWidth = std::max(40.0f, static_cast<float>(framebufferWidth) - controlsTotal);

    ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
    ImGui::InvisibleButton("##StudioWindowDrag", ImVec2(dragWidth, kStudioTitlebarHeight));
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        setStudioMaximized(!m_studioWindowMaximized);
        m_studioWindowDragging = false;
    }
    else if (!m_studioWindowMaximized && ImGui::IsItemActivated())
    {
        m_studioWindowDragging = true;
        glfwGetWindowPos(m_window, &m_studioDragStartWindowX, &m_studioDragStartWindowY);
        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetCursorPos(m_window, &cursorX, &cursorY);
        m_studioDragStartCursorX = static_cast<double>(m_studioDragStartWindowX) + cursorX;
        m_studioDragStartCursorY = static_cast<double>(m_studioDragStartWindowY) + cursorY;
    }

    if (m_studioWindowDragging)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !m_studioWindowMaximized)
        {
            int currentWindowX = 0;
            int currentWindowY = 0;
            double cursorX = 0.0;
            double cursorY = 0.0;
            glfwGetWindowPos(m_window, &currentWindowX, &currentWindowY);
            glfwGetCursorPos(m_window, &cursorX, &cursorY);
            const double screenCursorX = static_cast<double>(currentWindowX) + cursorX;
            const double screenCursorY = static_cast<double>(currentWindowY) + cursorY;
            glfwSetWindowPos(m_window,
                m_studioDragStartWindowX + static_cast<int>(screenCursorX - m_studioDragStartCursorX),
                m_studioDragStartWindowY + static_cast<int>(screenCursorY - m_studioDragStartCursorY));
        }
        else
        {
            m_studioWindowDragging = false;
            if (!m_studioWindowMaximized)
            {
                glfwGetWindowPos(m_window, &m_studioSavedX, &m_studioSavedY);
                glfwGetWindowSize(m_window, &m_studioSavedW, &m_studioSavedH);
            }
        }
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddLine(ImVec2(0.0f, kStudioTitlebarHeight - 1.0f),
        ImVec2(static_cast<float>(framebufferWidth), kStudioTitlebarHeight - 1.0f),
        IM_COL32(44, 49, 58, 255), 1.0f);

    if (g_headingFont)
        ImGui::PushFont(g_headingFont);
    draw->AddText(ImVec2(11.0f, 6.0f), IM_COL32(235, 238, 242, 255), "HERITAGE STUDIO");
    if (g_headingFont)
        ImGui::PopFont();
    draw->AddText(ImVec2(179.0f, 8.0f), IM_COL32(112, 122, 135, 255), "Racing United authoring");

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.14f, 0.16f, 0.19f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.22f, 0.26f, 1.0f));

    ImGui::SetCursorPos(ImVec2(static_cast<float>(framebufferWidth) - controlsTotal, 0.0f));
    if (ImGui::Button("_###StudioMinimize", ImVec2(controlWidth, kStudioTitlebarHeight - 1.0f)))
        glfwIconifyWindow(m_window);

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PushID("StudioMaximize");
    ImGui::InvisibleButton("##Button", ImVec2(controlWidth, kStudioTitlebarHeight - 1.0f));
    const ImVec2 maxMin = ImGui::GetItemRectMin();
    const ImVec2 maxMax = ImGui::GetItemRectMax();
    if (ImGui::IsItemHovered())
        draw->AddRectFilled(maxMin, maxMax, IM_COL32(36, 41, 48, 255));
    if (ImGui::IsItemActive())
        draw->AddRectFilled(maxMin, maxMax, IM_COL32(51, 57, 66, 255));
    const ImVec2 center{ (maxMin.x + maxMax.x) * 0.5f, (maxMin.y + maxMax.y) * 0.5f };
    const ImU32 iconColor = IM_COL32(225, 229, 235, 255);
    if (!m_studioWindowMaximized)
    {
        draw->AddRect(ImVec2(center.x - 5.5f, center.y - 4.5f), ImVec2(center.x + 5.5f, center.y + 4.5f), iconColor, 0.0f, 0, 1.0f);
    }
    else
    {
        draw->AddRect(ImVec2(center.x - 6.0f, center.y - 3.0f), ImVec2(center.x + 3.0f, center.y + 5.0f), iconColor, 0.0f, 0, 1.0f);
        draw->AddRect(ImVec2(center.x - 3.0f, center.y - 6.0f), ImVec2(center.x + 6.0f, center.y + 2.0f), iconColor, 0.0f, 0, 1.0f);
    }
    if (ImGui::IsItemClicked())
        setStudioMaximized(!m_studioWindowMaximized);
    ImGui::PopID();

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.72f, 0.08f, 0.10f, 1.0f));
    if (ImGui::Button("X###StudioClose", ImVec2(controlWidth, kStudioTitlebarHeight - 1.0f)))
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    ImGui::PopStyleColor();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void HeritageStudioApp::drawStudioResizeHandles()
{
    if (!m_window || m_studioWindowMaximized)
        return;

    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetWindowSize(m_window, &windowWidth, &windowHeight);
    double cursorXD = 0.0;
    double cursorYD = 0.0;
    glfwGetCursorPos(m_window, &cursorXD, &cursorYD);
    const int cursorX = static_cast<int>(cursorXD);
    const int cursorY = static_cast<int>(cursorYD);
    double screenCursorX = cursorXD;
    double screenCursorY = cursorYD;
#ifdef _WIN32
    POINT screenPoint{};
    if (GetCursorPos(&screenPoint))
    {
        screenCursorX = static_cast<double>(screenPoint.x);
        screenCursorY = static_cast<double>(screenPoint.y);
    }
#else
    int windowX = 0; int windowY = 0; glfwGetWindowPos(m_window, &windowX, &windowY);
    screenCursorX += windowX; screenCursorY += windowY;
#endif
    constexpr int border = 6;
    constexpr int corner = 14;

    int hoverDirection = 0;
    if (cursorX >= 0 && cursorX <= corner && cursorY >= windowHeight - corner && cursorY <= windowHeight) hoverDirection = 1 | 8;
    else if (cursorX >= windowWidth - corner && cursorX <= windowWidth && cursorY >= windowHeight - corner && cursorY <= windowHeight) hoverDirection = 2 | 8;
    else if (cursorX >= 0 && cursorX <= border) hoverDirection = 1;
    else if (cursorX >= windowWidth - border && cursorX <= windowWidth) hoverDirection = 2;
    else if (cursorY >= windowHeight - border && cursorY <= windowHeight) hoverDirection = 8;

#ifdef _WIN32
    const auto applyCursor = [&](int direction)
    {
        HCURSOR cursor = LoadCursor(nullptr, IDC_ARROW);
        if ((direction & (1 | 8)) == (1 | 8)) cursor = LoadCursor(nullptr, IDC_SIZENESW);
        else if ((direction & (2 | 8)) == (2 | 8)) cursor = LoadCursor(nullptr, IDC_SIZENWSE);
        else if (direction & (1 | 2)) cursor = LoadCursor(nullptr, IDC_SIZEWE);
        else if (direction & 8) cursor = LoadCursor(nullptr, IDC_SIZENS);
        if (cursor) SetCursor(cursor);
    };
#else
    const auto applyCursor = [](int) {};
#endif

    const bool mouseDown = glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    if (!m_studioWindowResizing)
    {
        if (hoverDirection != 0 && !ImGui::GetIO().WantTextInput)
        {
            applyCursor(hoverDirection);
            if (mouseDown)
            {
                m_studioWindowResizing = true;
                m_studioResizeDirection = hoverDirection;
                glfwGetWindowPos(m_window, &m_studioResizeStartX, &m_studioResizeStartY);
                glfwGetWindowSize(m_window, &m_studioResizeStartW, &m_studioResizeStartH);
                m_studioResizeStartCursorX = screenCursorX;
                m_studioResizeStartCursorY = screenCursorY;
            }
        }
        else if (!m_viewFly.active)
        {
            applyCursor(0);
        }
        return;
    }

    applyCursor(m_studioResizeDirection);
    const int deltaX = static_cast<int>(screenCursorX - m_studioResizeStartCursorX);
    const int deltaY = static_cast<int>(screenCursorY - m_studioResizeStartCursorY);
    int newX = m_studioResizeStartX;
    int newY = m_studioResizeStartY;
    int newW = m_studioResizeStartW;
    int newH = m_studioResizeStartH;
    if (m_studioResizeDirection & 1) { newX = m_studioResizeStartX + deltaX; newW = m_studioResizeStartW - deltaX; }
    if (m_studioResizeDirection & 2) newW = m_studioResizeStartW + deltaX;
    if (m_studioResizeDirection & 8) newH = m_studioResizeStartH + deltaY;

    if (newW < kStudioMinimumWidth)
    {
        if (m_studioResizeDirection & 1) newX -= kStudioMinimumWidth - newW;
        newW = kStudioMinimumWidth;
    }
    newH = std::max(newH, kStudioMinimumHeight);
    glfwSetWindowPos(m_window, newX, newY);
    glfwSetWindowSize(m_window, newW, newH);

    if (!mouseDown)
    {
        m_studioWindowResizing = false;
        m_studioResizeDirection = 0;
        glfwGetWindowPos(m_window, &m_studioSavedX, &m_studioSavedY);
        glfwGetWindowSize(m_window, &m_studioSavedW, &m_studioSavedH);
        applyCursor(0);
    }
}


void HeritageStudioApp::drawMainMenu()
{
    if (!ImGui::BeginMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Save All Authoring", "Ctrl+S"))
            saveAllAuthoring();
        if (ImGui::MenuItem("Reload Authoring"))
        {
            m_authoring.loadAll(m_studioProjectRoot, m_studioMessage);
            std::string runtimeMessage;
            if (loadRuntimeVehicleSpawn(runtimeMessage))
                m_studioMessage = runtimeMessage;
        }
        if (ImGui::MenuItem("Validate Authoring"))
            m_studioMessage = validateAuthoring();
        ImGui::Separator();
        if (ImGui::MenuItem("Open Racing United Assets"))
            openPathInShell(m_moduleRoot / "Assets");
        if (ImGui::MenuItem("Open Runtime Scene Folder"))
            openPathInShell(m_runtimeScenePath.parent_path());
        if (ImGui::MenuItem("Open Studio Project Folder"))
            openPathInShell(m_studioProjectRoot);
        if (ImGui::MenuItem("Open Engine Sound Lab Data"))
            openPathInShell(m_moduleUserRoot / "EngineSoundLab");
        ImGui::Separator();
        if (ImGui::MenuItem("Exit"))
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Workspace"))
    {
        const std::array<Workspace, 8> workspaces{
            Workspace::Scene, Workspace::Race, Workspace::Traffic, Workspace::Gameplay,
            Workspace::Weather, Workspace::Vehicle, Workspace::Audio, Workspace::Assets };
        for (const Workspace workspace : workspaces)
        {
            if (ImGui::MenuItem(workspaceName(workspace), nullptr, m_workspace == workspace))
                m_workspace = workspace;
        }
        ImGui::EndMenu();
    }

    ImGui::TextDisabled("  Heritage Studio  |  Racing United authoring");
    ImGui::EndMenuBar();
}

void HeritageStudioApp::drawWorkspaceRail()
{
    ImGui::TextUnformatted("WORKSPACES");
    ImGui::Separator();
    ImGui::Spacing();
    workspaceButton("SCENE", Workspace::Scene, m_workspace);
    workspaceButton("RACE", Workspace::Race, m_workspace);
    workspaceButton("TRAFFIC", Workspace::Traffic, m_workspace);
    workspaceButton("GAMEPLAY", Workspace::Gameplay, m_workspace);
    workspaceButton("WEATHER", Workspace::Weather, m_workspace);
    workspaceButton("VEHICLE", Workspace::Vehicle, m_workspace);
    workspaceButton("AUDIO", Workspace::Audio, m_workspace);
    workspaceButton("ASSETS", Workspace::Assets, m_workspace);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextWrapped("Studio is a separate authoring process. Racing United runtime systems are not booted just to edit content.");
}

void HeritageStudioApp::drawWorkspace()
{
    switch (m_workspace)
    {
    case Workspace::Scene: drawSceneWorkspace(); break;
    case Workspace::Race: drawRaceWorkspace(); break;
    case Workspace::Traffic: drawTrafficWorkspace(); break;
    case Workspace::Gameplay: drawGameplayWorkspace(); break;
    case Workspace::Weather: drawWeatherWorkspace(); break;
    case Workspace::Vehicle: drawVehicleWorkspace(); break;
    case Workspace::Audio: drawAudioWorkspace(); break;
    case Workspace::Assets: drawAssetsWorkspace(); break;
    }
}

void HeritageStudioApp::ensureScenePreviewInitialized()
{
    if (!m_scenePreview || m_scenePreviewInitialized)
        return;

    m_scenePreviewInitialized = true;
    std::string message;
    if (!m_scenePreview->initialize(m_moduleRoot / "Assets", message))
    {
        m_studioMessage = message;
        return;
    }
    if (!message.empty())
        m_studioMessage = message;
    if (m_scenePreview->loaded())
    {
        m_viewTarget = m_scenePreview->boundsCenter();
        m_viewDistanceM = std::clamp(m_scenePreview->boundsRadius() * 2.4f, 8.0f, 2500.0f);
    }
}

void HeritageStudioApp::drawSceneWorkspace()
{
    ensureScenePreviewInitialized();
    headingText("LAYOUT / SCENE");
    ImGui::SameLine();
    ImGui::TextDisabled("  Blender 5.2 interaction model + Racing United game authoring");
    ImGui::Separator();

    if (ImGui::Button("SAVE", ImVec2(82.0f, 26.0f)))
        saveSceneAuthoring();
    ImGui::SameLine();
    if (ImGui::Button("LOAD", ImVec2(82.0f, 26.0f)))
        loadSceneAuthoring();
    ImGui::SameLine();
    if (ImGui::Button("PROJECT FOLDER", ImVec2(132.0f, 26.0f)))
        openPathInShell(m_studioProjectRoot);
    ImGui::SameLine();
    ImGui::TextDisabled("Ctrl+S Save All + Runtime Spawn  |  Shift+A Add  |  Shift+D Duplicate  |  X Delete");
    ImGui::SameLine();
    ImGui::TextDisabled("  %s", m_studioMessage.c_str());

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float rightWidth = std::clamp(available.x * 0.26f, 330.0f, 460.0f);
    const float bottomHeight = 96.0f;

    // Blender Layout workspace convention: large 3D Viewport left, Outliner
    // top-right, Properties bottom-right, with a bottom editor strip. Heritage
    // uses the bottom strip for game-authoring/playtest state instead of animation.
    ImGui::BeginChild("SceneMainEditors", ImVec2(std::max(420.0f, available.x - rightWidth - 8.0f), 0.0f), false);
    const ImVec2 mainAvail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("SceneViewport", ImVec2(0.0f, std::max(240.0f, mainAvail.y - bottomHeight - 6.0f)), true);
    drawSceneViewportInteractive();
    ImGui::EndChild();

    ImGui::BeginChild("SceneGameStrip", ImVec2(0.0f, 0.0f), true);
    ImGui::TextDisabled("GAME AUTHORING / PLAYTEST");
    ImGui::Separator();
    ImGui::Text("Objects %d", static_cast<int>(m_authoring.sceneObjects.size()));
    ImGui::SameLine();
    ImGui::TextDisabled(" | Vehicle Spawn saves directly into the module entry .hscene; other authoring layers remain staged.");
    ImGui::TextDisabled("Viewport: MMB orbit | Shift+MMB pan | Ctrl+MMB/wheel zoom | Num1/3/7 | Num5 | Num. frame selected | Home frame all");
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("SceneRightEditors", ImVec2(0.0f, 0.0f), false);
    const float outlinerHeight = std::max(190.0f, ImGui::GetContentRegionAvail().y * 0.43f);

    ImGui::BeginChild("SceneOutliner", ImVec2(0.0f, outlinerHeight), true);
    ImGui::TextDisabled("OUTLINER");
    ImGui::SameLine();
    ImGui::TextDisabled("View Layer / Racing United Scene");
    ImGui::Separator();
    if (ImGui::Button("+ ADD", ImVec2(80.0f, 24.0f)))
        ImGui::OpenPopup("OutlinerAddGameObject");
    if (ImGui::BeginPopup("OutlinerAddGameObject"))
    {
        const auto addObject = [&](const char* label, authoring::SceneObjectType type)
        {
            if (ImGui::MenuItem(label))
            {
                m_sceneSelectedObject = static_cast<int>(m_authoring.sceneObjects.size());
                auto& created = m_authoring.addSceneObject(type);
                created.position = m_viewTarget;
                created.position.y = 0.0f;
            }
        };
        addObject("Empty", authoring::SceneObjectType::Empty);
        addObject("Player Spawn", authoring::SceneObjectType::PlayerSpawn);
        addObject("Vehicle Spawn", authoring::SceneObjectType::VehicleSpawn);
        addObject("Audio Zone", authoring::SceneObjectType::AudioZone);
        addObject("Weather Zone", authoring::SceneObjectType::WeatherZone);
        addObject("Trigger", authoring::SceneObjectType::Trigger);
        ImGui::EndPopup();
    }
    ImGui::Separator();
    for (int i = 0; i < static_cast<int>(m_authoring.sceneObjects.size()); ++i)
    {
        const auto& object = m_authoring.sceneObjects[static_cast<std::size_t>(i)];
        std::string label = object.name + "  [" + authoring::sceneObjectTypeName(object.type) + "]##Outliner" + std::to_string(object.id);
        if (ImGui::Selectable(label.c_str(), m_sceneSelectedObject == i))
            m_sceneSelectedObject = i;
    }
    ImGui::EndChild();

    ImGui::BeginChild("SceneProperties", ImVec2(0.0f, 0.0f), true);
    ImGui::TextDisabled("PROPERTIES");
    ImGui::Separator();
    if (!m_authoring.sceneObjects.empty())
    {
        m_sceneSelectedObject = std::clamp(m_sceneSelectedObject, 0, static_cast<int>(m_authoring.sceneObjects.size()) - 1);
        auto& object = m_authoring.sceneObjects[static_cast<std::size_t>(m_sceneSelectedObject)];
        inputString("Name", object.name);
        ImGui::Checkbox("Enabled", &object.enabled);
        int typeIndex = static_cast<int>(object.type);
        const char* types[] = { "Empty", "Mesh", "Player Spawn", "Vehicle Spawn", "Audio Zone", "Weather Zone", "Trigger" };
        if (ImGui::Combo("Type", &typeIndex, types, IM_ARRAYSIZE(types)))
            object.type = static_cast<authoring::SceneObjectType>(typeIndex);
        sectionTitle("TRANSFORM");
        ImGui::DragFloat3("Location", &object.position.x, 0.05f);
        ImGui::DragFloat3("Rotation", &object.rotation.x, 0.25f);
        ImGui::DragFloat3("Scale", &object.scale.x, 0.01f, 0.001f, 1000.0f);
        sectionTitle("GAME OBJECT");
        inputString("Tag", object.tag);
        inputString("Asset path", object.assetPath, 1024);
        if (ImGui::Button("DUPLICATE  [Shift+D]", ImVec2(-1.0f, 28.0f)))
        {
            auto copy = object;
            copy.id = 0;
            copy.name += " Copy";
            auto& created = m_authoring.addSceneObject(copy.type, copy.name.c_str());
            const auto newId = created.id;
            created = copy;
            created.id = newId;
            m_sceneSelectedObject = static_cast<int>(m_authoring.sceneObjects.size()) - 1;
        }
        if (m_authoring.sceneObjects.size() > 1 && ImGui::Button("DELETE  [X]", ImVec2(-1.0f, 28.0f)))
        {
            m_authoring.removeSceneObject(static_cast<std::size_t>(m_sceneSelectedObject));
            m_sceneSelectedObject = std::max(0, m_sceneSelectedObject - 1);
        }
    }
    ImGui::EndChild();
    ImGui::EndChild();
}


void HeritageStudioApp::drawSceneViewportInteractive()
{
    sectionTitle("3D VIEWPORT");
    drawViewportToolbar(m_viewGridVisible, m_viewSnapEnabled, m_viewSnapM,
        m_viewYawDeg, m_viewPitchDeg, m_viewDistanceM, m_viewOrthographic);

    if (m_scenePreview)
    {
        if (ImGui::Checkbox("Scene GLB", &m_sceneGeometryVisible))
            m_scenePreview->setVisible(m_sceneGeometryVisible);
        ImGui::SameLine();
        if (ImGui::Checkbox("Wireframe", &m_sceneGeometryWireframe))
            m_scenePreview->setWireframe(m_sceneGeometryWireframe);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::SliderFloat("Exposure##ScenePreview", &m_sceneGeometryExposure, 0.25f, 3.0f, "%.2f"))
            m_scenePreview->setExposure(m_sceneGeometryExposure);
        ImGui::SameLine();
        if (ImGui::Button("LATEST Scene_*.glb"))
        {
            std::string message;
            if (m_scenePreview->discoverAndLoadLatest(message) && m_scenePreview->loaded())
            {
                m_viewTarget = m_scenePreview->boundsCenter();
                m_viewDistanceM = std::clamp(m_scenePreview->boundsRadius() * 2.4f, 8.0f, 2500.0f);
            }
            m_studioMessage = message;
        }
        ImGui::SameLine();
        if (ImGui::Button("RELOAD GLB"))
        {
            std::string message;
            m_scenePreview->reload(message);
            m_studioMessage = message;
        }
        if (m_scenePreview->loaded())
        {
            ImGui::TextDisabled("%s | %zu triangles | %zu materials",
                m_scenePreview->scenePath().filename().string().c_str(),
                m_scenePreview->triangleCount(), m_scenePreview->materialCount());
        }
        else
        {
            ImGui::TextDisabled("%s", m_scenePreview->status().c_str());
        }
    }

    if (m_viewToolbarVisible)
    {
        if (ImGui::BeginTable("BlenderToolShelf", 4, ImGuiTableFlags_SizingStretchSame))
        {
            const char* toolNames[] = { "SELECT [W]", "MOVE [G]", "ROTATE [R]", "SCALE [S]" };
            for (int tool = 0; tool < 4; ++tool)
            {
                ImGui::TableNextColumn();
                if (m_viewTool == tool) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                if (ImGui::Button(toolNames[tool], ImVec2(-1.0f, 26.0f))) m_viewTool = tool;
                if (m_viewTool == tool) ImGui::PopStyleColor();
            }
            ImGui::EndTable();
        }
    }

    if (m_viewToolbarVisible && ImGui::BeginTable("ScenePlacementToolbar", 4, ImGuiTableFlags_SizingStretchSame))
    {
        const auto placeButton = [&](const char* label, authoring::SceneObjectType type)
        {
            ImGui::TableNextColumn();
            const bool active = m_scenePlacementType == static_cast<int>(type);
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(label, ImVec2(-1.0f, 28.0f)))
                m_scenePlacementType = active ? -1 : static_cast<int>(type);
            if (active)
                ImGui::PopStyleColor();
        };
        placeButton("PLACE PLAYER", authoring::SceneObjectType::PlayerSpawn);
        placeButton("PLACE VEHICLE", authoring::SceneObjectType::VehicleSpawn);
        placeButton("PLACE AUDIO ZONE", authoring::SceneObjectType::AudioZone);
        placeButton("PLACE TRIGGER", authoring::SceneObjectType::Trigger);
        ImGui::EndTable();
    }
    if (m_scenePlacementType >= 0)
        ImGui::TextDisabled("Placement mode active: click the visible scene surface (ground-plane fallback). ESC cancels.");

    ImVec2 size = ImGui::GetContentRegionAvail();
    size.y = std::max(180.0f, size.y);
    bool hovered = false;
    const StudioViewportProjection view = prepareInteractiveViewport(
        m_window, m_viewFly, "##SceneInteractiveViewport", size,
        m_viewYawDeg, m_viewPitchDeg, m_viewDistanceM, m_viewTarget,
        m_viewGridVisible, m_viewOrthographic, hovered);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    drawSceneGeometryBackdrop(draw, view, m_scenePreview.get(), m_viewGridVisible);
    drawFlyNavigationOverlay(draw, view, m_viewFly);
    draw->PushClipRect(view.min, view.max, true);

    const ImGuiIO& io = ImGui::GetIO();
    if (hovered && !io.WantTextInput && m_viewTransformMode == 0)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_T, false)) m_viewToolbarVisible = !m_viewToolbarVisible;
        if (ImGui::IsKeyPressed(ImGuiKey_N, false)) m_viewSidebarVisible = !m_viewSidebarVisible;
        if (ImGui::IsKeyPressed(ImGuiKey_W, false)) m_viewTool = 0;
    }
    if (hovered && ImGui::IsKeyPressed(ImGuiKey_Escape, false) && m_viewTransformMode == 0)
        m_scenePlacementType = -1;

    bool transformCaptured = false;
    if (!m_authoring.sceneObjects.empty())
    {
        m_sceneSelectedObject = std::clamp(m_sceneSelectedObject, 0, static_cast<int>(m_authoring.sceneObjects.size()) - 1);
        auto& selected = m_authoring.sceneObjects[static_cast<std::size_t>(m_sceneSelectedObject)];

        if (hovered && m_scenePlacementType < 0 && m_viewTransformMode == 0)
        {
            int requestedMode = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_G, false)) requestedMode = 1;
            if (ImGui::IsKeyPressed(ImGuiKey_R, false)) requestedMode = 2;
            if (ImGui::IsKeyPressed(ImGuiKey_S, false)) requestedMode = 3;
            if (requestedMode != 0)
            {
                m_viewTransformMode = requestedMode;
                m_viewTransformAxis = -1;
                m_viewTransformStartMouseX = io.MousePos.x;
                m_viewTransformStartMouseY = io.MousePos.y;
                m_viewTransformStartPosition = selected.position;
                m_viewTransformStartRotation = selected.rotation;
                m_viewTransformStartScale = selected.scale;
            }

            // Blender convention: Shift+A opens Add. Heritage adds game-authoring entities.
            if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_A, false))
                ImGui::OpenPopup("HeritageAddMenu");

            if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_D, false))
            {
                auto copy = selected;
                copy.id = 0;
                copy.name += " Copy";
                auto& created = m_authoring.addSceneObject(copy.type, copy.name.c_str());
                const auto newId = created.id;
                created = copy;
                created.id = newId;
                m_sceneSelectedObject = static_cast<int>(m_authoring.sceneObjects.size()) - 1;
                m_viewTransformMode = 1;
                m_viewTransformAxis = -1;
                m_viewTransformStartMouseX = io.MousePos.x;
                m_viewTransformStartMouseY = io.MousePos.y;
                m_viewTransformStartPosition = created.position;
                m_viewTransformStartRotation = created.rotation;
                m_viewTransformStartScale = created.scale;
            }
        }

        if (ImGui::BeginPopup("HeritageAddMenu"))
        {
            ImGui::TextDisabled("ADD GAME OBJECT  [Shift+A]");
            ImGui::Separator();
            const auto addAtTarget = [&](const char* label, authoring::SceneObjectType type)
            {
                if (ImGui::MenuItem(label))
                {
                    auto& created = m_authoring.addSceneObject(type);
                    created.position = m_viewTarget;
                    created.position.y = 0.0f;
                    m_sceneSelectedObject = static_cast<int>(m_authoring.sceneObjects.size()) - 1;
                }
            };
            addAtTarget("Empty", authoring::SceneObjectType::Empty);
            addAtTarget("Player Spawn", authoring::SceneObjectType::PlayerSpawn);
            addAtTarget("Vehicle Spawn", authoring::SceneObjectType::VehicleSpawn);
            addAtTarget("Audio Zone", authoring::SceneObjectType::AudioZone);
            addAtTarget("Weather Zone", authoring::SceneObjectType::WeatherZone);
            addAtTarget("Trigger", authoring::SceneObjectType::Trigger);
            ImGui::EndPopup();
        }

        if (m_viewTransformMode != 0)
        {
            auto& transformObject = m_authoring.sceneObjects[static_cast<std::size_t>(m_sceneSelectedObject)];
            transformCaptured = true;
            if (ImGui::IsKeyPressed(ImGuiKey_X, false)) m_viewTransformAxis = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) m_viewTransformAxis = 1;
            if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) m_viewTransformAxis = 2;

            const bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false)
                || ImGui::IsMouseClicked(ImGuiMouseButton_Right);
            const bool confirm = ImGui::IsKeyPressed(ImGuiKey_Enter, false)
                || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)
                || ImGui::IsMouseClicked(ImGuiMouseButton_Left);

            if (cancel)
            {
                transformObject.position = m_viewTransformStartPosition;
                transformObject.rotation = m_viewTransformStartRotation;
                transformObject.scale = m_viewTransformStartScale;
                m_viewTransformMode = 0;
                m_viewTransformAxis = -1;
            }
            else
            {
                const float dx = io.MousePos.x - m_viewTransformStartMouseX;
                const float dy = io.MousePos.y - m_viewTransformStartMouseY;
                const float worldPerPixel = view.orthographic
                    ? (2.0f * view.orthoHalfHeight) / std::max(1.0f, view.size.y)
                    : (2.0f * length3(sub3(view.camera, transformObject.position)) * std::tan(view.fovYRad * 0.5f)) / std::max(1.0f, view.size.y);
                const float precision = io.KeyShift ? 0.1f : 1.0f;

                if (m_viewTransformMode == 1)
                {
                    if (m_viewTransformAxis < 0)
                    {
                        transformObject.position = add3(m_viewTransformStartPosition,
                            add3(mul3(view.right, dx * worldPerPixel * precision),
                                 mul3(view.up, -dy * worldPerPixel * precision)));
                    }
                    else
                    {
                        const authoring::Vec3 axisWorld[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
                        ImVec2 origin{}, end{};
                        const float axisLength = std::max(2.0f, length3(sub3(view.camera, transformObject.position)) * 0.15f);
                        if (projectWorldPoint(view, m_viewTransformStartPosition, origin)
                            && projectWorldPoint(view, add3(m_viewTransformStartPosition, mul3(axisWorld[m_viewTransformAxis], axisLength)), end))
                        {
                            const float ax = end.x - origin.x;
                            const float ay = end.y - origin.y;
                            const float al = std::max(1.0f, std::sqrt(ax * ax + ay * ay));
                            const float pixels = dx * (ax / al) + dy * (ay / al);
                            const float delta = pixels * worldPerPixel * precision;
                            transformObject.position = add3(m_viewTransformStartPosition, mul3(axisWorld[m_viewTransformAxis], delta));
                        }
                    }
                    if (m_viewSnapEnabled || io.KeyCtrl)
                    {
                        transformObject.position.x = snapValue(transformObject.position.x, true, m_viewSnapM);
                        transformObject.position.y = snapValue(transformObject.position.y, true, m_viewSnapM);
                        transformObject.position.z = snapValue(transformObject.position.z, true, m_viewSnapM);
                    }
                }
                else if (m_viewTransformMode == 2)
                {
                    const float deltaDeg = (dx - dy * 0.35f) * 0.45f * precision;
                    transformObject.rotation = m_viewTransformStartRotation;
                    const int axis = m_viewTransformAxis >= 0 ? m_viewTransformAxis : 1;
                    if (axis == 0) transformObject.rotation.x += deltaDeg;
                    if (axis == 1) transformObject.rotation.y += deltaDeg;
                    if (axis == 2) transformObject.rotation.z += deltaDeg;
                    if (io.KeyCtrl)
                    {
                        if (axis == 0) transformObject.rotation.x = snapValue(transformObject.rotation.x, true, 5.0f);
                        if (axis == 1) transformObject.rotation.y = snapValue(transformObject.rotation.y, true, 5.0f);
                        if (axis == 2) transformObject.rotation.z = snapValue(transformObject.rotation.z, true, 5.0f);
                    }
                }
                else if (m_viewTransformMode == 3)
                {
                    const float factor = std::max(0.001f, 1.0f + (dx - dy) * 0.006f * precision);
                    transformObject.scale = m_viewTransformStartScale;
                    if (m_viewTransformAxis < 0)
                    {
                        transformObject.scale = mul3(m_viewTransformStartScale, factor);
                    }
                    else
                    {
                        if (m_viewTransformAxis == 0) transformObject.scale.x = m_viewTransformStartScale.x * factor;
                        if (m_viewTransformAxis == 1) transformObject.scale.y = m_viewTransformStartScale.y * factor;
                        if (m_viewTransformAxis == 2) transformObject.scale.z = m_viewTransformStartScale.z * factor;
                    }
                }

                if (confirm)
                {
                    m_viewTransformMode = 0;
                    m_viewTransformAxis = -1;
                }
            }

            const char* modeName = m_viewTransformMode == 1 ? "Move" : (m_viewTransformMode == 2 ? "Rotate" : "Scale");
            const char* axisName = m_viewTransformAxis == 0 ? " X" : (m_viewTransformAxis == 1 ? " Y" : (m_viewTransformAxis == 2 ? " Z" : ""));
            if (m_viewTransformMode != 0)
            {
                draw->AddRectFilled(ImVec2(view.min.x + 12.0f, view.min.y + 12.0f), ImVec2(view.min.x + 425.0f, view.min.y + 38.0f), IM_COL32(18, 22, 28, 225), 3.0f);
                char transformText[256]{};
                std::snprintf(transformText, sizeof(transformText), "%s%s | X/Y/Z constrain | Shift precision | Ctrl snap | LMB/Enter confirm | RMB/Esc cancel", modeName, axisName);
                draw->AddText(ImVec2(view.min.x + 20.0f, view.min.y + 18.0f), IM_COL32(235, 235, 235, 255), transformText);
            }
        }
    }

    if (!m_authoring.sceneObjects.empty())
    {
        m_sceneSelectedObject = std::clamp(m_sceneSelectedObject, 0, static_cast<int>(m_authoring.sceneObjects.size()) - 1);
        const bool frameSelected = hovered && (ImGui::IsKeyPressed(ImGuiKey_KeypadDecimal, false) || ImGui::IsKeyPressed(ImGuiKey_F, false));
        if (frameSelected)
        {
            m_viewTarget = m_authoring.sceneObjects[static_cast<std::size_t>(m_sceneSelectedObject)].position;
            m_viewDistanceM = std::clamp(m_viewDistanceM, 8.0f, 80.0f);
        }
        if (hovered && ImGui::IsKeyPressed(ImGuiKey_Home, false))
        {
            authoring::Vec3 center{};
            float radius = 5.0f;
            if (m_scenePreview && m_scenePreview->loaded())
            {
                center = m_scenePreview->boundsCenter();
                radius = std::max(radius, m_scenePreview->boundsRadius());
                for (const auto& object : m_authoring.sceneObjects)
                    radius = std::max(radius, length3(sub3(object.position, center)));
            }
            else
            {
                for (const auto& object : m_authoring.sceneObjects) center = add3(center, object.position);
                center = mul3(center, 1.0f / static_cast<float>(m_authoring.sceneObjects.size()));
                for (const auto& object : m_authoring.sceneObjects) radius = std::max(radius, length3(sub3(object.position, center)));
            }
            m_viewTarget = center;
            m_viewDistanceM = std::clamp(radius * 2.4f, 8.0f, 2500.0f);
        }
        if (hovered && m_viewTransformMode == 0 && !io.WantTextInput
            && (ImGui::IsKeyPressed(ImGuiKey_X, false) || ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            && m_authoring.sceneObjects.size() > 1)
        {
            m_authoring.removeSceneObject(static_cast<std::size_t>(m_sceneSelectedObject));
            m_sceneSelectedObject = std::max(0, m_sceneSelectedObject - 1);
            m_studioMessage = "Deleted scene object (Blender X/Delete).";
        }
    }

    // Draw every authored object as a pickable 3D marker.
    for (int i = 0; i < static_cast<int>(m_authoring.sceneObjects.size()); ++i)
    {
        const auto& object = m_authoring.sceneObjects[static_cast<std::size_t>(i)];
        if (!object.enabled)
            continue;
        ImVec2 screen{};
        if (!projectWorldPoint(view, object.position, screen))
            continue;
        ImU32 color = IM_COL32(175, 185, 195, 255);
        switch (object.type)
        {
        case authoring::SceneObjectType::PlayerSpawn: color = IM_COL32(85, 220, 120, 255); break;
        case authoring::SceneObjectType::VehicleSpawn: color = IM_COL32(90, 155, 240, 255); break;
        case authoring::SceneObjectType::AudioZone: color = IM_COL32(210, 120, 235, 255); break;
        case authoring::SceneObjectType::WeatherZone: color = IM_COL32(90, 205, 220, 255); break;
        case authoring::SceneObjectType::Trigger: color = IM_COL32(245, 180, 70, 255); break;
        default: break;
        }
        drawMarker(draw, screen, color, i == m_sceneSelectedObject, object.name.c_str());
    }

    bool gizmoCaptured = false;
    if (!m_authoring.sceneObjects.empty() && m_scenePlacementType < 0 && m_viewTransformMode == 0)
    {
        auto& selected = m_authoring.sceneObjects[static_cast<std::size_t>(m_sceneSelectedObject)];
        const int previousAxis = m_viewGizmoAxis;
        drawMoveGizmo(draw, view, selected.position, hovered,
            m_viewSnapEnabled, m_viewSnapM, m_viewGizmoAxis, m_viewGizmoDragAccumulator);
        gizmoCaptured = m_viewGizmoAxis >= 0 || previousAxis >= 0;
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !gizmoCaptured && !transformCaptured)
    {
        if (m_scenePlacementType >= 0)
        {
            authoring::Vec3 ground{};
            if (viewportAuthoringPoint(view, io.MousePos, m_scenePreview.get(), ground))
            {
                ground.x = snapValue(ground.x, m_viewSnapEnabled, m_viewSnapM);
                ground.z = snapValue(ground.z, m_viewSnapEnabled, m_viewSnapM);
                auto& created = m_authoring.addSceneObject(static_cast<authoring::SceneObjectType>(m_scenePlacementType));
                created.position = ground;
                m_sceneSelectedObject = static_cast<int>(m_authoring.sceneObjects.size()) - 1;
                m_scenePlacementType = -1;
                m_studioMessage = "Placed scene object in 3D viewport.";
            }
        }
        else
        {
            int bestIndex = -1;
            float bestDistance = 13.0f;
            for (int i = 0; i < static_cast<int>(m_authoring.sceneObjects.size()); ++i)
            {
                ImVec2 screen{};
                if (!projectWorldPoint(view, m_authoring.sceneObjects[static_cast<std::size_t>(i)].position, screen))
                    continue;
                const float dx = io.MousePos.x - screen.x;
                const float dy = io.MousePos.y - screen.y;
                const float distance = std::sqrt(dx * dx + dy * dy);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestIndex = i;
                }
            }
            if (bestIndex >= 0)
                m_sceneSelectedObject = bestIndex;
        }
    }

    if (m_viewSidebarVisible && !m_authoring.sceneObjects.empty())
    {
        const auto& selectedObject = m_authoring.sceneObjects[static_cast<std::size_t>(m_sceneSelectedObject)];
        const float sidebarWidth = 250.0f;
        const ImVec2 sidebarMin{ view.max.x - sidebarWidth - 8.0f, view.min.y + 48.0f };
        const ImVec2 sidebarMax{ view.max.x - 8.0f, view.min.y + 190.0f };
        draw->AddRectFilled(sidebarMin, sidebarMax, IM_COL32(22, 25, 30, 225), 4.0f);
        draw->AddText(ImVec2(sidebarMin.x + 10.0f, sidebarMin.y + 8.0f), IM_COL32(205, 210, 218, 255), "ITEM  [N]");
        char line[160]{};
        std::snprintf(line, sizeof(line), "Location  %.2f  %.2f  %.2f", selectedObject.position.x, selectedObject.position.y, selectedObject.position.z);
        draw->AddText(ImVec2(sidebarMin.x + 10.0f, sidebarMin.y + 34.0f), IM_COL32(180, 188, 198, 255), line);
        std::snprintf(line, sizeof(line), "Rotation  %.1f  %.1f  %.1f", selectedObject.rotation.x, selectedObject.rotation.y, selectedObject.rotation.z);
        draw->AddText(ImVec2(sidebarMin.x + 10.0f, sidebarMin.y + 56.0f), IM_COL32(180, 188, 198, 255), line);
        std::snprintf(line, sizeof(line), "Scale     %.2f  %.2f  %.2f", selectedObject.scale.x, selectedObject.scale.y, selectedObject.scale.z);
        draw->AddText(ImVec2(sidebarMin.x + 10.0f, sidebarMin.y + 78.0f), IM_COL32(180, 188, 198, 255), line);
        draw->AddText(ImVec2(sidebarMin.x + 10.0f, sidebarMin.y + 108.0f), IM_COL32(130, 145, 160, 255), "T toolbar | N sidebar | Shift+A add");
    }

    draw->PopClipRect();
}

void HeritageStudioApp::drawEditorScaffold(
    const char* title,
    const char* description,
    const std::vector<const char*>& features)
{
    ImGui::TextUnformatted(title);
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextWrapped("%s", description);
    ImGui::Spacing();
    ImGui::BeginChild("AuthoringScaffold", ImVec2(0.0f, 0.0f), true);
    ImGui::TextUnformatted("AUTHORING SYSTEMS IN THIS WORKSPACE");
    ImGui::Separator();
    for (const char* feature : features)
        ImGui::BulletText("%s", feature);
    ImGui::Spacing();
    ImGui::TextDisabled("This workspace is housed in HeritageStudio.exe and will serialize data consumed by Racing United; it is not a game-runtime LAB panel.");
    ImGui::EndChild();
}

void HeritageStudioApp::drawRaceWorkspace()
{
    ensureScenePreviewInitialized();
    headingText("RACE / VENUE AUTHORING");
    ImGui::SameLine();
    ImGui::TextDisabled("  timing gates / grids / routes / sessions / race control / broadcast cameras / cone courses");
    ImGui::Separator();

    if (ImGui::Button("SAVE RACE", ImVec2(130.0f, 30.0f)))
    {
        if (m_authoring.saveRace(m_studioProjectRoot / "race.hrace", m_studioMessage))
        {
            std::string runtimeMessage;
            if (saveRuntimeGameplay(runtimeMessage)) m_studioMessage = runtimeMessage;
            else m_studioMessage += " | " + runtimeMessage;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("LOAD RACE", ImVec2(130.0f, 30.0f)))
        m_authoring.loadRace(m_studioProjectRoot / "race.hrace", m_studioMessage);
    ImGui::SameLine();
    if (ImGui::Button("VALIDATE", ImVec2(105.0f, 30.0f)))
        m_studioMessage = validateAuthoring();
    ImGui::SameLine();
    ImGui::TextDisabled("HRACE v7 | %d markers | %d routes / %d nodes | %d layouts | %d sessions | %d camera paths | %d cones / %d course gates",
        static_cast<int>(m_authoring.raceMarkers.size()), static_cast<int>(m_authoring.raceRoutes.size()),
        static_cast<int>(m_authoring.raceRouteNodes.size()), static_cast<int>(m_authoring.raceLayouts.size()),
        static_cast<int>(m_authoring.raceSessions.size()), static_cast<int>(m_authoring.broadcastCameraPaths.size()),
        static_cast<int>(m_authoring.courseCones.size()), static_cast<int>(m_authoring.coneCourseGates.size()));

    ImGui::SetNextItemWidth(95.0f); ImGui::InputInt("Laps", &m_authoring.race.laps);
    ImGui::SameLine(); ImGui::SetNextItemWidth(105.0f); ImGui::InputInt("Grid", &m_authoring.race.gridSlots);
    ImGui::SameLine(); ImGui::SetNextItemWidth(125.0f); ImGui::DragFloat("Pit km/h", &m_authoring.race.pitSpeedKmh, 1.0f, 20.0f, 200.0f, "%.0f");
    ImGui::SameLine();
    int gridTemplateIndex = static_cast<int>(m_authoring.race.gridTemplate);
    const char* gridTemplates[] = { "Staggered 2-wide", "2-wide", "3-wide", "Single file", "Endurance angled" };
    ImGui::SetNextItemWidth(175.0f);
    if (ImGui::Combo("Grid template", &gridTemplateIndex, gridTemplates, IM_ARRAYSIZE(gridTemplates)))
        m_authoring.race.gridTemplate = static_cast<authoring::GridTemplate>(gridTemplateIndex);
    ImGui::SameLine(); ImGui::SetNextItemWidth(95.0f); ImGui::DragFloat("Row m", &m_authoring.race.gridRowSpacingM, 0.1f, 1.0f, 30.0f, "%.1f");
    ImGui::SameLine(); ImGui::SetNextItemWidth(95.0f); ImGui::DragFloat("Width m", &m_authoring.race.gridLateralSpacingM, 0.1f, 1.0f, 12.0f, "%.1f");
    ImGui::SameLine(); ImGui::SetNextItemWidth(95.0f); ImGui::DragFloat("Back m", &m_authoring.race.gridBackOffsetM, 0.1f, 0.0f, 50.0f, "%.1f");

    m_authoring.race.laps = std::max(1, m_authoring.race.laps);
    m_authoring.race.gridSlots = std::clamp(m_authoring.race.gridSlots, 1, 200);
    m_authoring.race.gridRowSpacingM = std::max(0.5f, m_authoring.race.gridRowSpacingM);
    m_authoring.race.gridLateralSpacingM = std::max(0.5f, m_authoring.race.gridLateralSpacingM);
    m_authoring.race.gridBackOffsetM = std::max(0.0f, m_authoring.race.gridBackOffsetM);

    if (ImGui::Button("GENERATE MISSING GRID", ImVec2(190.0f, 28.0f)))
    {
        const authoring::RaceMarker* startMarker = nullptr;
        for (const auto& marker : m_authoring.raceMarkers)
            if (marker.type == authoring::RaceMarkerType::StartFinish) { startMarker = &marker; break; }
        if (!startMarker)
            m_studioMessage = "Add a Start / Finish marker before generating grid slots.";
        else
        {
            const authoring::Vec3 startPosition = startMarker->position;
            const float startHeadingDeg = startMarker->headingDeg;
            const float heading = radians(startHeadingDeg);
            const authoring::Vec3 forward{ std::sin(heading), 0.0f, std::cos(heading) };
            const authoring::Vec3 right{ forward.z, 0.0f, -forward.x };
            std::vector<bool> occupied(static_cast<std::size_t>(m_authoring.race.gridSlots), false);
            for (const auto& marker : m_authoring.raceMarkers)
                if (marker.type == authoring::RaceMarkerType::GridSlot && marker.slot >= 0 && marker.slot < m_authoring.race.gridSlots)
                    occupied[static_cast<std::size_t>(marker.slot)] = true;

            int columns = 2;
            switch (m_authoring.race.gridTemplate)
            {
            case authoring::GridTemplate::ThreeWide: columns = 3; break;
            case authoring::GridTemplate::SingleFile:
            case authoring::GridTemplate::EnduranceAngled: columns = 1; break;
            default: columns = 2; break;
            }
            int createdCount = 0;
            for (int slot = 0; slot < m_authoring.race.gridSlots; ++slot)
            {
                if (occupied[static_cast<std::size_t>(slot)]) continue;
                const int row = slot / columns;
                const int column = slot % columns;
                float lateral = 0.0f;
                if (columns == 2) lateral = (column == 0 ? -0.5f : 0.5f) * m_authoring.race.gridLateralSpacingM;
                else if (columns == 3) lateral = static_cast<float>(column - 1) * m_authoring.race.gridLateralSpacingM;

                float longitudinal = -m_authoring.race.gridBackOffsetM - row * m_authoring.race.gridRowSpacingM;
                if (m_authoring.race.gridTemplate == authoring::GridTemplate::StaggeredTwoWide)
                    longitudinal -= column * std::min(2.0f, m_authoring.race.gridRowSpacingM * 0.35f);

                auto& grid = m_authoring.addRaceMarker(authoring::RaceMarkerType::GridSlot);
                grid.name = "Grid " + std::to_string(slot + 1);
                grid.slot = slot; grid.order = slot; grid.headingDeg = startHeadingDeg;
                if (m_authoring.race.gridTemplate == authoring::GridTemplate::EnduranceAngled)
                {
                    lateral = -4.0f;
                    grid.headingDeg = startHeadingDeg + 25.0f;
                }
                grid.position = add3(startPosition, add3(mul3(forward, longitudinal), mul3(right, lateral)));
                ++createdCount;
            }
            m_studioMessage = "Generated " + std::to_string(createdCount) + " missing "
                + authoring::gridTemplateName(m_authoring.race.gridTemplate) + " grid slot(s); existing slots were preserved.";
        }
    }
    ImGui::SameLine(); ImGui::Checkbox("Formation lap", &m_authoring.race.formationLap);
    ImGui::SameLine(); ImGui::Checkbox("Standing start", &m_authoring.race.standingStart);
    ImGui::SameLine(); ImGui::Checkbox("False-start", &m_authoring.race.falseStartPenalty);
    ImGui::SameLine(); ImGui::Checkbox("Track limits", &m_authoring.race.trackLimitsEnabled);
    ImGui::SameLine(); ImGui::Checkbox("Penalties", &m_authoring.race.penaltiesEnabled);

    const char* tabs[] = { "MARKERS + GRID", "ROUTES + CORRIDORS", "LAYOUTS", "SESSIONS", "RACE CONTROL", "TV CAMERAS", "CONE COURSES" };
    for (int i = 0; i < IM_ARRAYSIZE(tabs); ++i)
    {
        if (i > 0) ImGui::SameLine();
        if (m_raceTab == i) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(tabs[i], ImVec2(i == 1 ? 180.0f : ((i == 5 || i == 6) ? 135.0f : 145.0f), 28.0f)))
        {
            m_raceTab = i;
            m_racePlacementType = -1;
            m_racePlaceRouteNode = false;
            m_raceSupportPlacementType = -1;
            m_raceConePlacementRole = -1;
            m_raceConeGatePlacementType = -1;
        }
        if (m_raceTab == i) ImGui::PopStyleColor();
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float listWidth = 330.0f;
    const float inspectorWidth = 390.0f;
    ImGui::BeginChild("RaceAuthoringList", ImVec2(listWidth, 0.0f), true);

    if (m_raceTab == 0)
    {
        sectionTitle("TIMING / GRID / PIT OBJECTS");
        if (ImGui::BeginTable("RaceAddButtons", 2, ImGuiTableFlags_SizingStretchSame))
        {
            const auto addButton = [&](const char* label, authoring::RaceMarkerType type)
            {
                ImGui::TableNextColumn();
                if (ImGui::Button(label, ImVec2(-1.0f, 27.0f)))
                {
                    m_raceSelectedMarker = static_cast<int>(m_authoring.raceMarkers.size());
                    m_authoring.addRaceMarker(type);
                }
            };
            addButton("+ START / FINISH", authoring::RaceMarkerType::StartFinish);
            addButton("+ CHECKPOINT", authoring::RaceMarkerType::Checkpoint);
            addButton("+ SECTOR", authoring::RaceMarkerType::Sector);
            addButton("+ TIMING LOOP", authoring::RaceMarkerType::TimingLoop);
            addButton("+ SPEED TRAP IN", authoring::RaceMarkerType::SpeedTrapStart);
            addButton("+ SPEED TRAP OUT", authoring::RaceMarkerType::SpeedTrapFinish);
            addButton("+ GRID SLOT", authoring::RaceMarkerType::GridSlot);
            addButton("+ PIT BOX", authoring::RaceMarkerType::PitBox);
            addButton("+ PIT ENTRY", authoring::RaceMarkerType::PitEntry);
            addButton("+ PIT EXIT", authoring::RaceMarkerType::PitExit);
            addButton("+ PIT SPEED LINE", authoring::RaceMarkerType::PitSpeedLine);
            addButton("+ SAFETY CAR LINE", authoring::RaceMarkerType::SafetyCarLine);
            addButton("+ FORMATION LINE", authoring::RaceMarkerType::FormationLine);
            addButton("+ RECOVERY", authoring::RaceMarkerType::Recovery);
            addButton("+ REPLAY CAMERA", authoring::RaceMarkerType::ReplayCamera);
            addButton("+ AI RACE LINE", authoring::RaceMarkerType::AiLineNode);
            addButton("+ AI WET LINE", authoring::RaceMarkerType::WetLineNode);
            addButton("+ TRACK LIMIT L", authoring::RaceMarkerType::TrackLimitLeft);
            addButton("+ TRACK LIMIT R", authoring::RaceMarkerType::TrackLimitRight);
            ImGui::EndTable();
        }
        ImGui::Separator();
        for (int i = 0; i < static_cast<int>(m_authoring.raceMarkers.size()); ++i)
        {
            const auto& marker = m_authoring.raceMarkers[static_cast<std::size_t>(i)];
            std::string label = std::to_string(marker.order) + "  " + marker.name + "  [" + authoring::raceMarkerTypeName(marker.type) + "]##race" + std::to_string(marker.id);
            if (ImGui::Selectable(label.c_str(), m_raceSelectedMarker == i)) m_raceSelectedMarker = i;
        }
    }
    else if (m_raceTab == 1)
    {
        sectionTitle("VENUE ROUTES");
        if (ImGui::Button("+ MAIN", ImVec2(72.0f, 27.0f))) { m_raceSelectedRoute = static_cast<int>(m_authoring.raceRoutes.size()); m_authoring.addRaceRoute(authoring::RaceRouteType::MainCircuit); }
        ImGui::SameLine();
        if (ImGui::Button("+ PIT", ImVec2(65.0f, 27.0f))) { m_raceSelectedRoute = static_cast<int>(m_authoring.raceRoutes.size()); m_authoring.addRaceRoute(authoring::RaceRouteType::PitLane); }
        ImGui::SameLine();
        if (ImGui::Button("+ ALT", ImVec2(65.0f, 27.0f))) { m_raceSelectedRoute = static_cast<int>(m_authoring.raceRoutes.size()); m_authoring.addRaceRoute(authoring::RaceRouteType::AlternateLayout); }
        ImGui::SameLine();
        if (ImGui::Button("+ SC", ImVec2(60.0f, 27.0f))) { m_raceSelectedRoute = static_cast<int>(m_authoring.raceRoutes.size()); m_authoring.addRaceRoute(authoring::RaceRouteType::SafetyCar); }
        for (int i = 0; i < static_cast<int>(m_authoring.raceRoutes.size()); ++i)
        {
            const auto& route = m_authoring.raceRoutes[static_cast<std::size_t>(i)];
            std::string label = route.name + " [" + authoring::raceRouteTypeName(route.type) + "]##route" + std::to_string(route.id);
            if (ImGui::Selectable(label.c_str(), m_raceSelectedRoute == i)) m_raceSelectedRoute = i;
        }
        ImGui::Separator();
        sectionTitle("ORDERED SPLINE NODES");
        if (!m_authoring.raceRoutes.empty())
        {
            m_raceSelectedRoute = std::clamp(m_raceSelectedRoute, 0, static_cast<int>(m_authoring.raceRoutes.size()) - 1);
            const std::uint32_t routeId = m_authoring.raceRoutes[static_cast<std::size_t>(m_raceSelectedRoute)].id;
            std::vector<int> indices;
            for (int i = 0; i < static_cast<int>(m_authoring.raceRouteNodes.size()); ++i)
                if (m_authoring.raceRouteNodes[static_cast<std::size_t>(i)].routeId == routeId) indices.push_back(i);
            std::stable_sort(indices.begin(), indices.end(), [&](int a, int b) { return m_authoring.raceRouteNodes[a].order < m_authoring.raceRouteNodes[b].order; });
            for (int index : indices)
            {
                const auto& node = m_authoring.raceRouteNodes[static_cast<std::size_t>(index)];
                char label[160]{}; std::snprintf(label, sizeof(label), "%03d  Node %u  L%.1f/R%.1f##rnode%u", node.order, node.id, node.leftWidthM, node.rightWidthM, node.id);
                if (ImGui::Selectable(label, m_raceSelectedRouteNode == index)) m_raceSelectedRouteNode = index;
            }
        }
    }
    else if (m_raceTab == 2)
    {
        sectionTitle("VENUE LAYOUTS");
        if (ImGui::Button("+ NEW LAYOUT", ImVec2(-1.0f, 28.0f)))
        {
            m_raceSelectedLayout = static_cast<int>(m_authoring.raceLayouts.size());
            auto& layout = m_authoring.addRaceLayout();
            if (!m_authoring.raceRoutes.empty()) layout.routeId = m_authoring.raceRoutes.front().id;
            for (const auto& marker : m_authoring.raceMarkers)
                if (marker.type == authoring::RaceMarkerType::StartFinish) { layout.startFinishMarkerId = marker.id; break; }
        }
        for (int i = 0; i < static_cast<int>(m_authoring.raceLayouts.size()); ++i)
        {
            const auto& layout = m_authoring.raceLayouts[static_cast<std::size_t>(i)];
            std::string label = layout.name + (layout.enabled ? "" : " [disabled]") + "##layout" + std::to_string(layout.id);
            if (ImGui::Selectable(label.c_str(), m_raceSelectedLayout == i)) m_raceSelectedLayout = i;
        }
        ImGui::Spacing();
        ImGui::TextWrapped("One physical venue can have multiple layouts sharing geometry, pits and timing infrastructure while choosing different route splines.");
    }
    else if (m_raceTab == 3)
    {
        sectionTitle("WEEKEND / SESSION CHAIN");
        if (ImGui::BeginTable("SessionAdd", 2, ImGuiTableFlags_SizingStretchSame))
        {
            const auto addSession = [&](const char* label, authoring::RaceSessionType type)
            {
                ImGui::TableNextColumn();
                if (ImGui::Button(label, ImVec2(-1.0f, 27.0f)))
                {
                    m_raceSelectedSession = static_cast<int>(m_authoring.raceSessions.size());
                    m_authoring.addRaceSession(type);
                }
            };
            addSession("+ PRACTICE", authoring::RaceSessionType::Practice);
            addSession("+ QUALIFYING", authoring::RaceSessionType::Qualifying);
            addSession("+ WARM-UP", authoring::RaceSessionType::Warmup);
            addSession("+ RACE", authoring::RaceSessionType::Race);
            addSession("+ TIME ATTACK", authoring::RaceSessionType::TimeAttack);
            addSession("+ TEST", authoring::RaceSessionType::TestSession);
            ImGui::EndTable();
        }
        if (ImGui::Button("NORMALIZE SESSION ORDER", ImVec2(-1.0f, 27.0f)))
        {
            std::stable_sort(m_authoring.raceSessions.begin(), m_authoring.raceSessions.end(), [](const auto& a, const auto& b) { return a.order < b.order; });
            for (int i = 0; i < static_cast<int>(m_authoring.raceSessions.size()); ++i) m_authoring.raceSessions[static_cast<std::size_t>(i)].order = i;
            m_raceSelectedSession = std::clamp(m_raceSelectedSession, 0, std::max(0, static_cast<int>(m_authoring.raceSessions.size()) - 1));
        }
        for (int i = 0; i < static_cast<int>(m_authoring.raceSessions.size()); ++i)
        {
            const auto& session = m_authoring.raceSessions[static_cast<std::size_t>(i)];
            std::string label = std::to_string(session.order) + "  " + session.name + " [" + authoring::raceSessionTypeName(session.type) + "]##session" + std::to_string(session.id);
            if (ImGui::Selectable(label.c_str(), m_raceSelectedSession == i)) m_raceSelectedSession = i;
        }
    }
    else if (m_raceTab == 4)
    {
        sectionTitle("MARSHAL / RECOVERY INFRASTRUCTURE");
        if (ImGui::BeginTable("SupportAdd", 2, ImGuiTableFlags_SizingStretchSame))
        {
            const auto addSupport = [&](const char* label, authoring::RaceSupportPointType type)
            {
                ImGui::TableNextColumn();
                if (ImGui::Button(label, ImVec2(-1.0f, 27.0f)))
                {
                    m_raceSelectedSupportPoint = static_cast<int>(m_authoring.raceSupportPoints.size());
                    m_authoring.addRaceSupportPoint(type);
                }
            };
            addSupport("+ MARSHAL", authoring::RaceSupportPointType::MarshalPost);
            addSupport("+ RECOVERY", authoring::RaceSupportPointType::RecoveryVehicle);
            addSupport("+ TOW", authoring::RaceSupportPointType::TowTruck);
            addSupport("+ MEDICAL", authoring::RaceSupportPointType::Medical);
            addSupport("+ FIRE", authoring::RaceSupportPointType::FireCrew);
            addSupport("+ RACE CONTROL", authoring::RaceSupportPointType::RaceControl);
            addSupport("+ SAFETY CAR", authoring::RaceSupportPointType::SafetyCarStandby);
            addSupport("+ TIMING", authoring::RaceSupportPointType::TimingEquipment);
            ImGui::EndTable();
        }
        for (int i = 0; i < static_cast<int>(m_authoring.raceSupportPoints.size()); ++i)
        {
            const auto& point = m_authoring.raceSupportPoints[static_cast<std::size_t>(i)];
            std::string label = point.name + " [" + authoring::raceSupportPointTypeName(point.type) + "]##support" + std::to_string(point.id);
            if (ImGui::Selectable(label.c_str(), m_raceSelectedSupportPoint == i)) m_raceSelectedSupportPoint = i;
        }
    }
    else if (m_raceTab == 5)
    {
        sectionTitle("MOVING BROADCAST CAMERA PATHS");
        if (ImGui::BeginTable("BroadcastCameraAdd", 2, ImGuiTableFlags_SizingStretchSame))
        {
            const auto addPath = [&](const char* label, authoring::BroadcastCameraPathType type)
            {
                ImGui::TableNextColumn();
                if (ImGui::Button(label, ImVec2(-1.0f, 27.0f)))
                {
                    m_raceSelectedCameraPath = static_cast<int>(m_authoring.broadcastCameraPaths.size());
                    m_authoring.addBroadcastCameraPath(type);
                    m_raceSelectedCameraNode = 0;
                    m_viewGizmoAxis = -1;
                    m_viewGizmoDragAccumulator = 0.0f;
                }
            };
            addPath("+ DOLLY", authoring::BroadcastCameraPathType::Dolly);
            addPath("+ CRANE", authoring::BroadcastCameraPathType::Crane);
            addPath("+ CABLE CAM", authoring::BroadcastCameraPathType::Cable);
            addPath("+ DRONE", authoring::BroadcastCameraPathType::Drone);
            ImGui::EndTable();
        }
        for (int i = 0; i < static_cast<int>(m_authoring.broadcastCameraPaths.size()); ++i)
        {
            const auto& path = m_authoring.broadcastCameraPaths[static_cast<std::size_t>(i)];
            std::string label = path.name + " [" + authoring::broadcastCameraPathTypeName(path.type) + "]##campath" + std::to_string(path.id);
            if (ImGui::Selectable(label.c_str(), m_raceSelectedCameraPath == i))
            {
                m_raceSelectedCameraPath = i;
                m_viewGizmoAxis = -1;
                m_viewGizmoDragAccumulator = 0.0f;
                for (int nodeIndex = 0; nodeIndex < static_cast<int>(m_authoring.broadcastCameraNodes.size()); ++nodeIndex)
                    if (m_authoring.broadcastCameraNodes[static_cast<std::size_t>(nodeIndex)].pathId == path.id) { m_raceSelectedCameraNode = nodeIndex; break; }
            }
        }
        ImGui::Separator();
        sectionTitle("PATH CONTROL POINTS");
        if (!m_authoring.broadcastCameraPaths.empty())
        {
            m_raceSelectedCameraPath = std::clamp(m_raceSelectedCameraPath, 0, static_cast<int>(m_authoring.broadcastCameraPaths.size()) - 1);
            const auto pathId = m_authoring.broadcastCameraPaths[static_cast<std::size_t>(m_raceSelectedCameraPath)].id;
            std::vector<int> indices;
            for (int i = 0; i < static_cast<int>(m_authoring.broadcastCameraNodes.size()); ++i)
                if (m_authoring.broadcastCameraNodes[static_cast<std::size_t>(i)].pathId == pathId) indices.push_back(i);
            std::stable_sort(indices.begin(), indices.end(), [&](int a, int b) { return m_authoring.broadcastCameraNodes[a].order < m_authoring.broadcastCameraNodes[b].order; });
            for (int index : indices)
            {
                const auto& node = m_authoring.broadcastCameraNodes[static_cast<std::size_t>(index)];
                char label[128]{}; std::snprintf(label, sizeof(label), "%03d  Camera point %u##camnode%u", node.order, node.id, node.id);
                if (ImGui::Selectable(label, m_raceSelectedCameraNode == index)) m_raceSelectedCameraNode = index;
            }
        }
    }
    else if (m_raceTab == 6)
    {
        sectionTitle("AUTHORING TARGET");
        bool targetEventExists = m_raceConeAuthorEventId == 0;
        const char* targetEventPreview = "Persistent / free roam";
        for (const auto& event : m_authoring.gameEvents)
            if (event.id == m_raceConeAuthorEventId) { targetEventExists = true; targetEventPreview = event.name.c_str(); break; }
        if (!targetEventExists) m_raceConeAuthorEventId = 0;
        if (ImGui::BeginCombo("New cones/elements belong to", targetEventPreview))
        {
            if (ImGui::Selectable("Persistent / free roam", m_raceConeAuthorEventId == 0)) m_raceConeAuthorEventId = 0;
            for (const auto& event : m_authoring.gameEvents)
            {
                std::string label = event.name + " [" + authoring::gameEventTypeName(event.type) + "]##coneAuthorEvent" + std::to_string(event.id);
                if (ImGui::Selectable(label.c_str(), m_raceConeAuthorEventId == event.id)) m_raceConeAuthorEventId = event.id;
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Choose persistent for roadworks/traffic guidance, or an event for a temporary course overlay. Then choose a tool and click the 3D scene.");
        bool targetIsConeCourseEvent = false;
        for (const auto& event : m_authoring.gameEvents)
            if (event.id == m_raceConeAuthorEventId && (event.type == authoring::GameEventType::Autoslalom || event.type == authoring::GameEventType::Gymkhana)) { targetIsConeCourseEvent = true; break; }
        ImGui::Separator();
        sectionTitle("PHYSICAL / TRAFFIC CONES");
        if (ImGui::BeginTable("ConeAddButtons", 2, ImGuiTableFlags_SizingStretchSame))
        {
            const auto addCone = [&](const char* label, authoring::ConeRole role)
            {
                ImGui::TableNextColumn();
                const bool active = m_raceConePlacementRole == static_cast<int>(role);
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                if (ImGui::Button(label, ImVec2(-1.0f, 27.0f)))
                {
                    m_raceConePlacementRole = active ? -1 : static_cast<int>(role);
                    m_raceConeGatePlacementType = -1; m_raceConeSelectionKind = 0; m_racePlacementType = -1; m_racePlaceRouteNode = false; m_raceSupportPlacementType = -1;
                    m_studioMessage = active ? "Cone placement cancelled." : "Cone placement armed: click the visible scene surface.";
                }
                if (active) ImGui::PopStyleColor();
            };
            addCone("+ BOUNDARY", authoring::ConeRole::Boundary);
            addCone("+ TRAFFIC GUIDE", authoring::ConeRole::TrafficGuide);
            addCone("+ ROAD CLOSURE", authoring::ConeRole::RoadClosure);
            addCone("+ GATE LEFT", authoring::ConeRole::GateLeft);
            addCone("+ GATE RIGHT", authoring::ConeRole::GateRight);
            addCone("+ SLALOM LEFT", authoring::ConeRole::SlalomLeft);
            addCone("+ SLALOM RIGHT", authoring::ConeRole::SlalomRight);
            addCone("+ START", authoring::ConeRole::Start);
            addCone("+ FINISH", authoring::ConeRole::Finish);
            addCone("+ TURNAROUND", authoring::ConeRole::Turnaround);
            addCone("+ STOP BOX", authoring::ConeRole::StopBox);
            addCone("+ POINTER", authoring::ConeRole::Pointer);
            ImGui::EndTable();
        }
        for (int i=0;i<static_cast<int>(m_authoring.courseCones.size());++i)
        {
            const auto& cone=m_authoring.courseCones[static_cast<std::size_t>(i)];
            std::string label=cone.name+" ["+authoring::coneRoleName(cone.role)+"]##cone"+std::to_string(cone.id);
            if (ImGui::Selectable(label.c_str(), m_raceConeSelectionKind==0 && m_raceSelectedCone==i)) { m_raceConeSelectionKind=0; m_raceSelectedCone=i; m_raceConeAuthorEventId=cone.eventId; }
        }
        ImGui::Separator();
        sectionTitle("INVISIBLE COURSE ELEMENTS");
        if (ImGui::BeginTable("ConeGateAddButtons", 2, ImGuiTableFlags_SizingStretchSame))
        {
            const auto addGate=[&](const char* label,authoring::ConeCourseGateType type)
            {
                ImGui::TableNextColumn();
                const bool active = m_raceConeGatePlacementType == static_cast<int>(type);
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                if (ImGui::Button(label,ImVec2(-1.0f,27.0f)))
                {
                    if (!active && !targetIsConeCourseEvent)
                    {
                        m_raceConeGatePlacementType = -1;
                        m_studioMessage = "Choose an Autoslalom or Gymkhana authoring target before placing course elements.";
                    }
                    else
                    {
                        m_raceConeGatePlacementType = active ? -1 : static_cast<int>(type);
                        m_raceConePlacementRole = -1; m_raceConeSelectionKind=1; m_racePlacementType=-1; m_racePlaceRouteNode=false; m_raceSupportPlacementType=-1;
                        m_studioMessage = active ? "Course-element placement cancelled." : "Course-element placement armed: click the visible scene surface.";
                    }
                }
                if (active) ImGui::PopStyleColor();
            };
            addGate("+ GATE",authoring::ConeCourseGateType::Gate);
            addGate("+ SLALOM LEFT",authoring::ConeCourseGateType::SlalomLeft);
            addGate("+ SLALOM RIGHT",authoring::ConeCourseGateType::SlalomRight);
            addGate("+ TURN LEFT",authoring::ConeCourseGateType::TurnaroundLeft);
            addGate("+ TURN RIGHT",authoring::ConeCourseGateType::TurnaroundRight);
            addGate("+ STOP BOX",authoring::ConeCourseGateType::StopBox);
            addGate("+ FINISH",authoring::ConeCourseGateType::Finish);
            addGate("+ 360 LEFT",authoring::ConeCourseGateType::CircleLeft);
            addGate("+ 360 RIGHT",authoring::ConeCourseGateType::CircleRight);
            ImGui::EndTable();
        }
        std::vector<int> gateIndices; gateIndices.reserve(m_authoring.coneCourseGates.size());
        for (int i=0;i<static_cast<int>(m_authoring.coneCourseGates.size());++i) gateIndices.push_back(i);
        std::stable_sort(gateIndices.begin(),gateIndices.end(),[&](int a,int b){ return m_authoring.coneCourseGates[a].order<m_authoring.coneCourseGates[b].order; });
        for (int i:gateIndices)
        {
            const auto& gate=m_authoring.coneCourseGates[static_cast<std::size_t>(i)];
            char label[192]{}; std::snprintf(label,sizeof(label),"%03d  %s [%s]##conegate%u",gate.order,gate.name.c_str(),authoring::coneCourseGateTypeName(gate.type),gate.id);
            if (ImGui::Selectable(label,m_raceConeSelectionKind==1 && m_raceSelectedConeGate==i)) { m_raceConeSelectionKind=1; m_raceSelectedConeGate=i; m_raceConeAuthorEventId=gate.eventId; }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("RaceViewport", ImVec2(std::max(320.0f, available.x - listWidth - inspectorWidth - 16.0f), 0.0f), true);
    drawRaceViewportInteractive();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("RaceInspector", ImVec2(0.0f, 0.0f), true);

    const auto markerCombo = [&](const char* label, std::uint32_t& markerId)
    {
        const char* preview = "(none)";
        for (const auto& marker : m_authoring.raceMarkers) if (marker.id == markerId) { preview = marker.name.c_str(); break; }
        if (ImGui::BeginCombo(label, preview))
        {
            if (ImGui::Selectable("(none)", markerId == 0)) markerId = 0;
            for (const auto& marker : m_authoring.raceMarkers)
            {
                std::string l = marker.name + " [" + authoring::raceMarkerTypeName(marker.type) + "]##mcombo" + std::to_string(marker.id) + label;
                if (ImGui::Selectable(l.c_str(), marker.id == markerId)) markerId = marker.id;
            }
            ImGui::EndCombo();
        }
    };
    const auto routeCombo = [&](const char* label, std::uint32_t& routeId)
    {
        const char* preview = "(none)";
        for (const auto& route : m_authoring.raceRoutes) if (route.id == routeId) { preview = route.name.c_str(); break; }
        if (ImGui::BeginCombo(label, preview))
        {
            if (ImGui::Selectable("(none)", routeId == 0)) routeId = 0;
            for (const auto& route : m_authoring.raceRoutes)
            {
                std::string l = route.name + " [" + authoring::raceRouteTypeName(route.type) + "]##rcombo" + std::to_string(route.id) + label;
                if (ImGui::Selectable(l.c_str(), route.id == routeId)) routeId = route.id;
            }
            ImGui::EndCombo();
        }
    };

    const auto layoutCombo = [&](const char* label, std::uint32_t& layoutId)
    {
        const char* preview = "Global / all layouts";
        for (const auto& layout : m_authoring.raceLayouts) if (layout.id == layoutId) { preview = layout.name.c_str(); break; }
        if (ImGui::BeginCombo(label, preview))
        {
            if (ImGui::Selectable("Global / all layouts", layoutId == 0)) layoutId = 0;
            for (const auto& layout : m_authoring.raceLayouts)
            {
                std::string l = layout.name + "##layoutscope" + std::to_string(layout.id) + label;
                if (ImGui::Selectable(l.c_str(), layout.id == layoutId)) layoutId = layout.id;
            }
            ImGui::EndCombo();
        }
    };

    if (m_raceTab == 0)
    {
        sectionTitle("RACE OBJECT INSPECTOR");
        if (!m_authoring.raceMarkers.empty())
        {
            m_raceSelectedMarker = std::clamp(m_raceSelectedMarker, 0, static_cast<int>(m_authoring.raceMarkers.size()) - 1);
            auto& marker = m_authoring.raceMarkers[static_cast<std::size_t>(m_raceSelectedMarker)];
            inputString("Name", marker.name);
            int typeIndex = static_cast<int>(marker.type);
            const char* types[] = { "Start / Finish", "Checkpoint", "Sector", "Grid Slot", "Pit Entry", "Pit Exit", "Pit Speed Line", "Pit Box", "Track Limit Left", "Track Limit Right", "Recovery", "Replay Camera", "AI Race Line", "AI Wet Line", "Timing Loop", "Speed Trap Start", "Speed Trap Finish", "Safety Car Line", "Formation Line" };
            if (ImGui::Combo("Type", &typeIndex, types, IM_ARRAYSIZE(types))) marker.type = static_cast<authoring::RaceMarkerType>(typeIndex);
            ImGui::DragFloat3("Position", &marker.position.x, 0.05f);
            ImGui::DragFloat("Heading deg", &marker.headingDeg, 0.25f, -360.0f, 360.0f);
            ImGui::DragFloat(marker.type == authoring::RaceMarkerType::ReplayCamera ? "Camera activation radius m" : "Trigger radius m", &marker.radiusM, 0.1f, 0.1f, 500.0f);
            const bool timingGate = marker.type == authoring::RaceMarkerType::StartFinish || marker.type == authoring::RaceMarkerType::Checkpoint
                || marker.type == authoring::RaceMarkerType::Sector || marker.type == authoring::RaceMarkerType::TimingLoop
                || marker.type == authoring::RaceMarkerType::SpeedTrapStart || marker.type == authoring::RaceMarkerType::SpeedTrapFinish
                || marker.type == authoring::RaceMarkerType::PitEntry || marker.type == authoring::RaceMarkerType::PitExit
                || marker.type == authoring::RaceMarkerType::PitSpeedLine || marker.type == authoring::RaceMarkerType::SafetyCarLine
                || marker.type == authoring::RaceMarkerType::FormationLine;
            if (timingGate)
            {
                ImGui::DragFloat("Gate width m", &marker.gateWidthM, 0.1f, 0.5f, 100.0f);
                ImGui::DragFloat("Gate height m", &marker.gateHeightM, 0.1f, 0.5f, 20.0f);
                ImGui::Checkbox("Require correct crossing direction", &marker.directionRequired);
            }
            layoutCombo("Layout scope", marker.layoutId);
            ImGui::InputInt("Order", &marker.order);
            if (marker.type == authoring::RaceMarkerType::GridSlot || marker.type == authoring::RaceMarkerType::PitBox) ImGui::InputInt("Slot", &marker.slot);
            if (marker.type == authoring::RaceMarkerType::PitSpeedLine) ImGui::DragFloat("Speed limit km/h", &marker.speedLimitKmh, 1.0f, 0.0f, 250.0f);
            if (marker.type == authoring::RaceMarkerType::ReplayCamera)
            {
                if (ImGui::Button("SET REPLAY CAMERA FROM CURRENT VIEW", ImVec2(-1.0f, 30.0f)))
                {
                    const StudioViewportProjection cameraView = makeViewportProjection(
                        ImVec2(0.0f, 0.0f), ImVec2(100.0f, 100.0f),
                        m_viewYawDeg, m_viewPitchDeg, m_viewDistanceM, m_viewTarget, false);
                    marker.position = cameraView.camera;
                    marker.headingDeg = std::atan2(cameraView.forward.x, cameraView.forward.z) * (180.0f / kPi);
                    m_studioMessage = "Replay camera snapped to the current Studio viewport. Runtime Trackside review will use it when the incident/racer is inside its activation radius.";
                }
                ImGui::TextWrapped("STUDIO26 authored broadcast camera: fly to the shot with Shift+`, confirm the view, then press the button above. Trackside replay selects the nearest applicable Replay Camera inside its activation radius and pans it toward the reviewed cars; procedural trackside framing remains the fallback.");
            }
            else
            {
                ImGui::Spacing();
                ImGui::TextWrapped("Timing objects are directional gates with authored width/height; their heading defines legal crossing direction. Layout scope keeps alternate circuit/street layouts from consuming each other's checkpoints, grids or pit gates. Global markers remain shared by every layout.");
            }
            if (ImGui::Button("DELETE RACE OBJECT", ImVec2(-1.0f, 30.0f)))
            {
                m_authoring.removeRaceMarker(static_cast<std::size_t>(m_raceSelectedMarker));
                m_raceSelectedMarker = std::max(0, m_raceSelectedMarker - 1);
            }
        }
    }
    else if (m_raceTab == 1)
    {
        sectionTitle("ROUTE / SPLINE INSPECTOR");
        if (!m_authoring.raceRoutes.empty())
        {
            m_raceSelectedRoute = std::clamp(m_raceSelectedRoute, 0, static_cast<int>(m_authoring.raceRoutes.size()) - 1);
            auto& route = m_authoring.raceRoutes[static_cast<std::size_t>(m_raceSelectedRoute)];
            inputString("Route name", route.name);
            int type = static_cast<int>(route.type);
            const char* routeTypes[] = { "Main Circuit", "Pit Lane", "Safety Car", "Formation", "Alternate Layout", "Sprint", "Hillclimb", "Drag" };
            if (ImGui::Combo("Route type", &type, routeTypes, IM_ARRAYSIZE(routeTypes))) route.type = static_cast<authoring::RaceRouteType>(type);
            ImGui::Checkbox("Enabled", &route.enabled);
            ImGui::Checkbox("Closed loop", &route.closedLoop);
            ImGui::Checkbox("Reverse allowed", &route.reverseAllowed);
            ImGui::DragFloat("Default left corridor m", &route.defaultLeftWidthM, 0.1f, 0.5f, 50.0f);
            ImGui::DragFloat("Default right corridor m", &route.defaultRightWidthM, 0.1f, 0.5f, 50.0f);
            if (ImGui::Button(m_racePlaceRouteNode ? "CANCEL NODE PLACEMENT" : "PLACE ROUTE NODE", ImVec2(-1.0f, 28.0f)))
            {
                m_racePlaceRouteNode = !m_racePlaceRouteNode;
                m_racePlacementType = -1;
            }
            if (ImGui::Button("ADD NODE AT VIEW TARGET", ImVec2(-1.0f, 27.0f)))
            {
                auto& node = m_authoring.addRaceRouteNode(route.id); node.position = m_viewTarget;
                m_raceSelectedRouteNode = static_cast<int>(m_authoring.raceRouteNodes.size()) - 1;
            }
            if (ImGui::Button("AUTO-SMOOTH ROUTE TANGENTS", ImVec2(-1.0f, 27.0f)))
            {
                std::vector<authoring::RaceRouteNode*> nodes;
                for (auto& node : m_authoring.raceRouteNodes) if (node.routeId == route.id) nodes.push_back(&node);
                std::stable_sort(nodes.begin(), nodes.end(), [](const auto* a, const auto* b) { return a->order < b->order; });
                for (std::size_t i = 0; i < nodes.size(); ++i)
                {
                    if (!nodes[i]->automaticTangents || nodes.size() < 2) continue;
                    const bool closed = route.closedLoop && nodes.size() > 2;
                    const authoring::Vec3 prev = (i > 0) ? nodes[i - 1]->position : (closed ? nodes.back()->position : nodes[i]->position);
                    const authoring::Vec3 next = (i + 1 < nodes.size()) ? nodes[i + 1]->position : (closed ? nodes.front()->position : nodes[i]->position);
                    const authoring::Vec3 tangent = mul3(sub3(next, prev), 0.25f);
                    nodes[i]->handleIn = mul3(tangent, -1.0f);
                    nodes[i]->handleOut = tangent;
                }
                m_studioMessage = "Auto-smoothed tangent handles for route '" + route.name + "'.";
            }
            ImGui::Separator();
            if (!m_authoring.raceRouteNodes.empty())
            {
                m_raceSelectedRouteNode = std::clamp(m_raceSelectedRouteNode, 0, static_cast<int>(m_authoring.raceRouteNodes.size()) - 1);
                auto& node = m_authoring.raceRouteNodes[static_cast<std::size_t>(m_raceSelectedRouteNode)];
                if (node.routeId == route.id)
                {
                    sectionTitle("SELECTED SPLINE NODE");
                    ImGui::InputInt("Node order", &node.order);
                    ImGui::DragFloat3("Position", &node.position.x, 0.05f);
                    ImGui::Checkbox("Automatic tangents", &node.automaticTangents);
                    ImGui::DragFloat3("Bezier handle in", &node.handleIn.x, 0.05f);
                    ImGui::DragFloat3("Bezier handle out", &node.handleOut.x, 0.05f);
                    ImGui::DragFloat("Left track-limit corridor m", &node.leftWidthM, 0.1f, 0.0f, 100.0f);
                    ImGui::DragFloat("Right track-limit corridor m", &node.rightWidthM, 0.1f, 0.0f, 100.0f);
                    ImGui::DragFloat("Target speed km/h", &node.targetSpeedKmh, 1.0f, 0.0f, 500.0f);
                    ImGui::DragFloat("Banking deg", &node.bankingDeg, 0.1f, -45.0f, 45.0f);
                    ImGui::Checkbox("Overtaking preferred", &node.overtakingPreferred);
                    if (ImGui::Button("DELETE SELECTED NODE", ImVec2(-1.0f, 27.0f)))
                    {
                        m_authoring.removeRaceRouteNode(static_cast<std::size_t>(m_raceSelectedRouteNode));
                        m_raceSelectedRouteNode = std::max(0, m_raceSelectedRouteNode - 1);
                    }
                }
            }
            ImGui::Spacing();
            if (ImGui::Button("DELETE ROUTE + ITS NODES", ImVec2(-1.0f, 28.0f)))
            {
                m_authoring.removeRaceRoute(static_cast<std::size_t>(m_raceSelectedRoute));
                m_raceSelectedRoute = std::max(0, m_raceSelectedRoute - 1);
            }
        }
    }
    else if (m_raceTab == 2)
    {
        sectionTitle("LAYOUT INSPECTOR");
        if (!m_authoring.raceLayouts.empty())
        {
            m_raceSelectedLayout = std::clamp(m_raceSelectedLayout, 0, static_cast<int>(m_authoring.raceLayouts.size()) - 1);
            auto& layout = m_authoring.raceLayouts[static_cast<std::size_t>(m_raceSelectedLayout)];
            inputString("Name", layout.name); ImGui::Checkbox("Enabled", &layout.enabled);
            routeCombo("Race route", layout.routeId); routeCombo("Pit route", layout.pitRouteId); markerCombo("Start / Finish", layout.startFinishMarkerId);
            ImGui::InputInt("Default laps", &layout.defaultLaps); layout.defaultLaps = std::max(1, layout.defaultLaps);
            ImGui::Checkbox("Reverse", &layout.reverse); ImGui::Checkbox("Pits enabled", &layout.pitsEnabled);
            ImGui::Spacing();
            ImGui::TextWrapped("Layouts reference reusable route splines. A club layout, GP layout, reverse layout or endurance variant can share the same physical scene without duplicating venue geometry.");
            if (ImGui::Button("DUPLICATE AS REVERSE", ImVec2(-1.0f, 27.0f)))
            {
                const auto copy = layout;
                auto& duplicate = m_authoring.addRaceLayout();
                const std::uint32_t newId = duplicate.id;
                duplicate = copy; duplicate.id = newId; duplicate.name = copy.name + " Reverse"; duplicate.reverse = !copy.reverse;
                m_raceSelectedLayout = static_cast<int>(m_authoring.raceLayouts.size()) - 1;
            }
            if (ImGui::Button("DELETE LAYOUT", ImVec2(-1.0f, 27.0f)))
            {
                m_authoring.removeRaceLayout(static_cast<std::size_t>(m_raceSelectedLayout)); m_raceSelectedLayout = std::max(0, m_raceSelectedLayout - 1);
            }
        }
    }
    else if (m_raceTab == 3)
    {
        sectionTitle("SESSION INSPECTOR");
        if (!m_authoring.raceSessions.empty())
        {
            m_raceSelectedSession = std::clamp(m_raceSelectedSession, 0, static_cast<int>(m_authoring.raceSessions.size()) - 1);
            auto& session = m_authoring.raceSessions[static_cast<std::size_t>(m_raceSelectedSession)];
            inputString("Name", session.name); ImGui::Checkbox("Enabled", &session.enabled);
            int type = static_cast<int>(session.type);
            const char* sessionTypes[] = { "Practice", "Qualifying", "Warm-up", "Race", "Time Attack", "Test Session" };
            if (ImGui::Combo("Type", &type, sessionTypes, IM_ARRAYSIZE(sessionTypes))) session.type = static_cast<authoring::RaceSessionType>(type);
            ImGui::InputInt("Order", &session.order);
            ImGui::InputInt("Duration minutes (0 = lap based)", &session.durationMinutes); session.durationMinutes = std::max(0, session.durationMinutes);
            ImGui::InputInt("Laps (0 = time based)", &session.laps); session.laps = std::max(0, session.laps);
            ImGui::InputInt("Mandatory pit stops", &session.mandatoryPitStops); session.mandatoryPitStops = std::max(0, session.mandatoryPitStops);
            ImGui::Checkbox("Formation lap", &session.formationLap); ImGui::Checkbox("Rolling start", &session.rollingStart);
            ImGui::Checkbox("Weather may change", &session.weatherChangeAllowed);
            ImGui::SliderFloat("Starting fuel %", &session.startingFuelPercent, 0.0f, 100.0f, "%.0f%%");
            ImGui::Separator();
            ImGui::TextDisabled("ENDURANCE / PIT SERVICE");
            ImGui::Checkbox("Timed race format", &session.timedRace);
            ImGui::Checkbox("Time + one lap", &session.timePlusOneLap);
            ImGui::InputInt("Maximum stint minutes (0 = unlimited)", &session.maximumStintMinutes); session.maximumStintMinutes = std::max(0, session.maximumStintMinutes);
            ImGui::Checkbox("Refueling allowed", &session.refuelingAllowed);
            ImGui::Checkbox("Tire changes allowed", &session.tireChangesAllowed);
            ImGui::Checkbox("Mandatory tire change", &session.mandatoryTireChange);
            ImGui::DragFloat("Minimum pit-service seconds", &session.minimumPitServiceSeconds, 0.25f, 0.0f, 600.0f, "%.1f s");
            ImGui::SliderFloat("Classification distance %", &session.classificationPercent, 0.0f, 100.0f, "%.0f%%");
            int gridSource = static_cast<int>(session.gridSource);
            const char* gridSources[] = { "Event order", "Previous session", "Qualifying", "Championship", "Reverse top N" };
            if (ImGui::Combo("Grid source", &gridSource, gridSources, IM_ARRAYSIZE(gridSources))) session.gridSource = static_cast<authoring::SessionGridSource>(gridSource);
            if (session.gridSource == authoring::SessionGridSource::ReverseTopN) { ImGui::InputInt("Reverse top N", &session.reverseTopN); session.reverseTopN = std::max(0, session.reverseTopN); }
            ImGui::Spacing();
            ImGui::TextWrapped("This is the persistent session chain for practice/qualifying/warm-up/race or test weekends. Runtime can consume the ordered list directly.");
            if (ImGui::Button("DELETE SESSION", ImVec2(-1.0f, 27.0f)))
            {
                m_authoring.removeRaceSession(static_cast<std::size_t>(m_raceSelectedSession)); m_raceSelectedSession = std::max(0, m_raceSelectedSession - 1);
            }
        }
    }
    else if (m_raceTab == 4)
    {
        sectionTitle("RACE DIRECTOR RULES");
        auto& control = m_authoring.raceControl;
        ImGui::Checkbox("Local yellow", &control.localYellow);
        ImGui::Checkbox("Full-course yellow", &control.fullCourseYellow);
        ImGui::Checkbox("Virtual Safety Car", &control.virtualSafetyCar);
        ImGui::Checkbox("Safety Car", &control.safetyCar);
        ImGui::Checkbox("Red flag", &control.redFlag);
        ImGui::Checkbox("Blue flags", &control.blueFlags);
        ImGui::Checkbox("Pit lane open during Safety Car", &control.pitLaneOpenDuringSafetyCar);
        ImGui::InputInt("Track-limit warnings", &control.maxTrackLimitWarnings); control.maxTrackLimitWarnings = std::max(0, control.maxTrackLimitWarnings);
        ImGui::InputInt("Drive-through after warnings", &control.driveThroughAfterWarnings); control.driveThroughAfterWarnings = std::max(0, control.driveThroughAfterWarnings);
        ImGui::InputInt("Pit window start lap", &control.pitWindowStartLap); control.pitWindowStartLap = std::max(0, control.pitWindowStartLap);
        ImGui::InputInt("Pit window end lap", &control.pitWindowEndLap); control.pitWindowEndLap = std::max(0, control.pitWindowEndLap);
        routeCombo("Safety-car route", control.safetyCarRouteId); markerCombo("Restart line", control.restartMarkerId);
        ImGui::Separator();
        sectionTitle("SUPPORT POINT INSPECTOR");
        if (!m_authoring.raceSupportPoints.empty())
        {
            m_raceSelectedSupportPoint = std::clamp(m_raceSelectedSupportPoint, 0, static_cast<int>(m_authoring.raceSupportPoints.size()) - 1);
            auto& point = m_authoring.raceSupportPoints[static_cast<std::size_t>(m_raceSelectedSupportPoint)];
            inputString("Name", point.name); ImGui::Checkbox("Enabled", &point.enabled);
            int type = static_cast<int>(point.type);
            const char* types[] = { "Marshal Post", "Recovery Vehicle", "Tow Truck", "Medical", "Fire Crew", "Race Control", "Safety Car Standby", "Timing Equipment" };
            if (ImGui::Combo("Type", &type, types, IM_ARRAYSIZE(types))) point.type = static_cast<authoring::RaceSupportPointType>(type);
            ImGui::DragFloat3("Position", &point.position.x, 0.05f); ImGui::DragFloat("Heading deg", &point.headingDeg, 0.25f, -360.0f, 360.0f);
            ImGui::DragFloat("Service radius m", &point.serviceRadiusM, 0.5f, 1.0f, 1000.0f); ImGui::InputInt("Sector", &point.sector);
            if (ImGui::Button("PLACE THIS TYPE IN VIEWPORT", ImVec2(-1.0f, 27.0f)))
            {
                m_raceSupportPlacementType = static_cast<int>(point.type); m_racePlaceRouteNode = false; m_racePlacementType = -1;
            }
            if (ImGui::Button("DELETE SUPPORT POINT", ImVec2(-1.0f, 27.0f)))
            {
                m_authoring.removeRaceSupportPoint(static_cast<std::size_t>(m_raceSelectedSupportPoint)); m_raceSelectedSupportPoint = std::max(0, m_raceSelectedSupportPoint - 1);
            }
        }
        ImGui::Spacing();
        ImGui::TextWrapped("Race-control data is authored now even though flag-state simulation will be implemented incrementally. Nothing here replaces existing penalty or event rules; it gives them a proper venue-level authority to bind to.");
    }
    else if (m_raceTab == 5)
    {
        sectionTitle("BROADCAST CAMERA PATH INSPECTOR");
        if (!m_authoring.broadcastCameraPaths.empty())
        {
            m_raceSelectedCameraPath = std::clamp(m_raceSelectedCameraPath, 0, static_cast<int>(m_authoring.broadcastCameraPaths.size()) - 1);
            auto& path = m_authoring.broadcastCameraPaths[static_cast<std::size_t>(m_raceSelectedCameraPath)];
            inputString("Name", path.name);
            ImGui::Checkbox("Enabled", &path.enabled);
            int type = static_cast<int>(path.type);
            const char* types[] = { "Dolly", "Crane", "Cable", "Drone" };
            if (ImGui::Combo("Rig type", &type, types, IM_ARRAYSIZE(types))) path.type = static_cast<authoring::BroadcastCameraPathType>(type);
            layoutCombo("Layout scope", path.layoutId);
            ImGui::DragFloat("Activation radius m", &path.activationRadiusM, 1.0f, 5.0f, 2000.0f, "%.0f m");
            ImGui::DragFloat("Move duration s", &path.durationSeconds, 0.1f, 0.5f, 60.0f, "%.1f s");
            ImGui::SliderFloat("Ease", &path.easing, 0.0f, 1.0f, "%.2f");
            ImGui::Checkbox("Reverse path", &path.reverse);
            ImGui::Separator();
            if (ImGui::Button("ADD CONTROL POINT FROM CURRENT VIEW", ImVec2(-1.0f, 30.0f)))
            {
                const StudioViewportProjection cameraView = makeViewportProjection(
                    ImVec2(0.0f, 0.0f), ImVec2(100.0f, 100.0f),
                    m_viewYawDeg, m_viewPitchDeg, m_viewDistanceM, m_viewTarget, false);
                auto& node = m_authoring.addBroadcastCameraNode(path.id);
                node.position = cameraView.camera;
                m_raceSelectedCameraNode = static_cast<int>(m_authoring.broadcastCameraNodes.size()) - 1;
                m_studioMessage = "Added moving broadcast-camera control point from the current Studio view.";
            }
            ImGui::TextWrapped("Use Shift+` to fly exactly where the TV camera should travel, confirm, then add a control point. Two or more points form a smooth Catmull-Rom camera path. During replay the path position moves while the camera continuously pans toward the reviewed cars.");
            if (!m_authoring.broadcastCameraNodes.empty())
            {
                m_raceSelectedCameraNode = std::clamp(m_raceSelectedCameraNode, 0, static_cast<int>(m_authoring.broadcastCameraNodes.size()) - 1);
                auto& node = m_authoring.broadcastCameraNodes[static_cast<std::size_t>(m_raceSelectedCameraNode)];
                if (node.pathId == path.id)
                {
                    sectionTitle("SELECTED CAMERA POINT");
                    ImGui::InputInt("Order", &node.order);
                    ImGui::DragFloat3("Position", &node.position.x, 0.05f);
                    if (ImGui::Button("SNAP POINT TO CURRENT VIEW", ImVec2(-1.0f, 27.0f)))
                    {
                        const StudioViewportProjection cameraView = makeViewportProjection(
                            ImVec2(0.0f, 0.0f), ImVec2(100.0f, 100.0f),
                            m_viewYawDeg, m_viewPitchDeg, m_viewDistanceM, m_viewTarget, false);
                        node.position = cameraView.camera;
                    }
                    if (ImGui::Button("DELETE CAMERA POINT", ImVec2(-1.0f, 27.0f)))
                    {
                        m_authoring.removeBroadcastCameraNode(static_cast<std::size_t>(m_raceSelectedCameraNode));
                        m_raceSelectedCameraNode = std::max(0, m_raceSelectedCameraNode - 1);
                        m_viewGizmoAxis = -1;
                        m_viewGizmoDragAccumulator = 0.0f;
                    }
                }
            }
            ImGui::Spacing();
            if (ImGui::Button("DELETE CAMERA PATH + POINTS", ImVec2(-1.0f, 29.0f)))
            {
                m_authoring.removeBroadcastCameraPath(static_cast<std::size_t>(m_raceSelectedCameraPath));
                m_raceSelectedCameraPath = std::max(0, m_raceSelectedCameraPath - 1);
                m_raceSelectedCameraNode = 0;
                m_viewGizmoAxis = -1;
                m_viewGizmoDragAccumulator = 0.0f;
            }
        }
        else ImGui::TextDisabled("Add a Dolly, Crane, Cable Cam or Drone path to author a moving replay/broadcast shot.");
    }
    else if (m_raceTab == 6)
    {
        sectionTitle("CONE COURSE / TRAFFIC POLICY");
        auto& config=m_authoring.coneCourse;
        ImGui::Checkbox("Cone system enabled",&config.enabled);
        inputString("Default cone GLB",config.defaultAssetPath);
        ImGui::DragFloat("Minimum hit impulse N.s",&config.minimumContactImpulseNs,0.1f,0.0f,1000.0f,"%.1f");
        ImGui::DragFloat("Default cone penalty s",&config.defaultHitPenaltySeconds,0.1f,0.0f,60.0f,"%.1f");
        ImGui::DragFloat("Default displacement m",&config.defaultDisplacementToleranceM,0.01f,0.01f,2.0f,"%.2f");
        ImGui::DragFloat("Wrong element penalty s",&config.wrongElementPenaltySeconds,0.25f,0.0f,120.0f,"%.1f");
        ImGui::Checkbox("Missed element = DNF",&config.missedElementDnf);
        ImGui::Checkbox("Reset event cones at start",&config.resetEventConesOnStart);
        ImGui::Checkbox("Bookmark cone hits in replay",&config.recordConeHitsToReplay);
        ImGui::Checkbox("Event cones only visible while active",&config.eventConesVisibleOnlyWhileActive);
        ImGui::Separator();

        const auto eventCombo=[&](const char* label,std::uint32_t& eventId)
        {
            const char* preview="Free roam / persistent";
            for (const auto& e:m_authoring.gameEvents) if (e.id==eventId) { preview=e.name.c_str(); break; }
            if (ImGui::BeginCombo(label,preview))
            {
                if (ImGui::Selectable("Free roam / persistent",eventId==0)) eventId=0;
                for (const auto& e:m_authoring.gameEvents)
                {
                    std::string l=e.name+" ["+authoring::gameEventTypeName(e.type)+"]##coneevent"+std::to_string(e.id)+label;
                    if (ImGui::Selectable(l.c_str(),eventId==e.id)) eventId=e.id;
                }
                ImGui::EndCombo();
            }
        };
        const auto coneCombo=[&](const char* label,std::uint32_t& coneId)
        {
            const char* preview="(none)";
            for (const auto& c:m_authoring.courseCones) if (c.id==coneId) { preview=c.name.c_str(); break; }
            if (ImGui::BeginCombo(label,preview))
            {
                if (ImGui::Selectable("(none)",coneId==0)) coneId=0;
                for (const auto& c:m_authoring.courseCones)
                {
                    std::string l=c.name+" ["+authoring::coneRoleName(c.role)+"]##conecombo"+std::to_string(c.id)+label;
                    if (ImGui::Selectable(l.c_str(),coneId==c.id)) coneId=c.id;
                }
                ImGui::EndCombo();
            }
        };

        if (ImGui::CollapsingHeader("QUICK COURSE BUILDERS", ImGuiTreeNodeFlags_DefaultOpen))
        {
            static int quickSlalomCount = 8;
            static float quickSpacingM = 10.0f;
            static float quickWidthM = 5.0f;
            static float quickStopLengthM = 5.0f;
            static float quickTaperLateralM = 0.0f;
            static float quickHeadingDeg = 0.0f;
            static bool quickStartLeft = true;
            ImGui::InputInt("Builder count##quick", &quickSlalomCount); quickSlalomCount = std::clamp(quickSlalomCount, 1, 64);
            ImGui::DragFloat("Element spacing m##quick", &quickSpacingM, 0.25f, 1.0f, 100.0f, "%.2f");
            ImGui::DragFloat("Gate / box width m##quick", &quickWidthM, 0.10f, 1.0f, 20.0f, "%.2f");
            ImGui::DragFloat("Stop-box length m##quick", &quickStopLengthM, 0.10f, 1.0f, 30.0f, "%.2f");
            ImGui::DragFloat("Line/taper lateral shift m##quick", &quickTaperLateralM, 0.10f, -20.0f, 20.0f, "%.2f");
            ImGui::DragFloat("Builder heading deg##quick", &quickHeadingDeg, 0.25f, -360.0f, 360.0f, "%.1f");
            ImGui::Checkbox("First slalom passes left##quick", &quickStartLeft);
            if (ImGui::Button("HEADING FROM CURRENT VIEW", ImVec2(-1.0f, 26.0f)))
            {
                quickHeadingDeg = m_viewYawDeg + 180.0f;
                while (quickHeadingDeg > 180.0f) quickHeadingDeg -= 360.0f;
                while (quickHeadingDeg < -180.0f) quickHeadingDeg += 360.0f;
            }

            authoring::Vec3 quickOrigin = m_viewTarget;
            const char* originLabel = "current 3D view target";
            if (m_raceConeSelectionKind == 0 && !m_authoring.courseCones.empty())
            {
                const int index = std::clamp(m_raceSelectedCone, 0, static_cast<int>(m_authoring.courseCones.size()) - 1);
                quickOrigin = m_authoring.courseCones[static_cast<std::size_t>(index)].position; originLabel = "selected cone";
            }
            else if (m_raceConeSelectionKind == 1 && !m_authoring.coneCourseGates.empty())
            {
                const int index = std::clamp(m_raceSelectedConeGate, 0, static_cast<int>(m_authoring.coneCourseGates.size()) - 1);
                quickOrigin = m_authoring.coneCourseGates[static_cast<std::size_t>(index)].position; originLabel = "selected course element";
            }
            ImGui::TextDisabled("Builder origin: %s  (%.2f, %.2f, %.2f)", originLabel, quickOrigin.x, quickOrigin.y, quickOrigin.z);

            bool builderEventOk = false;
            for (const auto& event : m_authoring.gameEvents)
                if (event.id == m_raceConeAuthorEventId && (event.type == authoring::GameEventType::Autoslalom || event.type == authoring::GameEventType::Gymkhana)) { builderEventOk = true; break; }
            if (!builderEventOk) ImGui::TextDisabled("Choose an Autoslalom/Gymkhana authoring target before generating rule-bearing course pieces.");

            const auto snapCoursePoint = [&](authoring::Vec3 point)
            {
                if (m_scenePreview && m_scenePreview->loaded())
                {
                    authoring::Vec3 hit{}; const authoring::Vec3 down{0.0f,-1.0f,0.0f};
                    authoring::Vec3 origin{point.x, point.y + 5.0f, point.z};
                    if (m_scenePreview->raycast(origin, down, hit)) return hit;
                    origin.y = point.y + 25.0f;
                    if (m_scenePreview->raycast(origin, down, hit)) return hit;
                }
                return point;
            };
            const auto nextCourseOrder = [&]()
            {
                int order = 0;
                for (const auto& existing : m_authoring.coneCourseGates) if (existing.eventId == m_raceConeAuthorEventId) order = std::max(order, existing.order + 1);
                return order;
            };
            const auto addBuilderCone = [&](authoring::ConeRole role, const authoring::Vec3& position, const char* name)
            {
                auto& cone = m_authoring.addCourseCone(role, name); cone.eventId = m_raceConeAuthorEventId; cone.position = snapCoursePoint(position); cone.headingDeg = quickHeadingDeg; return cone.id;
            };
            const auto addBuilderGate = [&](authoring::ConeCourseGateType type, const authoring::Vec3& position, const char* name, std::uint32_t leftConeId, std::uint32_t rightConeId)
            {
                const int order = nextCourseOrder();
                auto& gate = m_authoring.addConeCourseGate(type, name); gate.eventId = m_raceConeAuthorEventId; gate.order = order; gate.position = snapCoursePoint(position); gate.headingDeg = quickHeadingDeg; gate.widthM = quickWidthM;
                gate.leftConeId = leftConeId; gate.rightConeId = rightConeId; return gate.id;
            };
            const float heading = radians(quickHeadingDeg);
            const authoring::Vec3 forward{std::sin(heading),0.0f,std::cos(heading)};
            const authoring::Vec3 right{std::cos(heading),0.0f,-std::sin(heading)};

            if (ImGui::Button("GENERATE ALTERNATING SLALOM", ImVec2(-1.0f, 28.0f)))
            {
                if (!builderEventOk) m_studioMessage = "Quick builder needs an Autoslalom/Gymkhana target event.";
                else
                {
                    for (int i=0;i<quickSlalomCount;++i)
                    {
                        const bool passLeft = ((i % 2)==0) == quickStartLeft;
                        const authoring::Vec3 center = add3(quickOrigin, mul3(forward, quickSpacingM * static_cast<float>(i + 1)));
                        char coneName[96]{}; std::snprintf(coneName,sizeof(coneName),"Slalom %02d %s",i+1,passLeft?"L":"R");
                        const auto coneId = addBuilderCone(passLeft ? authoring::ConeRole::SlalomLeft : authoring::ConeRole::SlalomRight, center, coneName);
                        const int order = nextCourseOrder();
                        auto& gate = m_authoring.addConeCourseGate(passLeft ? authoring::ConeCourseGateType::SlalomLeft : authoring::ConeCourseGateType::SlalomRight, coneName);
                        gate.eventId=m_raceConeAuthorEventId; gate.order=order; gate.position=snapCoursePoint(center); gate.headingDeg=quickHeadingDeg; gate.widthM=quickWidthM; gate.leftConeId=coneId;
                    }
                    m_raceConeSelectionKind=1; m_raceSelectedConeGate=static_cast<int>(m_authoring.coneCourseGates.size())-1;
                    m_studioMessage="Generated alternating slalom cones + deterministic course elements.";
                }
            }
            if (ImGui::Button("DUPLICATE SELECTED CONE LINE / TAPER", ImVec2(-1.0f, 28.0f)))
            {
                if (m_raceConeSelectionKind != 0 || m_authoring.courseCones.empty()) m_studioMessage = "Select a physical cone first; its visual, physics, penalty and traffic semantics will be copied.";
                else
                {
                    const int sourceIndex = std::clamp(m_raceSelectedCone, 0, static_cast<int>(m_authoring.courseCones.size()) - 1);
                    const authoring::CourseCone source = m_authoring.courseCones[static_cast<std::size_t>(sourceIndex)];
                    int lastIndex = sourceIndex;
                    for (int i=0;i<quickSlalomCount;++i)
                    {
                        const float progress = static_cast<float>(i + 1) / static_cast<float>(std::max(1, quickSlalomCount));
                        authoring::Vec3 position = add3(quickOrigin, mul3(forward, quickSpacingM * static_cast<float>(i + 1)));
                        position = add3(position, mul3(right, quickTaperLateralM * progress));
                        char name[160]{}; std::snprintf(name, sizeof(name), "%s %02d", source.name.c_str(), i + 2);
                        auto& cone = m_authoring.addCourseCone(source.role, name);
                        const auto newId = cone.id;
                        cone = source; cone.id = newId; cone.name = name; cone.eventId = m_raceConeAuthorEventId; cone.position = snapCoursePoint(position); cone.headingDeg = quickHeadingDeg;
                        lastIndex = static_cast<int>(m_authoring.courseCones.size()) - 1;
                    }
                    m_raceConeSelectionKind=0; m_raceSelectedCone=lastIndex;
                    m_studioMessage = std::abs(quickTaperLateralM) < 0.001f ? "Generated straight cone line from selected cone semantics." : "Generated diagonal cone taper from selected cone semantics.";
                }
            }
            if (ImGui::BeginTable("QuickConeCourseBuilders", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextColumn();
                if (ImGui::Button("START PAIR",ImVec2(-1.0f,27.0f)))
                {
                    if (!builderEventOk) m_studioMessage="Start pair needs an Autoslalom/Gymkhana target event.";
                    else { addBuilderCone(authoring::ConeRole::Start,add3(quickOrigin,mul3(right,-quickWidthM*0.5f)),"Start Left"); addBuilderCone(authoring::ConeRole::Start,add3(quickOrigin,mul3(right,quickWidthM*0.5f)),"Start Right"); m_studioMessage="Generated course Start cone pair."; }
                }
                ImGui::TableNextColumn();
                if (ImGui::Button("GATE PAIR",ImVec2(-1.0f,27.0f)))
                {
                    if (!builderEventOk) m_studioMessage="Gate pair needs an Autoslalom/Gymkhana target event.";
                    else { const auto l=addBuilderCone(authoring::ConeRole::GateLeft,add3(quickOrigin,mul3(right,-quickWidthM*0.5f)),"Gate Left"); const auto r=addBuilderCone(authoring::ConeRole::GateRight,add3(quickOrigin,mul3(right,quickWidthM*0.5f)),"Gate Right"); addBuilderGate(authoring::ConeCourseGateType::Gate,quickOrigin,"Gate",l,r); m_studioMessage="Generated physical gate pair + invisible timing authority."; }
                }
                ImGui::TableNextColumn();
                if (ImGui::Button("FINISH PAIR",ImVec2(-1.0f,27.0f)))
                {
                    if (!builderEventOk) m_studioMessage="Finish pair needs an Autoslalom/Gymkhana target event.";
                    else { const auto l=addBuilderCone(authoring::ConeRole::Finish,add3(quickOrigin,mul3(right,-quickWidthM*0.5f)),"Finish Left"); const auto r=addBuilderCone(authoring::ConeRole::Finish,add3(quickOrigin,mul3(right,quickWidthM*0.5f)),"Finish Right"); addBuilderGate(authoring::ConeCourseGateType::Finish,quickOrigin,"Finish",l,r); m_studioMessage="Generated Finish cone pair + finish element."; }
                }
                ImGui::TableNextColumn();
                if (ImGui::Button("STOP BOX",ImVec2(-1.0f,27.0f)))
                {
                    if (!builderEventOk) m_studioMessage="Stop box needs an Autoslalom/Gymkhana target event.";
                    else
                    {
                        const auto f=mul3(forward,quickStopLengthM*0.5f), r=mul3(right,quickWidthM*0.5f);
                        addBuilderCone(authoring::ConeRole::StopBox,add3(add3(quickOrigin,f),r),"Stop Box FR"); addBuilderCone(authoring::ConeRole::StopBox,add3(add3(quickOrigin,f),mul3(r,-1.0f)),"Stop Box FL");
                        addBuilderCone(authoring::ConeRole::StopBox,add3(add3(quickOrigin,mul3(f,-1.0f)),r),"Stop Box RR"); addBuilderCone(authoring::ConeRole::StopBox,add3(add3(quickOrigin,mul3(f,-1.0f)),mul3(r,-1.0f)),"Stop Box RL");
                        const int order=nextCourseOrder(); auto& gate=m_authoring.addConeCourseGate(authoring::ConeCourseGateType::StopBox,"Stop Box"); gate.eventId=m_raceConeAuthorEventId; gate.order=order; gate.position=snapCoursePoint(quickOrigin); gate.headingDeg=quickHeadingDeg; gate.widthM=quickWidthM; gate.lengthM=quickStopLengthM;
                        m_studioMessage="Generated four-cone Stop Box + low-speed dwell rule.";
                    }
                }
                ImGui::TableNextColumn();
                if (ImGui::Button("180 LEFT",ImVec2(-1.0f,27.0f)))
                {
                    if (!builderEventOk) m_studioMessage="Turnaround needs an Autoslalom/Gymkhana target event."; else { const auto c=addBuilderCone(authoring::ConeRole::Turnaround,quickOrigin,"180 Left Cone"); addBuilderGate(authoring::ConeCourseGateType::TurnaroundLeft,quickOrigin,"180 Left",c,0); m_studioMessage="Generated 180-degree left turnaround."; }
                }
                ImGui::TableNextColumn();
                if (ImGui::Button("180 RIGHT",ImVec2(-1.0f,27.0f)))
                {
                    if (!builderEventOk) m_studioMessage="Turnaround needs an Autoslalom/Gymkhana target event."; else { const auto c=addBuilderCone(authoring::ConeRole::Turnaround,quickOrigin,"180 Right Cone"); addBuilderGate(authoring::ConeCourseGateType::TurnaroundRight,quickOrigin,"180 Right",c,0); m_studioMessage="Generated 180-degree right turnaround."; }
                }
                ImGui::TableNextColumn();
                if (ImGui::Button("360 LEFT",ImVec2(-1.0f,27.0f)))
                {
                    if (!builderEventOk) m_studioMessage="360 circle needs an Autoslalom/Gymkhana target event."; else { const auto c=addBuilderCone(authoring::ConeRole::Turnaround,quickOrigin,"360 Left Center"); const int order=nextCourseOrder(); auto& gate=m_authoring.addConeCourseGate(authoring::ConeCourseGateType::CircleLeft,"360 Circle Left"); gate.eventId=m_raceConeAuthorEventId; gate.order=order; gate.position=snapCoursePoint(quickOrigin); gate.headingDeg=quickHeadingDeg; gate.widthM=std::max(4.0f,quickWidthM*1.6f); gate.lengthM=std::max(1.0f,quickWidthM*0.35f); gate.leftConeId=c; m_studioMessage="Generated full 360-degree left gymkhana circle."; }
                }
                ImGui::TableNextColumn();
                if (ImGui::Button("360 RIGHT",ImVec2(-1.0f,27.0f)))
                {
                    if (!builderEventOk) m_studioMessage="360 circle needs an Autoslalom/Gymkhana target event."; else { const auto c=addBuilderCone(authoring::ConeRole::Turnaround,quickOrigin,"360 Right Center"); const int order=nextCourseOrder(); auto& gate=m_authoring.addConeCourseGate(authoring::ConeCourseGateType::CircleRight,"360 Circle Right"); gate.eventId=m_raceConeAuthorEventId; gate.order=order; gate.position=snapCoursePoint(quickOrigin); gate.headingDeg=quickHeadingDeg; gate.widthM=std::max(4.0f,quickWidthM*1.6f); gate.lengthM=std::max(1.0f,quickWidthM*0.35f); gate.leftConeId=c; m_studioMessage="Generated full 360-degree right gymkhana circle."; }
                }
                ImGui::EndTable();
            }
            ImGui::TextWrapped("Quick builders use the selected cone/course element as their origin (otherwise the current 3D view target), inherit the authoring target event, and vertically snap generated pieces back onto nearby Scene GLB geometry. The line/taper tool copies the selected cone's complete visual, physics, penalty and traffic-control semantics, so it also works for persistent free-roam roadworks.");
            ImGui::Separator();
        }

        if (m_raceConeSelectionKind==0 && !m_authoring.courseCones.empty())
        {
            m_raceSelectedCone=std::clamp(m_raceSelectedCone,0,static_cast<int>(m_authoring.courseCones.size())-1);
            auto& cone=m_authoring.courseCones[static_cast<std::size_t>(m_raceSelectedCone)];
            sectionTitle("SELECTED CONE");
            inputString("Name",cone.name); ImGui::Checkbox("Enabled",&cone.enabled);
            int role=static_cast<int>(cone.role);
            const char* roles[]={"Traffic Guide","Boundary","Gate Left","Gate Right","Slalom Left","Slalom Right","Start","Finish","Turnaround","Stop Box","Chicane","Pointer","No-Go Boundary","Road Closure"};
            if (ImGui::Combo("Semantic role",&role,roles,IM_ARRAYSIZE(roles))) cone.role=static_cast<authoring::ConeRole>(role);
            eventCombo("Overlay / event",cone.eventId);
            ImGui::DragFloat3("Position",&cone.position.x,0.05f); ImGui::DragFloat("Heading deg",&cone.headingDeg,0.25f,-360.0f,360.0f);
            inputString("Cone GLB override",cone.assetPath);
            ImGui::DragFloat("Visual scale",&cone.visualScale,0.01f,0.05f,10.0f,"%.2f");
            ImGui::DragFloat("Base radius m",&cone.baseRadiusM,0.01f,0.05f,1.0f,"%.2f");
            ImGui::DragFloat("Height m",&cone.heightM,0.01f,0.10f,2.0f,"%.2f");
            ImGui::Checkbox("Physical / knockable",&cone.physical);
            if (cone.physical)
            {
                ImGui::DragFloat("Mass kg",&cone.massKg,0.05f,0.05f,20.0f,"%.2f");
                ImGui::SliderFloat("Friction",&cone.friction,0.0f,1.5f,"%.2f");
                ImGui::SliderFloat("Restitution",&cone.restitution,0.0f,0.8f,"%.2f");
            }
            int penalty=static_cast<int>(cone.penaltyMode); const char* penalties[]={"None","Contact","Displaced","Knocked Down"};
            if (ImGui::Combo("Competition penalty trigger",&penalty,penalties,IM_ARRAYSIZE(penalties))) cone.penaltyMode=static_cast<authoring::ConePenaltyMode>(penalty);
            ImGui::DragFloat("Hit penalty seconds",&cone.hitPenaltySeconds,0.1f,0.0f,60.0f,"%.1f");
            ImGui::DragFloat("Displacement tolerance m",&cone.displacementToleranceM,0.01f,0.01f,2.0f,"%.2f");
            int traffic=static_cast<int>(cone.trafficMode); const char* trafficModes[]={"None","Guide / discourage","Slow","Close lane","Close road"};
            if (ImGui::Combo("Traffic meaning",&traffic,trafficModes,IM_ARRAYSIZE(trafficModes))) cone.trafficMode=static_cast<authoring::ConeTrafficMode>(traffic);
            if (cone.trafficMode!=authoring::ConeTrafficMode::None)
            {
                const char* roadPreview="(unset / visual only)";
                for (const auto& road:m_authoring.roadSplines) if (road.id==cone.roadId) { roadPreview=road.name.c_str(); break; }
                if (ImGui::BeginCombo("Affected road",roadPreview))
                {
                    if (ImGui::Selectable("(unset / visual only)",cone.roadId==0)) cone.roadId=0;
                    for (const auto& road:m_authoring.roadSplines)
                    {
                        std::string l=road.name+"##coneroad"+std::to_string(road.id);
                        if (ImGui::Selectable(l.c_str(),cone.roadId==road.id)) { cone.roadId=road.id; cone.linkId=0; }
                    }
                    ImGui::EndCombo();
                }
                std::string linkPreview="(unset / whole selected road)";
                for (const auto& link:m_authoring.trafficLinks) if (link.id==cone.linkId) { linkPreview="Link #"+std::to_string(link.id)+"  "+std::to_string(link.fromNodeId)+" -> "+std::to_string(link.toNodeId); break; }
                if (ImGui::BeginCombo("Exact traffic link",linkPreview.c_str()))
                {
                    if (ImGui::Selectable("(unset / whole selected road)",cone.linkId==0)) cone.linkId=0;
                    for (const auto& link:m_authoring.trafficLinks)
                    {
                        std::string l="Link #"+std::to_string(link.id)+"  "+std::to_string(link.fromNodeId)+" -> "+std::to_string(link.toNodeId)+"##conelink"+std::to_string(link.id);
                        if (ImGui::Selectable(l.c_str(),cone.linkId==link.id)) cone.linkId=link.id;
                    }
                    ImGui::EndCombo();
                }
                ImGui::InputInt("Lane index (-1 = all)",&cone.laneIndex);
                ImGui::DragFloat("Traffic speed limit km/h",&cone.trafficSpeedLimitKmh,1.0f,0.0f,160.0f,"%.0f");
                ImGui::DragFloat("Route cost multiplier",&cone.routeCostMultiplier,0.1f,1.0f,100.0f,"%.1f");
            }
            if (ImGui::Button("PLACE THIS CONE TYPE IN VIEWPORT",ImVec2(-1.0f,28.0f)))
            { m_raceConePlacementRole=static_cast<int>(cone.role); m_raceConeGatePlacementType=-1; m_raceConeSelectionKind=0; }
            if (ImGui::Button("DELETE CONE",ImVec2(-1.0f,27.0f)))
            { m_authoring.removeCourseCone(static_cast<std::size_t>(m_raceSelectedCone)); m_raceSelectedCone=std::max(0,m_raceSelectedCone-1); }
        }
        else if (m_raceConeSelectionKind==1 && !m_authoring.coneCourseGates.empty())
        {
            m_raceSelectedConeGate=std::clamp(m_raceSelectedConeGate,0,static_cast<int>(m_authoring.coneCourseGates.size())-1);
            auto& gate=m_authoring.coneCourseGates[static_cast<std::size_t>(m_raceSelectedConeGate)];
            sectionTitle("SELECTED INVISIBLE COURSE ELEMENT");
            inputString("Name",gate.name); ImGui::Checkbox("Enabled",&gate.enabled); eventCombo("Event",gate.eventId);
            int type=static_cast<int>(gate.type); const char* types[]={"Gate","Slalom Left","Slalom Right","Turnaround Left","Turnaround Right","Stop Box","Finish","360 Circle Left","360 Circle Right"};
            if (ImGui::Combo("Type",&type,types,IM_ARRAYSIZE(types))) gate.type=static_cast<authoring::ConeCourseGateType>(type);
            ImGui::InputInt("Order",&gate.order); ImGui::DragFloat3("Position",&gate.position.x,0.05f); ImGui::DragFloat("Heading deg",&gate.headingDeg,0.25f,-360.0f,360.0f);
            if (gate.type==authoring::ConeCourseGateType::CircleLeft || gate.type==authoring::ConeCourseGateType::CircleRight)
            {
                ImGui::DragFloat("Circle diameter m",&gate.widthM,0.05f,1.0f,50.0f,"%.2f");
                ImGui::DragFloat("Radial tolerance m",&gate.lengthM,0.05f,0.5f,20.0f,"%.2f");
            }
            else
            {
                ImGui::DragFloat("Width m",&gate.widthM,0.05f,0.5f,50.0f,"%.2f"); ImGui::DragFloat("Length / zone m",&gate.lengthM,0.05f,0.5f,50.0f,"%.2f");
            }
            ImGui::Checkbox("Require forward direction",&gate.directionRequired); ImGui::DragFloat("Side clearance m",&gate.sideClearanceM,0.01f,0.0f,5.0f,"%.2f");
            if (gate.type==authoring::ConeCourseGateType::StopBox)
            {
                ImGui::DragFloat("Required stop speed km/h",&gate.stopSpeedKmh,0.1f,0.0f,20.0f,"%.1f");
                ImGui::DragFloat("Required dwell s",&gate.stopDwellS,0.05f,0.0f,10.0f,"%.2f");
            }
            ImGui::DragFloat("Wrong/missed penalty s",&gate.wrongElementPenaltySeconds,0.25f,0.0f,120.0f,"%.1f"); ImGui::Checkbox("Miss = DNF",&gate.dnfOnMiss);
            coneCombo("Left / reference cone",gate.leftConeId); coneCombo("Right / second cone",gate.rightConeId);
            if (ImGui::Button("FIT GATE TO REFERENCED CONES",ImVec2(-1.0f,28.0f)))
            {
                const authoring::CourseCone* a=nullptr; const authoring::CourseCone* b=nullptr;
                for (const auto& c:m_authoring.courseCones) { if (c.id==gate.leftConeId) a=&c; if (c.id==gate.rightConeId) b=&c; }
                if (a && b)
                {
                    gate.position={(a->position.x+b->position.x)*0.5f,(a->position.y+b->position.y)*0.5f,(a->position.z+b->position.z)*0.5f};
                    const float dx=b->position.x-a->position.x,dz=b->position.z-a->position.z; gate.widthM=std::max(0.5f,std::sqrt(dx*dx+dz*dz));
                    gate.headingDeg=std::atan2(-dz,dx)*180.0f/3.14159265358979323846f;
                    m_studioMessage="Course gate fitted to the two referenced cone centers.";
                }
                else m_studioMessage="Choose two valid cone references before fitting the gate.";
            }
            if (ImGui::Button("PLACE THIS ELEMENT IN VIEWPORT",ImVec2(-1.0f,28.0f)))
            { m_raceConeGatePlacementType=static_cast<int>(gate.type); m_raceConePlacementRole=-1; m_raceConeSelectionKind=1; }
            if (ImGui::Button("DELETE COURSE ELEMENT",ImVec2(-1.0f,27.0f)))
            { m_authoring.removeConeCourseGate(static_cast<std::size_t>(m_raceSelectedConeGate)); m_raceSelectedConeGate=std::max(0,m_raceSelectedConeGate-1); }
        }
        ImGui::Spacing();
        ImGui::TextWrapped("Cones are physical/visual props. Course elements are invisible rules, so a cone may tumble away while timing and gate validation remain deterministic. eventId 0 cones also act as persistent free-roam traffic control.");
    }
    ImGui::EndChild();
}


void HeritageStudioApp::drawRaceViewportInteractive()
{
    sectionTitle("3D VENUE / ROUTE PREVIEW");
    drawViewportToolbar(m_viewGridVisible, m_viewSnapEnabled, m_viewSnapM,
        m_viewYawDeg, m_viewPitchDeg, m_viewDistanceM, m_viewOrthographic);

    if (m_raceTab == 0 && ImGui::BeginTable("RacePlacementToolbar", 3, ImGuiTableFlags_SizingStretchSame))
    {
        const auto placeButton = [&](const char* label, authoring::RaceMarkerType type)
        {
            ImGui::TableNextColumn();
            const bool active = m_racePlacementType == static_cast<int>(type);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(label, ImVec2(-1.0f, 27.0f)))
            {
                m_racePlacementType = active ? -1 : static_cast<int>(type);
                m_racePlaceRouteNode = false; m_raceSupportPlacementType = -1; m_raceConePlacementRole = -1; m_raceConeGatePlacementType = -1;
            }
            if (active) ImGui::PopStyleColor();
        };
        placeButton("CHECKPOINT", authoring::RaceMarkerType::Checkpoint);
        placeButton("GRID SLOT", authoring::RaceMarkerType::GridSlot);
        placeButton("TIMING LOOP", authoring::RaceMarkerType::TimingLoop);
        placeButton("PIT BOX", authoring::RaceMarkerType::PitBox);
        placeButton("AI RACE LINE", authoring::RaceMarkerType::AiLineNode);
        placeButton("RECOVERY", authoring::RaceMarkerType::Recovery);
        ImGui::EndTable();
    }
    if (m_raceTab == 1)
    {
        if (ImGui::Button(m_racePlaceRouteNode ? "CANCEL ROUTE NODE PLACEMENT" : "PLACE ROUTE NODE ON SURFACE", ImVec2(-1.0f, 27.0f)))
        {
            m_racePlaceRouteNode = !m_racePlaceRouteNode; m_racePlacementType = -1; m_raceSupportPlacementType = -1; m_raceConePlacementRole = -1; m_raceConeGatePlacementType = -1;
        }
    }
    if (m_raceTab == 4 && m_raceSupportPlacementType >= 0)
        ImGui::TextDisabled("Support-point placement active: click scene surface. ESC cancels.");
    else if (m_racePlaceRouteNode)
        ImGui::TextDisabled("Route-node placement active: click scene surface. ESC cancels.");
    else if (m_racePlacementType >= 0)
        ImGui::TextDisabled("Marker placement active: click scene surface. ESC cancels.");
    else if (m_raceConePlacementRole >= 0)
        ImGui::TextDisabled("Cone placement active: click scene surface. ESC cancels.");
    else if (m_raceConeGatePlacementType >= 0)
        ImGui::TextDisabled("Course-element placement active: click scene surface. ESC cancels.");

    ImVec2 size = ImGui::GetContentRegionAvail();
    size.y = std::max(180.0f, size.y);
    bool hovered = false;
    const StudioViewportProjection view = prepareInteractiveViewport(
        m_window, m_viewFly, "##RaceInteractiveViewport", size,
        m_viewYawDeg, m_viewPitchDeg, m_viewDistanceM, m_viewTarget,
        m_viewGridVisible, m_viewOrthographic, hovered);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    drawSceneGeometryBackdrop(draw, view, m_scenePreview.get(), m_viewGridVisible);
    drawFlyNavigationOverlay(draw, view, m_viewFly);
    draw->PushClipRect(view.min, view.max, true);
    const ImGuiIO& io = ImGui::GetIO();

    if (hovered && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        m_racePlacementType = -1; m_racePlaceRouteNode = false; m_raceSupportPlacementType = -1; m_raceConePlacementRole = -1; m_raceConeGatePlacementType = -1;
    }

    const auto routeColor = [](authoring::RaceRouteType type) -> ImU32
    {
        switch (type)
        {
        case authoring::RaceRouteType::PitLane: return IM_COL32(225, 125, 235, 235);
        case authoring::RaceRouteType::SafetyCar: return IM_COL32(245, 160, 70, 235);
        case authoring::RaceRouteType::Formation: return IM_COL32(115, 210, 230, 235);
        case authoring::RaceRouteType::AlternateLayout: return IM_COL32(160, 125, 235, 235);
        case authoring::RaceRouteType::Sprint: return IM_COL32(95, 220, 145, 235);
        case authoring::RaceRouteType::Hillclimb: return IM_COL32(210, 180, 80, 235);
        case authoring::RaceRouteType::Drag: return IM_COL32(240, 105, 105, 235);
        default: return IM_COL32(245, 210, 65, 235);
        }
    };
    const auto bezier = [](const authoring::Vec3& p0, const authoring::Vec3& p1, const authoring::Vec3& p2, const authoring::Vec3& p3, float t)
    {
        const float u = 1.0f - t;
        return add3(add3(mul3(p0, u * u * u), mul3(p1, 3.0f * u * u * t)),
            add3(mul3(p2, 3.0f * u * t * t), mul3(p3, t * t * t)));
    };
    const auto catmullRom = [](const authoring::Vec3& p0, const authoring::Vec3& p1, const authoring::Vec3& p2, const authoring::Vec3& p3, float t)
    {
        const float t2 = t * t, t3 = t2 * t;
        return mul3(add3(add3(mul3(p1, 2.0f), mul3(sub3(p2, p0), t)),
            add3(mul3(add3(add3(mul3(p0, 2.0f), mul3(p1, -5.0f)), add3(mul3(p2, 4.0f), mul3(p3, -1.0f))), t2),
                 mul3(add3(add3(mul3(p0, -1.0f), mul3(p1, 3.0f)), add3(mul3(p2, -3.0f), p3)), t3))), 0.5f);
    };

    // HRACE v3 spline routes and their legal-driving corridors. The route is
    // the shared authority for lap progression, AI line generation, track
    // limits, minimap generation and alternate venue layouts.
    for (int routeIndex = 0; routeIndex < static_cast<int>(m_authoring.raceRoutes.size()); ++routeIndex)
    {
        const auto& route = m_authoring.raceRoutes[static_cast<std::size_t>(routeIndex)];
        if (!route.enabled) continue;
        std::vector<const authoring::RaceRouteNode*> nodes;
        for (const auto& node : m_authoring.raceRouteNodes) if (node.routeId == route.id) nodes.push_back(&node);
        std::stable_sort(nodes.begin(), nodes.end(), [](const auto* a, const auto* b) { return a->order < b->order; });
        if (nodes.size() < 2) continue;
        const std::size_t segmentCount = route.closedLoop && nodes.size() > 2 ? nodes.size() : nodes.size() - 1;
        const ImU32 centerColor = routeColor(route.type);
        const bool selectedRoute = m_raceTab == 1 && routeIndex == m_raceSelectedRoute;
        for (std::size_t segment = 0; segment < segmentCount; ++segment)
        {
            const auto* a = nodes[segment];
            const auto* b = nodes[(segment + 1) % nodes.size()];
            const authoring::Vec3 p0 = a->position;
            const authoring::Vec3 p1 = add3(a->position, a->handleOut);
            const authoring::Vec3 p2 = add3(b->position, b->handleIn);
            const authoring::Vec3 p3 = b->position;
            authoring::Vec3 previous = p0;
            for (int step = 1; step <= 16; ++step)
            {
                const float t = static_cast<float>(step) / 16.0f;
                const authoring::Vec3 current = bezier(p0, p1, p2, p3, t);
                const authoring::Vec3 tangent = normalize3(sub3(current, previous));
                const authoring::Vec3 side{ tangent.z, 0.0f, -tangent.x };
                const float leftWidth = a->leftWidthM + (b->leftWidthM - a->leftWidthM) * t;
                const float rightWidth = a->rightWidthM + (b->rightWidthM - a->rightWidthM) * t;
                ImVec2 aCenter{}, bCenter{}, aLeft{}, bLeft{}, aRight{}, bRight{};
                const float priorT = static_cast<float>(step - 1) / 16.0f;
                const float priorLeft = a->leftWidthM + (b->leftWidthM - a->leftWidthM) * priorT;
                const float priorRight = a->rightWidthM + (b->rightWidthM - a->rightWidthM) * priorT;
                const authoring::Vec3 previousLeft = add3(previous, mul3(side, -priorLeft));
                const authoring::Vec3 currentLeft = add3(current, mul3(side, -leftWidth));
                const authoring::Vec3 previousRight = add3(previous, mul3(side, priorRight));
                const authoring::Vec3 currentRight = add3(current, mul3(side, rightWidth));
                if (projectWorldPoint(view, previous, aCenter) && projectWorldPoint(view, current, bCenter))
                    draw->AddLine(aCenter, bCenter, centerColor, selectedRoute ? 3.4f : 2.2f);
                if (projectWorldPoint(view, previousLeft, aLeft) && projectWorldPoint(view, currentLeft, bLeft))
                    draw->AddLine(aLeft, bLeft, IM_COL32(235, 90, 90, selectedRoute ? 210 : 125), selectedRoute ? 1.8f : 1.0f);
                if (projectWorldPoint(view, previousRight, aRight) && projectWorldPoint(view, currentRight, bRight))
                    draw->AddLine(aRight, bRight, IM_COL32(90, 225, 120, selectedRoute ? 210 : 125), selectedRoute ? 1.8f : 1.0f);
                previous = current;
            }
        }
        if (selectedRoute)
        {
            for (int nodeIndex = 0; nodeIndex < static_cast<int>(m_authoring.raceRouteNodes.size()); ++nodeIndex)
            {
                const auto& node = m_authoring.raceRouteNodes[static_cast<std::size_t>(nodeIndex)];
                if (node.routeId != route.id) continue;
                ImVec2 screen{};
                if (projectWorldPoint(view, node.position, screen))
                {
                    char label[64]{}; std::snprintf(label, sizeof(label), "N%d", node.order);
                    drawMarker(draw, screen, centerColor, nodeIndex == m_raceSelectedRouteNode, label);
                    if (node.targetSpeedKmh > 0.0f)
                    {
                        char speed[48]{}; std::snprintf(speed, sizeof(speed), "%.0f km/h", node.targetSpeedKmh);
                        draw->AddText(ImVec2(screen.x + 9.0f, screen.y + 10.0f), IM_COL32(220, 225, 232, 235), speed);
                    }
                }
            }
        }
    }

    // STUDIO27 moving broadcast camera paths. They are authored as ordered
    // FP32 venue points, published to runtime, then evaluated against replay
    // FP64 vehicle positions. The camera path owns only presentation; incident
    // truth and replay history remain STUDIO23/24 responsibilities.
    for (int pathIndex = 0; pathIndex < static_cast<int>(m_authoring.broadcastCameraPaths.size()); ++pathIndex)
    {
        const auto& path = m_authoring.broadcastCameraPaths[static_cast<std::size_t>(pathIndex)];
        if (!path.enabled) continue;
        std::vector<std::pair<int, int>> indices;
        for (int i = 0; i < static_cast<int>(m_authoring.broadcastCameraNodes.size()); ++i)
            if (m_authoring.broadcastCameraNodes[static_cast<std::size_t>(i)].pathId == path.id)
                indices.push_back({ m_authoring.broadcastCameraNodes[static_cast<std::size_t>(i)].order, i });
        std::stable_sort(indices.begin(), indices.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        const bool selectedPath = m_raceTab == 5 && pathIndex == m_raceSelectedCameraPath;
        const ImU32 pathColor = path.type == authoring::BroadcastCameraPathType::Crane ? IM_COL32(245, 175, 80, 235)
            : path.type == authoring::BroadcastCameraPathType::Cable ? IM_COL32(125, 205, 255, 235)
            : path.type == authoring::BroadcastCameraPathType::Drone ? IM_COL32(195, 130, 255, 235)
            : IM_COL32(80, 235, 205, 235);
        if (indices.size() >= 2)
        {
            authoring::Vec3 previous = m_authoring.broadcastCameraNodes[static_cast<std::size_t>(indices.front().second)].position;
            const int segmentCount = static_cast<int>(indices.size()) - 1;
            for (int segment = 0; segment < segmentCount; ++segment)
            {
                const auto& p1 = m_authoring.broadcastCameraNodes[static_cast<std::size_t>(indices[segment].second)].position;
                const auto& p2 = m_authoring.broadcastCameraNodes[static_cast<std::size_t>(indices[segment + 1].second)].position;
                const auto& p0 = segment > 0 ? m_authoring.broadcastCameraNodes[static_cast<std::size_t>(indices[segment - 1].second)].position : p1;
                const auto& p3 = segment + 2 < static_cast<int>(indices.size()) ? m_authoring.broadcastCameraNodes[static_cast<std::size_t>(indices[segment + 2].second)].position : p2;
                for (int step = 1; step <= 12; ++step)
                {
                    const authoring::Vec3 current = catmullRom(p0, p1, p2, p3, static_cast<float>(step) / 12.0f);
                    ImVec2 a{}, b{};
                    if (projectWorldPoint(view, previous, a) && projectWorldPoint(view, current, b))
                        draw->AddLine(a, b, pathColor, selectedPath ? 3.2f : 1.8f);
                    previous = current;
                }
            }
        }
        if (selectedPath)
        {
            for (const auto& pair : indices)
            {
                const int nodeIndex = pair.second;
                const auto& node = m_authoring.broadcastCameraNodes[static_cast<std::size_t>(nodeIndex)];
                ImVec2 screen{};
                if (projectWorldPoint(view, node.position, screen))
                {
                    char label[48]{}; std::snprintf(label, sizeof(label), "C%d", node.order);
                    drawMarker(draw, screen, pathColor, nodeIndex == m_raceSelectedCameraNode, label);
                }
            }
        }
    }

    const auto drawOrderedPolyline = [&](authoring::RaceMarkerType type, ImU32 color, float thickness)
    {
        std::vector<std::pair<int, authoring::Vec3>> points;
        for (const auto& marker : m_authoring.raceMarkers) if (marker.type == type) points.push_back({ marker.order, marker.position });
        std::stable_sort(points.begin(), points.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (std::size_t i = 1; i < points.size(); ++i)
        {
            ImVec2 a{}, b{};
            if (projectWorldPoint(view, points[i - 1].second, a) && projectWorldPoint(view, points[i].second, b)) draw->AddLine(a, b, color, thickness);
        }
    };
    drawOrderedPolyline(authoring::RaceMarkerType::AiLineNode, IM_COL32(235, 195, 60, 210), 2.0f);
    drawOrderedPolyline(authoring::RaceMarkerType::WetLineNode, IM_COL32(70, 165, 240, 210), 1.8f);
    drawOrderedPolyline(authoring::RaceMarkerType::TrackLimitLeft, IM_COL32(235, 85, 85, 160), 1.5f);
    drawOrderedPolyline(authoring::RaceMarkerType::TrackLimitRight, IM_COL32(85, 220, 115, 160), 1.5f);

    for (int i = 0; i < static_cast<int>(m_authoring.raceMarkers.size()); ++i)
    {
        const auto& marker = m_authoring.raceMarkers[static_cast<std::size_t>(i)];
        ImVec2 screen{}; if (!projectWorldPoint(view, marker.position, screen)) continue;
        ImU32 color = IM_COL32(235, 210, 80, 255);
        switch (marker.type)
        {
        case authoring::RaceMarkerType::StartFinish: color = IM_COL32(255, 255, 255, 255); break;
        case authoring::RaceMarkerType::Checkpoint: color = IM_COL32(80, 210, 235, 255); break;
        case authoring::RaceMarkerType::Sector:
        case authoring::RaceMarkerType::TimingLoop: color = IM_COL32(85, 185, 255, 255); break;
        case authoring::RaceMarkerType::SpeedTrapStart:
        case authoring::RaceMarkerType::SpeedTrapFinish: color = IM_COL32(255, 165, 70, 255); break;
        case authoring::RaceMarkerType::GridSlot: color = IM_COL32(90, 220, 120, 255); break;
        case authoring::RaceMarkerType::PitEntry:
        case authoring::RaceMarkerType::PitExit:
        case authoring::RaceMarkerType::PitSpeedLine:
        case authoring::RaceMarkerType::PitBox: color = IM_COL32(225, 130, 235, 255); break;
        case authoring::RaceMarkerType::SafetyCarLine:
        case authoring::RaceMarkerType::FormationLine: color = IM_COL32(245, 175, 75, 255); break;
        case authoring::RaceMarkerType::Recovery: color = IM_COL32(245, 145, 65, 255); break;
        case authoring::RaceMarkerType::ReplayCamera: color = IM_COL32(110, 235, 215, 255); break;
        case authoring::RaceMarkerType::AiLineNode: color = IM_COL32(235, 195, 60, 255); break;
        case authoring::RaceMarkerType::WetLineNode: color = IM_COL32(70, 165, 240, 255); break;
        default: break;
        }
        drawMarker(draw, screen, color, m_raceTab == 0 && i == m_raceSelectedMarker, marker.name.c_str());

        const bool gate = marker.type == authoring::RaceMarkerType::StartFinish || marker.type == authoring::RaceMarkerType::Checkpoint
            || marker.type == authoring::RaceMarkerType::Sector || marker.type == authoring::RaceMarkerType::TimingLoop
            || marker.type == authoring::RaceMarkerType::SpeedTrapStart || marker.type == authoring::RaceMarkerType::SpeedTrapFinish
            || marker.type == authoring::RaceMarkerType::PitEntry || marker.type == authoring::RaceMarkerType::PitExit
            || marker.type == authoring::RaceMarkerType::PitSpeedLine || marker.type == authoring::RaceMarkerType::SafetyCarLine
            || marker.type == authoring::RaceMarkerType::FormationLine;
        if (gate)
        {
            const float h = radians(marker.headingDeg);
            const authoring::Vec3 forward{ std::sin(h), 0.0f, std::cos(h) };
            const authoring::Vec3 right{ forward.z, 0.0f, -forward.x };
            const authoring::Vec3 leftWorld = add3(marker.position, mul3(right, -marker.gateWidthM * 0.5f));
            const authoring::Vec3 rightWorld = add3(marker.position, mul3(right, marker.gateWidthM * 0.5f));
            const authoring::Vec3 arrowWorld = add3(marker.position, mul3(forward, 4.0f));
            ImVec2 leftScreen{}, rightScreen{}, arrowScreen{};
            if (projectWorldPoint(view, leftWorld, leftScreen) && projectWorldPoint(view, rightWorld, rightScreen)) draw->AddLine(leftScreen, rightScreen, color, 2.0f);
            if (marker.directionRequired && projectWorldPoint(view, arrowWorld, arrowScreen)) draw->AddLine(screen, arrowScreen, color, 2.0f);
        }
    }

    for (int i = 0; i < static_cast<int>(m_authoring.raceSupportPoints.size()); ++i)
    {
        const auto& point = m_authoring.raceSupportPoints[static_cast<std::size_t>(i)];
        if (!point.enabled) continue;
        ImVec2 screen{}; if (!projectWorldPoint(view, point.position, screen)) continue;
        const ImU32 color = point.type == authoring::RaceSupportPointType::MarshalPost ? IM_COL32(245, 215, 75, 255) : IM_COL32(245, 135, 75, 255);
        drawMarker(draw, screen, color, m_raceTab == 4 && i == m_raceSelectedSupportPoint, point.name.c_str());
    }

    // STUDIO28 physical traffic/course cones plus independent invisible rule gates.
    // The rule geometry deliberately survives a cone being knocked away at runtime.
    for (int i = 0; i < static_cast<int>(m_authoring.courseCones.size()); ++i)
    {
        const auto& cone = m_authoring.courseCones[static_cast<std::size_t>(i)];
        if (!cone.enabled) continue;
        ImVec2 screen{}; if (!projectWorldPoint(view, cone.position, screen)) continue;
        ImU32 color = IM_COL32(245, 135, 55, 255);
        if (cone.trafficMode == authoring::ConeTrafficMode::CloseRoad || cone.trafficMode == authoring::ConeTrafficMode::CloseLane) color = IM_COL32(245, 75, 65, 255);
        else if (cone.role == authoring::ConeRole::Start) color = IM_COL32(80, 225, 120, 255);
        else if (cone.role == authoring::ConeRole::Finish) color = IM_COL32(235, 235, 235, 255);
        else if (cone.role == authoring::ConeRole::GateLeft || cone.role == authoring::ConeRole::SlalomLeft) color = IM_COL32(85, 175, 255, 255);
        else if (cone.role == authoring::ConeRole::GateRight || cone.role == authoring::ConeRole::SlalomRight) color = IM_COL32(245, 195, 65, 255);
        const bool selected = m_raceTab == 6 && m_raceConeSelectionKind == 0 && i == m_raceSelectedCone;
        drawMarker(draw, screen, color, selected, cone.name.c_str());
        const float h = radians(cone.headingDeg);
        const authoring::Vec3 forward{ std::sin(h), 0.0f, std::cos(h) };
        ImVec2 arrow{};
        if (projectWorldPoint(view, add3(cone.position, mul3(forward, 1.25f)), arrow)) draw->AddLine(screen, arrow, color, selected ? 2.5f : 1.2f);
    }

    for (int i = 0; i < static_cast<int>(m_authoring.coneCourseGates.size()); ++i)
    {
        const auto& gate = m_authoring.coneCourseGates[static_cast<std::size_t>(i)];
        if (!gate.enabled) continue;
        const float h = radians(gate.headingDeg);
        const authoring::Vec3 forward{ std::sin(h), 0.0f, std::cos(h) };
        const authoring::Vec3 right{ forward.z, 0.0f, -forward.x };
        const authoring::Vec3 leftWorld = add3(gate.position, mul3(right, -gate.widthM * 0.5f));
        const authoring::Vec3 rightWorld = add3(gate.position, mul3(right, gate.widthM * 0.5f));
        ImVec2 center{}, left{}, rightScreen{}, arrow{};
        if (!projectWorldPoint(view, gate.position, center)) continue;
        const bool selected = m_raceTab == 6 && m_raceConeSelectionKind == 1 && i == m_raceSelectedConeGate;
        const bool circleGate = gate.type == authoring::ConeCourseGateType::CircleLeft || gate.type == authoring::ConeCourseGateType::CircleRight;
        ImU32 color = gate.type == authoring::ConeCourseGateType::Finish ? IM_COL32(235, 235, 235, 230)
            : gate.type == authoring::ConeCourseGateType::StopBox ? IM_COL32(245, 105, 200, 230)
            : circleGate ? IM_COL32(185, 120, 255, 230)
            : IM_COL32(90, 225, 225, 225);
        if (circleGate)
        {
            const float radius = std::max(0.5f, gate.widthM * 0.5f);
            ImVec2 previous{}; bool previousOk = false;
            for (int segment = 0; segment <= 32; ++segment)
            {
                const float a = (static_cast<float>(segment) / 32.0f) * 2.0f * 3.14159265358979323846f;
                const authoring::Vec3 world{ gate.position.x + std::cos(a) * radius, gate.position.y, gate.position.z + std::sin(a) * radius };
                ImVec2 point{}; const bool ok = projectWorldPoint(view, world, point);
                if (ok && previousOk) draw->AddLine(previous, point, color, selected ? 3.0f : 1.5f);
                previous = point; previousOk = ok;
            }
        }
        else if (projectWorldPoint(view, leftWorld, left) && projectWorldPoint(view, rightWorld, rightScreen))
            draw->AddLine(left, rightScreen, color, selected ? 4.0f : 2.0f);
        if (!circleGate && gate.directionRequired && projectWorldPoint(view, add3(gate.position, mul3(forward, std::max(2.0f, gate.lengthM))), arrow))
            draw->AddLine(center, arrow, color, selected ? 3.0f : 1.5f);
        if (gate.type == authoring::ConeCourseGateType::StopBox)
        {
            const authoring::Vec3 f = mul3(forward, gate.lengthM * 0.5f);
            const authoring::Vec3 r = mul3(right, gate.widthM * 0.5f);
            const authoring::Vec3 corners[4] = { add3(add3(gate.position,f),r), add3(add3(gate.position,f),mul3(r,-1.0f)), add3(add3(gate.position,mul3(f,-1.0f)),mul3(r,-1.0f)), add3(add3(gate.position,mul3(f,-1.0f)),r) };
            ImVec2 ps[4]{}; bool ok=true; for (int c=0;c<4;++c) ok = projectWorldPoint(view,corners[c],ps[c]) && ok;
            if (ok) for (int c=0;c<4;++c) draw->AddLine(ps[c],ps[(c+1)%4],color,selected?3.0f:1.5f);
        }
        char label[96]{}; std::snprintf(label,sizeof(label),"%d %s",gate.order,authoring::coneCourseGateTypeName(gate.type));
        drawMarker(draw,center,color,selected,label);
    }

    // F frames the currently edited spatial object.
    if (hovered && ImGui::IsKeyPressed(ImGuiKey_F, false))
    {
        if (m_raceTab == 1 && !m_authoring.raceRouteNodes.empty())
        {
            m_raceSelectedRouteNode = std::clamp(m_raceSelectedRouteNode, 0, static_cast<int>(m_authoring.raceRouteNodes.size()) - 1);
            m_viewTarget = m_authoring.raceRouteNodes[static_cast<std::size_t>(m_raceSelectedRouteNode)].position;
        }
        else if (m_raceTab == 4 && !m_authoring.raceSupportPoints.empty())
        {
            m_raceSelectedSupportPoint = std::clamp(m_raceSelectedSupportPoint, 0, static_cast<int>(m_authoring.raceSupportPoints.size()) - 1);
            m_viewTarget = m_authoring.raceSupportPoints[static_cast<std::size_t>(m_raceSelectedSupportPoint)].position;
        }
        else if (m_raceTab == 5 && !m_authoring.broadcastCameraPaths.empty())
        {
            m_raceSelectedCameraPath = std::clamp(m_raceSelectedCameraPath, 0, static_cast<int>(m_authoring.broadcastCameraPaths.size()) - 1);
            const auto pathId = m_authoring.broadcastCameraPaths[static_cast<std::size_t>(m_raceSelectedCameraPath)].id;
            int nodeIndex = -1;
            if (!m_authoring.broadcastCameraNodes.empty())
            {
                m_raceSelectedCameraNode = std::clamp(m_raceSelectedCameraNode, 0, static_cast<int>(m_authoring.broadcastCameraNodes.size()) - 1);
                if (m_authoring.broadcastCameraNodes[static_cast<std::size_t>(m_raceSelectedCameraNode)].pathId == pathId) nodeIndex = m_raceSelectedCameraNode;
            }
            if (nodeIndex < 0) for (int i = 0; i < static_cast<int>(m_authoring.broadcastCameraNodes.size()); ++i) if (m_authoring.broadcastCameraNodes[static_cast<std::size_t>(i)].pathId == pathId) { nodeIndex = i; break; }
            if (nodeIndex >= 0) { m_raceSelectedCameraNode = nodeIndex; m_viewTarget = m_authoring.broadcastCameraNodes[static_cast<std::size_t>(nodeIndex)].position; }
        }
        else if (m_raceTab == 6 && m_raceConeSelectionKind == 0 && !m_authoring.courseCones.empty())
        {
            m_raceSelectedCone = std::clamp(m_raceSelectedCone, 0, static_cast<int>(m_authoring.courseCones.size()) - 1);
            m_viewTarget = m_authoring.courseCones[static_cast<std::size_t>(m_raceSelectedCone)].position;
        }
        else if (m_raceTab == 6 && m_raceConeSelectionKind == 1 && !m_authoring.coneCourseGates.empty())
        {
            m_raceSelectedConeGate = std::clamp(m_raceSelectedConeGate, 0, static_cast<int>(m_authoring.coneCourseGates.size()) - 1);
            m_viewTarget = m_authoring.coneCourseGates[static_cast<std::size_t>(m_raceSelectedConeGate)].position;
        }
        else if (!m_authoring.raceMarkers.empty())
        {
            m_raceSelectedMarker = std::clamp(m_raceSelectedMarker, 0, static_cast<int>(m_authoring.raceMarkers.size()) - 1);
            m_viewTarget = m_authoring.raceMarkers[static_cast<std::size_t>(m_raceSelectedMarker)].position;
        }
        m_viewDistanceM = std::clamp(m_viewDistanceM, 8.0f, 100.0f);
    }

    bool gizmoCaptured = false;
    if (m_raceTab == 1 && !m_authoring.raceRouteNodes.empty() && !m_racePlaceRouteNode)
    {
        m_raceSelectedRouteNode = std::clamp(m_raceSelectedRouteNode, 0, static_cast<int>(m_authoring.raceRouteNodes.size()) - 1);
        auto& selected = m_authoring.raceRouteNodes[static_cast<std::size_t>(m_raceSelectedRouteNode)];
        const int previousAxis = m_viewGizmoAxis;
        drawMoveGizmo(draw, view, selected.position, hovered, m_viewSnapEnabled, m_viewSnapM, m_viewGizmoAxis, m_viewGizmoDragAccumulator);
        gizmoCaptured = m_viewGizmoAxis >= 0 || previousAxis >= 0;
    }
    else if (m_raceTab == 4 && !m_authoring.raceSupportPoints.empty() && m_raceSupportPlacementType < 0)
    {
        m_raceSelectedSupportPoint = std::clamp(m_raceSelectedSupportPoint, 0, static_cast<int>(m_authoring.raceSupportPoints.size()) - 1);
        auto& selected = m_authoring.raceSupportPoints[static_cast<std::size_t>(m_raceSelectedSupportPoint)];
        const int previousAxis = m_viewGizmoAxis;
        drawMoveGizmo(draw, view, selected.position, hovered, m_viewSnapEnabled, m_viewSnapM, m_viewGizmoAxis, m_viewGizmoDragAccumulator);
        gizmoCaptured = m_viewGizmoAxis >= 0 || previousAxis >= 0;
    }
    else if (m_raceTab == 5 && !m_authoring.broadcastCameraPaths.empty() && !m_authoring.broadcastCameraNodes.empty())
    {
        m_raceSelectedCameraPath = std::clamp(m_raceSelectedCameraPath, 0, static_cast<int>(m_authoring.broadcastCameraPaths.size()) - 1);
        m_raceSelectedCameraNode = std::clamp(m_raceSelectedCameraNode, 0, static_cast<int>(m_authoring.broadcastCameraNodes.size()) - 1);
        auto& selected = m_authoring.broadcastCameraNodes[static_cast<std::size_t>(m_raceSelectedCameraNode)];
        if (selected.pathId == m_authoring.broadcastCameraPaths[static_cast<std::size_t>(m_raceSelectedCameraPath)].id)
        {
            const int previousAxis = m_viewGizmoAxis;
            drawMoveGizmo(draw, view, selected.position, hovered, m_viewSnapEnabled, m_viewSnapM, m_viewGizmoAxis, m_viewGizmoDragAccumulator);
            gizmoCaptured = m_viewGizmoAxis >= 0 || previousAxis >= 0;
        }
    }
    else if (m_raceTab == 6 && m_raceConePlacementRole < 0 && m_raceConeGatePlacementType < 0 && m_raceConeSelectionKind == 0 && !m_authoring.courseCones.empty())
    {
        m_raceSelectedCone = std::clamp(m_raceSelectedCone, 0, static_cast<int>(m_authoring.courseCones.size()) - 1);
        auto& selected = m_authoring.courseCones[static_cast<std::size_t>(m_raceSelectedCone)];
        const int previousAxis = m_viewGizmoAxis;
        drawMoveGizmo(draw, view, selected.position, hovered, m_viewSnapEnabled, m_viewSnapM, m_viewGizmoAxis, m_viewGizmoDragAccumulator);
        gizmoCaptured = m_viewGizmoAxis >= 0 || previousAxis >= 0;
    }
    else if (m_raceTab == 6 && m_raceConePlacementRole < 0 && m_raceConeGatePlacementType < 0 && m_raceConeSelectionKind == 1 && !m_authoring.coneCourseGates.empty())
    {
        m_raceSelectedConeGate = std::clamp(m_raceSelectedConeGate, 0, static_cast<int>(m_authoring.coneCourseGates.size()) - 1);
        auto& selected = m_authoring.coneCourseGates[static_cast<std::size_t>(m_raceSelectedConeGate)];
        const int previousAxis = m_viewGizmoAxis;
        drawMoveGizmo(draw, view, selected.position, hovered, m_viewSnapEnabled, m_viewSnapM, m_viewGizmoAxis, m_viewGizmoDragAccumulator);
        gizmoCaptured = m_viewGizmoAxis >= 0 || previousAxis >= 0;
    }
    else if (m_raceTab == 0 && !m_authoring.raceMarkers.empty() && m_racePlacementType < 0 && !m_racePlaceRouteNode && m_raceSupportPlacementType < 0)
    {
        m_raceSelectedMarker = std::clamp(m_raceSelectedMarker, 0, static_cast<int>(m_authoring.raceMarkers.size()) - 1);
        auto& selected = m_authoring.raceMarkers[static_cast<std::size_t>(m_raceSelectedMarker)];
        const int previousAxis = m_viewGizmoAxis;
        drawMoveGizmo(draw, view, selected.position, hovered, m_viewSnapEnabled, m_viewSnapM, m_viewGizmoAxis, m_viewGizmoDragAccumulator);
        gizmoCaptured = m_viewGizmoAxis >= 0 || previousAxis >= 0;
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !gizmoCaptured)
    {
        authoring::Vec3 ground{};
        if ((m_racePlaceRouteNode || m_raceSupportPlacementType >= 0 || m_racePlacementType >= 0 || m_raceConePlacementRole >= 0 || m_raceConeGatePlacementType >= 0)
            && viewportAuthoringPoint(view, io.MousePos, m_scenePreview.get(), ground))
        {
            ground.x = snapValue(ground.x, m_viewSnapEnabled, m_viewSnapM);
            ground.z = snapValue(ground.z, m_viewSnapEnabled, m_viewSnapM);
            if (m_racePlaceRouteNode && !m_authoring.raceRoutes.empty())
            {
                m_raceSelectedRoute = std::clamp(m_raceSelectedRoute, 0, static_cast<int>(m_authoring.raceRoutes.size()) - 1);
                auto& node = m_authoring.addRaceRouteNode(m_authoring.raceRoutes[static_cast<std::size_t>(m_raceSelectedRoute)].id);
                node.position = ground; m_raceSelectedRouteNode = static_cast<int>(m_authoring.raceRouteNodes.size()) - 1;
                m_studioMessage = "Placed route spline node. Placement remains active for rapid route tracing; ESC cancels.";
            }
            else if (m_raceSupportPlacementType >= 0)
            {
                auto& point = m_authoring.addRaceSupportPoint(static_cast<authoring::RaceSupportPointType>(m_raceSupportPlacementType));
                point.position = ground; m_raceSelectedSupportPoint = static_cast<int>(m_authoring.raceSupportPoints.size()) - 1;
                m_raceSupportPlacementType = -1; m_studioMessage = "Placed race-support point.";
            }
            else if (m_racePlacementType >= 0)
            {
                auto& created = m_authoring.addRaceMarker(static_cast<authoring::RaceMarkerType>(m_racePlacementType));
                created.position = ground; created.order = static_cast<int>(m_authoring.raceMarkers.size()) - 1;
                m_raceSelectedMarker = static_cast<int>(m_authoring.raceMarkers.size()) - 1;
                m_racePlacementType = -1; m_studioMessage = "Placed race object in 3D viewport.";
            }
            else if (m_raceConePlacementRole >= 0)
            {
                auto& cone = m_authoring.addCourseCone(static_cast<authoring::ConeRole>(m_raceConePlacementRole));
                cone.position = ground; cone.eventId = m_raceConeAuthorEventId;
                m_raceSelectedCone = static_cast<int>(m_authoring.courseCones.size()) - 1; m_raceConeSelectionKind = 0;
                m_studioMessage = "Placed physical/traffic course cone. Placement tool remains active; Esc ends it.";
            }
            else if (m_raceConeGatePlacementType >= 0)
            {
                int nextOrder = 0;
                for (const auto& existing : m_authoring.coneCourseGates) if (existing.eventId == m_raceConeAuthorEventId) nextOrder = std::max(nextOrder, existing.order + 1);
                auto& gate = m_authoring.addConeCourseGate(static_cast<authoring::ConeCourseGateType>(m_raceConeGatePlacementType));
                gate.position = ground; gate.eventId = m_raceConeAuthorEventId; gate.order = nextOrder;
                m_raceSelectedConeGate = static_cast<int>(m_authoring.coneCourseGates.size()) - 1; m_raceConeSelectionKind = 1;
                m_studioMessage = "Placed invisible cone-course rule element. Placement tool remains active; Esc ends it.";
            }
        }
        else if (m_raceTab == 1)
        {
            int bestIndex = -1; float bestDistance = 13.0f;
            for (int i = 0; i < static_cast<int>(m_authoring.raceRouteNodes.size()); ++i)
            {
                if (!m_authoring.raceRoutes.empty())
                {
                    const auto routeId = m_authoring.raceRoutes[static_cast<std::size_t>(std::clamp(m_raceSelectedRoute, 0, static_cast<int>(m_authoring.raceRoutes.size()) - 1))].id;
                    if (m_authoring.raceRouteNodes[static_cast<std::size_t>(i)].routeId != routeId) continue;
                }
                ImVec2 screen{}; if (!projectWorldPoint(view, m_authoring.raceRouteNodes[static_cast<std::size_t>(i)].position, screen)) continue;
                const float dx = io.MousePos.x - screen.x, dy = io.MousePos.y - screen.y, d = std::sqrt(dx * dx + dy * dy);
                if (d < bestDistance) { bestDistance = d; bestIndex = i; }
            }
            if (bestIndex >= 0) m_raceSelectedRouteNode = bestIndex;
        }
        else if (m_raceTab == 4)
        {
            int bestIndex = -1; float bestDistance = 13.0f;
            for (int i = 0; i < static_cast<int>(m_authoring.raceSupportPoints.size()); ++i)
            {
                ImVec2 screen{}; if (!projectWorldPoint(view, m_authoring.raceSupportPoints[static_cast<std::size_t>(i)].position, screen)) continue;
                const float dx = io.MousePos.x - screen.x, dy = io.MousePos.y - screen.y, d = std::sqrt(dx * dx + dy * dy);
                if (d < bestDistance) { bestDistance = d; bestIndex = i; }
            }
            if (bestIndex >= 0) m_raceSelectedSupportPoint = bestIndex;
        }
        else if (m_raceTab == 5)
        {
            int bestIndex = -1; float bestDistance = 13.0f;
            std::uint32_t pathId = 0;
            if (!m_authoring.broadcastCameraPaths.empty())
            {
                m_raceSelectedCameraPath = std::clamp(m_raceSelectedCameraPath, 0, static_cast<int>(m_authoring.broadcastCameraPaths.size()) - 1);
                pathId = m_authoring.broadcastCameraPaths[static_cast<std::size_t>(m_raceSelectedCameraPath)].id;
            }
            for (int i = 0; i < static_cast<int>(m_authoring.broadcastCameraNodes.size()); ++i)
            {
                const auto& node = m_authoring.broadcastCameraNodes[static_cast<std::size_t>(i)];
                if (pathId != 0 && node.pathId != pathId) continue;
                ImVec2 screen{}; if (!projectWorldPoint(view, node.position, screen)) continue;
                const float dx = io.MousePos.x - screen.x, dy = io.MousePos.y - screen.y, d = std::sqrt(dx * dx + dy * dy);
                if (d < bestDistance) { bestDistance = d; bestIndex = i; }
            }
            if (bestIndex >= 0) m_raceSelectedCameraNode = bestIndex;
        }
        else if (m_raceTab == 6)
        {
            int bestKind = -1, bestIndex = -1; float bestDistance = 14.0f;
            for (int i = 0; i < static_cast<int>(m_authoring.courseCones.size()); ++i)
            {
                ImVec2 screen{}; if (!projectWorldPoint(view, m_authoring.courseCones[static_cast<std::size_t>(i)].position, screen)) continue;
                const float dx=io.MousePos.x-screen.x, dy=io.MousePos.y-screen.y, d=std::sqrt(dx*dx+dy*dy);
                if (d < bestDistance) { bestDistance=d; bestKind=0; bestIndex=i; }
            }
            for (int i = 0; i < static_cast<int>(m_authoring.coneCourseGates.size()); ++i)
            {
                ImVec2 screen{}; if (!projectWorldPoint(view, m_authoring.coneCourseGates[static_cast<std::size_t>(i)].position, screen)) continue;
                const float dx=io.MousePos.x-screen.x, dy=io.MousePos.y-screen.y, d=std::sqrt(dx*dx+dy*dy);
                if (d < bestDistance) { bestDistance=d; bestKind=1; bestIndex=i; }
            }
            if (bestIndex >= 0)
            {
                m_raceConeSelectionKind = bestKind;
                if (bestKind == 0) m_raceSelectedCone = bestIndex; else m_raceSelectedConeGate = bestIndex;
            }
        }
        else if (m_raceTab == 0)
        {
            int bestIndex = -1; float bestDistance = 13.0f;
            for (int i = 0; i < static_cast<int>(m_authoring.raceMarkers.size()); ++i)
            {
                ImVec2 screen{}; if (!projectWorldPoint(view, m_authoring.raceMarkers[static_cast<std::size_t>(i)].position, screen)) continue;
                const float dx = io.MousePos.x - screen.x, dy = io.MousePos.y - screen.y, d = std::sqrt(dx * dx + dy * dy);
                if (d < bestDistance) { bestDistance = d; bestIndex = i; }
            }
            if (bestIndex >= 0) m_raceSelectedMarker = bestIndex;
        }
    }
    draw->PopClipRect();
}


void HeritageStudioApp::drawTrafficWorkspace()
{
    ensureScenePreviewInitialized();
    headingText("TRAFFIC / ROAD NETWORK");
    ImGui::SameLine();
    ImGui::TextDisabled("  roads / lane graph / traffic operations / physical agents / parking / navigation");
    ImGui::Separator();

    if (ImGui::Button("SAVE ROAD WORLD", ImVec2(150.0f, 30.0f)))
    {
        if (m_authoring.navigationBuild.enabled && m_authoring.navigationBuild.rebuildOnSave)
        {
            int createdNodes = 0, updatedNodes = 0, createdLinks = 0;
            m_authoring.compileRoadSplinesToLaneGraph(createdNodes, updatedNodes, createdLinks);
        }
        if (m_authoring.saveTraffic(m_studioProjectRoot / "traffic.hroad", m_studioMessage))
        {
            std::string runtimeMessage;
            if (saveRuntimeGameplay(runtimeMessage)) m_studioMessage = runtimeMessage;
            else m_studioMessage += " | " + runtimeMessage;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("LOAD ROAD WORLD", ImVec2(150.0f, 30.0f)))
        m_authoring.loadTraffic(m_studioProjectRoot / "traffic.hroad", m_studioMessage);
    ImGui::SameLine();
    if (ImGui::Button("VALIDATE", ImVec2(105.0f, 30.0f))) m_studioMessage = validateAuthoring();
    ImGui::SameLine();
    ImGui::TextDisabled("HROAD v6 | %s", m_studioMessage.c_str());

    const char* tabs[] = { "GRAPH / OVERRIDES", "ROAD SPLINES", "JUNCTIONS / SIGNALS", "PARKING / POPULATION / NAV", "OPERATIONS / ROUTING", "AGENTS / SIMULATION", "PORTALS / DENSITY / INCIDENTS" };
    for (int i = 0; i < IM_ARRAYSIZE(tabs); ++i)
    {
        if (i > 0) ImGui::SameLine();
        std::string label = std::string(tabs[i]) + (m_trafficTab == i ? " [ACTIVE]" : "") + "##trafficTab" + std::to_string(i);
        if (ImGui::Button(label.c_str(), ImVec2(i == 6 ? 235.0f : (i >= 3 ? 205.0f : 165.0f), 27.0f)))
        {
            m_trafficTab = i;
            m_trafficPlacementType = -1;
            m_trafficPlaceRoadNode = false;
            m_trafficPlaceIntersection = false;
            m_trafficPlaceParking = false;
            m_trafficPlacePortal = false;
            m_trafficPlaceDensityRegion = false;
            m_trafficPlaceIncident = false;
        }
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float listWidth = 330.0f;
    const float inspectorWidth = 405.0f;
    ImGui::BeginChild("TrafficObjectList", ImVec2(listWidth, 0.0f), true);

    if (m_trafficTab == 0)
    {
        sectionTitle("ROAD GRAPH NODES / LOW-LEVEL OVERRIDES");
        if (ImGui::Button("AUTO LINK LANES / NODES", ImVec2(-1.0f, 28.0f)))
        {
            std::vector<std::uint32_t> laneIds;
            for (const auto& node : m_authoring.trafficNodes) if (node.type == authoring::TrafficNodeType::LaneNode) laneIds.push_back(node.id);
            int createdCount = 0;
            for (std::size_t i = 1; i < laneIds.size(); ++i)
            {
                bool exists = false;
                for (const auto& link : m_authoring.trafficLinks)
                    if ((link.fromNodeId == laneIds[i - 1] && link.toNodeId == laneIds[i]) || (link.bidirectional && link.fromNodeId == laneIds[i] && link.toNodeId == laneIds[i - 1])) { exists = true; break; }
                if (!exists) { m_authoring.addTrafficLink(laneIds[i - 1], laneIds[i]); ++createdCount; }
            }
            m_studioMessage = "Added " + std::to_string(createdCount) + " missing graph link(s); existing overrides preserved.";
        }
        if (ImGui::BeginTable("TrafficAddButtons", 2, ImGuiTableFlags_SizingStretchSame))
        {
            const auto addButton = [&](const char* label, authoring::TrafficNodeType type)
            {
                ImGui::TableNextColumn();
                if (ImGui::Button(label, ImVec2(-1.0f, 27.0f))) { m_trafficSelectedNode = static_cast<int>(m_authoring.trafficNodes.size()); m_authoring.addTrafficNode(type); }
            };
            addButton("+ LANE NODE", authoring::TrafficNodeType::LaneNode); addButton("+ INTERSECTION", authoring::TrafficNodeType::Intersection);
            addButton("+ STOP", authoring::TrafficNodeType::Stop); addButton("+ YIELD", authoring::TrafficNodeType::Yield);
            addButton("+ TRAFFIC LIGHT", authoring::TrafficNodeType::TrafficLight); addButton("+ PARKING", authoring::TrafficNodeType::Parking);
            addButton("+ SPAWN", authoring::TrafficNodeType::Spawn); addButton("+ DESPAWN", authoring::TrafficNodeType::Despawn);
            addButton("+ DESTINATION", authoring::TrafficNodeType::Destination);
            ImGui::EndTable();
        }
        ImGui::Separator();
        for (int i = 0; i < static_cast<int>(m_authoring.trafficNodes.size()); ++i)
        {
            const auto& node = m_authoring.trafficNodes[static_cast<std::size_t>(i)];
            std::string label = node.name + "  [" + authoring::trafficNodeTypeName(node.type) + "]##traffic" + std::to_string(node.id);
            if (ImGui::Selectable(label.c_str(), m_trafficSelectedNode == i)) m_trafficSelectedNode = i;
        }
    }
    else if (m_trafficTab == 1)
    {
        sectionTitle("ROAD CONSTRUCTION");
        const char* classes[] = { "Motorway", "Arterial", "Collector", "Local", "Residential", "Service", "Mountain", "Gravel", "Dirt" };
        ImGui::Combo("New road class", &m_trafficRoadClassToAdd, classes, IM_ARRAYSIZE(classes));
        if (ImGui::Button("+ ROAD SPLINE", ImVec2(-1.0f, 29.0f)))
        {
            auto& road = m_authoring.addRoadSpline(static_cast<authoring::RoadClass>(m_trafficRoadClassToAdd));
            m_trafficSelectedRoad = static_cast<int>(m_authoring.roadSplines.size()) - 1;
            m_studioMessage = "Added " + road.name + ". Use TRACE ROAD NODES and click the scene surface.";
        }
        if (!m_authoring.roadSplines.empty())
        {
            if (ImGui::Button(m_trafficPlaceRoadNode ? "STOP TRACING" : "TRACE ROAD NODES", ImVec2(-1.0f, 29.0f))) m_trafficPlaceRoadNode = !m_trafficPlaceRoadNode;
            if (ImGui::Button("AUTO-SMOOTH ROAD TANGENTS", ImVec2(-1.0f, 28.0f)))
            {
                m_trafficSelectedRoad = std::clamp(m_trafficSelectedRoad, 0, static_cast<int>(m_authoring.roadSplines.size()) - 1);
                const auto roadId = m_authoring.roadSplines[static_cast<std::size_t>(m_trafficSelectedRoad)].id;
                std::vector<authoring::RoadSplineNode*> nodes;
                for (auto& node : m_authoring.roadSplineNodes) if (node.roadId == roadId) nodes.push_back(&node);
                std::stable_sort(nodes.begin(), nodes.end(), [](const auto* a, const auto* b){ return a->order < b->order; });
                for (std::size_t i = 0; i < nodes.size(); ++i)
                {
                    if (!nodes[i]->automaticTangents) continue;
                    const auto& previous = nodes[i == 0 ? i : i - 1]->position;
                    const auto& next = nodes[i + 1 < nodes.size() ? i + 1 : i]->position;
                    const auto tangent = mul3(sub3(next, previous), 0.20f);
                    nodes[i]->handleIn = mul3(tangent, -1.0f); nodes[i]->handleOut = tangent;
                }
                m_studioMessage = "Auto-smoothed automatic road spline tangents; manual handles were preserved.";
            }
            ImGui::Separator();
            for (int i = 0; i < static_cast<int>(m_authoring.roadSplines.size()); ++i)
            {
                const auto& road = m_authoring.roadSplines[static_cast<std::size_t>(i)];
                std::string label = road.name + "  [" + authoring::roadClassName(road.roadClass) + "]##road" + std::to_string(road.id);
                if (ImGui::Selectable(label.c_str(), m_trafficSelectedRoad == i)) m_trafficSelectedRoad = i;
            }
            m_trafficSelectedRoad = std::clamp(m_trafficSelectedRoad, 0, static_cast<int>(m_authoring.roadSplines.size()) - 1);
            const auto selectedRoadId = m_authoring.roadSplines[static_cast<std::size_t>(m_trafficSelectedRoad)].id;
            ImGui::Separator(); sectionTitle("CONTROL NODES");
            for (int i = 0; i < static_cast<int>(m_authoring.roadSplineNodes.size()); ++i)
            {
                const auto& node = m_authoring.roadSplineNodes[static_cast<std::size_t>(i)]; if (node.roadId != selectedRoadId) continue;
                std::string label = "Node " + std::to_string(node.order) + "##roadNode" + std::to_string(node.id);
                if (ImGui::Selectable(label.c_str(), m_trafficSelectedRoadNode == i)) m_trafficSelectedRoadNode = i;
            }
        }
    }
    else if (m_trafficTab == 2)
    {
        sectionTitle("JUNCTION AUTHORING");
        if (ImGui::Button("+ INTERSECTION", ImVec2(-1.0f, 28.0f))) { m_authoring.addRoadIntersection(); m_trafficSelectedIntersection = static_cast<int>(m_authoring.roadIntersections.size()) - 1; }
        if (ImGui::Button(m_trafficPlaceIntersection ? "CANCEL PLACE JUNCTION" : "PLACE JUNCTION ON SURFACE", ImVec2(-1.0f, 28.0f))) m_trafficPlaceIntersection = !m_trafficPlaceIntersection;
        ImGui::Separator();
        for (int i = 0; i < static_cast<int>(m_authoring.roadIntersections.size()); ++i)
        {
            const auto& j = m_authoring.roadIntersections[static_cast<std::size_t>(i)];
            std::string label = j.name + " [" + authoring::junctionPriorityName(j.priority) + "]##junction" + std::to_string(j.id);
            if (ImGui::Selectable(label.c_str(), m_trafficSelectedIntersection == i)) m_trafficSelectedIntersection = i;
        }
    }
    else if (m_trafficTab == 3)
    {
        sectionTitle("PARKING STRIPS");
        if (ImGui::Button("+ PARKING STRIP", ImVec2(-1.0f, 28.0f))) { m_authoring.addParkingStrip(); m_trafficSelectedParking = static_cast<int>(m_authoring.parkingStrips.size()) - 1; }
        if (ImGui::Button(m_trafficPlaceParking ? "CANCEL PLACE PARKING" : "PLACE PARKING ON SURFACE", ImVec2(-1.0f, 28.0f))) m_trafficPlaceParking = !m_trafficPlaceParking;
        ImGui::Separator();
        for (int i = 0; i < static_cast<int>(m_authoring.parkingStrips.size()); ++i)
        {
            const auto& parking = m_authoring.parkingStrips[static_cast<std::size_t>(i)];
            std::string label = parking.name + "  (" + std::to_string(parking.spaces) + ")##parkingStrip" + std::to_string(parking.id);
            if (ImGui::Selectable(label.c_str(), m_trafficSelectedParking == i)) m_trafficSelectedParking = i;
        }
        ImGui::Separator();
        ImGui::TextWrapped("Traffic Population and Navigation Build settings live in the inspector. They are serialized with HROAD so the free-roam world has one road-authority package.");
    }

    else if (m_trafficTab == 4)
    {
        sectionTitle("TRAFFIC OPERATIONS / ROUTING");
        const char* restrictionTypes[] = { "Closure", "Incident", "Construction", "Event Closure", "Toll", "Low Emission", "Weight Limit", "Height Limit" };
        ImGui::Combo("New restriction", &m_trafficRestrictionTypeToAdd, restrictionTypes, IM_ARRAYSIZE(restrictionTypes));
        if (ImGui::Button("+ ROAD RESTRICTION", ImVec2(-1.0f, 29.0f)))
        {
            auto& restriction = m_authoring.addRoadRestriction(static_cast<authoring::RoadRestrictionType>(m_trafficRestrictionTypeToAdd));
            if (!m_authoring.roadSplines.empty()) restriction.roadId = m_authoring.roadSplines.front().id;
            m_trafficSelectedRestriction = static_cast<int>(m_authoring.roadRestrictions.size()) - 1;
            m_studioMessage = "Added runtime road restriction; routing will consume it immediately after publish.";
        }
        ImGui::Separator();
        sectionTitle("ACTIVE / SCHEDULED RESTRICTIONS");
        for (int i = 0; i < static_cast<int>(m_authoring.roadRestrictions.size()); ++i)
        {
            const auto& restriction = m_authoring.roadRestrictions[static_cast<std::size_t>(i)];
            std::string label = restriction.name + " [" + authoring::roadRestrictionTypeName(restriction.type) + "]##restriction" + std::to_string(restriction.id);
            if (ImGui::Selectable(label.c_str(), m_trafficSelectedRestriction == i)) m_trafficSelectedRestriction = i;
        }
        ImGui::Separator();
        ImGui::TextWrapped("HROAD v4 operational policy adds semantic travel/lane-change links, driving-side rules, signal control, traffic streaming and time-aware closures without removing the low-level graph.");
    }
    else if (m_trafficTab == 5)
    {
        sectionTitle("TRAFFIC AGENT ARCHETYPES");
        const char* classes[] = { "Compact", "Sedan", "Sport", "Van", "Truck", "Motorcycle", "Emergency" };
        ImGui::Combo("New agent class", &m_trafficAgentClassToAdd, classes, IM_ARRAYSIZE(classes));
        if (ImGui::Button("+ AGENT PROFILE", ImVec2(-1.0f, 29.0f)))
        {
            auto& profile = m_authoring.addTrafficAgentProfile(static_cast<authoring::TrafficAgentClass>(m_trafficAgentClassToAdd));
            m_trafficSelectedAgentProfile = static_cast<int>(m_authoring.trafficAgentProfiles.size()) - 1;
            m_studioMessage = "Added traffic-agent archetype " + profile.name + ".";
        }
        ImGui::Separator();
        for (int i = 0; i < static_cast<int>(m_authoring.trafficAgentProfiles.size()); ++i)
        {
            const auto& profile = m_authoring.trafficAgentProfiles[static_cast<std::size_t>(i)];
            std::string label = profile.name + " [" + authoring::trafficAgentClassName(profile.vehicleClass) + "]##trafficAgent" + std::to_string(profile.id);
            if (ImGui::Selectable(label.c_str(), m_trafficSelectedAgentProfile == i)) m_trafficSelectedAgentProfile = i;
        }
        ImGui::Separator();
        ImGui::TextWrapped("HROAD v5 adds persistent driver/vehicle archetypes and the physical-agent simulation policy. Live agents remain disabled by default until you enable them in the inspector.");
    }
    else
    {
        sectionTitle("TRAFFIC SPAWN / DESPAWN PORTALS");
        if (ImGui::Button("+ TRAFFIC PORTAL", ImVec2(-1.0f, 28.0f)))
        {
            auto& portal = m_authoring.addTrafficSpawnPortal();
            m_trafficSelectedPortal = static_cast<int>(m_authoring.trafficSpawnPortals.size()) - 1;
            if (!m_authoring.trafficNodes.empty()) portal.nodeId = m_authoring.trafficNodes.front().id;
        }
        if (ImGui::Button(m_trafficPlacePortal ? "CANCEL PLACE PORTAL" : "PLACE PORTAL ON SURFACE", ImVec2(-1.0f, 27.0f))) m_trafficPlacePortal = !m_trafficPlacePortal;
        for (int i = 0; i < static_cast<int>(m_authoring.trafficSpawnPortals.size()); ++i)
        {
            const auto& portal = m_authoring.trafficSpawnPortals[static_cast<std::size_t>(i)];
            std::string label = portal.name + " [" + authoring::trafficPortalModeName(portal.mode) + "]##portal" + std::to_string(portal.id);
            if (ImGui::Selectable(label.c_str(), m_trafficSelectedPortal == i)) m_trafficSelectedPortal = i;
        }
        ImGui::Separator();
        sectionTitle("TRAFFIC DENSITY REGIONS");
        if (ImGui::Button("+ DENSITY REGION", ImVec2(-1.0f, 28.0f))) { m_authoring.addTrafficDensityRegion(); m_trafficSelectedDensityRegion = static_cast<int>(m_authoring.trafficDensityRegions.size()) - 1; }
        if (ImGui::Button(m_trafficPlaceDensityRegion ? "CANCEL PLACE REGION" : "PLACE DENSITY REGION", ImVec2(-1.0f, 27.0f))) m_trafficPlaceDensityRegion = !m_trafficPlaceDensityRegion;
        for (int i = 0; i < static_cast<int>(m_authoring.trafficDensityRegions.size()); ++i)
        {
            const auto& region = m_authoring.trafficDensityRegions[static_cast<std::size_t>(i)];
            std::string label = region.name + " [x" + std::to_string(region.densityMultiplier) + "]##density" + std::to_string(region.id);
            if (ImGui::Selectable(label.c_str(), m_trafficSelectedDensityRegion == i)) m_trafficSelectedDensityRegion = i;
        }
        ImGui::Separator();
        sectionTitle("INCIDENT / BREAKDOWN AUTHORING");
        const char* incidentTypes[] = { "Breakdown", "Collision", "Roadworks", "Police Stop", "Debris", "Flooding" };
        ImGui::Combo("New incident type", &m_trafficIncidentTypeToAdd, incidentTypes, IM_ARRAYSIZE(incidentTypes));
        if (ImGui::Button("+ INCIDENT", ImVec2(-1.0f, 28.0f)))
        {
            auto& incident = m_authoring.addTrafficIncident(static_cast<authoring::TrafficIncidentType>(m_trafficIncidentTypeToAdd));
            m_trafficSelectedIncident = static_cast<int>(m_authoring.trafficIncidents.size()) - 1;
            if (!m_authoring.roadSplines.empty()) incident.roadId = m_authoring.roadSplines.front().id;
        }
        if (ImGui::Button(m_trafficPlaceIncident ? "CANCEL PLACE INCIDENT" : "PLACE INCIDENT ON SURFACE", ImVec2(-1.0f, 27.0f))) m_trafficPlaceIncident = !m_trafficPlaceIncident;
        for (int i = 0; i < static_cast<int>(m_authoring.trafficIncidents.size()); ++i)
        {
            const auto& incident = m_authoring.trafficIncidents[static_cast<std::size_t>(i)];
            std::string label = incident.name + " [" + authoring::trafficIncidentTypeName(incident.type) + "]##incident" + std::to_string(incident.id);
            if (ImGui::Selectable(label.c_str(), m_trafficSelectedIncident == i)) m_trafficSelectedIncident = i;
        }
        ImGui::Separator();
        ImGui::TextWrapped("HROAD v6 makes population spatial and operational: portals control where agents enter/leave, density regions shape traffic demand, and incidents feed routing + local behavior.");
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("TrafficViewport", ImVec2(std::max(320.0f, available.x - listWidth - inspectorWidth - 16.0f), 0.0f), true);
    drawTrafficViewportInteractive();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("TrafficInspector", ImVec2(0.0f, 0.0f), true);
    if (m_trafficTab == 0)
    {
        sectionTitle("GRAPH OVERRIDE INSPECTOR");
        if (!m_authoring.trafficNodes.empty())
        {
            m_trafficSelectedNode = std::clamp(m_trafficSelectedNode, 0, static_cast<int>(m_authoring.trafficNodes.size()) - 1);
            auto& node = m_authoring.trafficNodes[static_cast<std::size_t>(m_trafficSelectedNode)];
            inputString("Name", node.name);
            int typeIndex = static_cast<int>(node.type); const char* types[] = { "Lane Node", "Intersection", "Stop", "Yield", "Traffic Light", "Parking", "Spawn", "Despawn", "Destination" };
            if (ImGui::Combo("Type", &typeIndex, types, IM_ARRAYSIZE(types))) node.type = static_cast<authoring::TrafficNodeType>(typeIndex);
            ImGui::DragFloat3("Position", &node.position.x, 0.05f); ImGui::DragFloat("Heading deg", &node.headingDeg, 0.25f, -360.0f, 360.0f);
            ImGui::DragFloat("Speed limit km/h", &node.speedLimitKmh, 1.0f, 5.0f, 250.0f); ImGui::SliderInt("Lane count", &node.lanes, 1, 8);
            ImGui::InputInt("Priority", &node.priority); ImGui::Checkbox("Bidirectional", &node.bidirectional); ImGui::Checkbox("Overtaking allowed", &node.overtakingAllowed); ImGui::SliderFloat("Traffic density", &node.density, 0.0f, 2.0f, "%.2f");
            sectionTitle("ROAD CONNECTIONS");
            if (m_authoring.trafficNodes.size() > 1)
            {
                m_trafficLinkTargetNode = std::clamp(m_trafficLinkTargetNode, 0, static_cast<int>(m_authoring.trafficNodes.size()) - 1);
                if (m_trafficLinkTargetNode == m_trafficSelectedNode) m_trafficLinkTargetNode = (m_trafficSelectedNode + 1) % static_cast<int>(m_authoring.trafficNodes.size());
                if (ImGui::BeginCombo("Connect to", m_authoring.trafficNodes[static_cast<std::size_t>(m_trafficLinkTargetNode)].name.c_str()))
                {
                    for (int i = 0; i < static_cast<int>(m_authoring.trafficNodes.size()); ++i) { if (i == m_trafficSelectedNode) continue; if (ImGui::Selectable(m_authoring.trafficNodes[static_cast<std::size_t>(i)].name.c_str(), i == m_trafficLinkTargetNode)) m_trafficLinkTargetNode = i; } ImGui::EndCombo();
                }
                if (ImGui::Button("ADD DIRECTED LINK", ImVec2(-1.0f, 27.0f))) { m_authoring.addTrafficLink(node.id, m_authoring.trafficNodes[static_cast<std::size_t>(m_trafficLinkTargetNode)].id); m_trafficSelectedLink = static_cast<int>(m_authoring.trafficLinks.size()) - 1; }
            }
            if (!m_authoring.trafficLinks.empty())
            {
                m_trafficSelectedLink = std::clamp(m_trafficSelectedLink, 0, static_cast<int>(m_authoring.trafficLinks.size()) - 1); auto& link = m_authoring.trafficLinks[static_cast<std::size_t>(m_trafficSelectedLink)];
                ImGui::Separator(); ImGui::TextDisabled("SELECTED LINK #%u", link.id); ImGui::SliderInt("Link lanes", &link.lanes, 1, 8); ImGui::DragFloat("Link speed km/h", &link.speedLimitKmh, 1.0f, 5.0f, 250.0f);
                ImGui::Checkbox("Link bidirectional", &link.bidirectional); ImGui::Checkbox("Link overtaking", &link.overtakingAllowed); ImGui::SliderFloat("Link density", &link.density, 0.0f, 2.0f, "%.2f");
                int linkType = static_cast<int>(link.type); const char* linkTypes[] = { "Travel", "Lane Change", "Merge", "Junction Turn", "Parking Access", "Spawn Access" };
                if (ImGui::Combo("Movement semantic", &linkType, linkTypes, IM_ARRAYSIZE(linkTypes))) link.type = static_cast<authoring::TrafficLinkType>(linkType);
                ImGui::DragFloat("Route cost multiplier", &link.routeCostMultiplier, 0.01f, 0.05f, 50.0f, "%.2f"); ImGui::Checkbox("Link enabled", &link.enabled);
                ImGui::TextDisabled(link.generated ? "Generated link (manual edits retained until graph sync touches this movement)." : "Manual override link.");
                if (ImGui::Button("DELETE SELECTED LINK", ImVec2(-1.0f, 27.0f))) { m_authoring.removeTrafficLink(static_cast<std::size_t>(m_trafficSelectedLink)); m_trafficSelectedLink = std::max(0, m_trafficSelectedLink - 1); }
            }
            if (ImGui::Button("DELETE ROAD NODE", ImVec2(-1.0f, 28.0f))) { m_authoring.removeTrafficNode(static_cast<std::size_t>(m_trafficSelectedNode)); m_trafficSelectedNode = std::max(0, m_trafficSelectedNode - 1); }
        }
    }
    else if (m_trafficTab == 1)
    {
        sectionTitle("ROAD SPLINE INSPECTOR");
        if (!m_authoring.roadSplines.empty())
        {
            m_trafficSelectedRoad = std::clamp(m_trafficSelectedRoad, 0, static_cast<int>(m_authoring.roadSplines.size()) - 1); auto& road = m_authoring.roadSplines[static_cast<std::size_t>(m_trafficSelectedRoad)];
            inputString("Road name", road.name); int cls = static_cast<int>(road.roadClass); const char* classes[] = { "Motorway", "Arterial", "Collector", "Local", "Residential", "Service", "Mountain", "Gravel", "Dirt" };
            if (ImGui::Combo("Road class", &cls, classes, IM_ARRAYSIZE(classes))) road.roadClass = static_cast<authoring::RoadClass>(cls);
            ImGui::Checkbox("Enabled", &road.enabled); ImGui::Checkbox("One way", &road.oneWay); ImGui::SliderInt("Forward lanes", &road.lanesForward, 1, 8); ImGui::SliderInt("Backward lanes", &road.lanesBackward, 0, 8);
            ImGui::DragFloat("Lane width m", &road.laneWidthM, 0.05f, 2.0f, 5.0f); ImGui::DragFloat("Speed limit km/h", &road.speedLimitKmh, 1.0f, 5.0f, 250.0f);
            ImGui::DragFloat("Left shoulder m", &road.shoulderLeftM, 0.05f, 0.0f, 8.0f); ImGui::DragFloat("Right shoulder m", &road.shoulderRightM, 0.05f, 0.0f, 8.0f); ImGui::DragFloat("Median width m", &road.medianWidthM, 0.05f, 0.0f, 15.0f);
            ImGui::Checkbox("Left sidewalk", &road.sidewalkLeft); ImGui::Checkbox("Right sidewalk", &road.sidewalkRight); ImGui::Checkbox("Left parking", &road.parkingLeft); ImGui::Checkbox("Right parking", &road.parkingRight);
            ImGui::SliderFloat("Traffic density", &road.trafficDensity, 0.0f, 2.0f, "%.2f"); ImGui::SliderFloat("Spawn weight", &road.spawnWeight, 0.0f, 4.0f, "%.2f");
            if (!m_authoring.roadSplineNodes.empty())
            {
                m_trafficSelectedRoadNode = std::clamp(m_trafficSelectedRoadNode, 0, static_cast<int>(m_authoring.roadSplineNodes.size()) - 1); auto& node = m_authoring.roadSplineNodes[static_cast<std::size_t>(m_trafficSelectedRoadNode)];
                if (node.roadId == road.id)
                {
                    sectionTitle("SELECTED SPLINE NODE"); ImGui::InputInt("Order", &node.order); ImGui::DragFloat3("Position", &node.position.x, 0.05f); ImGui::Checkbox("Automatic tangents", &node.automaticTangents);
                    if (!node.automaticTangents) { ImGui::DragFloat3("Handle in", &node.handleIn.x, 0.05f); ImGui::DragFloat3("Handle out", &node.handleOut.x, 0.05f); }
                    ImGui::DragFloat("Width scale", &node.widthScale, 0.01f, 0.25f, 3.0f); ImGui::DragFloat("Banking deg", &node.bankingDeg, 0.1f, -30.0f, 30.0f);
                    if (ImGui::Button("DELETE SPLINE NODE", ImVec2(-1.0f, 27.0f))) { m_authoring.removeRoadSplineNode(static_cast<std::size_t>(m_trafficSelectedRoadNode)); m_trafficSelectedRoadNode = std::max(0, m_trafficSelectedRoadNode - 1); }
                }
            }
            if (ImGui::Button("DELETE ROAD SPLINE", ImVec2(-1.0f, 29.0f))) { m_authoring.removeRoadSpline(static_cast<std::size_t>(m_trafficSelectedRoad)); m_trafficSelectedRoad = std::max(0, m_trafficSelectedRoad - 1); }
        }
    }
    else if (m_trafficTab == 2)
    {
        sectionTitle("INTERSECTION / TURN RULES");
        if (!m_authoring.roadIntersections.empty())
        {
            m_trafficSelectedIntersection = std::clamp(m_trafficSelectedIntersection, 0, static_cast<int>(m_authoring.roadIntersections.size()) - 1); auto& j = m_authoring.roadIntersections[static_cast<std::size_t>(m_trafficSelectedIntersection)];
            inputString("Name", j.name); ImGui::DragFloat3("Position", &j.position.x, 0.05f); ImGui::DragFloat("Control radius m", &j.radiusM, 0.25f, 2.0f, 100.0f);
            int priority = static_cast<int>(j.priority); const char* priorities[] = { "Priority Road", "Yield", "Stop", "Signalized", "Roundabout", "Uncontrolled" }; if (ImGui::Combo("Right of way", &priority, priorities, IM_ARRAYSIZE(priorities))) j.priority = static_cast<authoring::JunctionPriority>(priority);
            ImGui::Checkbox("Traffic lights", &j.trafficLights); ImGui::Checkbox("Pedestrian crossing", &j.pedestrianCrossing); ImGui::DragFloat("Approach speed km/h", &j.approachSpeedKmh, 1.0f, 5.0f, 100.0f);
            if (m_authoring.roadSplines.size() >= 2)
            {
                m_trafficConnectorFromRoad = std::clamp(m_trafficConnectorFromRoad, 0, static_cast<int>(m_authoring.roadSplines.size()) - 1); m_trafficConnectorToRoad = std::clamp(m_trafficConnectorToRoad, 0, static_cast<int>(m_authoring.roadSplines.size()) - 1);
                if (ImGui::BeginCombo("From road", m_authoring.roadSplines[static_cast<std::size_t>(m_trafficConnectorFromRoad)].name.c_str())) { for (int i=0;i<static_cast<int>(m_authoring.roadSplines.size());++i) if (ImGui::Selectable(m_authoring.roadSplines[static_cast<std::size_t>(i)].name.c_str(), i==m_trafficConnectorFromRoad)) m_trafficConnectorFromRoad=i; ImGui::EndCombo(); }
                if (ImGui::BeginCombo("To road", m_authoring.roadSplines[static_cast<std::size_t>(m_trafficConnectorToRoad)].name.c_str())) { for (int i=0;i<static_cast<int>(m_authoring.roadSplines.size());++i) if (ImGui::Selectable(m_authoring.roadSplines[static_cast<std::size_t>(i)].name.c_str(), i==m_trafficConnectorToRoad)) m_trafficConnectorToRoad=i; ImGui::EndCombo(); }
                if (ImGui::Button("+ LEGAL TURN CONNECTOR", ImVec2(-1.0f, 27.0f))) { auto& c=m_authoring.addTurnConnector(j.id,m_authoring.roadSplines[static_cast<std::size_t>(m_trafficConnectorFromRoad)].id,m_authoring.roadSplines[static_cast<std::size_t>(m_trafficConnectorToRoad)].id); m_trafficSelectedConnector=static_cast<int>(m_authoring.turnConnectors.size())-1; }
            }
            if (!m_authoring.turnConnectors.empty())
            {
                m_trafficSelectedConnector=std::clamp(m_trafficSelectedConnector,0,static_cast<int>(m_authoring.turnConnectors.size())-1); auto& c=m_authoring.turnConnectors[static_cast<std::size_t>(m_trafficSelectedConnector)];
                if (c.intersectionId==j.id) { sectionTitle("SELECTED TURN"); ImGui::InputInt("From lane",&c.fromLane); ImGui::InputInt("To lane",&c.toLane); ImGui::Checkbox("Allowed",&c.enabled); ImGui::Checkbox("Yield",&c.yield); ImGui::Checkbox("U-turn",&c.uTurn); ImGui::DragFloat("Turn speed km/h",&c.speedLimitKmh,1.0f,5.0f,100.0f); ImGui::InputInt("Conflict group",&c.conflictGroup); ImGui::DragFloat("Reservation seconds",&c.reservationSeconds,0.1f,0.25f,15.0f); if(ImGui::Button("DELETE TURN",ImVec2(-1.0f,25.0f)))m_authoring.removeTurnConnector(static_cast<std::size_t>(m_trafficSelectedConnector)); }
            }
            sectionTitle("SIGNAL PHASES");
            if (ImGui::Button("+ SIGNAL PHASE", ImVec2(-1.0f, 27.0f))) { m_authoring.addTrafficSignalPhase(j.id); m_trafficSelectedSignalPhase=static_cast<int>(m_authoring.trafficSignalPhases.size())-1; }
            for (int i=0;i<static_cast<int>(m_authoring.trafficSignalPhases.size());++i) { const auto& ph=m_authoring.trafficSignalPhases[static_cast<std::size_t>(i)]; if(ph.intersectionId!=j.id)continue; std::string label=ph.name+"##phase"+std::to_string(ph.id); if(ImGui::Selectable(label.c_str(),m_trafficSelectedSignalPhase==i))m_trafficSelectedSignalPhase=i; }
            if (!m_authoring.trafficSignalPhases.empty()) { m_trafficSelectedSignalPhase=std::clamp(m_trafficSelectedSignalPhase,0,static_cast<int>(m_authoring.trafficSignalPhases.size())-1); auto& ph=m_authoring.trafficSignalPhases[static_cast<std::size_t>(m_trafficSelectedSignalPhase)]; if(ph.intersectionId==j.id){ inputString("Phase name",ph.name); ImGui::InputInt("Phase order",&ph.order); ImGui::DragFloat("Green sec",&ph.greenSeconds,0.5f,1.0f,180.0f); ImGui::DragFloat("Yellow sec",&ph.yellowSeconds,0.25f,0.5f,10.0f); ImGui::DragFloat("All red sec",&ph.allRedSeconds,0.25f,0.0f,10.0f); inputString("Connector IDs CSV",ph.connectorIds); if(ImGui::Button("DELETE SIGNAL PHASE",ImVec2(-1.0f,25.0f)))m_authoring.removeTrafficSignalPhase(static_cast<std::size_t>(m_trafficSelectedSignalPhase)); }}
            sectionTitle("LIVE SIGNAL CONTROLLER");
            authoring::IntersectionController* control = nullptr; for (auto& candidate : m_authoring.intersectionControllers) if (candidate.intersectionId == j.id) { control = &candidate; break; }
            if (control)
            {
                int mode = static_cast<int>(control->mode); const char* modes[] = { "Fixed Time", "Actuated", "Adaptive" };
                if (ImGui::Combo("Controller mode", &mode, modes, IM_ARRAYSIZE(modes))) control->mode = static_cast<authoring::SignalControlMode>(mode);
                ImGui::DragFloat("Phase offset sec", &control->phaseOffsetSeconds, 0.25f, 0.0f, 300.0f); ImGui::DragFloat("Minimum green sec", &control->minimumGreenSeconds, 0.25f, 1.0f, 120.0f);
                ImGui::DragFloat("Maximum green sec", &control->maximumGreenSeconds, 0.5f, 1.0f, 300.0f); ImGui::DragFloat("Detector distance m", &control->detectorDistanceM, 0.5f, 5.0f, 250.0f);
                ImGui::DragFloat("Gap-out seconds", &control->gapOutSeconds, 0.1f, 0.5f, 20.0f); ImGui::Checkbox("Queue adaptive", &control->queueAdaptive); ImGui::Checkbox("Emergency preemption", &control->emergencyPreemption);
            }
            if (ImGui::Button("DELETE INTERSECTION", ImVec2(-1.0f, 28.0f))) { m_authoring.removeRoadIntersection(static_cast<std::size_t>(m_trafficSelectedIntersection)); m_trafficSelectedIntersection=std::max(0,m_trafficSelectedIntersection-1); }
        }
    }
    else if (m_trafficTab == 3)
    {
        sectionTitle("PARKING STRIP INSPECTOR");
        if (!m_authoring.parkingStrips.empty())
        {
            m_trafficSelectedParking=std::clamp(m_trafficSelectedParking,0,static_cast<int>(m_authoring.parkingStrips.size())-1); auto& parking=m_authoring.parkingStrips[static_cast<std::size_t>(m_trafficSelectedParking)]; inputString("Name",parking.name); ImGui::DragFloat3("Position",&parking.position.x,0.05f); ImGui::DragFloat("Heading",&parking.headingDeg,0.25f,-360.0f,360.0f); ImGui::SliderInt("Spaces",&parking.spaces,1,200); ImGui::DragFloat("Spacing m",&parking.spacingM,0.05f,2.0f,15.0f); ImGui::DragFloat("Parking angle",&parking.angleDeg,1.0f,-90.0f,90.0f); ImGui::Checkbox("Right side",&parking.rightSide); ImGui::SliderFloat("Occupancy",&parking.occupancy,0.0f,1.0f,"%.2f");
            if (!m_authoring.roadSplines.empty()) { int roadIndex=0; for(int i=0;i<static_cast<int>(m_authoring.roadSplines.size());++i)if(m_authoring.roadSplines[static_cast<std::size_t>(i)].id==parking.roadId)roadIndex=i; if(ImGui::BeginCombo("Attach road",m_authoring.roadSplines[static_cast<std::size_t>(roadIndex)].name.c_str())){for(int i=0;i<static_cast<int>(m_authoring.roadSplines.size());++i)if(ImGui::Selectable(m_authoring.roadSplines[static_cast<std::size_t>(i)].name.c_str(),i==roadIndex)){roadIndex=i;parking.roadId=m_authoring.roadSplines[static_cast<std::size_t>(i)].id;}ImGui::EndCombo();}}
            if(ImGui::Button("DELETE PARKING STRIP",ImVec2(-1.0f,26.0f)))m_authoring.removeParkingStrip(static_cast<std::size_t>(m_trafficSelectedParking));
        }
        sectionTitle("TRAFFIC POPULATION"); auto& pop=m_authoring.trafficPopulation; ImGui::SliderFloat("Global density",&pop.globalDensity,0.0f,2.0f,"%.2f"); ImGui::SliderFloat("Parked density",&pop.parkedDensity,0.0f,1.0f,"%.2f"); ImGui::SliderFloat("Rush-hour multiplier",&pop.rushHourMultiplier,0.25f,3.0f,"%.2f"); ImGui::SliderFloat("Night multiplier",&pop.nightMultiplier,0.0f,2.0f,"%.2f"); ImGui::SliderFloat("Heavy vehicle share",&pop.heavyVehicleShare,0.0f,0.5f,"%.3f"); ImGui::SliderFloat("Motorcycle share",&pop.motorcycleShare,0.0f,0.5f,"%.3f"); ImGui::SliderFloat("Commercial share",&pop.commercialShare,0.0f,0.5f,"%.3f"); ImGui::SliderFloat("Emergency share",&pop.emergencyShare,0.0f,0.05f,"%.4f"); ImGui::SliderFloat("Lane-change aggression",&pop.laneChangeAggression,0.0f,1.0f,"%.2f"); ImGui::SliderFloat("Speed variance",&pop.speedVariance,0.0f,0.35f,"%.2f"); ImGui::SliderInt("Max active vehicles",&pop.maxActiveVehicles,0,1000);
        sectionTitle("NAVIGATION COMPILER"); auto& nav=m_authoring.navigationBuild; ImGui::Checkbox("Navigation enabled",&nav.enabled); ImGui::Checkbox("Sync generated lane graph on save",&nav.rebuildOnSave); ImGui::DragFloat("Max road slope deg",&nav.maxSlopeDeg,0.25f,1.0f,45.0f); ImGui::DragFloat("Minimum turn radius m",&nav.minimumTurnRadiusM,0.25f,1.0f,30.0f); ImGui::DragFloat("Lane-change length m",&nav.laneChangeLengthM,0.5f,5.0f,100.0f); ImGui::DragFloat("Junction lookahead m",&nav.junctionLookaheadM,0.5f,5.0f,150.0f); ImGui::DragFloat("Merge lookahead m",&nav.mergeLookaheadM,0.5f,5.0f,250.0f);
        if (ImGui::Button("SYNC / COMPILE LANE GRAPH", ImVec2(-1.0f, 30.0f)))
        {
            int createdNodes = 0, updatedNodes = 0, createdLinks = 0;
            m_authoring.compileRoadSplinesToLaneGraph(createdNodes, updatedNodes, createdLinks);
            m_studioMessage = "Navigation compile created " + std::to_string(createdNodes) + " lane node(s), synchronized " + std::to_string(updatedNodes)
                + " generated node(s), and added " + std::to_string(createdLinks) + " link(s). Manual graph content was preserved.";
        }
    }

    else if (m_trafficTab == 4)
    {
        sectionTitle("DRIVING / CAR-FOLLOWING RULES");
        auto& rules = m_authoring.trafficRules; int side = static_cast<int>(rules.drivingSide); const char* sides[] = { "Right-hand traffic", "Left-hand traffic" };
        if (ImGui::Combo("Driving side", &side, sides, IM_ARRAYSIZE(sides))) rules.drivingSide = static_cast<authoring::DrivingSide>(side);
        ImGui::Checkbox("Keep to driving side", &rules.keepToDrivingSide); ImGui::Checkbox("Allow legal turn on red", &rules.allowTurnOnRed); ImGui::Checkbox("Emergency corridor behavior", &rules.emergencyCorridor);
        ImGui::DragFloat("Desired time gap s", &rules.desiredTimeGapS, 0.05f, 0.5f, 5.0f); ImGui::DragFloat("Minimum gap m", &rules.minimumGapM, 0.1f, 0.5f, 20.0f);
        ImGui::DragFloat("Desired acceleration m/s2", &rules.desiredAccelerationMps2, 0.05f, 0.2f, 8.0f); ImGui::DragFloat("Comfortable braking m/s2", &rules.comfortableBrakingMps2, 0.05f, 0.2f, 10.0f);
        ImGui::DragFloat("Lane-change cooldown s", &rules.laneChangeCooldownS, 0.1f, 0.0f, 20.0f); ImGui::DragFloat("Lane-change minimum gap m", &rules.laneChangeMinimumGapM, 0.25f, 1.0f, 50.0f);
        ImGui::DragFloat("Lane-change route cost", &rules.laneChangeRouteCost, 0.01f, 1.0f, 5.0f); ImGui::DragFloat("Merge route cost", &rules.mergeRouteCost, 0.01f, 1.0f, 5.0f);
        ImGui::DragFloat("Emergency yield radius m", &rules.emergencyYieldRadiusM, 1.0f, 10.0f, 500.0f); ImGui::DragFloat("Roundabout yield lookahead m", &rules.roundaboutYieldDistanceM, 0.5f, 2.0f, 100.0f);

        sectionTitle("TRAFFIC STREAMING SECTORS"); auto& stream = m_authoring.trafficStreaming;
        ImGui::DragFloat("Full simulation radius m", &stream.fullSimulationRadiusM, 5.0f, 50.0f, 5000.0f); ImGui::DragFloat("Simplified simulation radius m", &stream.simplifiedSimulationRadiusM, 10.0f, 100.0f, 10000.0f);
        ImGui::DragFloat("Dormant persistence radius m", &stream.dormantPersistenceRadiusM, 25.0f, 250.0f, 50000.0f); ImGui::DragFloat("Sector size m", &stream.sectorSizeM, 5.0f, 50.0f, 2000.0f);
        ImGui::SliderInt("Max spawns / second", &stream.maxSpawnsPerSecond, 1, 100); ImGui::SliderInt("Max despawns / second", &stream.maxDespawnsPerSecond, 1, 200);
        ImGui::DragFloat("Despawn-behind distance m", &stream.despawnBehindDistanceM, 5.0f, 25.0f, 5000.0f); ImGui::Checkbox("Retain dormant traffic state", &stream.retainDormantState); ImGui::DragFloat("Dormant state minutes", &stream.dormantStateMinutes, 0.5f, 0.0f, 120.0f);

        sectionTitle("SELECTED ROAD RESTRICTION");
        if (!m_authoring.roadRestrictions.empty())
        {
            m_trafficSelectedRestriction = std::clamp(m_trafficSelectedRestriction, 0, static_cast<int>(m_authoring.roadRestrictions.size()) - 1); auto& restriction = m_authoring.roadRestrictions[static_cast<std::size_t>(m_trafficSelectedRestriction)];
            inputString("Restriction name", restriction.name); int type = static_cast<int>(restriction.type); const char* types[] = { "Closure", "Incident", "Construction", "Event Closure", "Toll", "Low Emission", "Weight Limit", "Height Limit" };
            if (ImGui::Combo("Restriction type", &type, types, IM_ARRAYSIZE(types))) restriction.type = static_cast<authoring::RoadRestrictionType>(type); ImGui::Checkbox("Restriction enabled", &restriction.enabled);
            if (!m_authoring.roadSplines.empty())
            {
                int roadIndex = 0; for (int i=0;i<static_cast<int>(m_authoring.roadSplines.size());++i) if (m_authoring.roadSplines[static_cast<std::size_t>(i)].id == restriction.roadId) roadIndex=i;
                if (ImGui::BeginCombo("Affected road", m_authoring.roadSplines[static_cast<std::size_t>(roadIndex)].name.c_str())) { for (int i=0;i<static_cast<int>(m_authoring.roadSplines.size());++i) if (ImGui::Selectable(m_authoring.roadSplines[static_cast<std::size_t>(i)].name.c_str(), i==roadIndex)) { roadIndex=i; restriction.roadId=m_authoring.roadSplines[static_cast<std::size_t>(i)].id; } ImGui::EndCombo(); }
            }
            int restrictionLinkId = static_cast<int>(restriction.linkId); if (ImGui::InputInt("Specific graph link ID (0 = road)", &restrictionLinkId)) restriction.linkId = static_cast<std::uint32_t>(std::max(0, restrictionLinkId)); ImGui::Checkbox("Block normal traffic", &restriction.blockTraffic); ImGui::Checkbox("Emergency vehicles exempt", &restriction.emergencyExempt);
            ImGui::DragFloat("Temporary speed limit km/h", &restriction.speedLimitKmh, 1.0f, 0.0f, 250.0f); ImGui::DragFloat("Route cost multiplier", &restriction.routeCostMultiplier, 0.05f, 0.1f, 100.0f);
            ImGui::SliderFloat("Start hour", &restriction.startHour, 0.0f, 24.0f, "%.2f"); ImGui::SliderFloat("End hour", &restriction.endHour, 0.0f, 24.0f, "%.2f");
            ImGui::DragFloat("Mass limit kg", &restriction.vehicleMassLimitKg, 100.0f, 0.0f, 100000.0f); ImGui::DragFloat("Height limit m", &restriction.vehicleHeightLimitM, 0.05f, 0.0f, 10.0f);
            if (ImGui::Button("DELETE ROAD RESTRICTION", ImVec2(-1.0f, 27.0f))) { m_authoring.removeRoadRestriction(static_cast<std::size_t>(m_trafficSelectedRestriction)); m_trafficSelectedRestriction=std::max(0,m_trafficSelectedRestriction-1); }
        }
    }
    else if (m_trafficTab == 5)
    {
        sectionTitle("PHYSICAL TRAFFIC-AGENT SIMULATION");
        auto& sim = m_authoring.trafficAgentSimulation;
        ImGui::Checkbox("Enable live traffic agents", &sim.enabled);
        ImGui::Checkbox("Create debug proxy vehicles", &sim.createDebugProxyVehicles);
        ImGui::Checkbox("Use Heritage full vehicle dynamics for FULL-tier traffic", &sim.useHeritageVehicleDynamics);
        if (sim.useHeritageVehicleDynamics) ImGui::TextWrapped("FULL-tier traffic will use real Heritage Vehicle chassis/wheels/inputs. Simplified and dormant agents keep the cheap logical representation.");
        ImGui::Checkbox("Enable autonomous lane changes", &sim.enableLaneChanges);
        ImGui::Checkbox("Enable merge negotiation", &sim.enableMerges);
        ImGui::Checkbox("Enable parking entry / exit", &sim.enableParking);
        ImGui::SliderInt("Maximum full-physics agents", &sim.maxFullPhysicsAgents, 0, 500);
        ImGui::SliderInt("Route lookahead links", &sim.routeLookaheadLinks, 2, 64);
        ImGui::DragFloat("Traffic vehicle high-rate physics Hz", &sim.trafficVehicleHighRateHz, 25.0f, 125.0f, 1000.0f);
        ImGui::DragFloat("Full simulation update Hz", &sim.fullSimulationHz, 1.0f, 5.0f, 120.0f);
        ImGui::DragFloat("Simplified simulation update Hz", &sim.simplifiedSimulationHz, 1.0f, 1.0f, 60.0f);
        ImGui::DragFloat("Perception range m", &sim.perceptionRangeM, 2.0f, 20.0f, 500.0f);
        ImGui::DragFloat("Stop-line buffer m", &sim.stopLineBufferM, 0.1f, 0.25f, 10.0f);
        ImGui::DragFloat("Intersection creep km/h", &sim.intersectionCreepSpeedKmh, 0.25f, 0.0f, 20.0f);
        ImGui::DragFloat("Parking approach km/h", &sim.parkingApproachSpeedKmh, 0.5f, 1.0f, 40.0f);
        ImGui::DragFloat("Spawn minimum player distance m", &sim.spawnMinDistancePlayerM, 5.0f, 0.0f, 5000.0f);
        ImGui::DragFloat("Spawn maximum player distance m", &sim.spawnMaxDistancePlayerM, 10.0f, 10.0f, 10000.0f);
        ImGui::DragFloat("Minimum spawn gap m", &sim.minimumSpawnGapM, 0.5f, 2.0f, 200.0f);
        ImGui::DragFloat("Stuck timeout s", &sim.stuckTimeoutS, 0.5f, 1.0f, 120.0f);
        ImGui::DragFloat("Despawn grace s", &sim.despawnGraceS, 0.25f, 0.0f, 60.0f);

        sectionTitle("ADVANCED TRAFFIC BEHAVIOR / RECOVERY");
        auto& behavior = m_authoring.trafficBehavior;
        ImGui::Checkbox("Zipper merge negotiation", &behavior.zipperMerging);
        ImGui::Checkbox("Roundabout gap negotiation", &behavior.roundaboutNegotiation);
        ImGui::Checkbox("Enforce stop-sign dwell", &behavior.enforceStopDwell);
        ImGui::Checkbox("Opportunistic overtaking", &behavior.opportunisticOvertaking);
        ImGui::Checkbox("Natural queue discharge reaction", &behavior.queueDischargeReaction);
        ImGui::Checkbox("Staged reverse parking maneuvers", &behavior.stagedParkingManeuvers);
        ImGui::Checkbox("Automatic stuck recovery", &behavior.stuckRecovery);
        ImGui::Checkbox("Generate collision incidents", &behavior.collisionIncidentResponse);
        ImGui::Checkbox("Emergency agents respond to incidents", &behavior.emergencyIncidentDispatch);
        ImGui::DragFloat("Zipper alternation window s", &behavior.zipperAlternationWindowS, 0.1f, 0.1f, 15.0f);
        ImGui::DragFloat("Merge courtesy gap s", &behavior.mergeCourtesyGapS, 0.05f, 0.2f, 5.0f);
        ImGui::DragFloat("Roundabout entry gap s", &behavior.roundaboutEntryGapS, 0.05f, 0.5f, 8.0f);
        ImGui::DragFloat("Stop-sign dwell s", &behavior.stopDwellS, 0.05f, 0.0f, 8.0f);
        ImGui::DragFloat("Yield creep speed km/h", &behavior.yieldCreepSpeedKmh, 0.25f, 0.0f, 15.0f);
        ImGui::DragFloat("Overtake minimum gain km/h", &behavior.overtakeMinimumGainKmh, 0.5f, 0.0f, 50.0f);
        ImGui::DragFloat("Overtake return gap m", &behavior.overtakeReturnGapM, 0.5f, 2.0f, 100.0f);
        ImGui::DragFloat("Queue reaction spread s", &behavior.queueReactionSpreadS, 0.05f, 0.0f, 4.0f);
        ImGui::DragFloat("Reverse parking speed km/h", &behavior.parkingReverseSpeedKmh, 0.25f, 0.5f, 15.0f);
        ImGui::DragFloat("Recovery reverse duration s", &behavior.recoveryReverseSeconds, 0.05f, 0.0f, 8.0f);
        ImGui::DragFloat("Recovery reroute threshold s", &behavior.recoveryRerouteSeconds, 0.25f, 1.0f, 60.0f);
        ImGui::DragFloat("Recovery relocate threshold s", &behavior.recoveryTeleportSeconds, 0.5f, 5.0f, 180.0f);
        ImGui::DragFloat("Collision proximity threshold m", &behavior.collisionDistanceM, 0.05f, 0.1f, 5.0f);
        ImGui::DragFloat("Emergency incident lookahead m", &behavior.emergencyIncidentLookaheadM, 25.0f, 50.0f, 10000.0f);

        sectionTitle("SELECTED DRIVER / VEHICLE ARCHETYPE");
        if (!m_authoring.trafficAgentProfiles.empty())
        {
            m_trafficSelectedAgentProfile = std::clamp(m_trafficSelectedAgentProfile, 0, static_cast<int>(m_authoring.trafficAgentProfiles.size()) - 1);
            auto& profile = m_authoring.trafficAgentProfiles[static_cast<std::size_t>(m_trafficSelectedAgentProfile)];
            inputString("Profile name", profile.name);
            int vehicleClass = static_cast<int>(profile.vehicleClass); const char* classes[] = { "Compact", "Sedan", "Sport", "Van", "Truck", "Motorcycle", "Emergency" };
            if (ImGui::Combo("Vehicle class", &vehicleClass, classes, IM_ARRAYSIZE(classes))) profile.vehicleClass = static_cast<authoring::TrafficAgentClass>(vehicleClass);
            ImGui::Checkbox("Profile enabled", &profile.enabled); inputString("Vehicle preset / factory key", profile.vehiclePreset);
            ImGui::DragFloat("Spawn weight", &profile.spawnWeight, 0.05f, 0.0f, 20.0f); ImGui::DragFloat("Vehicle length m", &profile.lengthM, 0.05f, 0.5f, 30.0f); ImGui::DragFloat("Vehicle width m", &profile.widthM, 0.05f, 0.3f, 5.0f);
            ImGui::SliderFloat("Maximum-speed factor", &profile.maxSpeedFactor, 0.25f, 2.0f, "%.2f"); ImGui::SliderFloat("Acceleration factor", &profile.accelerationFactor, 0.2f, 2.0f, "%.2f"); ImGui::SliderFloat("Braking factor", &profile.brakingFactor, 0.2f, 2.0f, "%.2f");
            ImGui::DragFloat("Desired time gap s", &profile.desiredTimeGapS, 0.05f, 0.3f, 5.0f); ImGui::DragFloat("Minimum gap m", &profile.minimumGapM, 0.1f, 0.5f, 20.0f); ImGui::DragFloat("Reaction time s", &profile.reactionTimeS, 0.02f, 0.05f, 2.0f);
            ImGui::SliderFloat("Lane-change aggression", &profile.laneChangeAggression, 0.0f, 1.0f, "%.2f"); ImGui::SliderFloat("Courtesy / yielding", &profile.courtesy, 0.0f, 1.0f, "%.2f"); ImGui::SliderFloat("Speed-limit compliance", &profile.speedCompliance, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Illegal-overtake chance", &profile.illegalOvertakeChance, 0.0f, 1.0f, "%.3f"); ImGui::SliderFloat("Parking skill", &profile.parkingSkill, 0.0f, 1.0f, "%.2f");
            if (ImGui::Button("DELETE AGENT PROFILE", ImVec2(-1.0f, 27.0f))) { m_authoring.removeTrafficAgentProfile(static_cast<std::size_t>(m_trafficSelectedAgentProfile)); m_trafficSelectedAgentProfile = std::max(0, m_trafficSelectedAgentProfile - 1); }
        }
        else ImGui::TextDisabled("Add at least one agent profile in the left panel.");
    }
    else
    {
        sectionTitle("SELECTED TRAFFIC PORTAL");
        if (!m_authoring.trafficSpawnPortals.empty())
        {
            m_trafficSelectedPortal = std::clamp(m_trafficSelectedPortal, 0, static_cast<int>(m_authoring.trafficSpawnPortals.size()) - 1);
            auto& portal = m_authoring.trafficSpawnPortals[static_cast<std::size_t>(m_trafficSelectedPortal)];
            inputString("Portal name", portal.name); ImGui::Checkbox("Portal enabled", &portal.enabled);
            int mode = static_cast<int>(portal.mode); const char* portalModes[] = { "Spawn + Despawn", "Spawn Only", "Despawn Only" };
            if (ImGui::Combo("Portal mode", &mode, portalModes, IM_ARRAYSIZE(portalModes))) portal.mode = static_cast<authoring::TrafficPortalMode>(mode);
            ImGui::DragFloat3("Portal position", &portal.position.x, 0.1f); ImGui::DragFloat("Portal heading", &portal.headingDeg, 0.5f, -360.0f, 360.0f); ImGui::DragFloat("Portal radius m", &portal.radiusM, 0.5f, 2.0f, 250.0f);
            if (!m_authoring.trafficNodes.empty())
            {
                int nodeIndex = 0; for (int i = 0; i < static_cast<int>(m_authoring.trafficNodes.size()); ++i) if (m_authoring.trafficNodes[static_cast<std::size_t>(i)].id == portal.nodeId) nodeIndex = i;
                if (ImGui::BeginCombo("Graph anchor node", m_authoring.trafficNodes[static_cast<std::size_t>(nodeIndex)].name.c_str())) { for (int i = 0; i < static_cast<int>(m_authoring.trafficNodes.size()); ++i) if (ImGui::Selectable(m_authoring.trafficNodes[static_cast<std::size_t>(i)].name.c_str(), i == nodeIndex)) { nodeIndex = i; portal.nodeId = m_authoring.trafficNodes[static_cast<std::size_t>(i)].id; } ImGui::EndCombo(); }
            }
            ImGui::DragFloat("Spawn weight", &portal.spawnWeight, 0.05f, 0.0f, 50.0f); ImGui::SliderInt("Portal max concurrent", &portal.maxConcurrentAgents, 0, 500);
            ImGui::SliderFloat("Portal start hour", &portal.startHour, 0.0f, 24.0f, "%.2f"); ImGui::SliderFloat("Portal end hour", &portal.endHour, 0.0f, 24.0f, "%.2f");
            ImGui::DragFloat("Min player distance m", &portal.minimumPlayerDistanceM, 5.0f, 0.0f, 5000.0f); ImGui::DragFloat("Max player distance m", &portal.maximumPlayerDistanceM, 10.0f, 10.0f, 10000.0f);
            ImGui::Checkbox("Emergency traffic allowed", &portal.emergencyAllowed); inputString("Allowed classes CSV (blank = all)", portal.allowedClasses);
            if (ImGui::Button("DELETE TRAFFIC PORTAL", ImVec2(-1.0f, 27.0f))) { m_authoring.removeTrafficSpawnPortal(static_cast<std::size_t>(m_trafficSelectedPortal)); m_trafficSelectedPortal = std::max(0, m_trafficSelectedPortal - 1); }
        }
        sectionTitle("SELECTED DENSITY REGION");
        if (!m_authoring.trafficDensityRegions.empty())
        {
            m_trafficSelectedDensityRegion = std::clamp(m_trafficSelectedDensityRegion, 0, static_cast<int>(m_authoring.trafficDensityRegions.size()) - 1); auto& region = m_authoring.trafficDensityRegions[static_cast<std::size_t>(m_trafficSelectedDensityRegion)];
            inputString("Density region name", region.name); ImGui::Checkbox("Density region enabled", &region.enabled); ImGui::DragFloat3("Density center", &region.position.x, 0.1f); ImGui::DragFloat("Density radius m", &region.radiusM, 5.0f, 10.0f, 10000.0f);
            ImGui::SliderFloat("Density multiplier", &region.densityMultiplier, 0.0f, 4.0f, "%.2f"); ImGui::SliderFloat("Speed multiplier", &region.speedMultiplier, 0.25f, 1.5f, "%.2f"); ImGui::SliderFloat("Lane aggression offset", &region.laneChangeAggressionOffset, -1.0f, 1.0f, "%.2f"); ImGui::SliderFloat("Parking multiplier", &region.parkingMultiplier, 0.0f, 3.0f, "%.2f");
            ImGui::SliderFloat("Density start hour", &region.startHour, 0.0f, 24.0f, "%.2f"); ImGui::SliderFloat("Density end hour", &region.endHour, 0.0f, 24.0f, "%.2f");
            if (ImGui::Button("DELETE DENSITY REGION", ImVec2(-1.0f, 27.0f))) { m_authoring.removeTrafficDensityRegion(static_cast<std::size_t>(m_trafficSelectedDensityRegion)); m_trafficSelectedDensityRegion = std::max(0, m_trafficSelectedDensityRegion - 1); }
        }
        sectionTitle("SELECTED INCIDENT / BREAKDOWN");
        if (!m_authoring.trafficIncidents.empty())
        {
            m_trafficSelectedIncident = std::clamp(m_trafficSelectedIncident, 0, static_cast<int>(m_authoring.trafficIncidents.size()) - 1); auto& incident = m_authoring.trafficIncidents[static_cast<std::size_t>(m_trafficSelectedIncident)];
            inputString("Incident name", incident.name); int type = static_cast<int>(incident.type); const char* types[] = { "Breakdown", "Collision", "Roadworks", "Police Stop", "Debris", "Flooding" }; if (ImGui::Combo("Incident type", &type, types, IM_ARRAYSIZE(types))) incident.type = static_cast<authoring::TrafficIncidentType>(type); ImGui::Checkbox("Incident enabled", &incident.enabled);
            ImGui::DragFloat3("Incident position", &incident.position.x, 0.1f); ImGui::DragFloat("Incident radius m", &incident.radiusM, 0.5f, 1.0f, 500.0f);
            if (!m_authoring.roadSplines.empty()) { int roadIndex = 0; for (int i=0;i<static_cast<int>(m_authoring.roadSplines.size());++i) if (m_authoring.roadSplines[static_cast<std::size_t>(i)].id == incident.roadId) roadIndex=i; if (ImGui::BeginCombo("Incident road", m_authoring.roadSplines[static_cast<std::size_t>(roadIndex)].name.c_str())) { for (int i=0;i<static_cast<int>(m_authoring.roadSplines.size());++i) if (ImGui::Selectable(m_authoring.roadSplines[static_cast<std::size_t>(i)].name.c_str(), i==roadIndex)) { roadIndex=i; incident.roadId=m_authoring.roadSplines[static_cast<std::size_t>(i)].id; } ImGui::EndCombo(); } }
            int incidentLinkId = static_cast<int>(incident.linkId); if (ImGui::InputInt("Specific affected link ID (0 = area/road)", &incidentLinkId)) incident.linkId = static_cast<std::uint32_t>(std::max(0, incidentLinkId));
            ImGui::SliderFloat("Severity", &incident.severity, 0.0f, 1.0f, "%.2f"); ImGui::SliderFloat("Blocked lane fraction", &incident.blockedLaneFraction, 0.0f, 1.0f, "%.2f"); ImGui::DragFloat("Incident speed limit km/h", &incident.speedLimitKmh, 1.0f, 0.0f, 200.0f); ImGui::DragFloat("Incident route cost", &incident.routeCostMultiplier, 0.05f, 1.0f, 50.0f);
            ImGui::DragFloat("Response delay s", &incident.responseDelayS, 1.0f, 0.0f, 3600.0f); ImGui::DragFloat("Clear after s (0 persistent)", &incident.clearAfterS, 5.0f, 0.0f, 86400.0f); ImGui::Checkbox("Emergency response", &incident.emergencyResponse); ImGui::Checkbox("Hazard lights", &incident.hazardLights);
            if (ImGui::Button("DELETE INCIDENT", ImVec2(-1.0f, 27.0f))) { m_authoring.removeTrafficIncident(static_cast<std::size_t>(m_trafficSelectedIncident)); m_trafficSelectedIncident = std::max(0, m_trafficSelectedIncident - 1); }
        }
        sectionTitle("WEATHER / VISIBILITY DRIVING RESPONSE"); auto& env = m_authoring.trafficEnvironment;
        ImGui::SliderFloat("Wet-road speed factor", &env.wetSpeedFactor, 0.2f, 1.0f, "%.2f"); ImGui::SliderFloat("Heavy-rain speed factor", &env.heavyRainSpeedFactor, 0.2f, 1.0f, "%.2f"); ImGui::SliderFloat("Snow speed factor", &env.snowSpeedFactor, 0.1f, 1.0f, "%.2f"); ImGui::SliderFloat("Ice speed factor", &env.iceSpeedFactor, 0.05f, 1.0f, "%.2f"); ImGui::SliderFloat("Night speed factor", &env.nightSpeedFactor, 0.4f, 1.0f, "%.2f");
        ImGui::SliderFloat("Wet following-gap factor", &env.wetFollowingGapFactor, 1.0f, 3.0f, "%.2f"); ImGui::SliderFloat("Wet braking factor", &env.wetBrakingFactor, 0.2f, 1.0f, "%.2f"); ImGui::SliderFloat("Poor-visibility speed factor", &env.poorVisibilitySpeedFactor, 0.2f, 1.0f, "%.2f"); ImGui::Checkbox("Avoid standing water", &env.standingWaterAvoidance); ImGui::Checkbox("Weather-aware lane changing", &env.weatherAwareLaneChanges);
        sectionTitle("LIVE TRAFFIC DEBUG POLICY"); auto& debug = m_authoring.trafficDebug; ImGui::Checkbox("Traffic debug enabled", &debug.enabled); ImGui::Checkbox("Show agent IDs", &debug.showAgentIds); ImGui::Checkbox("Show routes", &debug.showRoutes); ImGui::Checkbox("Show intentions", &debug.showIntentions); ImGui::Checkbox("Show perception", &debug.showPerception); ImGui::Checkbox("Show following gaps", &debug.showFollowingGaps); ImGui::Checkbox("Show lane-change scores", &debug.showLaneChangeScores); ImGui::Checkbox("Show wait reasons", &debug.showWaitReasons); ImGui::Checkbox("Show streaming tiers", &debug.showStreamingTiers); ImGui::Checkbox("Show incident influence", &debug.showIncidentInfluence); ImGui::SliderInt("Max detailed agents", &debug.maxDetailedAgents, 1, 250);
    }
    ImGui::EndChild();
}


void HeritageStudioApp::drawTrafficViewportInteractive()
{
    sectionTitle("3D ROAD GRAPH");
    drawViewportToolbar(m_viewGridVisible, m_viewSnapEnabled, m_viewSnapM,
        m_viewYawDeg, m_viewPitchDeg, m_viewDistanceM, m_viewOrthographic);

    if (m_trafficTab == 0 && ImGui::BeginTable("TrafficPlacementToolbar", 3, ImGuiTableFlags_SizingStretchSame))
    {
        const auto placeButton = [&](const char* label, authoring::TrafficNodeType type)
        {
            ImGui::TableNextColumn();
            const bool active = m_trafficPlacementType == static_cast<int>(type);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(label, ImVec2(-1.0f, 27.0f))) m_trafficPlacementType = active ? -1 : static_cast<int>(type);
            if (active) ImGui::PopStyleColor();
        };
        placeButton("LANE NODE", authoring::TrafficNodeType::LaneNode); placeButton("INTERSECTION", authoring::TrafficNodeType::Intersection); placeButton("TRAFFIC LIGHT", authoring::TrafficNodeType::TrafficLight);
        placeButton("PARKING", authoring::TrafficNodeType::Parking); placeButton("SPAWN", authoring::TrafficNodeType::Spawn); placeButton("DESTINATION", authoring::TrafficNodeType::Destination); ImGui::EndTable();
    }
    if (m_trafficPlacementType >= 0 || m_trafficPlaceRoadNode || m_trafficPlaceIntersection || m_trafficPlaceParking || m_trafficPlacePortal || m_trafficPlaceDensityRegion || m_trafficPlaceIncident)
        ImGui::TextDisabled("Placement mode active: click the visible scene surface (ground-plane fallback). ESC cancels.");

    ImVec2 size = ImGui::GetContentRegionAvail();
    size.y = std::max(180.0f, size.y);
    bool hovered = false;
    const StudioViewportProjection view = prepareInteractiveViewport(
        m_window, m_viewFly, "##TrafficInteractiveViewport", size,
        m_viewYawDeg, m_viewPitchDeg, m_viewDistanceM, m_viewTarget,
        m_viewGridVisible, m_viewOrthographic, hovered);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    drawSceneGeometryBackdrop(draw, view, m_scenePreview.get(), m_viewGridVisible);
    drawFlyNavigationOverlay(draw, view, m_viewFly);
    draw->PushClipRect(view.min, view.max, true);
    const ImGuiIO& io = ImGui::GetIO();

    if (hovered && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        m_trafficPlacementType = -1;
        m_trafficPlaceRoadNode = false;
        m_trafficPlaceIntersection = false;
        m_trafficPlaceParking = false;
        m_trafficPlacePortal = false;
        m_trafficPlaceDensityRegion = false;
        m_trafficPlaceIncident = false;
    }
    if (!m_authoring.trafficNodes.empty())
    {
        m_trafficSelectedNode = std::clamp(m_trafficSelectedNode, 0, static_cast<int>(m_authoring.trafficNodes.size()) - 1);
        if (hovered && ImGui::IsKeyPressed(ImGuiKey_F, false))
        {
            m_viewTarget = m_authoring.trafficNodes[static_cast<std::size_t>(m_trafficSelectedNode)].position;
            m_viewDistanceM = std::clamp(m_viewDistanceM, 8.0f, 100.0f);
        }
    }

    // HROAD v2 owns explicit links. The viewport now displays the serialized
    // road graph rather than inferring a temporary chain from vector order.
    const auto nodeById = [&](std::uint32_t id) -> const authoring::TrafficNode*
    {
        for (const auto& node : m_authoring.trafficNodes)
            if (node.id == id) return &node;
        return nullptr;
    };
    for (const auto& link : m_authoring.trafficLinks)
    {
        const auto* from = nodeById(link.fromNodeId);
        const auto* to = nodeById(link.toNodeId);
        if (!from || !to)
            continue;
        ImVec2 a{}, b{};
        if (projectWorldPoint(view, from->position, a) && projectWorldPoint(view, to->position, b))
        {
            ImU32 linkColor = IM_COL32(70, 170, 235, link.enabled ? 210 : 70);
            if (link.type == authoring::TrafficLinkType::LaneChange) linkColor = IM_COL32(190, 125, 235, link.enabled ? 205 : 65);
            else if (link.type == authoring::TrafficLinkType::Merge) linkColor = IM_COL32(245, 175, 70, link.enabled ? 215 : 65);
            else if (link.type == authoring::TrafficLinkType::JunctionTurn) linkColor = IM_COL32(90, 220, 150, link.enabled ? 215 : 65);
            draw->AddLine(a, b, linkColor, link.type == authoring::TrafficLinkType::LaneChange ? 1.3f : (link.lanes > 1 ? 3.0f : 2.0f));
            if (link.bidirectional)
            {
                const ImVec2 midpoint{ (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f };
                draw->AddCircleFilled(midpoint, 3.0f, IM_COL32(190, 225, 250, 230));
            }
        }
    }


    const auto roadBezier = [](const authoring::Vec3& p0, const authoring::Vec3& p1, const authoring::Vec3& p2, const authoring::Vec3& p3, float t)
    {
        const float u = 1.0f - t;
        return add3(add3(mul3(p0, u * u * u), mul3(p1, 3.0f * u * u * t)), add3(mul3(p2, 3.0f * u * t * t), mul3(p3, t * t * t)));
    };
    for (int roadIndex = 0; roadIndex < static_cast<int>(m_authoring.roadSplines.size()); ++roadIndex)
    {
        const auto& road = m_authoring.roadSplines[static_cast<std::size_t>(roadIndex)]; if (!road.enabled) continue;
        std::vector<const authoring::RoadSplineNode*> nodes;
        for (const auto& node : m_authoring.roadSplineNodes) if (node.roadId == road.id) nodes.push_back(&node);
        std::stable_sort(nodes.begin(), nodes.end(), [](const auto* a, const auto* b){ return a->order < b->order; });
        const bool selectedRoad = m_trafficTab == 1 && roadIndex == m_trafficSelectedRoad;
        for (std::size_t segment = 1; segment < nodes.size(); ++segment)
        {
            const auto* a = nodes[segment - 1]; const auto* b = nodes[segment];
            const authoring::Vec3 p0=a->position, p1=add3(a->position,a->handleOut), p2=add3(b->position,b->handleIn), p3=b->position;
            authoring::Vec3 previous=p0;
            for(int step=1;step<=12;++step)
            {
                const float t=static_cast<float>(step)/12.0f; const auto current=roadBezier(p0,p1,p2,p3,t); const auto tangent=normalize3(sub3(current,previous)); const authoring::Vec3 side{tangent.z,0.0f,-tangent.x};
                const float lanes=static_cast<float>(std::max(1,road.lanesForward)+std::max(0,road.lanesBackward)); const float halfWidth=(lanes*road.laneWidthM+road.medianWidthM)*0.5f;
                ImVec2 pa{},pb{},la{},lb{},ra{},rb{};
                if(projectWorldPoint(view,previous,pa)&&projectWorldPoint(view,current,pb))draw->AddLine(pa,pb,IM_COL32(235,210,95,selectedRoad?245:175),selectedRoad?3.5f:2.2f);
                const auto prevL=add3(previous,mul3(side,-halfWidth*a->widthScale)); const auto curL=add3(current,mul3(side,-halfWidth*(a->widthScale+(b->widthScale-a->widthScale)*t)));
                const auto prevR=add3(previous,mul3(side,halfWidth*a->widthScale)); const auto curR=add3(current,mul3(side,halfWidth*(a->widthScale+(b->widthScale-a->widthScale)*t)));
                if(projectWorldPoint(view,prevL,la)&&projectWorldPoint(view,curL,lb))draw->AddLine(la,lb,IM_COL32(210,215,220,selectedRoad?170:90),1.0f);
                if(projectWorldPoint(view,prevR,ra)&&projectWorldPoint(view,curR,rb))draw->AddLine(ra,rb,IM_COL32(210,215,220,selectedRoad?170:90),1.0f);
                previous=current;
            }
        }
        if(selectedRoad)
        {
            for(int i=0;i<static_cast<int>(m_authoring.roadSplineNodes.size());++i){const auto& node=m_authoring.roadSplineNodes[static_cast<std::size_t>(i)];if(node.roadId!=road.id)continue;ImVec2 screen{};if(projectWorldPoint(view,node.position,screen)){char label[32]{};std::snprintf(label,sizeof(label),"R%d",node.order);drawMarker(draw,screen,IM_COL32(245,210,80,255),i==m_trafficSelectedRoadNode,label);}}
        }
    }
    for(int i=0;i<static_cast<int>(m_authoring.roadIntersections.size());++i)
    {
        const auto& junction=m_authoring.roadIntersections[static_cast<std::size_t>(i)]; ImVec2 screen{}; if(!projectWorldPoint(view,junction.position,screen))continue;
        drawMarker(draw,screen,IM_COL32(245,155,65,255),m_trafficTab==2&&i==m_trafficSelectedIntersection,junction.name.c_str());
        draw->AddCircle(screen,std::max(5.0f,junction.radiusM*0.25f),IM_COL32(245,155,65,120),0,1.2f);
    }
    for(int i=0;i<static_cast<int>(m_authoring.parkingStrips.size());++i)
    {
        const auto& parking=m_authoring.parkingStrips[static_cast<std::size_t>(i)]; ImVec2 screen{}; if(!projectWorldPoint(view,parking.position,screen))continue;
        drawMarker(draw,screen,IM_COL32(180,125,235,255),m_trafficTab==3&&i==m_trafficSelectedParking,parking.name.c_str());
    }

    for (int i = 0; i < static_cast<int>(m_authoring.trafficSpawnPortals.size()); ++i)
    {
        const auto& portal = m_authoring.trafficSpawnPortals[static_cast<std::size_t>(i)]; if (!portal.enabled) continue; ImVec2 screen{}; if (!projectWorldPoint(view, portal.position, screen)) continue;
        drawMarker(draw, screen, IM_COL32(80, 220, 205, 255), m_trafficTab == 6 && i == m_trafficSelectedPortal, portal.name.c_str());
        draw->AddCircle(screen, std::max(5.0f, portal.radiusM * 0.18f), IM_COL32(80, 220, 205, 125), 0, 1.2f);
    }
    for (int i = 0; i < static_cast<int>(m_authoring.trafficDensityRegions.size()); ++i)
    {
        const auto& region = m_authoring.trafficDensityRegions[static_cast<std::size_t>(i)]; if (!region.enabled) continue; ImVec2 screen{}; if (!projectWorldPoint(view, region.position, screen)) continue;
        const float radiusPx = std::clamp(region.radiusM * 0.08f, 8.0f, 160.0f);
        draw->AddCircle(screen, radiusPx, IM_COL32(100, 185, 250, m_trafficTab == 6 && i == m_trafficSelectedDensityRegion ? 220 : 105), 0, m_trafficTab == 6 && i == m_trafficSelectedDensityRegion ? 2.5f : 1.2f);
        if (m_trafficTab == 6 && i == m_trafficSelectedDensityRegion) drawMarker(draw, screen, IM_COL32(100, 185, 250, 255), true, region.name.c_str());
    }
    for (int i = 0; i < static_cast<int>(m_authoring.trafficIncidents.size()); ++i)
    {
        const auto& incident = m_authoring.trafficIncidents[static_cast<std::size_t>(i)]; if (!incident.enabled) continue; ImVec2 screen{}; if (!projectWorldPoint(view, incident.position, screen)) continue;
        drawMarker(draw, screen, IM_COL32(245, 100, 75, 255), m_trafficTab == 6 && i == m_trafficSelectedIncident, incident.name.c_str());
        draw->AddCircle(screen, std::max(5.0f, incident.radiusM * 0.20f), IM_COL32(245, 100, 75, 120), 0, 1.2f);
    }

    for (int i = 0; i < static_cast<int>(m_authoring.trafficNodes.size()); ++i)
    {
        const auto& node = m_authoring.trafficNodes[static_cast<std::size_t>(i)];
        ImVec2 screen{};
        if (!projectWorldPoint(view, node.position, screen))
            continue;
        ImU32 color = IM_COL32(95, 170, 235, 255);
        switch (node.type)
        {
        case authoring::TrafficNodeType::Intersection: color = IM_COL32(245, 195, 70, 255); break;
        case authoring::TrafficNodeType::Stop: color = IM_COL32(235, 75, 75, 255); break;
        case authoring::TrafficNodeType::Yield: color = IM_COL32(240, 145, 70, 255); break;
        case authoring::TrafficNodeType::TrafficLight: color = IM_COL32(85, 220, 120, 255); break;
        case authoring::TrafficNodeType::Parking: color = IM_COL32(190, 120, 235, 255); break;
        case authoring::TrafficNodeType::Spawn: color = IM_COL32(90, 220, 190, 255); break;
        case authoring::TrafficNodeType::Despawn: color = IM_COL32(150, 150, 150, 255); break;
        case authoring::TrafficNodeType::Destination: color = IM_COL32(240, 110, 190, 255); break;
        default: break;
        }
        drawMarker(draw, screen, color, i == m_trafficSelectedNode, node.name.c_str());

        const float h = radians(node.headingDeg);
        const authoring::Vec3 headingPoint = add3(node.position, { std::sin(h) * 3.0f, 0.0f, std::cos(h) * 3.0f });
        ImVec2 headingScreen{};
        if (projectWorldPoint(view, headingPoint, headingScreen))
            draw->AddLine(screen, headingScreen, color, i == m_trafficSelectedNode ? 2.2f : 1.2f);
    }

    bool gizmoCaptured = false;
    if (m_trafficTab == 0 && !m_authoring.trafficNodes.empty() && m_trafficPlacementType < 0)
    {
        auto& selected = m_authoring.trafficNodes[static_cast<std::size_t>(m_trafficSelectedNode)];
        const int previousAxis = m_viewGizmoAxis;
        drawMoveGizmo(draw, view, selected.position, hovered,
            m_viewSnapEnabled, m_viewSnapM, m_viewGizmoAxis, m_viewGizmoDragAccumulator);
        gizmoCaptured = m_viewGizmoAxis >= 0 || previousAxis >= 0;
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !gizmoCaptured)
    {
        authoring::Vec3 ground{};
        const bool hasGround = viewportAuthoringPoint(view, io.MousePos, m_scenePreview.get(), ground);
        if (hasGround) { ground.x=snapValue(ground.x,m_viewSnapEnabled,m_viewSnapM); ground.z=snapValue(ground.z,m_viewSnapEnabled,m_viewSnapM); }
        if (m_trafficTab == 1 && m_trafficPlaceRoadNode && hasGround && !m_authoring.roadSplines.empty())
        {
            m_trafficSelectedRoad=std::clamp(m_trafficSelectedRoad,0,static_cast<int>(m_authoring.roadSplines.size())-1); auto& node=m_authoring.addRoadSplineNode(m_authoring.roadSplines[static_cast<std::size_t>(m_trafficSelectedRoad)].id); node.position=ground; m_trafficSelectedRoadNode=static_cast<int>(m_authoring.roadSplineNodes.size())-1; m_studioMessage="Added road spline control node.";
        }
        else if (m_trafficTab == 2 && m_trafficPlaceIntersection && hasGround)
        {
            auto& junction=m_authoring.addRoadIntersection(); junction.position=ground; m_trafficSelectedIntersection=static_cast<int>(m_authoring.roadIntersections.size())-1; m_trafficPlaceIntersection=false; m_studioMessage="Placed road intersection.";
        }
        else if (m_trafficTab == 3 && m_trafficPlaceParking && hasGround)
        {
            auto& parking=m_authoring.addParkingStrip(); parking.position=ground; m_trafficSelectedParking=static_cast<int>(m_authoring.parkingStrips.size())-1; m_trafficPlaceParking=false; m_studioMessage="Placed parking strip anchor.";
        }
        else if (m_trafficTab == 6 && m_trafficPlacePortal && hasGround)
        {
            auto& portal = m_authoring.addTrafficSpawnPortal(); portal.position = ground; m_trafficSelectedPortal = static_cast<int>(m_authoring.trafficSpawnPortals.size()) - 1;
            const auto nearest = std::min_element(m_authoring.trafficNodes.begin(), m_authoring.trafficNodes.end(), [&](const auto& a, const auto& b) { const auto da=sub3(a.position,ground), db=sub3(b.position,ground); return dot3(da,da) < dot3(db,db); });
            if (nearest != m_authoring.trafficNodes.end()) portal.nodeId = nearest->id; m_trafficPlacePortal = false; m_studioMessage = "Placed traffic portal and anchored it to the nearest traffic graph node.";
        }
        else if (m_trafficTab == 6 && m_trafficPlaceDensityRegion && hasGround)
        {
            auto& region = m_authoring.addTrafficDensityRegion(); region.position = ground; m_trafficSelectedDensityRegion = static_cast<int>(m_authoring.trafficDensityRegions.size()) - 1; m_trafficPlaceDensityRegion = false; m_studioMessage = "Placed traffic density region.";
        }
        else if (m_trafficTab == 6 && m_trafficPlaceIncident && hasGround)
        {
            auto& incident = m_authoring.addTrafficIncident(static_cast<authoring::TrafficIncidentType>(m_trafficIncidentTypeToAdd)); incident.position = ground; m_trafficSelectedIncident = static_cast<int>(m_authoring.trafficIncidents.size()) - 1;
            if (!m_authoring.roadSplines.empty()) incident.roadId = m_authoring.roadSplines[static_cast<std::size_t>(std::clamp(m_trafficSelectedRoad, 0, static_cast<int>(m_authoring.roadSplines.size()) - 1))].id;
            m_trafficPlaceIncident = false; m_studioMessage = "Placed traffic incident / breakdown influence area.";
        }
        else if (m_trafficTab == 0 && m_trafficPlacementType >= 0 && hasGround)
        {
            auto& created=m_authoring.addTrafficNode(static_cast<authoring::TrafficNodeType>(m_trafficPlacementType)); created.position=ground; m_trafficSelectedNode=static_cast<int>(m_authoring.trafficNodes.size())-1; m_trafficPlacementType=-1; m_studioMessage="Placed traffic node in 3D viewport.";
        }
        else if (m_trafficTab == 1)
        {
            int best=-1;float bestD=13.0f;for(int i=0;i<static_cast<int>(m_authoring.roadSplineNodes.size());++i){ImVec2 sp{};if(!projectWorldPoint(view,m_authoring.roadSplineNodes[static_cast<std::size_t>(i)].position,sp))continue;float dx=io.MousePos.x-sp.x,dy=io.MousePos.y-sp.y,d=std::sqrt(dx*dx+dy*dy);if(d<bestD){bestD=d;best=i;}}if(best>=0)m_trafficSelectedRoadNode=best;
        }
        else if (m_trafficTab == 2)
        {
            int best=-1;float bestD=13.0f;for(int i=0;i<static_cast<int>(m_authoring.roadIntersections.size());++i){ImVec2 sp{};if(!projectWorldPoint(view,m_authoring.roadIntersections[static_cast<std::size_t>(i)].position,sp))continue;float dx=io.MousePos.x-sp.x,dy=io.MousePos.y-sp.y,d=std::sqrt(dx*dx+dy*dy);if(d<bestD){bestD=d;best=i;}}if(best>=0)m_trafficSelectedIntersection=best;
        }
        else if (m_trafficTab == 3)
        {
            int best=-1;float bestD=13.0f;for(int i=0;i<static_cast<int>(m_authoring.parkingStrips.size());++i){ImVec2 sp{};if(!projectWorldPoint(view,m_authoring.parkingStrips[static_cast<std::size_t>(i)].position,sp))continue;float dx=io.MousePos.x-sp.x,dy=io.MousePos.y-sp.y,d=std::sqrt(dx*dx+dy*dy);if(d<bestD){bestD=d;best=i;}}if(best>=0)m_trafficSelectedParking=best;
        }
        else if (m_trafficTab == 6)
        {
            float bestD = 13.0f; int bestPortal = -1, bestRegion = -1, bestIncident = -1;
            for (int i=0;i<static_cast<int>(m_authoring.trafficSpawnPortals.size());++i){ImVec2 sp{};if(!projectWorldPoint(view,m_authoring.trafficSpawnPortals[static_cast<std::size_t>(i)].position,sp))continue;float dx=io.MousePos.x-sp.x,dy=io.MousePos.y-sp.y,d=std::sqrt(dx*dx+dy*dy);if(d<bestD){bestD=d;bestPortal=i;bestRegion=-1;bestIncident=-1;}}
            for (int i=0;i<static_cast<int>(m_authoring.trafficDensityRegions.size());++i){ImVec2 sp{};if(!projectWorldPoint(view,m_authoring.trafficDensityRegions[static_cast<std::size_t>(i)].position,sp))continue;float dx=io.MousePos.x-sp.x,dy=io.MousePos.y-sp.y,d=std::sqrt(dx*dx+dy*dy);if(d<bestD){bestD=d;bestPortal=-1;bestRegion=i;bestIncident=-1;}}
            for (int i=0;i<static_cast<int>(m_authoring.trafficIncidents.size());++i){ImVec2 sp{};if(!projectWorldPoint(view,m_authoring.trafficIncidents[static_cast<std::size_t>(i)].position,sp))continue;float dx=io.MousePos.x-sp.x,dy=io.MousePos.y-sp.y,d=std::sqrt(dx*dx+dy*dy);if(d<bestD){bestD=d;bestPortal=-1;bestRegion=-1;bestIncident=i;}}
            if(bestPortal>=0)m_trafficSelectedPortal=bestPortal;else if(bestRegion>=0)m_trafficSelectedDensityRegion=bestRegion;else if(bestIncident>=0)m_trafficSelectedIncident=bestIncident;
        }
        else
        {
            int best=-1;float bestD=13.0f;for(int i=0;i<static_cast<int>(m_authoring.trafficNodes.size());++i){ImVec2 sp{};if(!projectWorldPoint(view,m_authoring.trafficNodes[static_cast<std::size_t>(i)].position,sp))continue;float dx=io.MousePos.x-sp.x,dy=io.MousePos.y-sp.y,d=std::sqrt(dx*dx+dy*dy);if(d<bestD){bestD=d;best=i;}}if(best>=0)m_trafficSelectedNode=best;
        }
    }
    draw->PopClipRect();
}

void HeritageStudioApp::drawGameplayWorkspace()
{
    ensureScenePreviewInitialized();
    headingText("GAMEPLAY / WORLD ACTIVITIES");
    ImGui::SameLine();
    ImGui::TextDisabled("  motorsport + clandestine events + free-roam services / activities");
    ImGui::Separator();

    if (ImGui::Button("SAVE GAMEPLAY", ImVec2(150.0f, 30.0f)))
    {
        if (m_authoring.saveGameplay(m_studioProjectRoot / "gameplay.hgame", m_studioMessage))
        {
            std::string runtimeMessage;
            if (saveRuntimeGameplay(runtimeMessage)) m_studioMessage = runtimeMessage;
            else m_studioMessage += " | " + runtimeMessage;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("LOAD GAMEPLAY", ImVec2(150.0f, 30.0f)))
        m_authoring.loadGameplay(m_studioProjectRoot / "gameplay.hgame", m_studioMessage);
    ImGui::SameLine();
    if (ImGui::Button("VALIDATE", ImVec2(110.0f, 30.0f)))
        m_studioMessage = validateAuthoring();
    ImGui::SameLine();
    if (ImGui::Button(m_gameplayTab == 0 ? "EVENTS [ACTIVE]" : "EVENTS", ImVec2(140.0f, 30.0f)))
        m_gameplayTab = 0;
    ImGui::SameLine();
    if (ImGui::Button(m_gameplayTab == 1 ? "WORLD POINTS [ACTIVE]" : "WORLD POINTS", ImVec2(190.0f, 30.0f)))
        m_gameplayTab = 1;
    ImGui::SameLine();
    if (ImGui::Button(m_gameplayTab == 2 ? "POLICE / UNDERGROUND [ACTIVE]" : "POLICE / UNDERGROUND", ImVec2(235.0f, 30.0f)))
        m_gameplayTab = 2;
    ImGui::SameLine();
    if (ImGui::Button(m_gameplayTab == 3 ? "EVENT RUNTIME / PRACTICE [ACTIVE]" : "EVENT RUNTIME / PRACTICE", ImVec2(260.0f, 30.0f)))
        m_gameplayTab = 3;
    ImGui::SameLine();
    if (ImGui::Button(m_gameplayTab == 4 ? "COMPETITORS / SERIES [ACTIVE]" : "COMPETITORS / SERIES", ImVec2(235.0f, 30.0f)))
        m_gameplayTab = 4;
    ImGui::SameLine();
    ImGui::TextDisabled("Events %d | Entrants %d | Series %d | World points %d | %s",
        static_cast<int>(m_authoring.gameEvents.size()), static_cast<int>(m_authoring.motorsportEntrants.size()), static_cast<int>(m_authoring.motorsportChampionships.size()),
        static_cast<int>(m_authoring.worldPoints.size()), m_studioMessage.c_str());

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float listWidth = 330.0f;
    const float inspectorWidth = 390.0f;

    ImGui::BeginChild("GameplayList", ImVec2(listWidth, 0.0f), true);
    if (m_gameplayTab == 0)
    {
        sectionTitle("EVENT DEFINITIONS");
        if (ImGui::BeginTable("GameplayEventAdd", 2, ImGuiTableFlags_SizingStretchSame))
        {
            const auto addEvent = [&](const char* label, authoring::GameEventType type)
            {
                ImGui::TableNextColumn();
                if (ImGui::Button(label, ImVec2(-1.0f, 27.0f)))
                {
                    m_gameplaySelectedEvent = static_cast<int>(m_authoring.gameEvents.size());
                    auto& event = m_authoring.addGameEvent(type);
                    std::uint32_t startFinishId = 0;
                    std::uint32_t lastCheckpointId = 0;
                    int lastCheckpointOrder = -2147483647;
                    for (const auto& marker : m_authoring.raceMarkers)
                    {
                        if (marker.type == authoring::RaceMarkerType::StartFinish && startFinishId == 0)
                            startFinishId = marker.id;
                        if (marker.type == authoring::RaceMarkerType::Checkpoint && marker.order >= lastCheckpointOrder)
                        {
                            lastCheckpointOrder = marker.order;
                            lastCheckpointId = marker.id;
                        }
                    }
                    event.startMarkerId = startFinishId;
                    const bool circuit = type == authoring::GameEventType::CircuitRace || type == authoring::GameEventType::ClandestineCircuit;
                    event.finishMarkerId = circuit ? startFinishId : lastCheckpointId;
                    if (!m_authoring.raceLayouts.empty()) event.layoutId = m_authoring.raceLayouts.front().id;
                }
            };
            addEvent("+ CIRCUIT", authoring::GameEventType::CircuitRace);
            addEvent("+ SPRINT", authoring::GameEventType::Sprint);
            addEvent("+ TIME ATTACK", authoring::GameEventType::TimeAttack);
            addEvent("+ TIME TRIAL", authoring::GameEventType::TimeTrial);
            addEvent("+ DRAG", authoring::GameEventType::Drag);
            addEvent("+ DRIFT", authoring::GameEventType::Drift);
            addEvent("+ TOUGE", authoring::GameEventType::Touge);
            addEvent("+ STREET CIRCUIT", authoring::GameEventType::ClandestineCircuit);
            addEvent("+ STREET SPRINT", authoring::GameEventType::ClandestineSprint);
            addEvent("+ CRUISE", authoring::GameEventType::Cruise);
            addEvent("+ TEST DRIVE", authoring::GameEventType::TestDrive);
            addEvent("+ AUTOSLALOM", authoring::GameEventType::Autoslalom);
            addEvent("+ GYMKHANA", authoring::GameEventType::Gymkhana);
            ImGui::EndTable();
        }
        ImGui::Separator();
        for (int i = 0; i < static_cast<int>(m_authoring.gameEvents.size()); ++i)
        {
            const auto& event = m_authoring.gameEvents[static_cast<std::size_t>(i)];
            std::string label = event.name + "  [" + authoring::gameEventTypeName(event.type) + "]##event" + std::to_string(event.id);
            if (ImGui::Selectable(label.c_str(), m_gameplaySelectedEvent == i))
                m_gameplaySelectedEvent = i;
        }
    }
    else if (m_gameplayTab == 1)
    {
        sectionTitle("FREE-ROAM WORLD POINTS");
        if (ImGui::BeginTable("GameplayPointAdd", 2, ImGuiTableFlags_SizingStretchSame))
        {
            const auto addPoint = [&](const char* label, authoring::WorldPointType type)
            {
                ImGui::TableNextColumn();
                if (ImGui::Button(label, ImVec2(-1.0f, 27.0f)))
                {
                    m_gameplaySelectedWorldPoint = static_cast<int>(m_authoring.worldPoints.size());
                    auto& point = m_authoring.addWorldPoint(type);
                    point.position = m_viewTarget;
                }
            };
            addPoint("+ GARAGE", authoring::WorldPointType::Garage);
            addPoint("+ DEALERSHIP", authoring::WorldPointType::Dealership);
            addPoint("+ FUEL", authoring::WorldPointType::FuelStation);
            addPoint("+ REPAIR", authoring::WorldPointType::RepairShop);
            addPoint("+ CAR WASH", authoring::WorldPointType::CarWash);
            addPoint("+ MEET SPOT", authoring::WorldPointType::MeetSpot);
            addPoint("+ EVENT HUB", authoring::WorldPointType::EventHub);
            addPoint("+ SAFEHOUSE", authoring::WorldPointType::Safehouse);
            addPoint("+ POLICE", authoring::WorldPointType::PoliceStation);
            addPoint("+ SPEED CAMERA", authoring::WorldPointType::SpeedCamera);
            addPoint("+ SPEED TRAP", authoring::WorldPointType::SpeedTrap);
            addPoint("+ FAST TRAVEL", authoring::WorldPointType::FastTravel);
            addPoint("+ LANDMARK", authoring::WorldPointType::Landmark);
            addPoint("+ PARKING", authoring::WorldPointType::ParkingArea);
            ImGui::EndTable();
        }
        ImGui::Separator();
        for (int i = 0; i < static_cast<int>(m_authoring.worldPoints.size()); ++i)
        {
            const auto& point = m_authoring.worldPoints[static_cast<std::size_t>(i)];
            std::string label = point.name + "  [" + authoring::worldPointTypeName(point.type) + "]##point" + std::to_string(point.id);
            if (ImGui::Selectable(label.c_str(), m_gameplaySelectedWorldPoint == i))
                m_gameplaySelectedWorldPoint = i;
        }
    }
    else if (m_gameplayTab == 2)
    {
        sectionTitle("POLICE / PURSUIT AUTHORING");
        if (ImGui::BeginTable("PoliceGameplayAdd", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn(); if (ImGui::Button("+ PATROL ZONE", ImVec2(-1.0f, 27.0f))) { m_gameplayPoliceSelectionKind=0; m_gameplayPoliceSelectedIndex=static_cast<int>(m_authoring.policePatrolZones.size()); auto& v=m_authoring.addPolicePatrolZone(); v.position=m_viewTarget; }
            ImGui::TableNextColumn(); if (ImGui::Button("+ ROADBLOCK SITE", ImVec2(-1.0f, 27.0f))) { m_gameplayPoliceSelectionKind=1; m_gameplayPoliceSelectedIndex=static_cast<int>(m_authoring.policeRoadblockSites.size()); auto& v=m_authoring.addPoliceRoadblockSite(); v.position=m_viewTarget; }
            ImGui::TableNextColumn(); if (ImGui::Button("+ ESCAPE ZONE", ImVec2(-1.0f, 27.0f))) { m_gameplayPoliceSelectionKind=2; m_gameplayPoliceSelectedIndex=static_cast<int>(m_authoring.policeEscapeZones.size()); auto& v=m_authoring.addPoliceEscapeZone(); v.position=m_viewTarget; }
            ImGui::TableNextColumn(); if (ImGui::Button("+ CLANDESTINE MEET", ImVec2(-1.0f, 27.0f))) { m_gameplayPoliceSelectionKind=3; m_gameplayPoliceSelectedIndex=static_cast<int>(m_authoring.clandestineMeets.size()); auto& v=m_authoring.addClandestineMeet(); v.position=m_viewTarget; }
            ImGui::EndTable();
        }
        ImGui::Separator();
        ImGui::TextDisabled("PATROL ZONES");
        for (int i=0;i<static_cast<int>(m_authoring.policePatrolZones.size());++i) { const auto& v=m_authoring.policePatrolZones[static_cast<std::size_t>(i)]; if (ImGui::Selectable((v.name+"##patrol"+std::to_string(v.id)).c_str(),m_gameplayPoliceSelectionKind==0&&m_gameplayPoliceSelectedIndex==i)){m_gameplayPoliceSelectionKind=0;m_gameplayPoliceSelectedIndex=i;} }
        ImGui::TextDisabled("ROADBLOCK SITES");
        for (int i=0;i<static_cast<int>(m_authoring.policeRoadblockSites.size());++i) { const auto& v=m_authoring.policeRoadblockSites[static_cast<std::size_t>(i)]; if (ImGui::Selectable((v.name+"##roadblock"+std::to_string(v.id)).c_str(),m_gameplayPoliceSelectionKind==1&&m_gameplayPoliceSelectedIndex==i)){m_gameplayPoliceSelectionKind=1;m_gameplayPoliceSelectedIndex=i;} }
        ImGui::TextDisabled("ESCAPE / COOLDOWN ZONES");
        for (int i=0;i<static_cast<int>(m_authoring.policeEscapeZones.size());++i) { const auto& v=m_authoring.policeEscapeZones[static_cast<std::size_t>(i)]; if (ImGui::Selectable((v.name+"##escape"+std::to_string(v.id)).c_str(),m_gameplayPoliceSelectionKind==2&&m_gameplayPoliceSelectedIndex==i)){m_gameplayPoliceSelectionKind=2;m_gameplayPoliceSelectedIndex=i;} }
        ImGui::TextDisabled("CLANDESTINE MEETS");
        for (int i=0;i<static_cast<int>(m_authoring.clandestineMeets.size());++i) { const auto& v=m_authoring.clandestineMeets[static_cast<std::size_t>(i)]; if (ImGui::Selectable((v.name+"##meet"+std::to_string(v.id)).c_str(),m_gameplayPoliceSelectionKind==3&&m_gameplayPoliceSelectedIndex==i)){m_gameplayPoliceSelectionKind=3;m_gameplayPoliceSelectedIndex=i;} }
    }
    else if (m_gameplayTab == 4)
    {
        sectionTitle("MOTORSPORT COMPETITION");
        if (ImGui::BeginTable("CompetitionAdd", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn(); if (ImGui::Button("+ CLASS", ImVec2(-1.0f, 27.0f))) { m_gameplayCompetitionSelectionKind=0; m_gameplayCompetitionSelectedIndex=static_cast<int>(m_authoring.motorsportClasses.size()); m_authoring.addMotorsportClass(); }
            ImGui::TableNextColumn(); if (ImGui::Button("+ ENTRANT", ImVec2(-1.0f, 27.0f))) { m_gameplayCompetitionSelectionKind=1; m_gameplayCompetitionSelectedIndex=static_cast<int>(m_authoring.motorsportEntrants.size()); auto& e=m_authoring.addMotorsportEntrant(); if(!m_authoring.gameEvents.empty()) e.eventId=m_authoring.gameEvents.front().id; }
            ImGui::TableNextColumn(); if (ImGui::Button("+ CHAMPIONSHIP", ImVec2(-1.0f, 27.0f))) { m_gameplayCompetitionSelectionKind=2; m_gameplayCompetitionSelectedIndex=static_cast<int>(m_authoring.motorsportChampionships.size()); m_authoring.addMotorsportChampionship(); }
            ImGui::TableNextColumn(); if (ImGui::Button("+ ROUND", ImVec2(-1.0f, 27.0f))) { if(!m_authoring.motorsportChampionships.empty()){ m_gameplayCompetitionSelectionKind=3; m_gameplayCompetitionSelectedIndex=static_cast<int>(m_authoring.motorsportRounds.size()); auto& r=m_authoring.addMotorsportRound(m_authoring.motorsportChampionships.front().id); if(!m_authoring.gameEvents.empty()) r.eventId=m_authoring.gameEvents.front().id; }}
            ImGui::EndTable();
        }
        ImGui::Separator();
        ImGui::TextDisabled("CLASSES");
        for(int i=0;i<static_cast<int>(m_authoring.motorsportClasses.size());++i){ const auto& v=m_authoring.motorsportClasses[static_cast<std::size_t>(i)]; if(ImGui::Selectable((v.name+" ["+v.code+"]##mclass"+std::to_string(v.id)).c_str(),m_gameplayCompetitionSelectionKind==0&&m_gameplayCompetitionSelectedIndex==i)){m_gameplayCompetitionSelectionKind=0;m_gameplayCompetitionSelectedIndex=i;} }
        ImGui::TextDisabled("ENTRANTS");
        for(int i=0;i<static_cast<int>(m_authoring.motorsportEntrants.size());++i){ const auto& v=m_authoring.motorsportEntrants[static_cast<std::size_t>(i)]; if(ImGui::Selectable((std::to_string(v.raceNumber)+"  "+v.driverName+"##entrant"+std::to_string(v.id)).c_str(),m_gameplayCompetitionSelectionKind==1&&m_gameplayCompetitionSelectedIndex==i)){m_gameplayCompetitionSelectionKind=1;m_gameplayCompetitionSelectedIndex=i;} }
        ImGui::TextDisabled("CHAMPIONSHIPS");
        for(int i=0;i<static_cast<int>(m_authoring.motorsportChampionships.size());++i){ const auto& v=m_authoring.motorsportChampionships[static_cast<std::size_t>(i)]; if(ImGui::Selectable((v.name+"##champ"+std::to_string(v.id)).c_str(),m_gameplayCompetitionSelectionKind==2&&m_gameplayCompetitionSelectedIndex==i)){m_gameplayCompetitionSelectionKind=2;m_gameplayCompetitionSelectedIndex=i;} }
        ImGui::TextDisabled("CALENDAR ROUNDS");
        for(int i=0;i<static_cast<int>(m_authoring.motorsportRounds.size());++i){ const auto& v=m_authoring.motorsportRounds[static_cast<std::size_t>(i)]; if(ImGui::Selectable((std::to_string(v.order+1)+". "+v.name+"##round"+std::to_string(v.id)).c_str(),m_gameplayCompetitionSelectionKind==3&&m_gameplayCompetitionSelectedIndex==i)){m_gameplayCompetitionSelectionKind=3;m_gameplayCompetitionSelectedIndex=i;} }
    }
    else
    {
        sectionTitle("EVENT EXECUTION PIPELINE");
        ImGui::TextWrapped("Runtime policy for authored circuit, point-to-point and clandestine events. The event runner consumes HRACE routes/markers/sessions and HGAME event definitions without duplicating venue data.");
        ImGui::Spacing();
        ImGui::BulletText("Staging -> countdown -> green -> timing -> finish -> results");
        ImGui::BulletText("Lap/checkpoint/sector gates, pits, track limits, flags and penalties");
        ImGui::BulletText("Practice Loop hotkeys restore exact entry position, heading and velocity");
        ImGui::Spacing();
        ImGui::TextDisabled("Default runtime hotkeys");
        ImGui::Text("F5  Capture loop start");
        ImGui::Text("F6  Capture loop end + begin looping");
        ImGui::Text("F4  Toggle loop playback");
        ImGui::Text("F3  Restart loop immediately");
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("GameplayViewport", ImVec2(std::max(320.0f, available.x - listWidth - inspectorWidth - 16.0f), 0.0f), true);
    drawGameplayViewportInteractive();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("GameplayInspector", ImVec2(0.0f, 0.0f), true);
    if (m_gameplayTab == 0)
    {
        sectionTitle("EVENT RULES");
        if (!m_authoring.gameEvents.empty())
        {
            m_gameplaySelectedEvent = std::clamp(m_gameplaySelectedEvent, 0, static_cast<int>(m_authoring.gameEvents.size()) - 1);
            auto& event = m_authoring.gameEvents[static_cast<std::size_t>(m_gameplaySelectedEvent)];
            inputString("Name", event.name);
            int typeIndex = static_cast<int>(event.type);
            const char* eventTypes[] = { "Circuit Race", "Sprint", "Time Trial", "Time Attack", "Drag", "Drift", "Touge", "Clandestine Circuit", "Clandestine Sprint", "Cruise", "Test Drive", "Autoslalom", "Gymkhana" };
            if (ImGui::Combo("Type", &typeIndex, eventTypes, IM_ARRAYSIZE(eventTypes)))
                event.type = static_cast<authoring::GameEventType>(typeIndex);
            ImGui::Checkbox("Enabled", &event.enabled);

            const auto markerCombo = [&](const char* label, std::uint32_t& markerId)
            {
                const char* preview = "(none)";
                for (const auto& marker : m_authoring.raceMarkers)
                    if (marker.id == markerId) { preview = marker.name.c_str(); break; }
                if (ImGui::BeginCombo(label, preview))
                {
                    if (ImGui::Selectable("(none)", markerId == 0)) markerId = 0;
                    for (const auto& marker : m_authoring.raceMarkers)
                    {
                        const bool selected = marker.id == markerId;
                        std::string markerLabel = marker.name + " [" + authoring::raceMarkerTypeName(marker.type) + "]##ref" + std::to_string(marker.id) + label;
                        if (ImGui::Selectable(markerLabel.c_str(), selected)) markerId = marker.id;
                    }
                    ImGui::EndCombo();
                }
            };
            markerCombo("Start marker", event.startMarkerId);
            markerCombo("Finish marker", event.finishMarkerId);
            const char* layoutPreview = "(none)";
            for (const auto& layout : m_authoring.raceLayouts) if (layout.id == event.layoutId) { layoutPreview = layout.name.c_str(); break; }
            if (ImGui::BeginCombo("Venue layout", layoutPreview))
            {
                if (ImGui::Selectable("(none)", event.layoutId == 0)) event.layoutId = 0;
                for (const auto& layout : m_authoring.raceLayouts)
                {
                    std::string layoutLabel = layout.name + "##eventLayout" + std::to_string(layout.id);
                    if (ImGui::Selectable(layoutLabel.c_str(), event.layoutId == layout.id)) event.layoutId = layout.id;
                }
                ImGui::EndCombo();
            }
            ImGui::InputInt("Laps", &event.laps); event.laps = std::max(1, event.laps);
            ImGui::InputInt("Max entrants", &event.maxEntrants); event.maxEntrants = std::clamp(event.maxEntrants, 1, 200);
            ImGui::Checkbox("Rolling start", &event.rollingStart);
            ImGui::Checkbox("Live traffic during event", &event.trafficEnabled);
            ImGui::Checkbox("Police response enabled", &event.policeEnabled);
            ImGui::Checkbox("Night only", &event.nightOnly);
            ImGui::DragFloat("Entry fee", &event.entryFee, 10.0f, 0.0f, 10000000.0f, "%.0f");
            ImGui::DragFloat("Reward", &event.reward, 50.0f, 0.0f, 100000000.0f, "%.0f");
            ImGui::SliderFloat("Heat / notoriety", &event.heat, 0.0f, 1.0f, "%.2f");
            ImGui::Spacing();
            ImGui::TextWrapped("Clandestine event types can coexist with live traffic and police response. Autoslalom/Gymkhana consume the CONE COURSES sequence from Race authoring; they may stage from a normal race marker or an event-scoped Start cone.");
            if (ImGui::Button("DELETE EVENT", ImVec2(-1.0f, 30.0f)))
            {
                m_authoring.removeGameEvent(static_cast<std::size_t>(m_gameplaySelectedEvent));
                m_gameplaySelectedEvent = std::max(0, m_gameplaySelectedEvent - 1);
            }
        }
    }
    else if (m_gameplayTab == 1)
    {
        sectionTitle("WORLD POINT INSPECTOR");
        if (!m_authoring.worldPoints.empty())
        {
            m_gameplaySelectedWorldPoint = std::clamp(m_gameplaySelectedWorldPoint, 0, static_cast<int>(m_authoring.worldPoints.size()) - 1);
            auto& point = m_authoring.worldPoints[static_cast<std::size_t>(m_gameplaySelectedWorldPoint)];
            inputString("Name", point.name);
            int typeIndex = static_cast<int>(point.type);
            const char* pointTypes[] = { "Garage", "Dealership", "Fuel Station", "Repair Shop", "Car Wash", "Meet Spot", "Event Hub", "Safehouse", "Police Station", "Speed Camera", "Speed Trap", "Fast Travel", "Landmark", "Parking Area" };
            if (ImGui::Combo("Type", &typeIndex, pointTypes, IM_ARRAYSIZE(pointTypes)))
                point.type = static_cast<authoring::WorldPointType>(typeIndex);
            ImGui::Checkbox("Enabled", &point.enabled);
            ImGui::DragFloat3("Position", &point.position.x, 0.05f);
            ImGui::DragFloat("Heading deg", &point.headingDeg, 0.25f, -360.0f, 360.0f);
            ImGui::DragFloat("Interaction radius m", &point.radiusM, 0.1f, 0.5f, 500.0f);
            ImGui::Checkbox("Discoverable on world map", &point.discoverable);
            ImGui::Checkbox("Fast travel enabled", &point.fastTravelEnabled);
            ImGui::DragFloat("Service price multiplier", &point.servicePriceMultiplier, 0.01f, 0.0f, 20.0f, "%.2fx");
            ImGui::Spacing();
            ImGui::TextWrapped("World points are reusable free-roam gameplay anchors: services, social/meet locations, police infrastructure, speed enforcement, parking, discoveries and travel destinations.");
            if (ImGui::Button("DELETE WORLD POINT", ImVec2(-1.0f, 30.0f)))
            {
                m_authoring.removeWorldPoint(static_cast<std::size_t>(m_gameplaySelectedWorldPoint));
                m_gameplaySelectedWorldPoint = std::max(0, m_gameplaySelectedWorldPoint - 1);
            }
        }
    }
    else if (m_gameplayTab == 2)
    {
        sectionTitle("PURSUIT / CLANDESTINE POLICY");
        auto& police=m_authoring.policeGameplay;
        ImGui::Checkbox("Enable police gameplay", &police.enabled);
        ImGui::SliderInt("Maximum heat level", &police.maxHeatLevel, 1, 10);
        ImGui::SliderInt("Maximum pursuit units", &police.maxPursuitUnits, 0, 64);
        ImGui::DragFloat("Civilian witness radius m", &police.civilianWitnessRadiusM, 1.0f, 0.0f, 2000.0f);
        ImGui::DragFloat("Police detection radius m", &police.policeDetectionRadiusM, 1.0f, 0.0f, 3000.0f);
        ImGui::DragFloat("Base speed tolerance km/h", &police.speedToleranceKmh, 0.5f, 0.0f, 100.0f);
        ImGui::DragFloat("Heat decay delay s", &police.heatDecayDelayS, 0.5f, 0.0f, 300.0f);
        ImGui::DragFloat("Heat decay / second", &police.heatDecayPerSecond, 0.002f, 0.0f, 2.0f);
        ImGui::DragFloat("Lost sight -> search s", &police.lostSightSeconds, 0.25f, 0.0f, 120.0f);
        ImGui::DragFloat("Search duration s", &police.searchDurationS, 1.0f, 0.0f, 900.0f);
        ImGui::DragFloat("Cooldown duration s", &police.cooldownDurationS, 1.0f, 0.0f, 600.0f);
        ImGui::DragFloat("Bust hold seconds", &police.bustHoldSeconds, 0.25f, 0.0f, 30.0f);
        ImGui::DragFloat("Backup dispatch delay s", &police.backupDelayS, 0.25f, 0.0f, 60.0f);
        ImGui::DragFloat("Roadblock minimum heat", &police.roadblockMinimumHeat, 0.1f, 0.0f, 10.0f);
        ImGui::Checkbox("Civilian witnesses", &police.civilianWitnesses);
        ImGui::Checkbox("Speeding generates heat", &police.speedingGeneratesHeat);
        ImGui::Checkbox("Collisions generate heat", &police.collisionsGenerateHeat);
        ImGui::Checkbox("Illegal races generate heat", &police.illegalRacesGenerateHeat);
        ImGui::Checkbox("Evasion escalates heat", &police.evasionEscalatesHeat);
        ImGui::Separator();
        if (m_gameplayPoliceSelectionKind==0 && !m_authoring.policePatrolZones.empty())
        {
            m_gameplayPoliceSelectedIndex=std::clamp(m_gameplayPoliceSelectedIndex,0,static_cast<int>(m_authoring.policePatrolZones.size())-1); auto& v=m_authoring.policePatrolZones[static_cast<std::size_t>(m_gameplayPoliceSelectedIndex)]; sectionTitle("PATROL ZONE"); inputString("Name",v.name); ImGui::Checkbox("Enabled##patrol",&v.enabled); ImGui::DragFloat3("Position##patrol",&v.position.x,0.1f); ImGui::DragFloat("Radius m##patrol",&v.radiusM,1.0f,5.0f,10000.0f); ImGui::DragFloat("Patrol weight",&v.patrolWeight,0.05f,0.0f,10.0f); ImGui::SliderInt("Maximum units##patrol",&v.maximumUnits,0,32); ImGui::DragFloat("Response multiplier",&v.responseMultiplier,0.05f,0.1f,5.0f); ImGui::DragFloat("Speed tolerance km/h##patrol",&v.speedToleranceKmh,0.5f,0.0f,100.0f); ImGui::DragFloat("Start hour##patrol",&v.startHour,0.25f,0.0f,24.0f); ImGui::DragFloat("End hour##patrol",&v.endHour,0.25f,0.0f,24.0f); ImGui::InputScalar("Response portal ID",ImGuiDataType_U32,&v.responsePortalId); if(ImGui::Button("DELETE PATROL ZONE",ImVec2(-1.0f,27.0f))){m_authoring.removePolicePatrolZone(static_cast<std::size_t>(m_gameplayPoliceSelectedIndex));m_gameplayPoliceSelectedIndex=std::max(0,m_gameplayPoliceSelectedIndex-1);}
        }
        else if (m_gameplayPoliceSelectionKind==1 && !m_authoring.policeRoadblockSites.empty())
        {
            m_gameplayPoliceSelectedIndex=std::clamp(m_gameplayPoliceSelectedIndex,0,static_cast<int>(m_authoring.policeRoadblockSites.size())-1); auto& v=m_authoring.policeRoadblockSites[static_cast<std::size_t>(m_gameplayPoliceSelectedIndex)]; sectionTitle("ROADBLOCK SITE"); inputString("Name",v.name); ImGui::Checkbox("Enabled##roadblock",&v.enabled); ImGui::InputScalar("Graph node ID",ImGuiDataType_U32,&v.nodeId); ImGui::DragFloat3("Position##roadblock",&v.position.x,0.1f); ImGui::DragFloat("Heading deg##roadblock",&v.headingDeg,0.5f,-360.0f,360.0f); ImGui::DragFloat("Block width m",&v.widthM,0.1f,2.0f,100.0f); ImGui::DragFloat("Minimum heat##roadblock",&v.minimumHeat,0.1f,0.0f,10.0f); ImGui::SliderInt("Unit count##roadblock",&v.unitCount,1,16); ImGui::Checkbox("Spike strip",&v.spikeStrip); ImGui::Checkbox("Leave escape gap",&v.leaveEscapeGap); ImGui::DragFloat("Selection weight##roadblock",&v.selectionWeight,0.05f,0.0f,10.0f); if(ImGui::Button("DELETE ROADBLOCK SITE",ImVec2(-1.0f,27.0f))){m_authoring.removePoliceRoadblockSite(static_cast<std::size_t>(m_gameplayPoliceSelectedIndex));m_gameplayPoliceSelectedIndex=std::max(0,m_gameplayPoliceSelectedIndex-1);}
        }
        else if (m_gameplayPoliceSelectionKind==2 && !m_authoring.policeEscapeZones.empty())
        {
            m_gameplayPoliceSelectedIndex=std::clamp(m_gameplayPoliceSelectedIndex,0,static_cast<int>(m_authoring.policeEscapeZones.size())-1); auto& v=m_authoring.policeEscapeZones[static_cast<std::size_t>(m_gameplayPoliceSelectedIndex)]; sectionTitle("ESCAPE / COOLDOWN ZONE"); inputString("Name",v.name); ImGui::Checkbox("Enabled##escape",&v.enabled); ImGui::DragFloat3("Position##escape",&v.position.x,0.1f); ImGui::DragFloat("Radius m##escape",&v.radiusM,1.0f,5.0f,5000.0f); ImGui::DragFloat("Search time multiplier",&v.searchTimeMultiplier,0.05f,0.0f,4.0f); ImGui::DragFloat("Heat decay multiplier",&v.heatDecayMultiplier,0.05f,0.0f,20.0f); ImGui::Checkbox("Break line of sight",&v.breakLineOfSight); ImGui::Checkbox("Safehouse zone",&v.safehouse); if(ImGui::Button("DELETE ESCAPE ZONE",ImVec2(-1.0f,27.0f))){m_authoring.removePoliceEscapeZone(static_cast<std::size_t>(m_gameplayPoliceSelectedIndex));m_gameplayPoliceSelectedIndex=std::max(0,m_gameplayPoliceSelectedIndex-1);}
        }
        else if (m_gameplayPoliceSelectionKind==3 && !m_authoring.clandestineMeets.empty())
        {
            m_gameplayPoliceSelectedIndex=std::clamp(m_gameplayPoliceSelectedIndex,0,static_cast<int>(m_authoring.clandestineMeets.size())-1); auto& v=m_authoring.clandestineMeets[static_cast<std::size_t>(m_gameplayPoliceSelectedIndex)]; sectionTitle("CLANDESTINE MEET"); inputString("Name",v.name); ImGui::Checkbox("Enabled##meet",&v.enabled); ImGui::DragFloat3("Position##meet",&v.position.x,0.1f); ImGui::DragFloat("Heading deg##meet",&v.headingDeg,0.5f,-360.0f,360.0f); ImGui::DragFloat("Meet radius m",&v.radiusM,0.5f,2.0f,1000.0f); ImGui::DragFloat("Open hour",&v.openHour,0.25f,0.0f,24.0f); ImGui::DragFloat("Close hour",&v.closeHour,0.25f,0.0f,24.0f); ImGui::SliderInt("Maximum vehicles",&v.maximumVehicles,1,256); ImGui::DragFloat("Police risk",&v.policeRisk,0.01f,0.0f,1.0f); ImGui::DragFloat("Heat multiplier",&v.heatMultiplier,0.05f,0.0f,10.0f); ImGui::InputScalar("Linked event ID",ImGuiDataType_U32,&v.eventId); ImGui::Checkbox("Discoverable",&v.discoverable); if(ImGui::Button("DELETE CLANDESTINE MEET",ImVec2(-1.0f,27.0f))){m_authoring.removeClandestineMeet(static_cast<std::size_t>(m_gameplayPoliceSelectedIndex));m_gameplayPoliceSelectedIndex=std::max(0,m_gameplayPoliceSelectedIndex-1);}
        }
    }
    else if (m_gameplayTab == 3)
    {
        sectionTitle("EVENT EXECUTION / PRACTICE LOOP");
        auto& execution = m_authoring.eventExecution;
        ImGui::Checkbox("Enable runtime event execution", &execution.enabled);
        ImGui::Checkbox("Auto-stage player at event/grid start", &execution.autoStagePlayer);
        ImGui::Checkbox("Auto-save personal bests", &execution.autoSavePersonalBests);
        ImGui::DragFloat("Grid settle seconds", &execution.gridSettleSeconds, 0.05f, 0.0f, 10.0f, "%.2f");
        ImGui::DragFloat("Countdown seconds", &execution.countdownSeconds, 0.1f, 0.0f, 30.0f, "%.1f");
        ImGui::DragFloat("False-start movement km/h", &execution.falseStartSpeedKmh, 0.1f, 0.0f, 20.0f, "%.1f");
        ImGui::DragFloat("Timing gate debounce s", &execution.gateDebounceSeconds, 0.01f, 0.05f, 3.0f, "%.2f");
        ImGui::DragFloat("Track-limit grace s", &execution.trackLimitGraceSeconds, 0.05f, 0.0f, 10.0f, "%.2f");
        ImGui::DragFloat("Track-limit rejoin s", &execution.trackLimitRejoinSeconds, 0.05f, 0.0f, 10.0f, "%.2f");
        ImGui::Separator();
        ImGui::TextDisabled("FLAG SPEED TARGETS");
        ImGui::DragFloat("Full-course yellow km/h", &execution.fullCourseYellowSpeedKmh, 1.0f, 1.0f, 300.0f);
        ImGui::DragFloat("Virtual Safety Car km/h", &execution.virtualSafetyCarSpeedKmh, 1.0f, 1.0f, 300.0f);
        ImGui::DragFloat("Safety Car km/h", &execution.safetyCarSpeedKmh, 1.0f, 1.0f, 300.0f);
        ImGui::DragFloat("Results hold seconds", &execution.resultsHoldSeconds, 0.5f, 0.0f, 120.0f);
        ImGui::Separator();
        ImGui::TextDisabled("PRACTICE LOOP");
        ImGui::Checkbox("Enable practice loop hotkeys", &execution.practiceLoopEnabled);
        ImGui::Checkbox("Auto-restart after crossing loop end", &execution.practiceLoopAutoRestart);
        ImGui::Checkbox("Restore captured angular velocity", &execution.practiceLoopRestoreAngularVelocity);
        ImGui::Checkbox("Restore captured gear", &execution.practiceLoopRestoreGear);
        ImGui::DragFloat("Loop end gate width m", &execution.practiceLoopEndGateWidthM, 0.25f, 1.0f, 100.0f);
        ImGui::DragFloat("Loop restore delay s", &execution.practiceLoopRestoreDelayS, 0.01f, 0.0f, 5.0f, "%.2f");
        ImGui::Spacing();
        ImGui::TextWrapped("The loop start stores the player's rigid-body position, Euler rotation, linear velocity vector, angular velocity and selected gear. Capturing the end immediately starts the loop; each valid end-gate crossing restores that exact entry state for repeatable section practice.");
    }
    else if (m_gameplayTab == 4)
    {
        sectionTitle("COMPETITION / CHAMPIONSHIP POLICY");
        auto& policy=m_authoring.motorsport;
        ImGui::Checkbox("Enable motorsport competition runtime",&policy.enabled);
        ImGui::Checkbox("Enable AI competitors",&policy.aiCompetitorsEnabled);
        ImGui::Checkbox("Auto-build grids",&policy.autoBuildGrid);
        ImGui::Checkbox("Simulate unspawned competitors",&policy.simulateUnspawnedCompetitors);
        ImGui::SliderInt("Maximum physical competitors",&policy.maxPhysicalCompetitors,0,150);
        ImGui::SliderFloat("Default AI skill",&policy.defaultAiSkill,0.0f,1.0f,"%.2f");
        ImGui::DragFloat("Qualifying pace spread %",&policy.qualifyingPaceSpreadPercent,0.1f,0.0f,30.0f,"%.1f%%");
        ImGui::DragFloat("Mechanical DNF chance / hour",&policy.baseMechanicalDnfChancePerHour,0.001f,0.0f,1.0f,"%.3f");
        ImGui::Checkbox("Multi-class timing",&policy.multiClassTiming); ImGui::Checkbox("Persist championship standings",&policy.championshipPersistence);
        ImGui::Separator();
        const auto eventReferenceCombo = [&](const char* label, std::uint32_t& eventId, bool allowAll)
        {
            const char* preview = allowAll && eventId == 0 ? "(all compatible events)" : "(missing event)";
            for (const auto& event : m_authoring.gameEvents) if (event.id == eventId) { preview = event.name.c_str(); break; }
            if (ImGui::BeginCombo(label, preview))
            {
                if (allowAll && ImGui::Selectable("(all compatible events)", eventId == 0)) eventId = 0;
                for (const auto& event : m_authoring.gameEvents)
                {
                    std::string item = event.name + " [" + authoring::gameEventTypeName(event.type) + "]##eventRef" + std::to_string(event.id) + label;
                    if (ImGui::Selectable(item.c_str(), event.id == eventId)) eventId = event.id;
                }
                ImGui::EndCombo();
            }
        };
        const auto classReferenceCombo = [&](const char* label, std::uint32_t& classId, bool allowAll)
        {
            const char* preview = allowAll && classId == 0 ? "(all classes)" : "(missing class)";
            for (const auto& cls : m_authoring.motorsportClasses) if (cls.id == classId) { preview = cls.name.c_str(); break; }
            if (ImGui::BeginCombo(label, preview))
            {
                if (allowAll && ImGui::Selectable("(all classes)", classId == 0)) classId = 0;
                for (const auto& cls : m_authoring.motorsportClasses)
                {
                    std::string item = cls.name + " [" + cls.code + "]##classRef" + std::to_string(cls.id) + label;
                    if (ImGui::Selectable(item.c_str(), cls.id == classId)) classId = cls.id;
                }
                ImGui::EndCombo();
            }
        };
        const auto championshipReferenceCombo = [&](const char* label, std::uint32_t& championshipId)
        {
            const char* preview = "(missing championship)";
            for (const auto& championship : m_authoring.motorsportChampionships) if (championship.id == championshipId) { preview = championship.name.c_str(); break; }
            if (ImGui::BeginCombo(label, preview))
            {
                for (const auto& championship : m_authoring.motorsportChampionships)
                {
                    std::string item = championship.name + "##champRef" + std::to_string(championship.id) + label;
                    if (ImGui::Selectable(item.c_str(), championship.id == championshipId)) championshipId = championship.id;
                }
                ImGui::EndCombo();
            }
        };
        if(m_gameplayCompetitionSelectionKind==0 && !m_authoring.motorsportClasses.empty())
        {
            m_gameplayCompetitionSelectedIndex=std::clamp(m_gameplayCompetitionSelectedIndex,0,static_cast<int>(m_authoring.motorsportClasses.size())-1); auto& v=m_authoring.motorsportClasses[static_cast<std::size_t>(m_gameplayCompetitionSelectedIndex)]; sectionTitle("CLASS"); inputString("Name",v.name); inputString("Class code",v.code); ImGui::Checkbox("Enabled##mclass",&v.enabled); ImGui::DragFloat("Minimum power kW",&v.minimumPowerKw,1.0f,0.0f,5000.0f); ImGui::DragFloat("Maximum power kW",&v.maximumPowerKw,1.0f,0.0f,5000.0f); ImGui::DragFloat("Minimum weight kg",&v.minimumWeightKg,1.0f,0.0f,10000.0f); ImGui::DragFloat("Maximum weight kg",&v.maximumWeightKg,1.0f,0.0f,10000.0f); ImGui::DragFloat("Balance ballast kg",&v.balanceBallastKg,1.0f,-500.0f,1000.0f); ImGui::SliderInt("Maximum entrants##mclass",&v.maximumEntrants,1,200); if(ImGui::Button("DELETE CLASS",ImVec2(-1.0f,27.0f))){m_authoring.removeMotorsportClass(static_cast<std::size_t>(m_gameplayCompetitionSelectedIndex));m_gameplayCompetitionSelectedIndex=std::max(0,m_gameplayCompetitionSelectedIndex-1);}
        }
        else if(m_gameplayCompetitionSelectionKind==1 && !m_authoring.motorsportEntrants.empty())
        {
            m_gameplayCompetitionSelectedIndex=std::clamp(m_gameplayCompetitionSelectedIndex,0,static_cast<int>(m_authoring.motorsportEntrants.size())-1); auto& v=m_authoring.motorsportEntrants[static_cast<std::size_t>(m_gameplayCompetitionSelectedIndex)]; sectionTitle("ENTRANT / RACING AI"); inputString("Driver",v.driverName); inputString("Team",v.teamName); inputString("Vehicle preset",v.vehiclePreset); ImGui::Checkbox("Enabled##entrant",&v.enabled); ImGui::InputInt("Race number",&v.raceNumber); eventReferenceCombo("Event assignment",v.eventId,true); classReferenceCombo("Competition class",v.classId,false); ImGui::SliderFloat("AI skill",&v.aiSkill,0.0f,1.0f,"%.2f"); ImGui::SliderFloat("Qualifying pace",&v.qualifyingPace,0.0f,1.0f,"%.2f"); ImGui::SliderFloat("Race pace",&v.racePace,0.0f,1.0f,"%.2f"); ImGui::SliderFloat("Wet skill",&v.wetSkill,0.0f,1.0f,"%.2f"); ImGui::SliderFloat("Aggression",&v.aggression,0.0f,1.0f,"%.2f"); ImGui::SliderFloat("Consistency",&v.consistency,0.0f,1.0f,"%.2f"); ImGui::SliderFloat("Pit skill",&v.pitSkill,0.0f,1.0f,"%.2f");
            ImGui::Separator(); ImGui::TextDisabled("RACECRAFT / DECISION PERSONALITY");
            ImGui::SliderFloat("Racecraft",&v.racecraft,0.0f,1.0f,"%.2f"); ImGui::SliderFloat("Opponent awareness",&v.awareness,0.0f,1.0f,"%.2f"); ImGui::SliderFloat("Defending tendency",&v.defending,0.0f,1.0f,"%.2f");
            ImGui::SliderFloat("Tire management",&v.tireManagement,0.0f,1.0f,"%.2f"); ImGui::SliderFloat("Fuel management",&v.fuelManagement,0.0f,1.0f,"%.2f"); ImGui::SliderFloat("Strategy risk",&v.strategyRisk,0.0f,1.0f,"%.2f");
            ImGui::DragFloat("Mistakes / hour",&v.mistakeRatePerHour,0.01f,0.0f,5.0f,"%.2f"); ImGui::DragFloat("Reaction time s",&v.reactionTimeS,0.01f,0.05f,1.5f,"%.2f"); ImGui::SliderFloat("Preferred line bias",&v.preferredLineBias,-1.0f,1.0f,"%.2f");
            ImGui::Checkbox("Clandestine opponent",&v.clandestine); ImGui::InputInt("Grid override (0 = automatic)",&v.gridOverride); v.gridOverride=std::max(0,v.gridOverride); if(ImGui::Button("DELETE ENTRANT",ImVec2(-1.0f,27.0f))){m_authoring.removeMotorsportEntrant(static_cast<std::size_t>(m_gameplayCompetitionSelectedIndex));m_gameplayCompetitionSelectedIndex=std::max(0,m_gameplayCompetitionSelectedIndex-1);}
        }
        else if(m_gameplayCompetitionSelectionKind==2 && !m_authoring.motorsportChampionships.empty())
        {
            m_gameplayCompetitionSelectedIndex=std::clamp(m_gameplayCompetitionSelectedIndex,0,static_cast<int>(m_authoring.motorsportChampionships.size())-1); auto& v=m_authoring.motorsportChampionships[static_cast<std::size_t>(m_gameplayCompetitionSelectedIndex)]; sectionTitle("CHAMPIONSHIP"); inputString("Name",v.name); ImGui::Checkbox("Enabled##champ",&v.enabled); classReferenceCombo("Championship class",v.classId,true); inputString("Points scheme",v.pointsScheme); ImGui::DragFloat("Pole bonus",&v.poleBonus,0.1f,0.0f,100.0f); ImGui::DragFloat("Fastest-lap bonus",&v.fastestLapBonus,0.1f,0.0f,100.0f); ImGui::InputInt("Drop worst rounds",&v.dropWorstRounds); v.dropWorstRounds=std::max(0,v.dropWorstRounds); if(ImGui::Button("DELETE CHAMPIONSHIP",ImVec2(-1.0f,27.0f))){m_authoring.removeMotorsportChampionship(static_cast<std::size_t>(m_gameplayCompetitionSelectedIndex));m_gameplayCompetitionSelectedIndex=std::max(0,m_gameplayCompetitionSelectedIndex-1);}
        }
        else if(m_gameplayCompetitionSelectionKind==3 && !m_authoring.motorsportRounds.empty())
        {
            m_gameplayCompetitionSelectedIndex=std::clamp(m_gameplayCompetitionSelectedIndex,0,static_cast<int>(m_authoring.motorsportRounds.size())-1); auto& v=m_authoring.motorsportRounds[static_cast<std::size_t>(m_gameplayCompetitionSelectedIndex)]; sectionTitle("CHAMPIONSHIP ROUND"); inputString("Name",v.name); ImGui::Checkbox("Enabled##round",&v.enabled); championshipReferenceCombo("Championship",v.championshipId); eventReferenceCombo("Calendar event",v.eventId,false); ImGui::InputInt("Calendar order",&v.order); ImGui::DragFloat("Points multiplier",&v.pointsMultiplier,0.05f,0.0f,10.0f,"%.2fx"); if(ImGui::Button("DELETE ROUND",ImVec2(-1.0f,27.0f))){m_authoring.removeMotorsportRound(static_cast<std::size_t>(m_gameplayCompetitionSelectedIndex));m_gameplayCompetitionSelectedIndex=std::max(0,m_gameplayCompetitionSelectedIndex-1);}
        }
        ImGui::Spacing();
        ImGui::Separator(); ImGui::TextDisabled("STUDIO20 RACING AI INTELLIGENCE");
        auto& ai=m_authoring.motorsportAi;
        ImGui::Checkbox("Enable Racing AI intelligence",&ai.enabled); ImGui::SameLine(); ImGui::Checkbox("Live decision telemetry",&ai.liveDecisionTelemetry);
        ImGui::DragFloat("AI decision update Hz",&ai.updateHz,1.0f,1.0f,240.0f,"%.0f Hz");
        ImGui::DragFloat("Lookahead minimum m",&ai.lookaheadMinimumM,1.0f,1.0f,250.0f); ImGui::DragFloat("Lookahead maximum m",&ai.lookaheadMaximumM,1.0f,5.0f,600.0f); ImGui::DragFloat("Braking lookahead m",&ai.brakingLookaheadM,1.0f,5.0f,800.0f);
        ImGui::DragFloat("Opponent awareness m",&ai.opponentAwarenessM,1.0f,5.0f,500.0f); ImGui::DragFloat("Overtake closing threshold km/h",&ai.overtakeMinimumClosingKmh,0.5f,0.0f,100.0f); ImGui::DragFloat("Defensive trigger gap m",&ai.defensiveTriggerGapM,0.5f,1.0f,150.0f); ImGui::DragFloat("Blue-flag yield gap m",&ai.blueFlagYieldGapM,0.5f,1.0f,200.0f);
        ImGui::Checkbox("Slipstream decisions",&ai.slipstreamEnabled); ImGui::DragFloat("Slipstream minimum gap m",&ai.slipstreamMinimumGapM,0.25f,0.0f,50.0f); ImGui::DragFloat("Slipstream maximum gap m",&ai.slipstreamMaximumGapM,0.5f,1.0f,150.0f);
        ImGui::Checkbox("Defending decisions",&ai.defendingEnabled); ImGui::SameLine(); ImGui::Checkbox("Multi-class negotiation",&ai.multiclassNegotiation); ImGui::Checkbox("Wet-line selection",&ai.wetLineEnabled); ImGui::SameLine(); ImGui::Checkbox("Pit/fuel/tire strategy",&ai.strategyEnabled); ImGui::Checkbox("Driver mistakes",&ai.mistakesEnabled);
        ImGui::SliderFloat("Wet-line activation",&ai.wetLineThreshold,0.0f,1.0f,"%.2f wetness"); ImGui::SliderFloat("Maximum wet speed penalty",&ai.maximumWetSpeedPenalty,0.0f,0.8f,"%.2f");
        ImGui::DragFloat("Fuel use L / 100 km",&ai.fuelUseLitersPer100Km,0.5f,0.0f,500.0f,"%.1f"); ImGui::DragFloat("Tire wear / 100 km",&ai.tireWearPer100Km,0.01f,0.0f,2.0f,"%.2f"); ImGui::DragFloat("Fuel reserve laps",&ai.fuelReserveLaps,0.05f,0.0f,10.0f,"%.2f"); ImGui::SliderFloat("Tire pit threshold",&ai.tirePitThreshold,0.0f,1.0f,"%.2f life"); ImGui::DragFloat("Mistake recovery seconds",&ai.mistakeRecoverySeconds,0.1f,0.0f,20.0f,"%.1f s");
        ImGui::Separator(); ImGui::TextDisabled("STUDIO21 FULL-PHYSICS RACING AI CONTROL");
        ImGui::Checkbox("Use native Heritage Vehicle dynamics for physical competitors",&ai.fullPhysicsCompetitors);
        ImGui::DragFloat("AI vehicle high-rate physics Hz",&ai.physicsHighRateHz,10.0f,120.0f,2000.0f,"%.0f Hz");
        ImGui::DragFloat("Steering lookahead seconds",&ai.steeringLookaheadSeconds,0.02f,0.10f,3.0f,"%.2f s"); ImGui::DragFloat("Steering gain",&ai.steeringGain,0.02f,0.1f,4.0f,"%.2f"); ImGui::DragFloat("Cross-track correction gain",&ai.crossTrackGain,0.01f,0.0f,2.0f,"%.2f");
        ImGui::DragFloat("Throttle gain",&ai.throttleGain,0.01f,0.01f,1.0f,"%.2f"); ImGui::DragFloat("Brake gain",&ai.brakeGain,0.01f,0.01f,1.0f,"%.2f"); ImGui::DragFloat("Maximum steering angle",&ai.maximumSteerAngleDeg,0.5f,5.0f,70.0f,"%.1f deg");
        ImGui::Checkbox("Grip-aware braking / throttle",&ai.gripAwareBraking); ImGui::SameLine(); ImGui::Checkbox("Spatial side-by-side avoidance",&ai.spatialAvoidance);
        ImGui::Checkbox("Track-limit-aware passing",&ai.trackLimitAwarePassing); ImGui::SameLine(); ImGui::Checkbox("Damage-aware strategy",&ai.damageStrategyEnabled); ImGui::Checkbox("Short-horizon weather forecast",&ai.weatherForecastEnabled);
        ImGui::DragFloat("Side-by-side safety margin m",&ai.sideBySideSafetyM,0.05f,0.1f,5.0f,"%.2f m"); ImGui::DragFloat("Track-limit safety margin m",&ai.trackLimitSafetyM,0.05f,0.0f,5.0f,"%.2f m");
        ImGui::DragFloat("Grip slip-ratio limit",&ai.gripSlipRatioLimit,0.01f,0.02f,1.0f,"%.2f"); ImGui::DragFloat("Grip slip-angle limit deg",&ai.gripSlipAngleDeg,0.25f,1.0f,45.0f,"%.1f deg");
        ImGui::DragFloat("Physical recovery distance m",&ai.physicalRecoveryDistanceM,0.5f,5.0f,200.0f,"%.1f m"); ImGui::DragFloat("Formation speed km/h",&ai.formationSpeedKmh,1.0f,5.0f,250.0f,"%.0f km/h"); ImGui::DragFloat("Rolling-start speed km/h",&ai.rollingStartSpeedKmh,1.0f,5.0f,250.0f,"%.0f km/h"); ImGui::DragFloat("Pit-lane AI speed km/h",&ai.pitLaneSpeedKmh,1.0f,5.0f,200.0f,"%.0f km/h");
        ImGui::SliderFloat("Damage pit threshold",&ai.damagePitThreshold,0.0f,1.0f,"%.2f health"); ImGui::SliderFloat("Damage DNF threshold",&ai.damageDnfThreshold,0.0f,1.0f,"%.2f health"); ImGui::DragFloat("Collision damage scale",&ai.collisionDamageScale,0.01f,0.0f,1.0f,"%.2f"); ImGui::DragFloat("Weather forecast horizon s",&ai.weatherForecastSeconds,1.0f,0.0f,600.0f,"%.0f s");
        ImGui::Separator(); ImGui::TextDisabled("STUDIO22 COLLIDER-AUTHORITATIVE RACECRAFT / STEWARDING");
        ImGui::Checkbox("Use body collision bounds as chassis footprint",&ai.colliderBoundsAuthority);
        ImGui::SameLine(); ImGui::Checkbox("Predict swept collision envelopes",&ai.predictiveCollisionAvoidance);
        ImGui::DragFloat("Collision envelope extra margin m",&ai.collisionEnvelopeMarginM,0.01f,0.0f,2.0f,"%.2f m");
        ImGui::DragFloat("Swept-envelope horizon s",&ai.sweptEnvelopeSeconds,0.02f,0.05f,5.0f,"%.2f s");
        ImGui::DragFloat("Side-by-side overlap tolerance m",&ai.sideBySideOverlapToleranceM,0.01f,0.0f,2.0f,"%.2f m");
        ImGui::Checkbox("Divebomb / braking-battle judgement",&ai.divebombJudgement); ImGui::SameLine(); ImGui::Checkbox("Blocking rules / stewarding",&ai.blockingRules);
        ImGui::DragFloat("Divebomb commit gap m",&ai.divebombCommitGapM,0.25f,1.0f,80.0f,"%.1f m"); ImGui::DragFloat("Divebomb closing threshold km/h",&ai.divebombClosingThresholdKmh,0.5f,0.0f,150.0f,"%.1f km/h");
        ImGui::DragFloat("Switchback/crossover window s",&ai.switchbackWindowS,0.05f,0.1f,10.0f,"%.2f s");
        ImGui::InputInt("Maximum defensive moves / straight",&ai.maximumDefensiveMovesPerStraight); ai.maximumDefensiveMovesPerStraight=std::max(0,ai.maximumDefensiveMovesPerStraight);
        ImGui::DragFloat("Blocking penalty seconds",&ai.blockingPenaltySeconds,0.25f,0.0f,60.0f,"+%.1f s");
        ImGui::Checkbox("Unsafe pit-release stewarding",&ai.unsafeReleaseStewarding); ImGui::DragFloat("Pit-release lookahead m",&ai.pitReleaseLookaheadM,1.0f,5.0f,300.0f,"%.0f m"); ImGui::DragFloat("Unsafe-release penalty seconds",&ai.unsafeReleasePenaltySeconds,0.25f,0.0f,60.0f,"+%.1f s");
        ImGui::DragFloat("Multi-class pass planning horizon s",&ai.multiclassPassHorizonS,0.05f,0.1f,10.0f,"%.2f s");
        ImGui::Checkbox("Tire-temperature strategy",&ai.tireThermalStrategy); ImGui::SameLine(); ImGui::Checkbox("Fuel-mass-aware control",&ai.fuelMassAwareness); ImGui::SameLine(); ImGui::Checkbox("Component-damage strategy",&ai.componentDamageStrategy);
        ImGui::DragFloat("Tire optimal minimum C",&ai.tireOptimalMinimumC,0.5f,-20.0f,180.0f,"%.1f C"); ImGui::DragFloat("Tire optimal maximum C",&ai.tireOptimalMaximumC,0.5f,-20.0f,220.0f,"%.1f C"); ImGui::DragFloat("Fuel density kg/L",&ai.fuelDensityKgPerLiter,0.005f,0.40f,1.20f,"%.3f kg/L");
        ImGui::Separator(); ImGui::TextDisabled("STUDIO23 SOLVER-CONTACT INCIDENT EVIDENCE / STEWARD REVIEW");
        ImGui::Checkbox("Capture real solver-contact evidence",&ai.contactEvidenceEnabled); ImGui::SameLine(); ImGui::Checkbox("Incident stewarding",&ai.incidentStewardingEnabled);
        ImGui::DragFloat("Minimum contact impulse N s",&ai.incidentMinimumNormalImpulseNs,10.0f,0.0f,20000.0f,"%.0f N s");
        ImGui::DragFloat("Minimum closing speed km/h",&ai.incidentMinimumClosingKmh,0.5f,0.0f,200.0f,"%.1f km/h");
        ImGui::DragFloat("Severe contact impulse N s",&ai.severeIncidentNormalImpulseNs,50.0f,0.0f,50000.0f,"%.0f N s");
        ImGui::DragFloat("Severe closing speed km/h",&ai.severeIncidentClosingKmh,0.5f,0.0f,300.0f,"%.1f km/h");
        ImGui::DragFloat("Avoidable-contact penalty seconds",&ai.avoidableContactPenaltySeconds,0.25f,0.0f,120.0f,"+%.1f s");
        ImGui::DragFloat("Severe-contact penalty seconds",&ai.severeContactPenaltySeconds,0.25f,0.0f,180.0f,"+%.1f s");
        ImGui::DragFloat("Duplicate-contact evidence cooldown s",&ai.contactEvidenceCooldownSeconds,0.05f,0.05f,10.0f,"%.2f s");
        ImGui::InputInt("Retained steward evidence events",&ai.retainedIncidentEvidence); ai.retainedIncidentEvidence=std::max(1,std::min(256,ai.retainedIncidentEvidence));
        ImGui::Separator(); ImGui::TextDisabled("STUDIO24 INCIDENT REPLAY / STEWARD GHOST REVIEW");
        auto& replay=m_authoring.motorsportReplay;
        ImGui::Checkbox("Record bounded incident replay clips",&replay.enabled); ImGui::SameLine(); ImGui::Checkbox("Capture player vehicle",&replay.capturePlayer);
        ImGui::Checkbox("Capture control telemetry",&replay.captureControls); ImGui::SameLine(); ImGui::Checkbox("Enable ghost review playback",&replay.ghostReviewEnabled);
        ImGui::DragFloat("Replay sample rate Hz",&replay.sampleHz,0.5f,1.0f,60.0f,"%.1f Hz");
        ImGui::DragFloat("Incident pre-roll seconds",&replay.preRollSeconds,0.25f,0.5f,30.0f,"%.1f s");
        ImGui::DragFloat("Incident post-roll seconds",&replay.postRollSeconds,0.25f,0.5f,30.0f,"%.1f s");
        ImGui::InputInt("Maximum retained incident clips",&replay.maximumIncidentClips); replay.maximumIncidentClips=std::max(1,std::min(64,replay.maximumIncidentClips));
        ImGui::InputInt("Maximum recorded competitors",&replay.maximumRecordedCompetitors); replay.maximumRecordedCompetitors=std::max(1,std::min(200,replay.maximumRecordedCompetitors));
        ImGui::InputInt("Maximum review ghost vehicles",&replay.maximumGhostVehicles); replay.maximumGhostVehicles=std::max(1,std::min(64,replay.maximumGhostVehicles));
        ImGui::Separator(); ImGui::TextDisabled("STUDIO25 REPLAY / BROADCAST CAMERA DIRECTOR");
        ImGui::Checkbox("Enable replay broadcast camera director",&replay.broadcastDirectorEnabled); ImGui::SameLine(); ImGui::Checkbox("Auto-enable incident camera on review",&replay.autoIncidentCamera);
        ImGui::DragFloat("Incident camera distance m",&replay.incidentCameraDistanceM,0.25f,2.0f,80.0f,"%.1f m");
        ImGui::DragFloat("Incident camera height m",&replay.incidentCameraHeightM,0.25f,0.5f,40.0f,"%.1f m");
        ImGui::DragFloat("Trackside camera lead m",&replay.tracksideCameraLeadM,0.5f,2.0f,150.0f,"%.1f m");
        ImGui::DragFloat("Helicopter camera height m",&replay.helicopterCameraHeightM,0.5f,5.0f,200.0f,"%.1f m");
        ImGui::DragFloat("Replay camera smoothing",&replay.cameraSmoothing,0.25f,0.0f,30.0f,"%.1f /s");
        ImGui::TextWrapped("STUDIO25 uses a native FP64 world camera, not a vehicle-local hack. Incident, trackside, chase and helicopter review shots can therefore follow replay ghosts after floating-origin shifts and can later be reused by spectator/broadcast systems.");
        ImGui::TextWrapped("Replay capture is intentionally incident-window based: a rolling pre-impact buffer is sealed with a short post-impact tail when STUDIO23 accepts physical contact evidence. This keeps review useful on very large grids without recording every car for an entire endurance race in Lua memory.");
        ImGui::TextWrapped("This does not infer contact from proximity. Heritage's resolved collision manifold supplies the actual body/collider pair, contact point/normal, penetration and solver impulse; Racing AI only classifies and reviews that physical evidence.");
        ImGui::TextWrapped("Primary footprint authority is the rigid body's real solid collider set. Manual dimensions exist only as runtime fallback when an asset has no usable collision representation. Trigger/sensor volumes are excluded.");
        ImGui::TextWrapped("Physical competitors use native dynamic bodies, suspension/wheel contacts and Vehicle.SetInputs. Their chassis speed and projected position become race progress; logical competitors remain the scalable fallback outside the physical budget.");
        ImGui::TextWrapped("Racing AI consumes authored AI Line / AI Wet Line markers, route target speeds, race-control flags and opponent state. These settings tune the decision layer independently of the physical representation.");
        ImGui::Spacing(); ImGui::TextWrapped("Classes, entrants and series are gameplay authorities that consume venue/session data. Entrants can be event-specific or reusable; championship rounds reference events rather than duplicating track definitions.");
    }
    ImGui::EndChild();
}

void HeritageStudioApp::drawGameplayViewportInteractive()
{
    sectionTitle("3D GAMEPLAY LAYOUT");
    drawViewportToolbar(m_viewGridVisible, m_viewSnapEnabled, m_viewSnapM,
        m_viewYawDeg, m_viewPitchDeg, m_viewDistanceM, m_viewOrthographic);

    if (m_gameplayTab == 1 && ImGui::BeginTable("GameplayPlacementToolbar", 4, ImGuiTableFlags_SizingStretchSame))
    {
        const auto placeButton = [&](const char* label, authoring::WorldPointType type)
        {
            ImGui::TableNextColumn();
            const bool active = m_gameplayPlacementType == static_cast<int>(type);
            if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(label, ImVec2(-1.0f, 27.0f)))
                m_gameplayPlacementType = active ? -1 : static_cast<int>(type);
            if (active) ImGui::PopStyleColor();
        };
        placeButton("GARAGE", authoring::WorldPointType::Garage);
        placeButton("MEET", authoring::WorldPointType::MeetSpot);
        placeButton("EVENT HUB", authoring::WorldPointType::EventHub);
        placeButton("FUEL", authoring::WorldPointType::FuelStation);
        placeButton("SAFEHOUSE", authoring::WorldPointType::Safehouse);
        placeButton("POLICE", authoring::WorldPointType::PoliceStation);
        placeButton("SPEED TRAP", authoring::WorldPointType::SpeedTrap);
        placeButton("FAST TRAVEL", authoring::WorldPointType::FastTravel);
        ImGui::EndTable();
    }
    if (m_gameplayTab == 2 && ImGui::BeginTable("PoliceGameplayPlacementToolbar", 4, ImGuiTableFlags_SizingStretchSame))
    {
        const auto policePlaceButton = [&](const char* label, int kind)
        {
            ImGui::TableNextColumn(); const bool active=m_gameplayPolicePlacementKind==kind;
            if(active) ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if(ImGui::Button(label,ImVec2(-1.0f,27.0f))) m_gameplayPolicePlacementKind=active?-1:kind;
            if(active) ImGui::PopStyleColor();
        };
        policePlaceButton("PATROL ZONE",0); policePlaceButton("ROADBLOCK",1); policePlaceButton("ESCAPE ZONE",2); policePlaceButton("MEET",3);
        ImGui::EndTable();
    }
    if (m_gameplayPlacementType >= 0 || m_gameplayPolicePlacementKind >= 0)
        ImGui::TextDisabled("Placement mode active: click scene surface. ESC cancels.");

    ImVec2 size = ImGui::GetContentRegionAvail();
    size.y = std::max(180.0f, size.y);
    bool hovered = false;
    const StudioViewportProjection view = prepareInteractiveViewport(
        m_window, m_viewFly, "##GameplayInteractiveViewport", size,
        m_viewYawDeg, m_viewPitchDeg, m_viewDistanceM, m_viewTarget,
        m_viewGridVisible, m_viewOrthographic, hovered);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    drawSceneGeometryBackdrop(draw, view, m_scenePreview.get(), m_viewGridVisible);
    drawFlyNavigationOverlay(draw, view, m_viewFly);
    draw->PushClipRect(view.min, view.max, true);
    const ImGuiIO& io = ImGui::GetIO();

    if (hovered && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    {
        m_gameplayPlacementType = -1;
        m_gameplayPolicePlacementKind = -1;
    }

    if (m_gameplayTab == 0 && !m_authoring.gameEvents.empty())
    {
        m_gameplaySelectedEvent = std::clamp(m_gameplaySelectedEvent, 0, static_cast<int>(m_authoring.gameEvents.size()) - 1);
        const auto& event = m_authoring.gameEvents[static_cast<std::size_t>(m_gameplaySelectedEvent)];
        const authoring::RaceMarker* start = nullptr;
        const authoring::RaceMarker* finish = nullptr;
        for (const auto& marker : m_authoring.raceMarkers)
        {
            if (marker.id == event.startMarkerId) start = &marker;
            if (marker.id == event.finishMarkerId) finish = &marker;
        }
        ImVec2 a{}, b{};
        if (start && projectWorldPoint(view, start->position, a))
            drawMarker(draw, a, IM_COL32(80, 230, 130, 255), true, "EVENT START");
        if (finish && projectWorldPoint(view, finish->position, b))
            drawMarker(draw, b, IM_COL32(245, 100, 90, 255), true, "EVENT FINISH");
        if (start && finish && projectWorldPoint(view, start->position, a) && projectWorldPoint(view, finish->position, b))
            draw->AddLine(a, b, IM_COL32(240, 210, 80, 180), 2.0f);
    }

    for (int i = 0; i < static_cast<int>(m_authoring.worldPoints.size()); ++i)
    {
        const auto& point = m_authoring.worldPoints[static_cast<std::size_t>(i)];
        if (!point.enabled) continue;
        ImVec2 screen{};
        if (!projectWorldPoint(view, point.position, screen)) continue;
        ImU32 color = IM_COL32(200, 200, 210, 255);
        switch (point.type)
        {
        case authoring::WorldPointType::Garage: color = IM_COL32(90, 190, 245, 255); break;
        case authoring::WorldPointType::Dealership: color = IM_COL32(190, 125, 245, 255); break;
        case authoring::WorldPointType::FuelStation: color = IM_COL32(90, 220, 120, 255); break;
        case authoring::WorldPointType::RepairShop: color = IM_COL32(245, 165, 70, 255); break;
        case authoring::WorldPointType::MeetSpot: color = IM_COL32(235, 105, 200, 255); break;
        case authoring::WorldPointType::EventHub: color = IM_COL32(245, 210, 75, 255); break;
        case authoring::WorldPointType::Safehouse: color = IM_COL32(80, 215, 195, 255); break;
        case authoring::WorldPointType::PoliceStation:
        case authoring::WorldPointType::SpeedCamera:
        case authoring::WorldPointType::SpeedTrap: color = IM_COL32(235, 90, 90, 255); break;
        case authoring::WorldPointType::FastTravel: color = IM_COL32(120, 155, 255, 255); break;
        default: break;
        }
        drawMarker(draw, screen, color, m_gameplayTab == 1 && i == m_gameplaySelectedWorldPoint, point.name.c_str());
    }

    if (m_gameplayTab == 2)
    {
        for (int i=0;i<static_cast<int>(m_authoring.policePatrolZones.size());++i) { const auto& v=m_authoring.policePatrolZones[static_cast<std::size_t>(i)]; if(!v.enabled) continue; ImVec2 p{}; if(projectWorldPoint(view,v.position,p)) drawMarker(draw,p,IM_COL32(70,140,255,255),m_gameplayPoliceSelectionKind==0&&m_gameplayPoliceSelectedIndex==i,v.name.c_str()); }
        for (int i=0;i<static_cast<int>(m_authoring.policeRoadblockSites.size());++i) { const auto& v=m_authoring.policeRoadblockSites[static_cast<std::size_t>(i)]; if(!v.enabled) continue; ImVec2 p{}; if(projectWorldPoint(view,v.position,p)) drawMarker(draw,p,IM_COL32(240,70,70,255),m_gameplayPoliceSelectionKind==1&&m_gameplayPoliceSelectedIndex==i,v.name.c_str()); }
        for (int i=0;i<static_cast<int>(m_authoring.policeEscapeZones.size());++i) { const auto& v=m_authoring.policeEscapeZones[static_cast<std::size_t>(i)]; if(!v.enabled) continue; ImVec2 p{}; if(projectWorldPoint(view,v.position,p)) drawMarker(draw,p,IM_COL32(70,220,180,255),m_gameplayPoliceSelectionKind==2&&m_gameplayPoliceSelectedIndex==i,v.name.c_str()); }
        for (int i=0;i<static_cast<int>(m_authoring.clandestineMeets.size());++i) { const auto& v=m_authoring.clandestineMeets[static_cast<std::size_t>(i)]; if(!v.enabled) continue; ImVec2 p{}; if(projectWorldPoint(view,v.position,p)) drawMarker(draw,p,IM_COL32(225,80,210,255),m_gameplayPoliceSelectionKind==3&&m_gameplayPoliceSelectedIndex==i,v.name.c_str()); }
    }

    bool gizmoCaptured = false;
    if (m_gameplayTab == 1 && !m_authoring.worldPoints.empty() && m_gameplayPlacementType < 0)
    {
        m_gameplaySelectedWorldPoint = std::clamp(m_gameplaySelectedWorldPoint, 0, static_cast<int>(m_authoring.worldPoints.size()) - 1);
        auto& selected = m_authoring.worldPoints[static_cast<std::size_t>(m_gameplaySelectedWorldPoint)];
        const int previousAxis = m_viewGizmoAxis;
        drawMoveGizmo(draw, view, selected.position, hovered,
            m_viewSnapEnabled, m_viewSnapM, m_viewGizmoAxis, m_viewGizmoDragAccumulator);
        gizmoCaptured = m_viewGizmoAxis >= 0 || previousAxis >= 0;
        if (hovered && ImGui::IsKeyPressed(ImGuiKey_F, false))
        {
            m_viewTarget = selected.position;
            m_viewDistanceM = std::clamp(m_viewDistanceM, 8.0f, 100.0f);
        }
    }

    if (m_gameplayTab == 2 && m_gameplayPolicePlacementKind < 0)
    {
        authoring::Vec3* position=nullptr;
        if(m_gameplayPoliceSelectionKind==0 && !m_authoring.policePatrolZones.empty()){m_gameplayPoliceSelectedIndex=std::clamp(m_gameplayPoliceSelectedIndex,0,static_cast<int>(m_authoring.policePatrolZones.size())-1); position=&m_authoring.policePatrolZones[static_cast<std::size_t>(m_gameplayPoliceSelectedIndex)].position;}
        else if(m_gameplayPoliceSelectionKind==1 && !m_authoring.policeRoadblockSites.empty()){m_gameplayPoliceSelectedIndex=std::clamp(m_gameplayPoliceSelectedIndex,0,static_cast<int>(m_authoring.policeRoadblockSites.size())-1); position=&m_authoring.policeRoadblockSites[static_cast<std::size_t>(m_gameplayPoliceSelectedIndex)].position;}
        else if(m_gameplayPoliceSelectionKind==2 && !m_authoring.policeEscapeZones.empty()){m_gameplayPoliceSelectedIndex=std::clamp(m_gameplayPoliceSelectedIndex,0,static_cast<int>(m_authoring.policeEscapeZones.size())-1); position=&m_authoring.policeEscapeZones[static_cast<std::size_t>(m_gameplayPoliceSelectedIndex)].position;}
        else if(m_gameplayPoliceSelectionKind==3 && !m_authoring.clandestineMeets.empty()){m_gameplayPoliceSelectedIndex=std::clamp(m_gameplayPoliceSelectedIndex,0,static_cast<int>(m_authoring.clandestineMeets.size())-1); position=&m_authoring.clandestineMeets[static_cast<std::size_t>(m_gameplayPoliceSelectedIndex)].position;}
        if(position){const int previousAxis=m_viewGizmoAxis; drawMoveGizmo(draw,view,*position,hovered,m_viewSnapEnabled,m_viewSnapM,m_viewGizmoAxis,m_viewGizmoDragAccumulator); gizmoCaptured=gizmoCaptured||m_viewGizmoAxis>=0||previousAxis>=0; if(hovered&&ImGui::IsKeyPressed(ImGuiKey_F,false)){m_viewTarget=*position;m_viewDistanceM=std::clamp(m_viewDistanceM,8.0f,200.0f);}}
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !gizmoCaptured && m_gameplayTab == 1)
    {
        if (m_gameplayPlacementType >= 0)
        {
            authoring::Vec3 ground{};
            if (viewportAuthoringPoint(view, io.MousePos, m_scenePreview.get(), ground))
            {
                ground.x = snapValue(ground.x, m_viewSnapEnabled, m_viewSnapM);
                ground.z = snapValue(ground.z, m_viewSnapEnabled, m_viewSnapM);
                auto& created = m_authoring.addWorldPoint(static_cast<authoring::WorldPointType>(m_gameplayPlacementType));
                created.position = ground;
                m_gameplaySelectedWorldPoint = static_cast<int>(m_authoring.worldPoints.size()) - 1;
                m_gameplayPlacementType = -1;
                m_studioMessage = "Placed free-roam gameplay point in 3D viewport.";
            }
        }
        else
        {
            int bestIndex = -1;
            float bestDistance = 13.0f;
            for (int i = 0; i < static_cast<int>(m_authoring.worldPoints.size()); ++i)
            {
                ImVec2 screen{};
                if (!projectWorldPoint(view, m_authoring.worldPoints[static_cast<std::size_t>(i)].position, screen)) continue;
                const float dx = io.MousePos.x - screen.x;
                const float dy = io.MousePos.y - screen.y;
                const float distance = std::sqrt(dx * dx + dy * dy);
                if (distance < bestDistance) { bestDistance = distance; bestIndex = i; }
            }
            if (bestIndex >= 0) m_gameplaySelectedWorldPoint = bestIndex;
        }
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !gizmoCaptured && m_gameplayTab == 2)
    {
        if(m_gameplayPolicePlacementKind>=0)
        {
            authoring::Vec3 ground{}; if(viewportAuthoringPoint(view,io.MousePos,m_scenePreview.get(),ground)){ground.x=snapValue(ground.x,m_viewSnapEnabled,m_viewSnapM); ground.z=snapValue(ground.z,m_viewSnapEnabled,m_viewSnapM); m_gameplayPoliceSelectionKind=m_gameplayPolicePlacementKind;
                if(m_gameplayPolicePlacementKind==0){auto& v=m_authoring.addPolicePatrolZone();v.position=ground;m_gameplayPoliceSelectedIndex=static_cast<int>(m_authoring.policePatrolZones.size())-1;}
                else if(m_gameplayPolicePlacementKind==1){auto& v=m_authoring.addPoliceRoadblockSite();v.position=ground;m_gameplayPoliceSelectedIndex=static_cast<int>(m_authoring.policeRoadblockSites.size())-1;}
                else if(m_gameplayPolicePlacementKind==2){auto& v=m_authoring.addPoliceEscapeZone();v.position=ground;m_gameplayPoliceSelectedIndex=static_cast<int>(m_authoring.policeEscapeZones.size())-1;}
                else {auto& v=m_authoring.addClandestineMeet();v.position=ground;m_gameplayPoliceSelectedIndex=static_cast<int>(m_authoring.clandestineMeets.size())-1;}
                m_gameplayPolicePlacementKind=-1; m_studioMessage="Placed police / underground gameplay authoring point in 3D viewport."; }
        }
        else
        {
            int bestKind=-1,bestIndex=-1; float bestDistance=13.0f; const auto consider=[&](int kind,int index,const authoring::Vec3& pos){ImVec2 p{}; if(!projectWorldPoint(view,pos,p)) return; float dx=io.MousePos.x-p.x,dy=io.MousePos.y-p.y,d=std::sqrt(dx*dx+dy*dy); if(d<bestDistance){bestDistance=d;bestKind=kind;bestIndex=index;}};
            for(int i=0;i<static_cast<int>(m_authoring.policePatrolZones.size());++i) consider(0,i,m_authoring.policePatrolZones[static_cast<std::size_t>(i)].position);
            for(int i=0;i<static_cast<int>(m_authoring.policeRoadblockSites.size());++i) consider(1,i,m_authoring.policeRoadblockSites[static_cast<std::size_t>(i)].position);
            for(int i=0;i<static_cast<int>(m_authoring.policeEscapeZones.size());++i) consider(2,i,m_authoring.policeEscapeZones[static_cast<std::size_t>(i)].position);
            for(int i=0;i<static_cast<int>(m_authoring.clandestineMeets.size());++i) consider(3,i,m_authoring.clandestineMeets[static_cast<std::size_t>(i)].position);
            if(bestKind>=0){m_gameplayPoliceSelectionKind=bestKind;m_gameplayPoliceSelectedIndex=bestIndex;}
        }
    }
    draw->PopClipRect();
}

void HeritageStudioApp::drawWeatherWorkspace()
{
    headingText("WEATHER / WORLD AUTHORING");
    ImGui::SameLine();
    ImGui::TextDisabled("  geography / climate / starting conditions");
    ImGui::Separator();

    if (ImGui::Button("SAVE WEATHER", ImVec2(140.0f, 30.0f)))
        m_authoring.saveWeather(m_studioProjectRoot / "weather.hweather", m_studioMessage);
    ImGui::SameLine();
    if (ImGui::Button("LOAD WEATHER", ImVec2(140.0f, 30.0f)))
        m_authoring.loadWeather(m_studioProjectRoot / "weather.hweather", m_studioMessage);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_studioMessage.c_str());

    auto& weather = m_authoring.weather;
    if (ImGui::BeginTable("WeatherAuthoringTable", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("PARAMETER", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("VALUE", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("PURPOSE", ImGuiTableColumnFlags_WidthStretch, 1.6f);
        ImGui::TableHeadersRow();
        describedSlider("weatherLat", "Latitude", "Drives astronomy, sun/moon orientation and regional climate context.", &weather.latitude, -90.0f, 90.0f, "%.5f deg");
        describedSlider("weatherLon", "Longitude", "World longitude used with date/time for the astronomical sky.", &weather.longitude, -180.0f, 180.0f, "%.5f deg");
        describedSlider("weatherElev", "Elevation", "Scene elevation above sea level for atmosphere and world metadata.", &weather.elevationM, -500.0f, 9000.0f, "%.0f m");
        describedSlider("weatherHour", "Starting hour", "Default local time when the scene/session starts.", &weather.startHour, 0.0f, 24.0f, "%.2f h");
        describedSlider("weatherCloud", "Cloud coverage", "Default regional cloud coverage for scene preview/session initialization.", &weather.cloudCoverage, 0.0f, 1.0f, "%.2f");
        describedSlider("weatherRain", "Rain intensity", "Default rainfall intensity for weather and track wetting initialization.", &weather.rainIntensity, 0.0f, 1.0f, "%.2f");
        describedSlider("weatherTemp", "Temperature", "Ambient temperature metadata used by future tire, engine and surface conditions.", &weather.temperatureC, -30.0f, 55.0f, "%.1f C");
        describedSlider("weatherHumidity", "Humidity", "Controls atmospheric moisture context and future haze / drying behavior.", &weather.humidity, 0.0f, 1.0f, "%.2f");
        describedSlider("weatherWet", "Starting surface wetness", "Initial wet-track state before live weather begins modifying the surface.", &weather.surfaceWetness, 0.0f, 1.0f, "%.2f");
        ImGui::EndTable();
    }

    sectionTitle("FUTURE PREVIEW MODULES");
    ImGui::BulletText("Regional weather map / radar authoring");
    ImGui::BulletText("Cloud and precipitation preview without booting Racing United");
    ImGui::BulletText("Astronomy / time-of-day scrubber");
    ImGui::BulletText("Track wetness and drying initialization preview");
}

void HeritageStudioApp::drawVehicleWorkspace()
{
    headingText("VEHICLE AUTHORING");
    ImGui::SameLine();
    ImGui::TextDisabled("  content identity / eligibility / acoustic assignment");
    ImGui::Separator();

    if (ImGui::Button("SAVE VEHICLE AUTHORING", ImVec2(190.0f, 30.0f)))
        m_authoring.saveVehicle(m_studioProjectRoot / "vehicle.hvehicleauthor", m_studioMessage);
    ImGui::SameLine();
    if (ImGui::Button("LOAD VEHICLE AUTHORING", ImVec2(190.0f, 30.0f)))
        m_authoring.loadVehicle(m_studioProjectRoot / "vehicle.hvehicleauthor", m_studioMessage);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_studioMessage.c_str());

    auto& vehicle = m_authoring.vehicle;
    sectionTitle("VEHICLE CONTENT LINKAGE");
    inputString("Display name", vehicle.displayName);
    inputString("Vehicle definition", vehicle.vehicleDefinition, 1024);
    inputString("Acoustic profile", vehicle.acousticProfile, 512);
    inputString("Tire set", vehicle.tireSet, 512);
    ImGui::DragFloat("Spawn height m", &vehicle.spawnHeightM, 0.01f, -1.0f, 5.0f);
    ImGui::DragFloat("Fuel liters", &vehicle.fuelLiters, 0.5f, 0.0f, 500.0f);
    ImGui::Checkbox("Eligible for free-roam traffic", &vehicle.trafficEligible);
    ImGui::Checkbox("Eligible for races", &vehicle.raceEligible);

    sectionTitle("AUTHORING MODULES TO GROW HERE");
    ImGui::BulletText("GLB part hierarchy / wheel and hub transforms");
    ImGui::BulletText("Suspension geometry, alignment and anti-roll setup");
    ImGui::BulletText("Mass / inertia / fuel / drivetrain editor");
    ImGui::BulletText("Tire specification and compound assignment");
    ImGui::BulletText("Cockpit / chase / replay camera authoring");
    ImGui::BulletText("Damage, lights, instrumentation and animation bindings");
    ImGui::BulletText("Acoustic profile and captured sound-bank assignment");
}

void HeritageStudioApp::drawAssetsWorkspace()
{
    headingText("ASSET BROWSER");
    ImGui::SameLine();
    ImGui::TextDisabled("  Racing United content / authoring inputs");
    ImGui::Separator();

    const auto assetRoot = m_moduleRoot / "Assets";
    if (m_assetBrowserPath.empty())
        m_assetBrowserPath = assetRoot;

    if (ImGui::Button("ASSET ROOT", ImVec2(120.0f, 30.0f)))
        m_assetBrowserPath = assetRoot;
    ImGui::SameLine();
    if (ImGui::Button("UP", ImVec2(70.0f, 30.0f)))
    {
        const auto parent = m_assetBrowserPath.parent_path();
        const auto rootText = assetRoot.lexically_normal().string();
        const auto parentText = parent.lexically_normal().string();
        if (!parent.empty() && parentText.rfind(rootText, 0) == 0)
            m_assetBrowserPath = parent;
    }
    ImGui::SameLine();
    if (ImGui::Button("OPEN IN EXPLORER", ImVec2(160.0f, 30.0f)))
        openPathInShell(m_assetBrowserPath);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_assetBrowserPath.string().c_str());

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float inspectorWidth = std::max(340.0f, available.x * 0.30f);

    ImGui::BeginChild("AssetBrowserList", ImVec2(available.x - inspectorWidth - 8.0f, 0.0f), true);
    sectionTitle("FILES / FOLDERS");
    std::error_code ec;
    std::vector<std::filesystem::directory_entry> entries;
    if (std::filesystem::exists(m_assetBrowserPath, ec))
    {
        for (std::filesystem::directory_iterator it(m_assetBrowserPath, ec), end; !ec && it != end; it.increment(ec))
            entries.push_back(*it);
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b)
    {
        const bool ad = a.is_directory();
        const bool bd = b.is_directory();
        if (ad != bd) return ad > bd;
        return a.path().filename().string() < b.path().filename().string();
    });

    for (const auto& entry : entries)
    {
        const bool directory = entry.is_directory(ec);
        const std::string prefix = directory ? "[DIR]  " : "       ";
        const std::string label = prefix + entry.path().filename().string() + "##asset" + entry.path().string();
        const bool selected = m_selectedAssetPath == entry.path();
        if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
        {
            m_selectedAssetPath = entry.path();
            if (directory && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                m_assetBrowserPath = entry.path();
        }
    }
    if (ec)
        ImGui::TextWrapped("Asset browser error: %s", ec.message().c_str());
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("AssetInspector", ImVec2(0.0f, 0.0f), true);
    sectionTitle("ASSET INSPECTOR");
    if (m_selectedAssetPath.empty())
    {
        ImGui::TextDisabled("Select an asset or folder.");
    }
    else
    {
        ImGui::TextWrapped("%s", m_selectedAssetPath.filename().string().c_str());
        ImGui::Separator();
        ImGui::TextWrapped("Path: %s", m_selectedAssetPath.string().c_str());
        std::error_code infoError;
        if (std::filesystem::is_regular_file(m_selectedAssetPath, infoError))
        {
            const auto bytes = std::filesystem::file_size(m_selectedAssetPath, infoError);
            if (!infoError)
                ImGui::Text("Size: %.2f MiB", static_cast<double>(bytes) / (1024.0 * 1024.0));
            ImGui::Text("Extension: %s", m_selectedAssetPath.extension().string().c_str());
        }
        else if (std::filesystem::is_directory(m_selectedAssetPath, infoError))
        {
            ImGui::TextUnformatted("Type: Folder");
        }
        ImGui::Spacing();
        if (ImGui::Button("OPEN", ImVec2(-1.0f, 30.0f)))
            openPathInShell(m_selectedAssetPath);
        if (ImGui::Button("COPY PATH", ImVec2(-1.0f, 30.0f)))
            ImGui::SetClipboardText(m_selectedAssetPath.string().c_str());
        ImGui::Spacing();
        ImGui::TextWrapped("Later this inspector becomes format-aware: GLB materials/meshes/animations, textures, tire definitions, audio banks, .hacoustic profiles, race/road/weather assets and validation status.");
    }
    ImGui::EndChild();
}

void HeritageStudioApp::drawAudioWorkspace()
{
    ImGui::TextUnformatted("ENGINE SOUND CAPTURE LABORATORY");
    ImGui::SameLine();
    ImGui::TextDisabled("  STUDIO04 readability + vehicle character authoring");
    ImGui::Separator();

    if (!m_soundLab)
    {
        ImGui::TextUnformatted("Engine Sound Lab is unavailable.");
        return;
    }

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float captureWidth = std::max(350.0f, available.x * 0.28f);
    const float perspectiveWidth = std::max(300.0f, available.x * 0.22f);
    const float centerWidth = std::max(500.0f, available.x - captureWidth - perspectiveWidth - 16.0f);

    ImGui::BeginChild("AudioCapturePanel", ImVec2(captureWidth, 0.0f), true);
    drawAudioCapturePanel();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("AudioShapePanel", ImVec2(centerWidth, 0.0f), true);
    ImGui::TextWrapped("Readable authoring lane: every important slider now shows its meaning in a dedicated description column, with the same help available on hover.");
    ImGui::Spacing();
    drawAudioAssistantPanel();
    ImGui::Spacing();
    drawAudioCharacterPanel();
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("ADVANCED FILTERS / MANUAL FINE TUNE", ImGuiTreeNodeFlags_DefaultOpen))
        drawAudioShapePanel();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("AudioPerspectivePanel", ImVec2(0.0f, 0.0f), true);
    drawAudioPerspectivePanel();
    ImGui::EndChild();
}

void HeritageStudioApp::drawAudioCapturePanel()
{
    const auto status = m_soundLab->status();
    const auto analysis = m_soundLab->analysis();
    ImGui::TextUnformatted("CAPTURE / BANK");
    ImGui::Separator();
    ImGui::TextWrapped("Run Engine Simulator Community Edition beside Studio. Heritage captures the Windows playback endpoint directly.");

    if (ImGui::Button("OPEN EW10J4S .MR", ImVec2(-1.0f, 30.0f)))
        openPathInShell(m_engineScriptPath);
    if (ImGui::Button("OPEN ENGINE RESEARCH", ImVec2(-1.0f, 30.0f)))
        openPathInShell(m_engineResearchPath);

    sectionTitle("CALIBRATION SAMPLE");
    slider("Capture seconds", &m_captureDurationSeconds, 2.0f, 10.0f, "%.1f s");
    if (!status.capturing)
    {
        if (ImGui::Button("CAPTURE CALIBRATION SAMPLE", ImVec2(-1.0f, 36.0f)))
            m_soundLab->startCalibrationCapture(m_captureDurationSeconds);
    }
    else
    {
        ImGui::ProgressBar(status.progress, ImVec2(-1.0f, 22.0f));
        if (ImGui::Button("STOP CAPTURE", ImVec2(-1.0f, 32.0f)))
            m_soundLab->stopCapture();
    }

    if (analysis.valid)
    {
        const bool good = lastCaptureQualityGood();
        ImGui::Text("Capture quality: %s", good ? "GOOD" : "CHECK");
        ImGui::SameLine();
        ImGui::TextDisabled("Peak %.1f dBFS | RMS %.1f | stability %.2f dB",
            analysis.peakDb, analysis.rmsDb, analysis.stabilityDb);
    }

    sectionTitle("PEUGEOT BANK");
    const int completed = countCompletedCaptureTargets();
    const int total = static_cast<int>(m_captureTargets.size());
    const float bankProgress = total > 0 ? static_cast<float>(completed) / static_cast<float>(total) : 0.0f;
    char progressText[64]{};
    std::snprintf(progressText, sizeof(progressText), "%d / %d captured", completed, total);
    ImGui::ProgressBar(bankProgress, ImVec2(-1.0f, 20.0f), progressText);

    ImGui::Checkbox("Auto-advance", &m_autoAdvanceCapture);
    ImGui::SameLine();
    ImGui::Checkbox("Quality gate", &m_captureQualityGate);
    ImGui::Checkbox("Skip already captured cells", &m_skipCompletedCaptureTargets);

    if (ImGui::Button("JUMP TO FIRST MISSING", ImVec2(-1.0f, 30.0f)))
        jumpToNextMissingCaptureTarget();

    if (!m_captureTargets.empty())
    {
        m_captureTargetIndex = std::clamp(m_captureTargetIndex, 0, static_cast<int>(m_captureTargets.size()) - 1);
        const auto& target = m_captureTargets[static_cast<std::size_t>(m_captureTargetIndex)];
        const bool alreadyCaptured = m_soundLab->bankCaptureExists(
            "Peugeot206RC", "EW10J4S", target.rpm, target.throttlePercent);
        ImGui::Text("Target %d / %d%s", m_captureTargetIndex + 1, total,
            alreadyCaptured ? "  [CAPTURED]" : "");
        if (target.idle)
            ImGui::Text("WARM IDLE: %d rpm", target.rpm);
        else
        {
            ImGui::Text("RPM HOLD: %d rpm", target.rpm);
            ImGui::Text("THROTTLE: %d%%", target.throttlePercent);
        }
        ImGui::TextWrapped("Set Engine Simulator to Dyno + RPM Hold, stabilize this target, then capture.");

        if (ImGui::Button("< PREVIOUS", ImVec2(110.0f, 30.0f)))
            m_captureTargetIndex = std::max(0, m_captureTargetIndex - 1);
        ImGui::SameLine();
        if (ImGui::Button("NEXT >", ImVec2(110.0f, 30.0f)))
            m_captureTargetIndex = std::min(total - 1, m_captureTargetIndex + 1);

        if (!status.capturing && ImGui::Button(
            alreadyCaptured ? "RECAPTURE CURRENT TARGET" : "CAPTURE CURRENT TARGET",
            ImVec2(-1.0f, 40.0f)))
        {
            m_soundLab->startBankCapture(
                "Peugeot206RC",
                "EW10J4S",
                target.rpm,
                target.throttlePercent,
                m_captureDurationSeconds);
        }
    }

    sectionTitle("LAST CAPTURE");
    ImGui::Text("Rate: %u Hz", status.sampleRate);
    ImGui::Text("Peak: %.3f", status.peak);
    ImGui::Text("RMS:  %.3f", status.rms);
    if (!status.lastRawPath.empty())
        ImGui::TextWrapped("%s", status.lastRawPath.string().c_str());
    if (ImGui::Button("OPEN CAPTURE FOLDER", ImVec2(-1.0f, 30.0f)))
        openPathInShell(m_soundLab->root());
}

void HeritageStudioApp::drawAudioAssistantPanel()
{
    const auto analysis = m_soundLab->analysis();
    ImGui::TextUnformatted("AUDIO ASSISTANT / QUICK TUNE");
    ImGui::Separator();
    ImGui::TextWrapped("For the lazy workflow: capture one representative raw sample, then let Studio establish a sane source-character profile before you touch individual filters.");

    if (ImGui::Button("AUTO PEUGEOT 206 RC STOCK", ImVec2(-1.0f, 42.0f)))
        autoTuneAudio(audio::lab::EngineSoundPreset::Peugeot206RCStock);
    if (ImGui::Button("AUTO CLEAN ENGINE-SIM RAW", ImVec2(-1.0f, 34.0f)))
        autoTuneAudio(audio::lab::EngineSoundPreset::EngineSimCleanup);

    if (ImGui::BeginTable("AudioPresetTable", 3, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextColumn();
        if (ImGui::Button("CLEAN", ImVec2(-1.0f, 30.0f)))
            applyAudioPreset(audio::lab::EngineSoundPreset::EngineSimCleanup);
        ImGui::TableNextColumn();
        if (ImGui::Button("WARM", ImVec2(-1.0f, 30.0f)))
            applyAudioPreset(audio::lab::EngineSoundPreset::WarmRoadCar);
        ImGui::TableNextColumn();
        if (ImGui::Button("SPORT", ImVec2(-1.0f, 30.0f)))
            applyAudioPreset(audio::lab::EngineSoundPreset::SportExhaust);
        ImGui::EndTable();
    }
    if (ImGui::Button("RESET NEUTRAL", ImVec2(-1.0f, 30.0f)))
        applyAudioPreset(audio::lab::EngineSoundPreset::Neutral);
    if (m_hasProfileUndo)
    {
        if (ImGui::Button("UNDO LAST ASSISTANT CHANGE", ImVec2(-1.0f, 30.0f)))
        {
            const auto current = m_soundLab->profile();
            m_soundLab->setProfile(m_profileUndo);
            m_profileUndo = current;
            m_audioAssistantMessage = "Restored the previous acoustic profile.";
        }
    }

    ImGui::Spacing();
    ImGui::TextWrapped("%s", m_audioAssistantMessage.c_str());

    sectionTitle("RAW CAPTURE ANALYSIS");
    if (!analysis.valid)
    {
        ImGui::TextDisabled("No analyzed capture yet.");
        ImGui::TextWrapped("Capture a 3-6 second steady sample at roughly 2500-4000 rpm and moderate load for a useful automatic starting point.");
        return;
    }

    ImGui::Text("Peak: %.1f dBFS", analysis.peakDb);
    ImGui::SameLine();
    ImGui::Text("RMS: %.1f dBFS", analysis.rmsDb);
    ImGui::Text("Crest: %.1f dB", analysis.crestDb);
    ImGui::SameLine();
    ImGui::Text("Stability: %.2f dB", analysis.stabilityDb);
    ImGui::Text("Harshness estimate: %.0f%%", analysis.harshness * 100.0f);
    ImGui::Text("Upper-mid focus: %.0f Hz", analysis.dominantPresenceHz);

    ImGui::PlotLines("##RawWaveform",
        analysis.waveform.data(), static_cast<int>(analysis.waveform.size()),
        0, "RAW ENVELOPE", 0.0f, 1.0f, ImVec2(-1.0f, 68.0f));
    ImGui::PlotHistogram("##RawSpectrum",
        analysis.spectrum.data(), static_cast<int>(analysis.spectrum.size()),
        0, "35 Hz -> 16 kHz", 0.0f, 1.0f, ImVec2(-1.0f, 76.0f));

    if (ImGui::BeginTable("BandEnergy", 5, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame))
    {
        const char* labels[] = { "LOW", "BODY", "MID", "PRES", "AIR" };
        const float values[] = { analysis.lowRatio, analysis.bodyRatio, analysis.midRatio,
            analysis.presenceRatio, analysis.airRatio };
        for (int column = 0; column < 5; ++column)
        {
            ImGui::TableNextColumn();
            ImGui::Text("%s\n%.0f%%", labels[column], values[column] * 100.0f);
        }
        ImGui::EndTable();
    }

    sectionTitle("CAPTURE VERDICT");
    if (lastCaptureQualityGood())
        ImGui::TextWrapped("GOOD: level and short-term stability are suitable for an RPM/load bank cell.");
    else
        ImGui::TextWrapped("CHECK: the capture is too quiet, too close to clipping, or varies too much in level. The Quality Gate will pause auto-advance so you can recapture it.");
}


void HeritageStudioApp::drawAudioCharacterPanel()
{
    audio::lab::EngineSoundAcousticProfile profile = m_soundLab->profile();
    bool changed = false;

    ImGui::TextUnformatted("VEHICLE CHARACTER DESIGNER");
    ImGui::Separator();
    ImGui::TextWrapped("The Assistant removes Engine-Sim ugliness. This section deliberately puts personality back in: intake architecture, exhaust construction, mechanical character and cabin construction remain unique per vehicle.");
    ImGui::TextDisabled("Use the description column for quick meaning, or hover the parameter name/value for the same explanation.");

    if (ImGui::CollapsingHeader("ENGINE CORE / COMBUSTION", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (beginParameterTable("EngineCoreTable"))
        {
            changed |= describedSlider("combustionPunch", "Combustion punch",
                "Adds transient bite to each firing pulse so the engine feels more eager and energetic.",
                &profile.combustionPunch, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("metallicCharacter", "Mechanical metal",
                "Blends in hard metallic overtones from valvetrain, chain and block hardware.",
                &profile.metallicCharacter, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("metallicFrequency", "Metallic focus",
                "Sets where the mechanical metal emphasis sits in frequency space.",
                &profile.metallicFrequencyHz, 800.0f, 7000.0f, "%.0f Hz");
            changed |= describedSlider("mechanicalPresence", "Valvetrain / mechanical presence",
                "Overall audibility of cam, valvetrain and upper-engine mechanical texture.",
                &profile.mechanicalPresence, 0.0f, 1.0f, "%.2f");
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("INTAKE ARCHITECTURE", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("IntakeArchetypes", 3, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            if (ImGui::Button("STOCK AIRBOX", ImVec2(-1.0f, 30.0f)))
            {
                profile.intakePresence = 0.22f; profile.intakeThroat = 0.24f;
                profile.intakeFrequencyHz = 1150.0f; profile.airboxDamping = 0.66f; changed = true;
            }
            ImGui::TableNextColumn();
            if (ImGui::Button("OPEN FILTER", ImVec2(-1.0f, 30.0f)))
            {
                profile.intakePresence = 0.52f; profile.intakeThroat = 0.56f;
                profile.intakeFrequencyHz = 1550.0f; profile.airboxDamping = 0.22f; changed = true;
            }
            ImGui::TableNextColumn();
            if (ImGui::Button("ITB / SHORT", ImVec2(-1.0f, 30.0f)))
            {
                profile.intakePresence = 0.78f; profile.intakeThroat = 0.84f;
                profile.intakeFrequencyHz = 2250.0f; profile.airboxDamping = 0.06f; changed = true;
            }
            ImGui::EndTable();
        }
        if (beginParameterTable("IntakeCharacterTable"))
        {
            changed |= describedSlider("intakePresence", "Induction level",
                "How much intake sound is present overall in the vehicle's character.",
                &profile.intakePresence, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("intakeThroat", "Throttle / throat growl",
                "Adds vocal growl from the intake path as the throttle opens.",
                &profile.intakeThroat, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("intakeFrequency", "Intake resonance",
                "Main resonance zone of the intake system, from stock airbox to short-runner bark.",
                &profile.intakeFrequencyHz, 250.0f, 4500.0f, "%.0f Hz");
            changed |= describedSlider("airboxDamping", "Airbox damping",
                "How strongly the airbox suppresses raw induction noise and resonance ringing.",
                &profile.airboxDamping, 0.0f, 1.0f, "%.2f");
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("EXHAUST ARCHITECTURE", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("ExhaustArchetypes", 3, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            if (ImGui::Button("STOCK MUFFLED", ImVec2(-1.0f, 30.0f)))
            {
                profile.exhaustMuffling = 0.58f; profile.exhaustBodyGainDb = 3.3f;
                profile.exhaustRasp = 0.12f; profile.exhaustDrone = 0.17f;
                profile.tailpipeBrightness = 0.14f; changed = true;
            }
            ImGui::TableNextColumn();
            if (ImGui::Button("SPORT CAT-BACK", ImVec2(-1.0f, 30.0f)))
            {
                profile.exhaustMuffling = 0.31f; profile.exhaustBodyGainDb = 4.8f;
                profile.exhaustRasp = 0.34f; profile.exhaustDrone = 0.29f;
                profile.tailpipeBrightness = 0.42f; changed = true;
            }
            ImGui::TableNextColumn();
            if (ImGui::Button("OPEN / RACE", ImVec2(-1.0f, 30.0f)))
            {
                profile.exhaustMuffling = 0.10f; profile.exhaustBodyGainDb = 5.5f;
                profile.exhaustRasp = 0.68f; profile.exhaustDrone = 0.38f;
                profile.tailpipeBrightness = 0.76f; changed = true;
            }
            ImGui::EndTable();
        }
        if (beginParameterTable("ExhaustCharacterTable"))
        {
            changed |= describedSlider("exhaustMuffling", "Muffler absorption",
                "How much the stock exhaust hardware smooths and absorbs the raw exhaust pulse energy.",
                &profile.exhaustMuffling, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("exhaustBodyGain", "Exhaust body",
                "Adds or removes low-mid exhaust fullness so the rear perspective feels larger or thinner.",
                &profile.exhaustBodyGainDb, -12.0f, 12.0f, "%.1f dB");
            changed |= describedSlider("exhaustRasp", "Exhaust rasp",
                "Controls edgy raspy high-mid texture typical of freer-flowing or harsher systems.",
                &profile.exhaustRasp, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("exhaustDrone", "Resonator / drone",
                "Controls sustained cabin and cruise resonance from the exhaust system.",
                &profile.exhaustDrone, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("tailpipeBrightness", "Tailpipe brightness",
                "Extra high-frequency bite radiated from the tailpipe and rear of the car.",
                &profile.tailpipeBrightness, 0.0f, 1.0f, "%.2f");
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("CABIN CONSTRUCTION", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::BeginTable("CabinArchetypes", 3, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            if (ImGui::Button("LIGHT HATCH", ImVec2(-1.0f, 30.0f)))
            {
                profile.cabinDamping = 0.58f; profile.firewallDamping = 0.56f;
                profile.glassLeak = 0.18f; profile.cabinLowFrequencyLeak = 0.70f;
                profile.cabinBoom = 0.24f; changed = true;
            }
            ImGui::TableNextColumn();
            if (ImGui::Button("GT / INSULATED", ImVec2(-1.0f, 30.0f)))
            {
                profile.cabinDamping = 0.82f; profile.firewallDamping = 0.82f;
                profile.glassLeak = 0.07f; profile.cabinLowFrequencyLeak = 0.58f;
                profile.cabinBoom = 0.34f; changed = true;
            }
            ImGui::TableNextColumn();
            if (ImGui::Button("STRIPPED / RACE", ImVec2(-1.0f, 30.0f)))
            {
                profile.cabinDamping = 0.20f; profile.firewallDamping = 0.16f;
                profile.glassLeak = 0.46f; profile.cabinLowFrequencyLeak = 0.82f;
                profile.cabinBoom = 0.10f; changed = true;
            }
            ImGui::EndTable();
        }
        if (beginParameterTable("CabinCharacterTable"))
        {
            changed |= describedSlider("cabinDamping", "Cabin trim absorption",
                "How much seats, trim and carpet absorb the engine and exhaust energy inside the car.",
                &profile.cabinDamping, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("firewallDamping", "Firewall isolation",
                "Amount of engine-bay isolation before sound reaches the driver's compartment.",
                &profile.firewallDamping, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("glassLeak", "Glass / body leakage",
                "How much upper-frequency exterior sound leaks into the cabin through seals and panels.",
                &profile.glassLeak, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("cabinLowFrequencyLeak", "Low-frequency intrusion",
                "How much bass and boom make it into the cabin structure.",
                &profile.cabinLowFrequencyLeak, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("cabinBoom", "Cabin boom",
                "Emphasizes enclosed low-frequency resonance and pressure build-up inside the cockpit.",
                &profile.cabinBoom, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("windowOpenPreview", "Window open preview",
                "Preview amount of extra exterior leakage as if the side windows are opened.",
                &profile.windowOpenPreview, 0.0f, 1.0f, "%.2f");
            ImGui::EndTable();
        }
    }

    if (changed)
        m_soundLab->setProfile(profile);
}

void HeritageStudioApp::drawAudioShapePanel()
{
    audio::lab::EngineSoundAcousticProfile profile = m_soundLab->profile();
    bool changed = false;

    ImGui::TextUnformatted("MANUAL DSP PARAMETERS");
    ImGui::Separator();
    ImGui::TextWrapped("Fine tuning remains non-destructive. The original Engine Simulator capture is never overwritten.");
    ImGui::TextDisabled("Each row now dedicates a right-hand lane to what the parameter actually does.");

    if (ImGui::CollapsingHeader("SOURCE CHARACTER", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (beginParameterTable("SourceCharacterTable"))
        {
            changed |= describedSlider("inputGain", "Input gain",
                "Pre-filter level trim for the captured source before any tonal shaping.",
                &profile.inputGainDb, -18.0f, 12.0f, "%.1f dB");
            changed |= describedSlider("highPass", "High-pass",
                "Cuts low rumble and DC-like buildup below the useful engine body region.",
                &profile.highPassHz, 10.0f, 300.0f, "%.0f Hz");
            changed |= describedSlider("lowPass", "Low-pass",
                "Caps top-end brightness and synthetic fizz in the captured source.",
                &profile.lowPassHz, 3000.0f, 22000.0f, "%.0f Hz");
            changed |= describedSlider("bodyGain", "Body gain",
                "Boosts or reduces the main low-mid engine body that gives the motor physical mass.",
                &profile.bodyGainDb, -12.0f, 12.0f, "%.1f dB");
            changed |= describedSlider("bodyFrequency", "Body frequency",
                "Center frequency of the broad engine-body emphasis.",
                &profile.bodyFrequencyHz, 40.0f, 400.0f, "%.0f Hz");
            changed |= describedSlider("bodyQ", "Body Q",
                "Width of the body emphasis: lower is broader, higher is narrower and more resonant.",
                &profile.bodyQ, 0.2f, 4.0f, "%.2f");
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("RAW / ELECTRIC HARSHNESS", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (beginParameterTable("HarshnessTable"))
        {
            changed |= describedSlider("presenceCut", "Presence reduction",
                "Reduces the aggressive upper-mid zone that makes raw Engine Simulator captures sound synthetic.",
                &profile.presenceCutDb, 0.0f, 18.0f, "%.1f dB");
            changed |= describedSlider("presenceFrequency", "Presence frequency",
                "Center frequency of the harshness reduction band.",
                &profile.presenceFrequencyHz, 500.0f, 8000.0f, "%.0f Hz");
            changed |= describedSlider("presenceQ", "Presence Q",
                "Controls how narrowly or broadly the harshness reduction acts.",
                &profile.presenceQ, 0.2f, 4.0f, "%.2f");
            changed |= describedSlider("highShelf", "High shelf",
                "Global trim for top-end brightness after the main harshness cut.",
                &profile.highShelfDb, -18.0f, 8.0f, "%.1f dB");
            changed |= describedSlider("pulseSoftening", "Pulse-edge softening",
                "Rounds off sharp synthetic attack spikes without deleting the core firing rhythm.",
                &profile.pulseSoftening, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("saturation", "Saturation",
                "Adds controlled harmonic density so the source feels less thin and digital.",
                &profile.saturation, 0.0f, 1.0f, "%.2f");
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("ENGINE / INTAKE", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (beginParameterTable("EngineIntakeDspTable"))
        {
            changed |= describedSlider("mechanicalPresenceDsp", "Mechanical presence",
                "Direct access to upper-engine hardware presence in the final result.",
                &profile.mechanicalPresence, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("intakePresenceDsp", "Intake presence",
                "Overall amount of intake contribution in the final sound.",
                &profile.intakePresence, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("intakeResonanceDsp", "Intake resonance",
                "Primary resonance zone of the intake path.",
                &profile.intakeFrequencyHz, 250.0f, 4500.0f, "%.0f Hz");
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("EXHAUST", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (beginParameterTable("ExhaustDspTable"))
        {
            changed |= describedSlider("stockMufflingDsp", "Stock muffling",
                "Amount of smoothing and suppression applied by the stock exhaust path.",
                &profile.exhaustMuffling, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("exhaustBodyDsp", "Exhaust body",
                "Low-mid fullness and physical weight of the exhaust note.",
                &profile.exhaustBodyGainDb, -12.0f, 12.0f, "%.1f dB");
            changed |= describedSlider("exhaustResonanceDsp", "Exhaust resonance",
                "Center frequency of the exhaust body emphasis.",
                &profile.exhaustBodyFrequencyHz, 40.0f, 350.0f, "%.0f Hz");
            changed |= describedSlider("exhaustQDsp", "Exhaust Q",
                "Width of the exhaust resonance shaping band.",
                &profile.exhaustBodyQ, 0.2f, 4.0f, "%.2f");
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("CABIN / WORLD PREVIEW", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (beginParameterTable("CabinPreviewTable"))
        {
            changed |= describedSlider("cabinDampingDsp", "Cabin damping",
                "How absorbent and insulated the cabin sounds during driver-perspective preview.",
                &profile.cabinDamping, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("cabinLeakDsp", "Low-frequency leakage",
                "Amount of low-frequency engine and exhaust energy that still enters the cabin.",
                &profile.cabinLowFrequencyLeak, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("cabinResonance", "Cabin resonance",
                "Strength of enclosed-cabin resonant coloration in preview.",
                &profile.cabinResonance, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("cabinResonanceHz", "Cabin resonance Hz",
                "Frequency where the cabin cavity resonance is centered.",
                &profile.cabinResonanceHz, 40.0f, 300.0f, "%.0f Hz");
            changed |= describedSlider("reverbPreview", "Reverb preview",
                "Simple preview of environmental reverberation around the vehicle.",
                &profile.reverbPreview, 0.0f, 0.6f, "%.2f");
            changed |= describedSlider("occlusionPreview", "Occlusion preview",
                "Preview of obstruction or muffling between the listener and the vehicle.",
                &profile.occlusionPreview, 0.0f, 1.0f, "%.2f");
            changed |= describedSlider("outputGain", "Output gain",
                "Final post-processing level trim for preview and saved profile output.",
                &profile.outputGainDb, -18.0f, 12.0f, "%.1f dB");
            ImGui::EndTable();
        }
    }

    if (changed)
        m_soundLab->setProfile(profile);
}

void HeritageStudioApp::drawAudioPerspectivePanel()
{
    const auto status = m_soundLab->status();
    ImGui::TextUnformatted("AUDITION / PROFILE");
    ImGui::Separator();
    ImGui::TextWrapped("The same raw capture is transformed into different listening perspectives. This is where the Engine Simulator source starts becoming a physical car.");

    const auto play = [&](const char* name, audio::lab::EngineSoundPerspective perspective)
    {
        if (ImGui::Button(name, ImVec2(-1.0f, 38.0f)))
            m_soundLab->playPreview(perspective);
    };
    play("RAW A/B", audio::lab::EngineSoundPerspective::Raw);
    play("ENGINE BAY", audio::lab::EngineSoundPerspective::EngineBay);
    play("REAR / EXHAUST", audio::lab::EngineSoundPerspective::RearExhaust);
    play("DRIVER CABIN", audio::lab::EngineSoundPerspective::DriverCabin);
    if (ImGui::Button("STOP PREVIEW", ImVec2(-1.0f, 30.0f)))
        m_soundLab->stopPreview();

    sectionTitle("ACOUSTIC PROFILE");
    ImGui::InputText("##ProfileName", m_profileName, sizeof(m_profileName));
    if (ImGui::Button("SAVE .HACOUSTIC", ImVec2(-1.0f, 34.0f)))
        m_soundLab->saveProfile(m_profileName);
    if (ImGui::Button("LOAD .HACOUSTIC", ImVec2(-1.0f, 34.0f)))
        m_soundLab->loadProfile(m_profileName);

    sectionTitle("STATUS");
    ImGui::TextWrapped("%s", m_audioBackendMessage.c_str());
    ImGui::Text("Capture available: %s", status.available ? "YES" : "NO");
    ImGui::Text("Preview: %s", status.previewPlaying ? "PLAYING" : "STOPPED");
    ImGui::Text("Profile: %s", status.profileName.c_str());
    if (!status.lastError.empty())
    {
        ImGui::Spacing();
        ImGui::TextWrapped("Last message: %s", status.lastError.c_str());
    }

    sectionTitle("RUNTIME POLICY");
    ImGui::TextWrapped("Bake source character only when useful. Listener-dependent effects such as cabin transmission, window state, distance, Doppler, occlusion and environmental reverb remain runtime DSP with audio LOD for large grids.");
}

void HeritageStudioApp::drawStatusBar()
{
    ImGui::Text("Workspace: %s", workspaceName(m_workspace));
    ImGui::SameLine();
    ImGui::TextDisabled(" | Runtime scene: %s", m_runtimeScenePath.filename().string().c_str());
    ImGui::SameLine();
    ImGui::TextDisabled(" | Root: %s", m_repositoryRoot.string().c_str());
}

bool HeritageStudioApp::saveSceneAuthoring()
{
    std::string backupMessage;
    if (!m_authoring.saveScene(m_studioProjectRoot / "scene.hscene", backupMessage))
    {
        m_studioMessage = backupMessage;
        return false;
    }

    std::string runtimeMessage;
    if (!saveRuntimeVehicleSpawn(runtimeMessage))
    {
        m_studioMessage = runtimeMessage + " | Studio backup was still saved.";
        return false;
    }

    m_studioMessage = runtimeMessage + " | safety copy: UserData HeritageStudio scene.hscene";
    return true;
}

bool HeritageStudioApp::saveAllAuthoring()
{
    if (m_authoring.navigationBuild.enabled && m_authoring.navigationBuild.rebuildOnSave)
    {
        int createdNodes = 0, updatedNodes = 0, createdLinks = 0;
        m_authoring.compileRoadSplinesToLaneGraph(createdNodes, updatedNodes, createdLinks);
    }
    std::string backupMessage;
    if (!m_authoring.saveAll(m_studioProjectRoot, backupMessage))
    {
        m_studioMessage = backupMessage;
        return false;
    }

    std::string runtimeMessage;
    if (!saveRuntimeVehicleSpawn(runtimeMessage))
    {
        m_studioMessage = runtimeMessage + " | Studio authoring set was still saved.";
        return false;
    }

    std::string gameplayMessage;
    if (!saveRuntimeGameplay(gameplayMessage))
    {
        m_studioMessage = runtimeMessage + " | " + gameplayMessage + " | Studio authoring set was still saved.";
        return false;
    }

    m_studioMessage = runtimeMessage + " | " + gameplayMessage + " | all Studio authoring backups saved.";
    return true;
}

bool HeritageStudioApp::loadSceneAuthoring()
{
    bool loadedBackup = false;
    std::string backupMessage;
    const auto backupPath = m_studioProjectRoot / "scene.hscene";
    if (std::filesystem::exists(backupPath))
        loadedBackup = m_authoring.loadScene(backupPath, backupMessage);

    std::string runtimeMessage;
    const bool loadedRuntime = loadRuntimeVehicleSpawn(runtimeMessage);
    if (loadedRuntime)
        m_studioMessage = runtimeMessage + (loadedBackup ? " | loaded Studio scene backup too." : "");
    else if (loadedBackup)
        m_studioMessage = backupMessage + " | " + runtimeMessage;
    else
        m_studioMessage = runtimeMessage;
    return loadedBackup || loadedRuntime;
}

bool HeritageStudioApp::saveRuntimeVehicleSpawn(std::string& message) const
{
    const authoring::SceneObject* spawn = nullptr;
    for (const auto& object : m_authoring.sceneObjects)
    {
        if (object.enabled && object.type == authoring::SceneObjectType::VehicleSpawn)
        {
            spawn = &object;
            break;
        }
    }
    if (!spawn)
    {
        message = "No enabled Vehicle Spawn exists; runtime .hscene was left unchanged and GLB spawn fallback remains active.";
        return true;
    }

    if (m_runtimeScenePath.empty() || !std::filesystem::exists(m_runtimeScenePath))
    {
        message = "Runtime entry scene does not exist: " + m_runtimeScenePath.string();
        return false;
    }

    const auto backupDirectory = m_studioProjectRoot / "RuntimeBackups";
    std::error_code directoryError;
    std::filesystem::create_directories(backupDirectory, directoryError);
    if (directoryError)
    {
        message = "Could not create runtime-scene safety backup folder: " + directoryError.message();
        return false;
    }

    const auto runtimeBackup = backupDirectory /
        (m_runtimeScenePath.stem().string() + "_before_last_studio_save.hscene");
    std::error_code backupError;
    std::filesystem::copy_file(
        m_runtimeScenePath,
        runtimeBackup,
        std::filesystem::copy_options::overwrite_existing,
        backupError);
    if (backupError)
    {
        message = "Refusing to overwrite the runtime scene because its safety backup failed: " + backupError.message();
        return false;
    }

    if (!writeStudioRuntimeSpawn(m_runtimeScenePath, spawn->position, spawn->rotation, message))
        return false;

    message += " (" + spawn->name + ")";
    return true;
}

std::string HeritageStudioApp::validateAuthoring() const
{
    std::vector<std::string> issues;

    int vehicleSpawns = 0;
    for (const auto& object : m_authoring.sceneObjects)
        if (object.enabled && object.type == authoring::SceneObjectType::VehicleSpawn) ++vehicleSpawns;
    if (vehicleSpawns > 1) issues.push_back("multiple enabled Vehicle Spawn objects");

    int startFinishCount = 0;
    int gridCount = 0;
    for (const auto& marker : m_authoring.raceMarkers)
    {
        if (marker.type == authoring::RaceMarkerType::StartFinish) ++startFinishCount;
        if (marker.type == authoring::RaceMarkerType::GridSlot) ++gridCount;
        const bool isGate = marker.type == authoring::RaceMarkerType::StartFinish || marker.type == authoring::RaceMarkerType::Checkpoint
            || marker.type == authoring::RaceMarkerType::Sector || marker.type == authoring::RaceMarkerType::TimingLoop
            || marker.type == authoring::RaceMarkerType::SpeedTrapStart || marker.type == authoring::RaceMarkerType::SpeedTrapFinish
            || marker.type == authoring::RaceMarkerType::PitEntry || marker.type == authoring::RaceMarkerType::PitExit
            || marker.type == authoring::RaceMarkerType::PitSpeedLine || marker.type == authoring::RaceMarkerType::SafetyCarLine
            || marker.type == authoring::RaceMarkerType::FormationLine;
        if (isGate && marker.gateWidthM <= 0.0f) issues.push_back("timing gate '" + marker.name + "' has non-positive width");
        if (marker.layoutId != 0)
        {
            bool layoutFound = false;
            for (const auto& layout : m_authoring.raceLayouts) if (layout.id == marker.layoutId) { layoutFound = true; break; }
            if (!layoutFound) issues.push_back("race marker '" + marker.name + "' references a missing layout");
        }
    }
    bool traditionalEventNeedsStartFinish = false;
    for (const auto& event : m_authoring.gameEvents)
        if (event.enabled && event.type != authoring::GameEventType::Autoslalom && event.type != authoring::GameEventType::Gymkhana
            && event.type != authoring::GameEventType::Cruise && event.type != authoring::GameEventType::TestDrive) { traditionalEventNeedsStartFinish = true; break; }
    if (traditionalEventNeedsStartFinish && startFinishCount == 0) issues.push_back("traditional gameplay events exist but Race has no Start / Finish marker");
    if (m_authoring.raceLayouts.empty() && gridCount < m_authoring.race.gridSlots) issues.push_back("configured grid slots exceed authored Grid Slot markers");

    const auto markerExists = [&](std::uint32_t id)
    {
        if (id == 0) return false;
        for (const auto& marker : m_authoring.raceMarkers) if (marker.id == id) return true;
        return false;
    };
    const auto routeExists = [&](std::uint32_t id)
    {
        if (id == 0) return false;
        for (const auto& route : m_authoring.raceRoutes) if (route.id == id) return true;
        return false;
    };
    const auto layoutExists = [&](std::uint32_t id)
    {
        if (id == 0) return false;
        for (const auto& layout : m_authoring.raceLayouts) if (layout.id == id) return true;
        return false;
    };
    const auto markerById = [&](std::uint32_t id) -> const authoring::RaceMarker*
    {
        if (id == 0) return nullptr;
        for (const auto& marker : m_authoring.raceMarkers) if (marker.id == id) return &marker;
        return nullptr;
    };
    const auto markerAppliesToLayout = [&](const authoring::RaceMarker& marker, std::uint32_t layoutId)
    {
        return marker.layoutId == 0 || (layoutId != 0 && marker.layoutId == layoutId);
    };

    for (const auto& route : m_authoring.raceRoutes)
    {
        if (!route.enabled) continue;
        int nodeCount = 0;
        std::vector<int> orders;
        for (const auto& node : m_authoring.raceRouteNodes)
        {
            if (node.routeId != route.id) continue;
            ++nodeCount; orders.push_back(node.order);
            if (node.leftWidthM < 0.0f || node.rightWidthM < 0.0f)
                issues.push_back("route '" + route.name + "' has a negative track-limit corridor width");
        }
        if (nodeCount == 1) issues.push_back("route '" + route.name + "' has only one spline node");
        if (route.closedLoop && nodeCount > 0 && nodeCount < 3) issues.push_back("closed route '" + route.name + "' needs at least three spline nodes");
        std::sort(orders.begin(), orders.end());
        for (std::size_t i = 1; i < orders.size(); ++i)
            if (orders[i] == orders[i - 1]) { issues.push_back("route '" + route.name + "' has duplicate node order values"); break; }
    }
    for (const auto& node : m_authoring.raceRouteNodes)
        if (!routeExists(node.routeId)) issues.push_back("route node " + std::to_string(node.id) + " references a missing route");

    for (const auto& layout : m_authoring.raceLayouts)
    {
        if (!layout.enabled) continue;
        if (!routeExists(layout.routeId)) issues.push_back("layout '" + layout.name + "' has no valid race route");
        if (layout.pitsEnabled && layout.pitRouteId != 0 && !routeExists(layout.pitRouteId)) issues.push_back("layout '" + layout.name + "' references a missing pit route");
        if (layout.startFinishMarkerId != 0)
        {
            const auto* startFinish = markerById(layout.startFinishMarkerId);
            if (startFinish == nullptr) issues.push_back("layout '" + layout.name + "' references a missing Start / Finish marker");
            else
            {
                if (startFinish->type != authoring::RaceMarkerType::StartFinish) issues.push_back("layout '" + layout.name + "' start/finish reference is not a Start / Finish marker");
                if (!markerAppliesToLayout(*startFinish, layout.id)) issues.push_back("layout '" + layout.name + "' references a Start / Finish marker scoped to another layout");
            }
        }
        if (layout.reverse)
        {
            for (const auto& route : m_authoring.raceRoutes)
                if (route.id == layout.routeId && !route.reverseAllowed) issues.push_back("layout '" + layout.name + "' is reversed but its route does not allow reverse running");
        }
    }

    std::vector<int> sessionOrders;
    for (const auto& session : m_authoring.raceSessions)
    {
        if (!session.enabled) continue;
        sessionOrders.push_back(session.order);
        if (session.durationMinutes <= 0 && session.laps <= 0 && session.type != authoring::RaceSessionType::TestSession)
            issues.push_back("session '" + session.name + "' has neither duration nor lap count");
        if (session.startingFuelPercent < 0.0f || session.startingFuelPercent > 100.0f)
            issues.push_back("session '" + session.name + "' has invalid starting fuel percentage");
        if (session.maximumStintMinutes < 0) issues.push_back("session '" + session.name + "' has a negative maximum stint time");
        if (session.minimumPitServiceSeconds < 0.0f) issues.push_back("session '" + session.name + "' has a negative minimum pit-service time");
        if (session.classificationPercent < 0.0f || session.classificationPercent > 100.0f) issues.push_back("session '" + session.name + "' has an invalid classification percentage");
        if (session.mandatoryTireChange && !session.tireChangesAllowed) issues.push_back("session '" + session.name + "' requires a tire change while tire changes are disabled");
        if (session.gridSource == authoring::SessionGridSource::ReverseTopN && session.reverseTopN < 1) issues.push_back("session '" + session.name + "' uses Reverse top N without a positive reversal count");
    }
    std::sort(sessionOrders.begin(), sessionOrders.end());
    for (std::size_t i = 1; i < sessionOrders.size(); ++i)
        if (sessionOrders[i] == sessionOrders[i - 1]) { issues.push_back("enabled race sessions have duplicate order values"); break; }

    if (m_authoring.raceControl.safetyCarRouteId != 0 && !routeExists(m_authoring.raceControl.safetyCarRouteId))
        issues.push_back("race control references a missing Safety Car route");
    if (m_authoring.raceControl.restartMarkerId != 0 && !markerExists(m_authoring.raceControl.restartMarkerId))
        issues.push_back("race control references a missing restart line");
    if (m_authoring.raceControl.pitWindowEndLap > 0 && m_authoring.raceControl.pitWindowEndLap < m_authoring.raceControl.pitWindowStartLap)
        issues.push_back("race-control pit window ends before it starts");
    for (const auto& point : m_authoring.raceSupportPoints)
        if (point.enabled && point.serviceRadiusM <= 0.0f) issues.push_back("race-support point '" + point.name + "' has non-positive service radius");

    for (const auto& event : m_authoring.gameEvents)
    {
        if (!event.enabled) continue;
        const bool coneCourse = event.type == authoring::GameEventType::Autoslalom || event.type == authoring::GameEventType::Gymkhana;
        const bool needsFinish = !coneCourse && event.type != authoring::GameEventType::Cruise && event.type != authoring::GameEventType::TestDrive;
        const bool circuit = event.type == authoring::GameEventType::CircuitRace || event.type == authoring::GameEventType::ClandestineCircuit;
        if (circuit && event.layoutId == 0) issues.push_back("circuit event '" + event.name + "' has no venue layout");
        if (event.layoutId != 0 && !layoutExists(event.layoutId)) issues.push_back("event '" + event.name + "' references a missing venue layout");

        const auto* startMarker = markerById(event.startMarkerId);
        const auto* finishMarker = markerById(event.finishMarkerId);
        bool hasStartCone = false;
        if (coneCourse) for (const auto& cone : m_authoring.courseCones)
            if (cone.enabled && cone.role == authoring::ConeRole::Start && (cone.eventId == 0 || cone.eventId == event.id)) { hasStartCone = true; break; }
        if (startMarker == nullptr && !hasStartCone && event.type != authoring::GameEventType::Cruise && event.type != authoring::GameEventType::TestDrive)
            issues.push_back("event '" + event.name + "' has no valid start marker" + std::string(coneCourse ? " or Start cone" : ""));
        else if (startMarker != nullptr && !markerAppliesToLayout(*startMarker, event.layoutId)) issues.push_back("event '" + event.name + "' start marker is scoped to another layout");
        if (needsFinish && finishMarker == nullptr) issues.push_back("event '" + event.name + "' has no valid finish marker");
        else if (finishMarker != nullptr && !markerAppliesToLayout(*finishMarker, event.layoutId)) issues.push_back("event '" + event.name + "' finish marker is scoped to another layout");
        if (coneCourse)
        {
            int gateCount = 0; bool hasFinishGate = false; std::vector<int> coneOrders;
            for (const auto& gate : m_authoring.coneCourseGates) if (gate.enabled && gate.eventId == event.id)
            {
                ++gateCount; coneOrders.push_back(gate.order); if (gate.type == authoring::ConeCourseGateType::Finish) hasFinishGate = true;
            }
            if (gateCount == 0) issues.push_back("cone-course event '" + event.name + "' has no invisible course elements");
            if (!hasFinishGate) issues.push_back("cone-course event '" + event.name + "' has no Finish course element");
            std::sort(coneOrders.begin(), coneOrders.end());
            for (std::size_t i=1;i<coneOrders.size();++i) if (coneOrders[i]==coneOrders[i-1]) { issues.push_back("cone-course event '" + event.name + "' has duplicate element order values"); break; }
        }

        std::vector<int> effectiveTimingOrders;
        int effectiveGridSlots = 0;
        for (const auto& marker : m_authoring.raceMarkers)
        {
            if (!markerAppliesToLayout(marker, event.layoutId)) continue;
            if (marker.type == authoring::RaceMarkerType::GridSlot) ++effectiveGridSlots;
            if (marker.id != event.startMarkerId && marker.id != event.finishMarkerId
                && (marker.type == authoring::RaceMarkerType::Checkpoint || marker.type == authoring::RaceMarkerType::Sector || marker.type == authoring::RaceMarkerType::TimingLoop))
                effectiveTimingOrders.push_back(marker.order);
        }
        std::sort(effectiveTimingOrders.begin(), effectiveTimingOrders.end());
        for (std::size_t i = 1; i < effectiveTimingOrders.size(); ++i)
            if (effectiveTimingOrders[i] == effectiveTimingOrders[i - 1])
            {
                issues.push_back("event '" + event.name + "' has duplicate effective checkpoint/sector/timing-loop order values for its layout");
                break;
            }
        if (circuit)
        {
            const int requiredGridSlots = std::min(m_authoring.race.gridSlots, event.maxEntrants);
            if (effectiveGridSlots < requiredGridSlots) issues.push_back("event '" + event.name + "' layout has fewer Grid Slot markers than its configured starting grid");
        }
        if (event.maxEntrants < 1) issues.push_back("event '" + event.name + "' has no entrants");
    }

    const auto eventExists = [&](std::uint32_t id)
    {
        for (const auto& event : m_authoring.gameEvents) if (event.id == id) return true;
        return false;
    };
    const auto motorsportClassExists = [&](std::uint32_t id)
    {
        for (const auto& cls : m_authoring.motorsportClasses) if (cls.id == id) return true;
        return false;
    };
    const auto championshipExists = [&](std::uint32_t id)
    {
        for (const auto& championship : m_authoring.motorsportChampionships) if (championship.id == id) return true;
        return false;
    };
    for (const auto& cls : m_authoring.motorsportClasses)
    {
        if (!cls.enabled) continue;
        if (cls.code.empty()) issues.push_back("motorsport class '" + cls.name + "' has no class code");
        if (cls.maximumEntrants < 1) issues.push_back("motorsport class '" + cls.name + "' has no entrant capacity");
        if (cls.minimumPowerKw > cls.maximumPowerKw) issues.push_back("motorsport class '" + cls.name + "' has minimum power above maximum power");
        if (cls.minimumWeightKg > cls.maximumWeightKg) issues.push_back("motorsport class '" + cls.name + "' has minimum weight above maximum weight");
    }
    for (const auto& entrant : m_authoring.motorsportEntrants)
    {
        if (!entrant.enabled) continue;
        if (entrant.driverName.empty()) issues.push_back("an enabled motorsport entrant has no driver name");
        if (entrant.raceNumber < 0) issues.push_back("entrant '" + entrant.driverName + "' has a negative race number");
        if (entrant.classId == 0 || !motorsportClassExists(entrant.classId)) issues.push_back("entrant '" + entrant.driverName + "' references a missing competition class");
        if (entrant.eventId != 0 && !eventExists(entrant.eventId)) issues.push_back("entrant '" + entrant.driverName + "' references a missing event");
        if (entrant.aiSkill < 0.0f || entrant.aiSkill > 1.0f || entrant.qualifyingPace < 0.0f || entrant.qualifyingPace > 1.0f || entrant.racePace < 0.0f || entrant.racePace > 1.0f) issues.push_back("entrant '" + entrant.driverName + "' has an AI pace/skill value outside 0..1");
    }
    for (const auto& championship : m_authoring.motorsportChampionships)
    {
        if (!championship.enabled) continue;
        if (championship.classId != 0 && !motorsportClassExists(championship.classId)) issues.push_back("championship '" + championship.name + "' references a missing competition class");
        if (championship.pointsScheme.empty()) issues.push_back("championship '" + championship.name + "' has no points scheme");
        if (championship.poleBonus < 0.0f || championship.fastestLapBonus < 0.0f || championship.dropWorstRounds < 0) issues.push_back("championship '" + championship.name + "' has invalid bonus/drop-round rules");
        std::vector<int> orders;
        for (const auto& round : m_authoring.motorsportRounds) if (round.enabled && round.championshipId == championship.id) orders.push_back(round.order);
        std::sort(orders.begin(), orders.end());
        for (std::size_t i=1;i<orders.size();++i) if (orders[i] == orders[i-1]) { issues.push_back("championship '" + championship.name + "' has duplicate calendar order values"); break; }
    }
    for (const auto& round : m_authoring.motorsportRounds)
    {
        if (!round.enabled) continue;
        if (!championshipExists(round.championshipId)) issues.push_back("championship round '" + round.name + "' references a missing championship");
        if (!eventExists(round.eventId)) issues.push_back("championship round '" + round.name + "' references a missing event");
        if (round.pointsMultiplier <= 0.0f) issues.push_back("championship round '" + round.name + "' has a non-positive points multiplier");
    }
    if (m_authoring.motorsport.maxPhysicalCompetitors < 0 || m_authoring.motorsport.maxPhysicalCompetitors > 200) issues.push_back("motorsport physical-competitor budget is outside 0..200");
    if (m_authoring.motorsport.defaultAiSkill < 0.0f || m_authoring.motorsport.defaultAiSkill > 1.0f) issues.push_back("motorsport default AI skill is outside 0..1");
    if (m_authoring.motorsport.baseMechanicalDnfChancePerHour < 0.0f || m_authoring.motorsport.baseMechanicalDnfChancePerHour > 1.0f) issues.push_back("motorsport mechanical-DNF chance is outside 0..1 per hour");

    const auto& racingAi = m_authoring.motorsportAi;
    if (racingAi.updateHz < 1.0f || racingAi.updateHz > 240.0f) issues.push_back("Racing AI decision update rate is outside 1..240 Hz");
    if (racingAi.lookaheadMinimumM <= 0.0f || racingAi.lookaheadMaximumM < racingAi.lookaheadMinimumM) issues.push_back("Racing AI lookahead distances are invalid or reversed");
    if (racingAi.brakingLookaheadM < racingAi.lookaheadMinimumM) issues.push_back("Racing AI braking lookahead is shorter than its minimum steering lookahead");
    if (racingAi.slipstreamMinimumGapM < 0.0f || racingAi.slipstreamMaximumGapM <= racingAi.slipstreamMinimumGapM) issues.push_back("Racing AI slipstream gap range is invalid");
    if (racingAi.wetLineThreshold < 0.0f || racingAi.wetLineThreshold > 1.0f || racingAi.maximumWetSpeedPenalty < 0.0f || racingAi.maximumWetSpeedPenalty > 1.0f) issues.push_back("Racing AI wet-line/speed policy is outside 0..1");
    if (racingAi.fuelUseLitersPer100Km < 0.0f || racingAi.tireWearPer100Km < 0.0f || racingAi.fuelReserveLaps < 0.0f) issues.push_back("Racing AI fuel/tire strategy contains a negative value");
    if (racingAi.tirePitThreshold < 0.0f || racingAi.tirePitThreshold > 1.0f) issues.push_back("Racing AI tire pit threshold is outside 0..1");
    if (racingAi.physicsHighRateHz < 60.0f || racingAi.physicsHighRateHz > 4000.0f) issues.push_back("STUDIO21 Racing AI high-rate vehicle physics is outside 60..4000 Hz");
    if (racingAi.steeringLookaheadSeconds <= 0.0f || racingAi.steeringGain <= 0.0f || racingAi.maximumSteerAngleDeg <= 0.0f) issues.push_back("STUDIO21 Racing AI steering control contains a non-positive value");
    if (racingAi.sideBySideSafetyM < 0.0f || racingAi.trackLimitSafetyM < 0.0f || racingAi.gripSlipRatioLimit <= 0.0f || racingAi.gripSlipAngleDeg <= 0.0f) issues.push_back("STUDIO21 Racing AI spatial/grip safety configuration is invalid");
    if (racingAi.damageDnfThreshold < 0.0f || racingAi.damageDnfThreshold > racingAi.damagePitThreshold || racingAi.damagePitThreshold > 1.0f || racingAi.collisionDamageScale < 0.0f) issues.push_back("STUDIO21 Racing AI damage strategy thresholds are invalid");
    if (racingAi.collisionEnvelopeMarginM < 0.0f || racingAi.sweptEnvelopeSeconds <= 0.0f || racingAi.sideBySideOverlapToleranceM < 0.0f) issues.push_back("STUDIO22 Racing AI collider/swept-envelope configuration is invalid");
    if (racingAi.maximumDefensiveMovesPerStraight < 0 || racingAi.blockingPenaltySeconds < 0.0f || racingAi.unsafeReleasePenaltySeconds < 0.0f || racingAi.pitReleaseLookaheadM <= 0.0f) issues.push_back("STUDIO22 Racing AI stewarding/pit-release policy is invalid");
    if (racingAi.tireOptimalMaximumC <= racingAi.tireOptimalMinimumC || racingAi.fuelDensityKgPerLiter <= 0.0f || racingAi.multiclassPassHorizonS <= 0.0f) issues.push_back("STUDIO22 Racing AI thermal/fuel/multiclass strategy configuration is invalid");
    if (racingAi.incidentMinimumNormalImpulseNs < 0.0f || racingAi.incidentMinimumClosingKmh < 0.0f || racingAi.severeIncidentNormalImpulseNs < racingAi.incidentMinimumNormalImpulseNs || racingAi.severeIncidentClosingKmh < racingAi.incidentMinimumClosingKmh) issues.push_back("STUDIO23 Racing AI incident evidence thresholds are invalid or reversed");
    if (racingAi.avoidableContactPenaltySeconds < 0.0f || racingAi.severeContactPenaltySeconds < racingAi.avoidableContactPenaltySeconds || racingAi.contactEvidenceCooldownSeconds <= 0.0f || racingAi.retainedIncidentEvidence < 1 || racingAi.retainedIncidentEvidence > 256) issues.push_back("STUDIO23 Racing AI incident stewarding/retention policy is invalid");
    const auto& replay = m_authoring.motorsportReplay;
    if (replay.sampleHz < 1.0f || replay.sampleHz > 60.0f || replay.preRollSeconds < 0.5f || replay.preRollSeconds > 30.0f || replay.postRollSeconds < 0.5f || replay.postRollSeconds > 30.0f) issues.push_back("STUDIO24 incident replay sampling/pre-post window policy is invalid");
    if (replay.maximumIncidentClips < 1 || replay.maximumIncidentClips > 64 || replay.maximumRecordedCompetitors < 1 || replay.maximumRecordedCompetitors > 200 || replay.maximumGhostVehicles < 1 || replay.maximumGhostVehicles > 64) issues.push_back("STUDIO24 incident replay retention/participant/ghost budget is invalid");
    if (replay.incidentCameraDistanceM < 2.0f || replay.incidentCameraDistanceM > 80.0f || replay.incidentCameraHeightM < 0.5f || replay.incidentCameraHeightM > 40.0f || replay.tracksideCameraLeadM < 2.0f || replay.tracksideCameraLeadM > 150.0f || replay.helicopterCameraHeightM < 5.0f || replay.helicopterCameraHeightM > 200.0f || replay.cameraSmoothing < 0.0f || replay.cameraSmoothing > 30.0f) issues.push_back("STUDIO25 replay/broadcast camera policy is invalid");
    for (const auto& path : m_authoring.broadcastCameraPaths)
    {
        int nodeCount = 0; std::vector<int> orders;
        for (const auto& node : m_authoring.broadcastCameraNodes) if (node.pathId == path.id) { ++nodeCount; orders.push_back(node.order); }
        if (path.enabled && nodeCount < 2) issues.push_back("broadcast camera path '" + path.name + "' needs at least two control points");
        if (path.layoutId != 0 && !layoutExists(path.layoutId)) issues.push_back("broadcast camera path '" + path.name + "' references a missing venue layout");
        if (path.activationRadiusM <= 0.0f || path.durationSeconds <= 0.0f || path.easing < 0.0f || path.easing > 1.0f) issues.push_back("broadcast camera path '" + path.name + "' has invalid radius/duration/easing");
        std::sort(orders.begin(), orders.end());
        for (std::size_t i = 1; i < orders.size(); ++i) if (orders[i] == orders[i - 1]) { issues.push_back("broadcast camera path '" + path.name + "' has duplicate control-point order values"); break; }
    }
    for (const auto& node : m_authoring.broadcastCameraNodes)
    {
        bool found = false; for (const auto& path : m_authoring.broadcastCameraPaths) if (path.id == node.pathId) { found = true; break; }
        if (!found) issues.push_back("broadcast camera control point references a missing path");
    }
    for (const auto& entrant : m_authoring.motorsportEntrants)
    {
        if (!entrant.enabled) continue;
        if (entrant.racecraft < 0.0f || entrant.racecraft > 1.0f || entrant.awareness < 0.0f || entrant.awareness > 1.0f || entrant.defending < 0.0f || entrant.defending > 1.0f
            || entrant.tireManagement < 0.0f || entrant.tireManagement > 1.0f || entrant.fuelManagement < 0.0f || entrant.fuelManagement > 1.0f || entrant.strategyRisk < 0.0f || entrant.strategyRisk > 1.0f)
            issues.push_back("entrant '" + entrant.driverName + "' has a Racing AI personality value outside 0..1");
        if (entrant.mistakeRatePerHour < 0.0f || entrant.reactionTimeS <= 0.0f || entrant.preferredLineBias < -1.0f || entrant.preferredLineBias > 1.0f)
            issues.push_back("entrant '" + entrant.driverName + "' has invalid mistake/reaction/line-bias tuning");
    }

    const auto trafficNodeExists = [&](std::uint32_t id)
    {
        for (const auto& node : m_authoring.trafficNodes) if (node.id == id) return true;
        return false;
    };
    for (const auto& link : m_authoring.trafficLinks)
    {
        if (link.fromNodeId == link.toNodeId) issues.push_back("road link " + std::to_string(link.id) + " connects a node to itself");
        if (!trafficNodeExists(link.fromNodeId) || !trafficNodeExists(link.toNodeId)) issues.push_back("road link " + std::to_string(link.id) + " references a missing node");
        if (link.routeCostMultiplier <= 0.0f) issues.push_back("road link " + std::to_string(link.id) + " has a non-positive route cost");
    }
    const auto trafficLinkExists = [&](std::uint32_t id)
    {
        for (const auto& link : m_authoring.trafficLinks) if (link.id == id) return true;
        return false;
    };

    const auto roadExists = [&](std::uint32_t id)
    {
        for (const auto& road : m_authoring.roadSplines) if (road.id == id) return true;
        return false;
    };
    const auto coneExists = [&](std::uint32_t id)
    {
        for (const auto& cone : m_authoring.courseCones) if (cone.id == id) return true;
        return false;
    };
    for (const auto& cone : m_authoring.courseCones)
    {
        if (!cone.enabled) continue;
        if (cone.eventId != 0 && !eventExists(cone.eventId)) issues.push_back("course cone '" + cone.name + "' references a missing gameplay event");
        if (cone.baseRadiusM <= 0.0f || cone.heightM <= 0.0f || cone.visualScale <= 0.0f) issues.push_back("course cone '" + cone.name + "' has invalid physical/visual dimensions");
        if (cone.physical && cone.massKg <= 0.0f) issues.push_back("physical course cone '" + cone.name + "' has non-positive mass");
        if (cone.displacementToleranceM < 0.0f || cone.hitPenaltySeconds < 0.0f) issues.push_back("course cone '" + cone.name + "' has invalid penalty thresholds");
        if (cone.roadId != 0 && !roadExists(cone.roadId)) issues.push_back("traffic-control cone '" + cone.name + "' references a missing road");
        if (cone.linkId != 0 && !trafficLinkExists(cone.linkId)) issues.push_back("traffic-control cone '" + cone.name + "' references a missing traffic link");
        if (cone.trafficMode != authoring::ConeTrafficMode::None && cone.roadId == 0 && cone.linkId == 0)
            issues.push_back("traffic-control cone '" + cone.name + "' has no target road/link (visual cone is valid, but routing control would do nothing)");
    }
    for (const auto& gate : m_authoring.coneCourseGates)
    {
        if (!gate.enabled) continue;
        if (gate.eventId == 0 || !eventExists(gate.eventId)) issues.push_back("cone-course element '" + gate.name + "' needs a valid gameplay event");
        if (gate.widthM <= 0.0f || gate.lengthM <= 0.0f) issues.push_back("cone-course element '" + gate.name + "' has invalid dimensions");
        if (gate.leftConeId != 0 && !coneExists(gate.leftConeId)) issues.push_back("cone-course element '" + gate.name + "' references a missing left/reference cone");
        if (gate.rightConeId != 0 && !coneExists(gate.rightConeId)) issues.push_back("cone-course element '" + gate.name + "' references a missing right cone");
        if (gate.wrongElementPenaltySeconds < 0.0f || gate.stopSpeedKmh < 0.0f || gate.stopDwellS < 0.0f) issues.push_back("cone-course element '" + gate.name + "' has invalid rule thresholds");
    }
    if (m_authoring.coneCourse.minimumContactImpulseNs < 0.0f || m_authoring.coneCourse.defaultHitPenaltySeconds < 0.0f || m_authoring.coneCourse.defaultDisplacementToleranceM < 0.0f || m_authoring.coneCourse.wrongElementPenaltySeconds < 0.0f)
        issues.push_back("STUDIO28 cone-course global penalty/contact policy is invalid");
    const auto intersectionExists = [&](std::uint32_t id)
    {
        for (const auto& junction : m_authoring.roadIntersections) if (junction.id == id) return true;
        return false;
    };
    for (const auto& road : m_authoring.roadSplines)
    {
        if (!road.enabled) continue;
        int nodeCount = 0; std::vector<int> orders;
        for (const auto& node : m_authoring.roadSplineNodes) if (node.roadId == road.id) { ++nodeCount; orders.push_back(node.order); if (node.widthScale <= 0.0f) issues.push_back("road '" + road.name + "' has a non-positive width scale"); }
        if (nodeCount == 1) issues.push_back("road '" + road.name + "' has only one spline node");
        if (road.lanesForward < 1) issues.push_back("road '" + road.name + "' has no forward lane");
        if (!road.oneWay && road.lanesBackward < 1) issues.push_back("two-way road '" + road.name + "' has no backward lane");
        if (road.laneWidthM <= 0.0f) issues.push_back("road '" + road.name + "' has invalid lane width");
        std::sort(orders.begin(), orders.end()); for (std::size_t i=1;i<orders.size();++i) if (orders[i]==orders[i-1]) { issues.push_back("road '" + road.name + "' has duplicate spline-node order values"); break; }
    }
    for (const auto& node : m_authoring.roadSplineNodes) if (!roadExists(node.roadId)) issues.push_back("road spline node " + std::to_string(node.id) + " references a missing road");
    for (const auto& junction : m_authoring.roadIntersections)
    {
        if (junction.radiusM <= 0.0f) issues.push_back("intersection '" + junction.name + "' has invalid control radius");
        if (junction.trafficLights && junction.priority != authoring::JunctionPriority::Signalized) issues.push_back("intersection '" + junction.name + "' has traffic lights but is not Signalized");
    }
    for (const auto& connector : m_authoring.turnConnectors)
    {
        if (!intersectionExists(connector.intersectionId)) issues.push_back("turn connector " + std::to_string(connector.id) + " references a missing intersection");
        if (!roadExists(connector.fromRoadId) || !roadExists(connector.toRoadId)) issues.push_back("turn connector " + std::to_string(connector.id) + " references a missing road");
        if (!connector.uTurn && connector.fromRoadId == connector.toRoadId) issues.push_back("turn connector " + std::to_string(connector.id) + " loops to the same road without U-turn permission");
        if (connector.conflictGroup < 0) issues.push_back("turn connector " + std::to_string(connector.id) + " has a negative conflict group");
        if (connector.reservationSeconds <= 0.0f) issues.push_back("turn connector " + std::to_string(connector.id) + " has a non-positive reservation time");
    }
    for (const auto& phase : m_authoring.trafficSignalPhases)
    {
        if (!intersectionExists(phase.intersectionId)) issues.push_back("signal phase '" + phase.name + "' references a missing intersection");
        if (phase.greenSeconds <= 0.0f || phase.yellowSeconds < 0.0f || phase.allRedSeconds < 0.0f) issues.push_back("signal phase '" + phase.name + "' has invalid timing");
    }
    for (const auto& parking : m_authoring.parkingStrips)
    {
        if (parking.roadId != 0 && !roadExists(parking.roadId)) issues.push_back("parking strip '" + parking.name + "' references a missing road");
        if (parking.spaces < 1 || parking.spacingM <= 0.0f) issues.push_back("parking strip '" + parking.name + "' has invalid spacing/count");
    }
    if (m_authoring.trafficPopulation.maxActiveVehicles < 0) issues.push_back("traffic population has a negative vehicle cap");
    if (m_authoring.navigationBuild.minimumTurnRadiusM <= 0.0f) issues.push_back("navigation build has an invalid minimum turn radius");
    if (m_authoring.trafficRules.desiredTimeGapS <= 0.0f || m_authoring.trafficRules.minimumGapM < 0.0f) issues.push_back("traffic driving rules have invalid following gaps");
    if (m_authoring.trafficRules.laneChangeRouteCost < 1.0f || m_authoring.trafficRules.mergeRouteCost < 1.0f) issues.push_back("traffic maneuver route costs must be at least 1.0");
    if (m_authoring.trafficStreaming.fullSimulationRadiusM <= 0.0f || m_authoring.trafficStreaming.simplifiedSimulationRadiusM < m_authoring.trafficStreaming.fullSimulationRadiusM
        || m_authoring.trafficStreaming.dormantPersistenceRadiusM < m_authoring.trafficStreaming.simplifiedSimulationRadiusM) issues.push_back("traffic streaming radii are not ordered full <= simplified <= dormant");
    if (m_authoring.trafficStreaming.sectorSizeM <= 0.0f || m_authoring.trafficStreaming.maxSpawnsPerSecond < 1 || m_authoring.trafficStreaming.maxDespawnsPerSecond < 1) issues.push_back("traffic streaming sector/spawn limits are invalid");
    std::vector<std::uint32_t> controllerIds;
    for (const auto& control : m_authoring.intersectionControllers)
    {
        if (!intersectionExists(control.intersectionId)) issues.push_back("signal controller references a missing intersection");
        if (control.minimumGreenSeconds <= 0.0f || control.maximumGreenSeconds < control.minimumGreenSeconds) issues.push_back("signal controller has invalid green-time bounds");
        controllerIds.push_back(control.intersectionId);
    }
    std::sort(controllerIds.begin(), controllerIds.end()); for (std::size_t i=1;i<controllerIds.size();++i) if (controllerIds[i]==controllerIds[i-1]) { issues.push_back("an intersection has duplicate live signal controllers"); break; }
    for (const auto& restriction : m_authoring.roadRestrictions)
    {
        if (restriction.roadId != 0 && !roadExists(restriction.roadId)) issues.push_back("road restriction '" + restriction.name + "' references a missing road");
        if (restriction.linkId != 0 && !trafficLinkExists(restriction.linkId)) issues.push_back("road restriction '" + restriction.name + "' references a missing graph link");
        if (restriction.roadId == 0 && restriction.linkId == 0) issues.push_back("road restriction '" + restriction.name + "' has no road or graph-link target");
        if (restriction.startHour < 0.0f || restriction.startHour > 24.0f || restriction.endHour < 0.0f || restriction.endHour > 24.0f) issues.push_back("road restriction '" + restriction.name + "' has an invalid active-hour window");
        if (restriction.routeCostMultiplier <= 0.0f) issues.push_back("road restriction '" + restriction.name + "' has a non-positive route cost");
    }
    const auto& agentSim = m_authoring.trafficAgentSimulation;
    if (agentSim.maxFullPhysicsAgents < 0 || agentSim.routeLookaheadLinks < 2) issues.push_back("traffic-agent simulation has invalid physics-agent or route-lookahead limits");
    if (agentSim.fullSimulationHz <= 0.0f || agentSim.simplifiedSimulationHz <= 0.0f) issues.push_back("traffic-agent simulation has a non-positive update cadence");
    if (agentSim.spawnMinDistancePlayerM < 0.0f || agentSim.spawnMaxDistancePlayerM <= agentSim.spawnMinDistancePlayerM) issues.push_back("traffic-agent spawn distances must satisfy 0 <= minimum < maximum");
    if (agentSim.perceptionRangeM <= 0.0f || agentSim.minimumSpawnGapM <= 0.0f || agentSim.stuckTimeoutS <= 0.0f) issues.push_back("traffic-agent perception/spawn/stuck parameters must be positive");
    float enabledProfileWeight = 0.0f;
    for (const auto& profile : m_authoring.trafficAgentProfiles)
    {
        if (!profile.enabled) continue;
        enabledProfileWeight += std::max(0.0f, profile.spawnWeight);
        if (profile.spawnWeight < 0.0f) issues.push_back("traffic-agent profile '" + profile.name + "' has a negative spawn weight");
        if (profile.lengthM <= 0.0f || profile.widthM <= 0.0f) issues.push_back("traffic-agent profile '" + profile.name + "' has invalid vehicle dimensions");
        if (profile.maxSpeedFactor <= 0.0f || profile.accelerationFactor <= 0.0f || profile.brakingFactor <= 0.0f) issues.push_back("traffic-agent profile '" + profile.name + "' has invalid dynamics factors");
        if (profile.desiredTimeGapS <= 0.0f || profile.minimumGapM < 0.0f || profile.reactionTimeS < 0.0f) issues.push_back("traffic-agent profile '" + profile.name + "' has invalid following/reaction parameters");
        if (profile.laneChangeAggression < 0.0f || profile.laneChangeAggression > 1.0f || profile.courtesy < 0.0f || profile.courtesy > 1.0f
            || profile.speedCompliance < 0.0f || profile.speedCompliance > 1.0f || profile.illegalOvertakeChance < 0.0f || profile.illegalOvertakeChance > 1.0f
            || profile.parkingSkill < 0.0f || profile.parkingSkill > 1.0f) issues.push_back("traffic-agent profile '" + profile.name + "' has a behavior value outside 0..1");
    }
    if (agentSim.enabled && enabledProfileWeight <= 0.0f) issues.push_back("live traffic agents are enabled but there is no enabled profile with positive spawn weight");
    if (agentSim.trafficVehicleHighRateHz < 60.0f || agentSim.trafficVehicleHighRateHz > 2000.0f) issues.push_back("traffic full-vehicle high-rate physics frequency is outside 60..2000 Hz");
    for (const auto& portal : m_authoring.trafficSpawnPortals)
    {
        if (!portal.enabled) continue;
        if (!trafficNodeExists(portal.nodeId)) issues.push_back("traffic portal '" + portal.name + "' references a missing graph anchor node");
        if (portal.radiusM <= 0.0f || portal.spawnWeight < 0.0f || portal.maxConcurrentAgents < 0) issues.push_back("traffic portal '" + portal.name + "' has invalid radius/weight/capacity");
        if (portal.startHour < 0.0f || portal.startHour > 24.0f || portal.endHour < 0.0f || portal.endHour > 24.0f) issues.push_back("traffic portal '" + portal.name + "' has an invalid active-hour window");
        if (portal.minimumPlayerDistanceM < 0.0f || portal.maximumPlayerDistanceM <= portal.minimumPlayerDistanceM) issues.push_back("traffic portal '" + portal.name + "' has invalid player-distance limits");
    }
    for (const auto& region : m_authoring.trafficDensityRegions)
    {
        if (!region.enabled) continue;
        if (region.radiusM <= 0.0f || region.densityMultiplier < 0.0f || region.speedMultiplier <= 0.0f || region.parkingMultiplier < 0.0f) issues.push_back("traffic density region '" + region.name + "' has invalid radius/multipliers");
        if (region.startHour < 0.0f || region.startHour > 24.0f || region.endHour < 0.0f || region.endHour > 24.0f) issues.push_back("traffic density region '" + region.name + "' has an invalid active-hour window");
    }
    for (const auto& incident : m_authoring.trafficIncidents)
    {
        if (!incident.enabled) continue;
        if (incident.roadId != 0 && !roadExists(incident.roadId)) issues.push_back("traffic incident '" + incident.name + "' references a missing road");
        if (incident.linkId != 0 && !trafficLinkExists(incident.linkId)) issues.push_back("traffic incident '" + incident.name + "' references a missing graph link");
        if (incident.radiusM <= 0.0f || incident.severity < 0.0f || incident.severity > 1.0f || incident.blockedLaneFraction < 0.0f || incident.blockedLaneFraction > 1.0f) issues.push_back("traffic incident '" + incident.name + "' has invalid radius/severity/blockage");
        if (incident.routeCostMultiplier < 1.0f || incident.speedLimitKmh < 0.0f || incident.responseDelayS < 0.0f || incident.clearAfterS < 0.0f) issues.push_back("traffic incident '" + incident.name + "' has invalid operational timing/cost/speed");
    }
    const auto& trafficEnvironment = m_authoring.trafficEnvironment;
    if (trafficEnvironment.wetSpeedFactor <= 0.0f || trafficEnvironment.heavyRainSpeedFactor <= 0.0f || trafficEnvironment.snowSpeedFactor <= 0.0f || trafficEnvironment.iceSpeedFactor <= 0.0f || trafficEnvironment.nightSpeedFactor <= 0.0f || trafficEnvironment.poorVisibilitySpeedFactor <= 0.0f) issues.push_back("traffic weather speed factors must stay positive");
    if (trafficEnvironment.wetFollowingGapFactor < 1.0f || trafficEnvironment.wetBrakingFactor <= 0.0f) issues.push_back("traffic wet-weather following/braking factors are invalid");
    const auto& behavior = m_authoring.trafficBehavior;
    if (behavior.zipperAlternationWindowS <= 0.0f || behavior.mergeCourtesyGapS <= 0.0f || behavior.roundaboutEntryGapS <= 0.0f) issues.push_back("advanced traffic merge/roundabout timing must stay positive");
    if (behavior.stopDwellS < 0.0f || behavior.yieldCreepSpeedKmh < 0.0f || behavior.overtakeMinimumGainKmh < 0.0f || behavior.overtakeReturnGapM <= 0.0f) issues.push_back("advanced traffic priority/overtake parameters are invalid");
    if (behavior.recoveryRerouteSeconds <= 0.0f || behavior.recoveryTeleportSeconds <= behavior.recoveryRerouteSeconds || behavior.collisionDistanceM <= 0.0f) issues.push_back("traffic recovery/collision thresholds are invalid");
    if (behavior.emergencyIncidentLookaheadM <= 0.0f) issues.push_back("emergency incident response lookahead must be positive");
    if (m_authoring.trafficDebug.maxDetailedAgents < 1) issues.push_back("traffic debug detailed-agent cap must be positive");
    for (const auto& point : m_authoring.worldPoints)
        if (point.enabled && point.radiusM <= 0.0f) issues.push_back("world point '" + point.name + "' has a non-positive interaction radius");

    const auto& police = m_authoring.policeGameplay;
    if (police.maxHeatLevel < 1 || police.maxPursuitUnits < 0) issues.push_back("police gameplay has invalid heat/unit limits");
    if (police.civilianWitnessRadiusM < 0.0f || police.policeDetectionRadiusM < 0.0f || police.speedToleranceKmh < 0.0f) issues.push_back("police detection/witness/speed-tolerance values cannot be negative");
    if (police.heatDecayDelayS < 0.0f || police.heatDecayPerSecond < 0.0f || police.lostSightSeconds < 0.0f || police.searchDurationS < 0.0f || police.cooldownDurationS < 0.0f || police.bustHoldSeconds < 0.0f || police.backupDelayS < 0.0f) issues.push_back("police pursuit/search/cooldown timing cannot be negative");
    for (const auto& zone : m_authoring.policePatrolZones)
    {
        if (!zone.enabled) continue;
        if (zone.radiusM <= 0.0f || zone.patrolWeight < 0.0f || zone.maximumUnits < 0 || zone.responseMultiplier <= 0.0f || zone.speedToleranceKmh < 0.0f) issues.push_back("police patrol zone '" + zone.name + "' has invalid radius/weight/response values");
        if (zone.responsePortalId != 0) { bool found=false; for (const auto& portal : m_authoring.trafficSpawnPortals) if (portal.id==zone.responsePortalId) { found=true; break; } if(!found) issues.push_back("police patrol zone '" + zone.name + "' references a missing response traffic portal"); }
    }
    for (const auto& site : m_authoring.policeRoadblockSites)
    {
        if (!site.enabled) continue;
        if (site.nodeId != 0 && !trafficNodeExists(site.nodeId)) issues.push_back("police roadblock site '" + site.name + "' references a missing graph node");
        if (site.widthM <= 0.0f || site.minimumHeat < 0.0f || site.unitCount < 1 || site.selectionWeight < 0.0f) issues.push_back("police roadblock site '" + site.name + "' has invalid width/heat/unit/weight values");
    }
    for (const auto& zone : m_authoring.policeEscapeZones)
        if (zone.enabled && (zone.radiusM <= 0.0f || zone.searchTimeMultiplier < 0.0f || zone.heatDecayMultiplier < 0.0f)) issues.push_back("police escape zone '" + zone.name + "' has invalid radius/search/decay values");
    for (const auto& meet : m_authoring.clandestineMeets)
    {
        if (!meet.enabled) continue;
        if (meet.radiusM <= 0.0f || meet.maximumVehicles < 1 || meet.policeRisk < 0.0f || meet.policeRisk > 1.0f || meet.heatMultiplier < 0.0f) issues.push_back("clandestine meet '" + meet.name + "' has invalid radius/capacity/risk/heat values");
        if (meet.eventId != 0) { bool found=false; for (const auto& event : m_authoring.gameEvents) if (event.id==meet.eventId) { found=true; break; } if(!found) issues.push_back("clandestine meet '" + meet.name + "' references a missing event"); }
    }

    const auto& execution = m_authoring.eventExecution;
    if (execution.gridSettleSeconds < 0.0f || execution.countdownSeconds < 0.0f || execution.falseStartSpeedKmh < 0.0f || execution.gateDebounceSeconds <= 0.0f) issues.push_back("event execution staging/countdown/gate timing is invalid");
    if (execution.trackLimitGraceSeconds < 0.0f || execution.trackLimitRejoinSeconds < 0.0f) issues.push_back("event execution track-limit timing cannot be negative");
    if (execution.fullCourseYellowSpeedKmh <= 0.0f || execution.virtualSafetyCarSpeedKmh <= 0.0f || execution.safetyCarSpeedKmh <= 0.0f) issues.push_back("event execution flag speed targets must be positive");
    if (execution.practiceLoopEndGateWidthM <= 0.0f || execution.practiceLoopRestoreDelayS < 0.0f) issues.push_back("practice-loop gate width/restore delay is invalid");

    if (issues.empty())
    {
        return "VALIDATION OK | " + std::to_string(m_authoring.raceMarkers.size()) + " race markers | "
            + std::to_string(m_authoring.raceRoutes.size()) + " routes / " + std::to_string(m_authoring.raceRouteNodes.size()) + " spline nodes | "
            + std::to_string(m_authoring.raceLayouts.size()) + " layouts | " + std::to_string(m_authoring.raceSessions.size()) + " sessions | "
            + std::to_string(m_authoring.broadcastCameraPaths.size()) + " TV camera paths / " + std::to_string(m_authoring.broadcastCameraNodes.size()) + " camera points | "
            + std::to_string(m_authoring.trafficNodes.size()) + " graph nodes / " + std::to_string(m_authoring.trafficLinks.size()) + " links | "
            + std::to_string(m_authoring.roadSplines.size()) + " road splines / " + std::to_string(m_authoring.roadSplineNodes.size()) + " control nodes | "
            + std::to_string(m_authoring.roadIntersections.size()) + " junctions / " + std::to_string(m_authoring.turnConnectors.size()) + " turns / " + std::to_string(m_authoring.roadRestrictions.size()) + " restrictions | "
            + std::to_string(m_authoring.trafficAgentProfiles.size()) + " traffic-agent profiles | " + std::to_string(m_authoring.trafficSpawnPortals.size()) + " portals / "
            + std::to_string(m_authoring.trafficDensityRegions.size()) + " density regions / " + std::to_string(m_authoring.trafficIncidents.size()) + " incidents | "
            + std::to_string(m_authoring.gameEvents.size()) + " events | " + std::to_string(m_authoring.worldPoints.size()) + " world points";
    }

    std::string result = "VALIDATION " + std::to_string(issues.size()) + " ISSUE(S): ";
    const std::size_t shown = std::min<std::size_t>(issues.size(), 3);
    for (std::size_t i = 0; i < shown; ++i)
    {
        if (i > 0) result += " | "; result += issues[i];
    }
    if (issues.size() > shown) result += " | +" + std::to_string(issues.size() - shown) + " more";
    return result;
}

bool HeritageStudioApp::saveRuntimeGameplay(std::string& message) const
{
    const auto generatedDir = m_moduleRoot / "Scripts" / "Generated";
    const auto generatedFile = generatedDir / "StudioGameplay.lua";
    std::error_code ec;
    std::filesystem::create_directories(generatedDir, ec);
    if (ec) { message = "Could not create runtime gameplay output folder: " + ec.message(); return false; }

    const auto backupDir = m_studioProjectRoot / "RuntimeBackups";
    std::filesystem::create_directories(backupDir, ec);
    if (ec) { message = "Could not create gameplay runtime backup folder: " + ec.message(); return false; }
    if (std::filesystem::exists(generatedFile))
    {
        const auto backup = backupDir / "StudioGameplay_before_last_publish.lua";
        std::filesystem::copy_file(generatedFile, backup, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) { message = "Refusing gameplay publish because the previous generated Lua backup failed: " + ec.message(); return false; }
    }

    const auto temp = generatedFile.string() + ".tmp";
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out) { message = "Could not write generated runtime gameplay: " + temp; return false; }

    out << "-- AUTO-GENERATED BY HERITAGE STUDIO. Edit the source authoring assets instead.\n";
    out << "StudioGameplay = {\n";
    out << "  version = 18,\n";
    out << "  race = { laps = " << m_authoring.race.laps
        << ", gridSlots = " << m_authoring.race.gridSlots
        << ", pitSpeedKmh = " << m_authoring.race.pitSpeedKmh
        << ", formationLap = " << (m_authoring.race.formationLap ? "true" : "false")
        << ", standingStart = " << (m_authoring.race.standingStart ? "true" : "false")
        << ", falseStartPenalty = " << (m_authoring.race.falseStartPenalty ? "true" : "false")
        << ", trackLimitsEnabled = " << (m_authoring.race.trackLimitsEnabled ? "true" : "false")
        << ", penaltiesEnabled = " << (m_authoring.race.penaltiesEnabled ? "true" : "false")
        << ", gridTemplate = " << luaQuote(authoring::gridTemplateName(m_authoring.race.gridTemplate))
        << ", gridRowSpacingM = " << m_authoring.race.gridRowSpacingM
        << ", gridLateralSpacingM = " << m_authoring.race.gridLateralSpacingM
        << ", gridBackOffsetM = " << m_authoring.race.gridBackOffsetM << " },\n";

    out << "  raceMarkers = {\n";
    for (const auto& marker : m_authoring.raceMarkers)
    {
        out << "    { id=" << marker.id << ", name=" << luaQuote(marker.name)
            << ", type=" << luaQuote(authoring::raceMarkerTypeName(marker.type))
            << ", x=" << marker.position.x << ", y=" << marker.position.y << ", z=" << marker.position.z
            << ", headingDeg=" << marker.headingDeg << ", radiusM=" << marker.radiusM
            << ", gateWidthM=" << marker.gateWidthM << ", gateHeightM=" << marker.gateHeightM
            << ", directionRequired=" << (marker.directionRequired ? "true" : "false")
            << ", layoutId=" << marker.layoutId
            << ", order=" << marker.order << ", slot=" << marker.slot << ", speedLimitKmh=" << marker.speedLimitKmh << " },\n";
    }
    out << "  },\n";

    out << "  broadcastCameraPaths = {\n";
    for (const auto& path : m_authoring.broadcastCameraPaths)
    {
        out << "    { id=" << path.id << ", name=" << luaQuote(path.name)
            << ", type=" << luaQuote(authoring::broadcastCameraPathTypeName(path.type))
            << ", enabled=" << (path.enabled ? "true" : "false") << ", layoutId=" << path.layoutId
            << ", activationRadiusM=" << path.activationRadiusM << ", durationSeconds=" << path.durationSeconds
            << ", easing=" << path.easing << ", reverse=" << (path.reverse ? "true" : "false") << " },\n";
    }
    out << "  },\n";

    out << "  broadcastCameraNodes = {\n";
    for (const auto& node : m_authoring.broadcastCameraNodes)
    {
        out << "    { id=" << node.id << ", pathId=" << node.pathId << ", order=" << node.order
            << ", x=" << node.position.x << ", y=" << node.position.y << ", z=" << node.position.z << " },\n";
    }
    out << "  },\n";

    const auto& coneConfig = m_authoring.coneCourse;
    out << "  coneCourse = { enabled=" << (coneConfig.enabled ? "true" : "false")
        << ", defaultAssetPath=" << luaQuote(coneConfig.defaultAssetPath)
        << ", minimumContactImpulseNs=" << coneConfig.minimumContactImpulseNs
        << ", defaultHitPenaltySeconds=" << coneConfig.defaultHitPenaltySeconds
        << ", defaultDisplacementToleranceM=" << coneConfig.defaultDisplacementToleranceM
        << ", wrongElementPenaltySeconds=" << coneConfig.wrongElementPenaltySeconds
        << ", missedElementDnf=" << (coneConfig.missedElementDnf ? "true" : "false")
        << ", resetEventConesOnStart=" << (coneConfig.resetEventConesOnStart ? "true" : "false")
        << ", recordConeHitsToReplay=" << (coneConfig.recordConeHitsToReplay ? "true" : "false")
        << ", eventConesVisibleOnlyWhileActive=" << (coneConfig.eventConesVisibleOnlyWhileActive ? "true" : "false") << " },\n";

    out << "  courseCones = {\n";
    for (const auto& cone : m_authoring.courseCones)
    {
        out << "    { id=" << cone.id << ", name=" << luaQuote(cone.name) << ", enabled=" << (cone.enabled ? "true" : "false")
            << ", role=" << luaQuote(authoring::coneRoleName(cone.role)) << ", eventId=" << cone.eventId
            << ", x=" << cone.position.x << ", y=" << cone.position.y << ", z=" << cone.position.z << ", headingDeg=" << cone.headingDeg
            << ", assetPath=" << luaQuote(cone.assetPath) << ", visualScale=" << cone.visualScale << ", baseRadiusM=" << cone.baseRadiusM << ", heightM=" << cone.heightM
            << ", massKg=" << cone.massKg << ", friction=" << cone.friction << ", restitution=" << cone.restitution
            << ", physical=" << (cone.physical ? "true" : "false") << ", penaltyMode=" << luaQuote(authoring::conePenaltyModeName(cone.penaltyMode))
            << ", hitPenaltySeconds=" << cone.hitPenaltySeconds << ", displacementToleranceM=" << cone.displacementToleranceM
            << ", trafficMode=" << luaQuote(authoring::coneTrafficModeName(cone.trafficMode)) << ", roadId=" << cone.roadId << ", linkId=" << cone.linkId
            << ", laneIndex=" << cone.laneIndex << ", trafficSpeedLimitKmh=" << cone.trafficSpeedLimitKmh << ", routeCostMultiplier=" << cone.routeCostMultiplier << " },\n";
    }
    out << "  },\n";

    out << "  coneCourseGates = {\n";
    for (const auto& gate : m_authoring.coneCourseGates)
    {
        out << "    { id=" << gate.id << ", name=" << luaQuote(gate.name) << ", enabled=" << (gate.enabled ? "true" : "false")
            << ", eventId=" << gate.eventId << ", order=" << gate.order << ", type=" << luaQuote(authoring::coneCourseGateTypeName(gate.type))
            << ", x=" << gate.position.x << ", y=" << gate.position.y << ", z=" << gate.position.z << ", headingDeg=" << gate.headingDeg
            << ", widthM=" << gate.widthM << ", lengthM=" << gate.lengthM << ", directionRequired=" << (gate.directionRequired ? "true" : "false")
            << ", sideClearanceM=" << gate.sideClearanceM << ", stopSpeedKmh=" << gate.stopSpeedKmh << ", stopDwellS=" << gate.stopDwellS
            << ", wrongElementPenaltySeconds=" << gate.wrongElementPenaltySeconds << ", dnfOnMiss=" << (gate.dnfOnMiss ? "true" : "false")
            << ", leftConeId=" << gate.leftConeId << ", rightConeId=" << gate.rightConeId << " },\n";
    }
    out << "  },\n";

    out << "  raceRoutes = {\n";
    for (const auto& route : m_authoring.raceRoutes)
    {
        out << "    { id=" << route.id << ", name=" << luaQuote(route.name) << ", type=" << luaQuote(authoring::raceRouteTypeName(route.type))
            << ", enabled=" << (route.enabled ? "true" : "false") << ", closedLoop=" << (route.closedLoop ? "true" : "false")
            << ", reverseAllowed=" << (route.reverseAllowed ? "true" : "false")
            << ", defaultLeftWidthM=" << route.defaultLeftWidthM << ", defaultRightWidthM=" << route.defaultRightWidthM << " },\n";
    }
    out << "  },\n";

    out << "  raceRouteNodes = {\n";
    for (const auto& node : m_authoring.raceRouteNodes)
    {
        out << "    { id=" << node.id << ", routeId=" << node.routeId << ", order=" << node.order
            << ", x=" << node.position.x << ", y=" << node.position.y << ", z=" << node.position.z
            << ", handleInX=" << node.handleIn.x << ", handleInY=" << node.handleIn.y << ", handleInZ=" << node.handleIn.z
            << ", handleOutX=" << node.handleOut.x << ", handleOutY=" << node.handleOut.y << ", handleOutZ=" << node.handleOut.z
            << ", automaticTangents=" << (node.automaticTangents ? "true" : "false")
            << ", leftWidthM=" << node.leftWidthM << ", rightWidthM=" << node.rightWidthM
            << ", targetSpeedKmh=" << node.targetSpeedKmh << ", bankingDeg=" << node.bankingDeg
            << ", overtakingPreferred=" << (node.overtakingPreferred ? "true" : "false") << " },\n";
    }
    out << "  },\n";

    out << "  raceLayouts = {\n";
    for (const auto& layout : m_authoring.raceLayouts)
    {
        out << "    { id=" << layout.id << ", name=" << luaQuote(layout.name) << ", enabled=" << (layout.enabled ? "true" : "false")
            << ", routeId=" << layout.routeId << ", pitRouteId=" << layout.pitRouteId << ", startFinishMarkerId=" << layout.startFinishMarkerId
            << ", defaultLaps=" << layout.defaultLaps << ", reverse=" << (layout.reverse ? "true" : "false")
            << ", pitsEnabled=" << (layout.pitsEnabled ? "true" : "false") << " },\n";
    }
    out << "  },\n";

    out << "  raceSessions = {\n";
    for (const auto& session : m_authoring.raceSessions)
    {
        out << "    { id=" << session.id << ", name=" << luaQuote(session.name) << ", type=" << luaQuote(authoring::raceSessionTypeName(session.type))
            << ", enabled=" << (session.enabled ? "true" : "false") << ", order=" << session.order
            << ", durationMinutes=" << session.durationMinutes << ", laps=" << session.laps << ", mandatoryPitStops=" << session.mandatoryPitStops
            << ", formationLap=" << (session.formationLap ? "true" : "false") << ", rollingStart=" << (session.rollingStart ? "true" : "false")
            << ", weatherChangeAllowed=" << (session.weatherChangeAllowed ? "true" : "false")
            << ", startingFuelPercent=" << session.startingFuelPercent
            << ", timedRace=" << (session.timedRace ? "true" : "false") << ", timePlusOneLap=" << (session.timePlusOneLap ? "true" : "false")
            << ", maximumStintMinutes=" << session.maximumStintMinutes << ", refuelingAllowed=" << (session.refuelingAllowed ? "true" : "false")
            << ", tireChangesAllowed=" << (session.tireChangesAllowed ? "true" : "false") << ", mandatoryTireChange=" << (session.mandatoryTireChange ? "true" : "false")
            << ", minimumPitServiceSeconds=" << session.minimumPitServiceSeconds << ", classificationPercent=" << session.classificationPercent
            << ", gridSource=" << luaQuote(authoring::sessionGridSourceName(session.gridSource)) << ", reverseTopN=" << session.reverseTopN << " },\n";
    }
    out << "  },\n";

    const auto& control = m_authoring.raceControl;
    out << "  raceControl = { localYellow=" << (control.localYellow ? "true" : "false")
        << ", fullCourseYellow=" << (control.fullCourseYellow ? "true" : "false")
        << ", virtualSafetyCar=" << (control.virtualSafetyCar ? "true" : "false")
        << ", safetyCar=" << (control.safetyCar ? "true" : "false")
        << ", redFlag=" << (control.redFlag ? "true" : "false")
        << ", blueFlags=" << (control.blueFlags ? "true" : "false")
        << ", pitLaneOpenDuringSafetyCar=" << (control.pitLaneOpenDuringSafetyCar ? "true" : "false")
        << ", maxTrackLimitWarnings=" << control.maxTrackLimitWarnings
        << ", driveThroughAfterWarnings=" << control.driveThroughAfterWarnings
        << ", pitWindowStartLap=" << control.pitWindowStartLap << ", pitWindowEndLap=" << control.pitWindowEndLap
        << ", safetyCarRouteId=" << control.safetyCarRouteId << ", restartMarkerId=" << control.restartMarkerId << " },\n";

    out << "  raceSupportPoints = {\n";
    for (const auto& point : m_authoring.raceSupportPoints)
    {
        out << "    { id=" << point.id << ", name=" << luaQuote(point.name) << ", type=" << luaQuote(authoring::raceSupportPointTypeName(point.type))
            << ", enabled=" << (point.enabled ? "true" : "false") << ", x=" << point.position.x << ", y=" << point.position.y << ", z=" << point.position.z
            << ", headingDeg=" << point.headingDeg << ", serviceRadiusM=" << point.serviceRadiusM << ", sector=" << point.sector << " },\n";
    }
    out << "  },\n";

    out << "  trafficNodes = {\n";
    for (const auto& node : m_authoring.trafficNodes)
    {
        out << "    { id=" << node.id << ", name=" << luaQuote(node.name) << ", type=" << luaQuote(authoring::trafficNodeTypeName(node.type))
            << ", x=" << node.position.x << ", y=" << node.position.y << ", z=" << node.position.z
            << ", headingDeg=" << node.headingDeg << ", speedLimitKmh=" << node.speedLimitKmh << ", lanes=" << node.lanes << ", priority=" << node.priority
            << ", bidirectional=" << (node.bidirectional ? "true" : "false") << ", overtakingAllowed=" << (node.overtakingAllowed ? "true" : "false")
            << ", density=" << node.density << ", roadId=" << node.roadId << ", laneIndex=" << node.laneIndex << ", laneDirection=" << node.laneDirection
            << ", generated=" << (node.generated ? "true" : "false") << " },\n";
    }
    out << "  },\n";

    out << "  trafficLinks = {\n";
    for (const auto& link : m_authoring.trafficLinks)
    {
        out << "    { id=" << link.id << ", fromNodeId=" << link.fromNodeId << ", toNodeId=" << link.toNodeId
            << ", lanes=" << link.lanes << ", speedLimitKmh=" << link.speedLimitKmh
            << ", bidirectional=" << (link.bidirectional ? "true" : "false")
            << ", overtakingAllowed=" << (link.overtakingAllowed ? "true" : "false") << ", density=" << link.density
            << ", type=" << luaQuote(authoring::trafficLinkTypeName(link.type)) << ", routeCostMultiplier=" << link.routeCostMultiplier
            << ", enabled=" << (link.enabled ? "true" : "false") << ", generated=" << (link.generated ? "true" : "false") << " },\n";
    }
    out << "  },\n";


    out << "  roadSplines = {\n";
    for (const auto& road : m_authoring.roadSplines)
    {
        out << "    { id=" << road.id << ", name=" << luaQuote(road.name) << ", class=" << luaQuote(authoring::roadClassName(road.roadClass))
            << ", enabled=" << (road.enabled ? "true" : "false") << ", oneWay=" << (road.oneWay ? "true" : "false")
            << ", lanesForward=" << road.lanesForward << ", lanesBackward=" << road.lanesBackward << ", laneWidthM=" << road.laneWidthM
            << ", shoulderLeftM=" << road.shoulderLeftM << ", shoulderRightM=" << road.shoulderRightM << ", medianWidthM=" << road.medianWidthM
            << ", speedLimitKmh=" << road.speedLimitKmh << ", sidewalkLeft=" << (road.sidewalkLeft ? "true" : "false")
            << ", sidewalkRight=" << (road.sidewalkRight ? "true" : "false") << ", parkingLeft=" << (road.parkingLeft ? "true" : "false")
            << ", parkingRight=" << (road.parkingRight ? "true" : "false") << ", trafficDensity=" << road.trafficDensity << ", spawnWeight=" << road.spawnWeight << " },\n";
    }
    out << "  },\n";
    out << "  roadSplineNodes = {\n";
    for (const auto& node : m_authoring.roadSplineNodes)
    {
        out << "    { id=" << node.id << ", roadId=" << node.roadId << ", order=" << node.order
            << ", x=" << node.position.x << ", y=" << node.position.y << ", z=" << node.position.z
            << ", handleInX=" << node.handleIn.x << ", handleInY=" << node.handleIn.y << ", handleInZ=" << node.handleIn.z
            << ", handleOutX=" << node.handleOut.x << ", handleOutY=" << node.handleOut.y << ", handleOutZ=" << node.handleOut.z
            << ", automaticTangents=" << (node.automaticTangents ? "true" : "false") << ", widthScale=" << node.widthScale << ", bankingDeg=" << node.bankingDeg << " },\n";
    }
    out << "  },\n";
    out << "  roadIntersections = {\n";
    for (const auto& junction : m_authoring.roadIntersections)
        out << "    { id=" << junction.id << ", name=" << luaQuote(junction.name) << ", x=" << junction.position.x << ", y=" << junction.position.y << ", z=" << junction.position.z
            << ", radiusM=" << junction.radiusM << ", priority=" << luaQuote(authoring::junctionPriorityName(junction.priority)) << ", trafficLights=" << (junction.trafficLights ? "true" : "false")
            << ", pedestrianCrossing=" << (junction.pedestrianCrossing ? "true" : "false") << ", approachSpeedKmh=" << junction.approachSpeedKmh << " },\n";
    out << "  },\n";
    out << "  turnConnectors = {\n";
    for (const auto& connector : m_authoring.turnConnectors)
        out << "    { id=" << connector.id << ", intersectionId=" << connector.intersectionId << ", fromRoadId=" << connector.fromRoadId << ", toRoadId=" << connector.toRoadId
            << ", fromLane=" << connector.fromLane << ", toLane=" << connector.toLane << ", enabled=" << (connector.enabled ? "true" : "false") << ", yield=" << (connector.yield ? "true" : "false")
            << ", uTurn=" << (connector.uTurn ? "true" : "false") << ", speedLimitKmh=" << connector.speedLimitKmh
            << ", conflictGroup=" << connector.conflictGroup << ", reservationSeconds=" << connector.reservationSeconds << " },\n";
    out << "  },\n";
    out << "  trafficSignalPhases = {\n";
    for (const auto& phase : m_authoring.trafficSignalPhases)
        out << "    { id=" << phase.id << ", intersectionId=" << phase.intersectionId << ", order=" << phase.order << ", name=" << luaQuote(phase.name)
            << ", greenSeconds=" << phase.greenSeconds << ", yellowSeconds=" << phase.yellowSeconds << ", allRedSeconds=" << phase.allRedSeconds << ", connectorIds=" << luaQuote(phase.connectorIds) << " },\n";
    out << "  },\n";
    out << "  parkingStrips = {\n";
    for (const auto& parking : m_authoring.parkingStrips)
        out << "    { id=" << parking.id << ", name=" << luaQuote(parking.name) << ", roadId=" << parking.roadId << ", x=" << parking.position.x << ", y=" << parking.position.y << ", z=" << parking.position.z
            << ", headingDeg=" << parking.headingDeg << ", spaces=" << parking.spaces << ", spacingM=" << parking.spacingM << ", angleDeg=" << parking.angleDeg
            << ", rightSide=" << (parking.rightSide ? "true" : "false") << ", occupancy=" << parking.occupancy << " },\n";
    out << "  },\n";
    const auto& population = m_authoring.trafficPopulation;
    out << "  trafficPopulation = { globalDensity=" << population.globalDensity << ", parkedDensity=" << population.parkedDensity << ", rushHourMultiplier=" << population.rushHourMultiplier
        << ", nightMultiplier=" << population.nightMultiplier << ", heavyVehicleShare=" << population.heavyVehicleShare << ", motorcycleShare=" << population.motorcycleShare
        << ", commercialShare=" << population.commercialShare << ", emergencyShare=" << population.emergencyShare << ", laneChangeAggression=" << population.laneChangeAggression
        << ", speedVariance=" << population.speedVariance << ", maxActiveVehicles=" << population.maxActiveVehicles << " },\n";
    const auto& nav = m_authoring.navigationBuild;
    out << "  navigationBuild = { enabled=" << (nav.enabled ? "true" : "false") << ", rebuildOnSave=" << (nav.rebuildOnSave ? "true" : "false")
        << ", maxSlopeDeg=" << nav.maxSlopeDeg << ", minimumTurnRadiusM=" << nav.minimumTurnRadiusM << ", laneChangeLengthM=" << nav.laneChangeLengthM
        << ", junctionLookaheadM=" << nav.junctionLookaheadM << ", mergeLookaheadM=" << nav.mergeLookaheadM << " },\n";
    const auto& rules = m_authoring.trafficRules;
    out << "  trafficRules = { drivingSide=" << luaQuote(authoring::drivingSideName(rules.drivingSide)) << ", keepToDrivingSide=" << (rules.keepToDrivingSide ? "true" : "false")
        << ", allowTurnOnRed=" << (rules.allowTurnOnRed ? "true" : "false") << ", emergencyCorridor=" << (rules.emergencyCorridor ? "true" : "false")
        << ", desiredTimeGapS=" << rules.desiredTimeGapS << ", minimumGapM=" << rules.minimumGapM << ", desiredAccelerationMps2=" << rules.desiredAccelerationMps2
        << ", comfortableBrakingMps2=" << rules.comfortableBrakingMps2 << ", laneChangeCooldownS=" << rules.laneChangeCooldownS
        << ", laneChangeMinimumGapM=" << rules.laneChangeMinimumGapM << ", laneChangeRouteCost=" << rules.laneChangeRouteCost << ", mergeRouteCost=" << rules.mergeRouteCost
        << ", emergencyYieldRadiusM=" << rules.emergencyYieldRadiusM << ", roundaboutYieldDistanceM=" << rules.roundaboutYieldDistanceM << " },\n";
    const auto& streaming = m_authoring.trafficStreaming;
    out << "  trafficStreaming = { fullSimulationRadiusM=" << streaming.fullSimulationRadiusM << ", simplifiedSimulationRadiusM=" << streaming.simplifiedSimulationRadiusM
        << ", dormantPersistenceRadiusM=" << streaming.dormantPersistenceRadiusM << ", sectorSizeM=" << streaming.sectorSizeM
        << ", maxSpawnsPerSecond=" << streaming.maxSpawnsPerSecond << ", maxDespawnsPerSecond=" << streaming.maxDespawnsPerSecond
        << ", despawnBehindDistanceM=" << streaming.despawnBehindDistanceM << ", retainDormantState=" << (streaming.retainDormantState ? "true" : "false")
        << ", dormantStateMinutes=" << streaming.dormantStateMinutes << " },\n";
    out << "  intersectionControllers = {\n";
    for (const auto& controller : m_authoring.intersectionControllers)
        out << "    { intersectionId=" << controller.intersectionId << ", mode=" << luaQuote(authoring::signalControlModeName(controller.mode))
            << ", phaseOffsetSeconds=" << controller.phaseOffsetSeconds << ", minimumGreenSeconds=" << controller.minimumGreenSeconds
            << ", maximumGreenSeconds=" << controller.maximumGreenSeconds << ", detectorDistanceM=" << controller.detectorDistanceM
            << ", gapOutSeconds=" << controller.gapOutSeconds << ", queueAdaptive=" << (controller.queueAdaptive ? "true" : "false")
            << ", emergencyPreemption=" << (controller.emergencyPreemption ? "true" : "false") << " },\n";
    out << "  },\n";
    out << "  roadRestrictions = {\n";
    for (const auto& restriction : m_authoring.roadRestrictions)
        out << "    { id=" << restriction.id << ", name=" << luaQuote(restriction.name) << ", type=" << luaQuote(authoring::roadRestrictionTypeName(restriction.type))
            << ", enabled=" << (restriction.enabled ? "true" : "false") << ", roadId=" << restriction.roadId << ", linkId=" << restriction.linkId
            << ", blockTraffic=" << (restriction.blockTraffic ? "true" : "false") << ", emergencyExempt=" << (restriction.emergencyExempt ? "true" : "false")
            << ", speedLimitKmh=" << restriction.speedLimitKmh << ", routeCostMultiplier=" << restriction.routeCostMultiplier
            << ", startHour=" << restriction.startHour << ", endHour=" << restriction.endHour << ", vehicleMassLimitKg=" << restriction.vehicleMassLimitKg
            << ", vehicleHeightLimitM=" << restriction.vehicleHeightLimitM << " },\n";
    out << "  },\n";

    const auto& agentSim = m_authoring.trafficAgentSimulation;
    out << "  trafficAgentSimulation = { enabled=" << (agentSim.enabled ? "true" : "false")
        << ", createDebugProxyVehicles=" << (agentSim.createDebugProxyVehicles ? "true" : "false")
        << ", useHeritageVehicleDynamics=" << (agentSim.useHeritageVehicleDynamics ? "true" : "false")
        << ", enableLaneChanges=" << (agentSim.enableLaneChanges ? "true" : "false") << ", enableMerges=" << (agentSim.enableMerges ? "true" : "false")
        << ", enableParking=" << (agentSim.enableParking ? "true" : "false") << ", maxFullPhysicsAgents=" << agentSim.maxFullPhysicsAgents
        << ", routeLookaheadLinks=" << agentSim.routeLookaheadLinks << ", trafficVehicleHighRateHz=" << agentSim.trafficVehicleHighRateHz << ", fullSimulationHz=" << agentSim.fullSimulationHz
        << ", simplifiedSimulationHz=" << agentSim.simplifiedSimulationHz << ", perceptionRangeM=" << agentSim.perceptionRangeM
        << ", stopLineBufferM=" << agentSim.stopLineBufferM << ", intersectionCreepSpeedKmh=" << agentSim.intersectionCreepSpeedKmh
        << ", parkingApproachSpeedKmh=" << agentSim.parkingApproachSpeedKmh << ", spawnMinDistancePlayerM=" << agentSim.spawnMinDistancePlayerM
        << ", spawnMaxDistancePlayerM=" << agentSim.spawnMaxDistancePlayerM << ", minimumSpawnGapM=" << agentSim.minimumSpawnGapM
        << ", stuckTimeoutS=" << agentSim.stuckTimeoutS << ", despawnGraceS=" << agentSim.despawnGraceS << " },\n";
    out << "  trafficAgentProfiles = {\n";
    for (const auto& profile : m_authoring.trafficAgentProfiles)
        out << "    { id=" << profile.id << ", name=" << luaQuote(profile.name) << ", class=" << luaQuote(authoring::trafficAgentClassName(profile.vehicleClass))
            << ", enabled=" << (profile.enabled ? "true" : "false") << ", vehiclePreset=" << luaQuote(profile.vehiclePreset) << ", spawnWeight=" << profile.spawnWeight
            << ", lengthM=" << profile.lengthM << ", widthM=" << profile.widthM << ", maxSpeedFactor=" << profile.maxSpeedFactor
            << ", accelerationFactor=" << profile.accelerationFactor << ", brakingFactor=" << profile.brakingFactor << ", desiredTimeGapS=" << profile.desiredTimeGapS
            << ", minimumGapM=" << profile.minimumGapM << ", reactionTimeS=" << profile.reactionTimeS << ", laneChangeAggression=" << profile.laneChangeAggression
            << ", courtesy=" << profile.courtesy << ", speedCompliance=" << profile.speedCompliance << ", illegalOvertakeChance=" << profile.illegalOvertakeChance
            << ", parkingSkill=" << profile.parkingSkill << " },\n";
    out << "  },\n";

    out << "  trafficSpawnPortals = {\n";
    for (const auto& portal : m_authoring.trafficSpawnPortals)
        out << "    { id=" << portal.id << ", name=" << luaQuote(portal.name) << ", enabled=" << (portal.enabled ? "true" : "false")
            << ", nodeId=" << portal.nodeId << ", mode=" << luaQuote(authoring::trafficPortalModeName(portal.mode)) << ", x=" << portal.position.x << ", y=" << portal.position.y << ", z=" << portal.position.z
            << ", headingDeg=" << portal.headingDeg << ", radiusM=" << portal.radiusM << ", spawnWeight=" << portal.spawnWeight << ", maxConcurrentAgents=" << portal.maxConcurrentAgents
            << ", startHour=" << portal.startHour << ", endHour=" << portal.endHour << ", minimumPlayerDistanceM=" << portal.minimumPlayerDistanceM
            << ", maximumPlayerDistanceM=" << portal.maximumPlayerDistanceM << ", emergencyAllowed=" << (portal.emergencyAllowed ? "true" : "false")
            << ", allowedClasses=" << luaQuote(portal.allowedClasses) << " },\n";
    out << "  },\n";
    out << "  trafficDensityRegions = {\n";
    for (const auto& region : m_authoring.trafficDensityRegions)
        out << "    { id=" << region.id << ", name=" << luaQuote(region.name) << ", enabled=" << (region.enabled ? "true" : "false")
            << ", x=" << region.position.x << ", y=" << region.position.y << ", z=" << region.position.z << ", radiusM=" << region.radiusM
            << ", densityMultiplier=" << region.densityMultiplier << ", speedMultiplier=" << region.speedMultiplier
            << ", laneChangeAggressionOffset=" << region.laneChangeAggressionOffset << ", parkingMultiplier=" << region.parkingMultiplier
            << ", startHour=" << region.startHour << ", endHour=" << region.endHour << " },\n";
    out << "  },\n";
    out << "  trafficIncidents = {\n";
    for (const auto& incident : m_authoring.trafficIncidents)
        out << "    { id=" << incident.id << ", name=" << luaQuote(incident.name) << ", type=" << luaQuote(authoring::trafficIncidentTypeName(incident.type))
            << ", enabled=" << (incident.enabled ? "true" : "false") << ", roadId=" << incident.roadId << ", linkId=" << incident.linkId
            << ", x=" << incident.position.x << ", y=" << incident.position.y << ", z=" << incident.position.z << ", radiusM=" << incident.radiusM
            << ", severity=" << incident.severity << ", blockedLaneFraction=" << incident.blockedLaneFraction << ", speedLimitKmh=" << incident.speedLimitKmh
            << ", routeCostMultiplier=" << incident.routeCostMultiplier << ", responseDelayS=" << incident.responseDelayS << ", clearAfterS=" << incident.clearAfterS
            << ", emergencyResponse=" << (incident.emergencyResponse ? "true" : "false") << ", hazardLights=" << (incident.hazardLights ? "true" : "false") << " },\n";
    out << "  },\n";
    const auto& trafficEnvironment = m_authoring.trafficEnvironment;
    out << "  trafficEnvironment = { wetSpeedFactor=" << trafficEnvironment.wetSpeedFactor << ", heavyRainSpeedFactor=" << trafficEnvironment.heavyRainSpeedFactor
        << ", snowSpeedFactor=" << trafficEnvironment.snowSpeedFactor << ", iceSpeedFactor=" << trafficEnvironment.iceSpeedFactor << ", nightSpeedFactor=" << trafficEnvironment.nightSpeedFactor
        << ", wetFollowingGapFactor=" << trafficEnvironment.wetFollowingGapFactor << ", wetBrakingFactor=" << trafficEnvironment.wetBrakingFactor
        << ", poorVisibilitySpeedFactor=" << trafficEnvironment.poorVisibilitySpeedFactor << ", standingWaterAvoidance=" << (trafficEnvironment.standingWaterAvoidance ? "true" : "false")
        << ", weatherAwareLaneChanges=" << (trafficEnvironment.weatherAwareLaneChanges ? "true" : "false") << " },\n";
    const auto& trafficBehavior = m_authoring.trafficBehavior;
    out << "  trafficBehavior = { zipperMerging=" << (trafficBehavior.zipperMerging ? "true" : "false")
        << ", roundaboutNegotiation=" << (trafficBehavior.roundaboutNegotiation ? "true" : "false") << ", enforceStopDwell=" << (trafficBehavior.enforceStopDwell ? "true" : "false")
        << ", opportunisticOvertaking=" << (trafficBehavior.opportunisticOvertaking ? "true" : "false") << ", queueDischargeReaction=" << (trafficBehavior.queueDischargeReaction ? "true" : "false")
        << ", stagedParkingManeuvers=" << (trafficBehavior.stagedParkingManeuvers ? "true" : "false") << ", stuckRecovery=" << (trafficBehavior.stuckRecovery ? "true" : "false")
        << ", collisionIncidentResponse=" << (trafficBehavior.collisionIncidentResponse ? "true" : "false") << ", emergencyIncidentDispatch=" << (trafficBehavior.emergencyIncidentDispatch ? "true" : "false")
        << ", zipperAlternationWindowS=" << trafficBehavior.zipperAlternationWindowS << ", mergeCourtesyGapS=" << trafficBehavior.mergeCourtesyGapS
        << ", roundaboutEntryGapS=" << trafficBehavior.roundaboutEntryGapS << ", stopDwellS=" << trafficBehavior.stopDwellS << ", yieldCreepSpeedKmh=" << trafficBehavior.yieldCreepSpeedKmh
        << ", overtakeMinimumGainKmh=" << trafficBehavior.overtakeMinimumGainKmh << ", overtakeReturnGapM=" << trafficBehavior.overtakeReturnGapM
        << ", queueReactionSpreadS=" << trafficBehavior.queueReactionSpreadS << ", parkingReverseSpeedKmh=" << trafficBehavior.parkingReverseSpeedKmh
        << ", recoveryReverseSeconds=" << trafficBehavior.recoveryReverseSeconds << ", recoveryRerouteSeconds=" << trafficBehavior.recoveryRerouteSeconds
        << ", recoveryTeleportSeconds=" << trafficBehavior.recoveryTeleportSeconds << ", collisionDistanceM=" << trafficBehavior.collisionDistanceM
        << ", emergencyIncidentLookaheadM=" << trafficBehavior.emergencyIncidentLookaheadM << " },\n";
    const auto& trafficDebug = m_authoring.trafficDebug;
    out << "  trafficDebug = { enabled=" << (trafficDebug.enabled ? "true" : "false") << ", showAgentIds=" << (trafficDebug.showAgentIds ? "true" : "false")
        << ", showRoutes=" << (trafficDebug.showRoutes ? "true" : "false") << ", showIntentions=" << (trafficDebug.showIntentions ? "true" : "false")
        << ", showPerception=" << (trafficDebug.showPerception ? "true" : "false") << ", showFollowingGaps=" << (trafficDebug.showFollowingGaps ? "true" : "false")
        << ", showLaneChangeScores=" << (trafficDebug.showLaneChangeScores ? "true" : "false") << ", showWaitReasons=" << (trafficDebug.showWaitReasons ? "true" : "false")
        << ", showStreamingTiers=" << (trafficDebug.showStreamingTiers ? "true" : "false") << ", showIncidentInfluence=" << (trafficDebug.showIncidentInfluence ? "true" : "false")
        << ", maxDetailedAgents=" << trafficDebug.maxDetailedAgents << " },\n";

    out << "  events = {\n";
    for (const auto& event : m_authoring.gameEvents)
    {
        out << "    { id=" << event.id << ", name=" << luaQuote(event.name) << ", type=" << luaQuote(authoring::gameEventTypeName(event.type))
            << ", enabled=" << (event.enabled ? "true" : "false") << ", startMarkerId=" << event.startMarkerId
            << ", finishMarkerId=" << event.finishMarkerId << ", layoutId=" << event.layoutId
            << ", laps=" << event.laps << ", maxEntrants=" << event.maxEntrants
            << ", rollingStart=" << (event.rollingStart ? "true" : "false")
            << ", trafficEnabled=" << (event.trafficEnabled ? "true" : "false")
            << ", policeEnabled=" << (event.policeEnabled ? "true" : "false")
            << ", nightOnly=" << (event.nightOnly ? "true" : "false")
            << ", entryFee=" << event.entryFee << ", reward=" << event.reward << ", heat=" << event.heat << " },\n";
    }
    out << "  },\n";
    const auto& motorsport = m_authoring.motorsport;
    out << "  motorsport = { enabled=" << (motorsport.enabled ? "true" : "false") << ", aiCompetitorsEnabled=" << (motorsport.aiCompetitorsEnabled ? "true" : "false")
        << ", autoBuildGrid=" << (motorsport.autoBuildGrid ? "true" : "false") << ", simulateUnspawnedCompetitors=" << (motorsport.simulateUnspawnedCompetitors ? "true" : "false")
        << ", maxPhysicalCompetitors=" << motorsport.maxPhysicalCompetitors << ", defaultAiSkill=" << motorsport.defaultAiSkill
        << ", qualifyingPaceSpreadPercent=" << motorsport.qualifyingPaceSpreadPercent << ", baseMechanicalDnfChancePerHour=" << motorsport.baseMechanicalDnfChancePerHour
        << ", multiClassTiming=" << (motorsport.multiClassTiming ? "true" : "false") << ", championshipPersistence=" << (motorsport.championshipPersistence ? "true" : "false") << " },\n";
    const auto& motorsportAi = m_authoring.motorsportAi;
    out << "  motorsportAi = { enabled=" << (motorsportAi.enabled ? "true" : "false") << ", updateHz=" << motorsportAi.updateHz
        << ", lookaheadMinimumM=" << motorsportAi.lookaheadMinimumM << ", lookaheadMaximumM=" << motorsportAi.lookaheadMaximumM
        << ", brakingLookaheadM=" << motorsportAi.brakingLookaheadM << ", opponentAwarenessM=" << motorsportAi.opponentAwarenessM
        << ", slipstreamMinimumGapM=" << motorsportAi.slipstreamMinimumGapM << ", slipstreamMaximumGapM=" << motorsportAi.slipstreamMaximumGapM
        << ", overtakeMinimumClosingKmh=" << motorsportAi.overtakeMinimumClosingKmh << ", defensiveTriggerGapM=" << motorsportAi.defensiveTriggerGapM
        << ", blueFlagYieldGapM=" << motorsportAi.blueFlagYieldGapM << ", wetLineThreshold=" << motorsportAi.wetLineThreshold
        << ", maximumWetSpeedPenalty=" << motorsportAi.maximumWetSpeedPenalty << ", fuelUseLitersPer100Km=" << motorsportAi.fuelUseLitersPer100Km
        << ", tireWearPer100Km=" << motorsportAi.tireWearPer100Km << ", fuelReserveLaps=" << motorsportAi.fuelReserveLaps
        << ", tirePitThreshold=" << motorsportAi.tirePitThreshold << ", mistakeRecoverySeconds=" << motorsportAi.mistakeRecoverySeconds
        << ", strategyEnabled=" << (motorsportAi.strategyEnabled ? "true" : "false") << ", mistakesEnabled=" << (motorsportAi.mistakesEnabled ? "true" : "false")
        << ", slipstreamEnabled=" << (motorsportAi.slipstreamEnabled ? "true" : "false") << ", defendingEnabled=" << (motorsportAi.defendingEnabled ? "true" : "false")
        << ", multiclassNegotiation=" << (motorsportAi.multiclassNegotiation ? "true" : "false") << ", wetLineEnabled=" << (motorsportAi.wetLineEnabled ? "true" : "false")
        << ", liveDecisionTelemetry=" << (motorsportAi.liveDecisionTelemetry ? "true" : "false")
        << ", fullPhysicsCompetitors=" << (motorsportAi.fullPhysicsCompetitors ? "true" : "false") << ", physicsHighRateHz=" << motorsportAi.physicsHighRateHz
        << ", steeringLookaheadSeconds=" << motorsportAi.steeringLookaheadSeconds << ", steeringGain=" << motorsportAi.steeringGain << ", crossTrackGain=" << motorsportAi.crossTrackGain
        << ", throttleGain=" << motorsportAi.throttleGain << ", brakeGain=" << motorsportAi.brakeGain << ", maximumSteerAngleDeg=" << motorsportAi.maximumSteerAngleDeg
        << ", sideBySideSafetyM=" << motorsportAi.sideBySideSafetyM << ", trackLimitSafetyM=" << motorsportAi.trackLimitSafetyM
        << ", gripSlipRatioLimit=" << motorsportAi.gripSlipRatioLimit << ", gripSlipAngleDeg=" << motorsportAi.gripSlipAngleDeg
        << ", physicalRecoveryDistanceM=" << motorsportAi.physicalRecoveryDistanceM << ", formationSpeedKmh=" << motorsportAi.formationSpeedKmh
        << ", rollingStartSpeedKmh=" << motorsportAi.rollingStartSpeedKmh << ", pitLaneSpeedKmh=" << motorsportAi.pitLaneSpeedKmh
        << ", damagePitThreshold=" << motorsportAi.damagePitThreshold << ", damageDnfThreshold=" << motorsportAi.damageDnfThreshold
        << ", collisionDamageScale=" << motorsportAi.collisionDamageScale << ", weatherForecastSeconds=" << motorsportAi.weatherForecastSeconds
        << ", gripAwareBraking=" << (motorsportAi.gripAwareBraking ? "true" : "false") << ", spatialAvoidance=" << (motorsportAi.spatialAvoidance ? "true" : "false")
        << ", trackLimitAwarePassing=" << (motorsportAi.trackLimitAwarePassing ? "true" : "false") << ", damageStrategyEnabled=" << (motorsportAi.damageStrategyEnabled ? "true" : "false")
        << ", weatherForecastEnabled=" << (motorsportAi.weatherForecastEnabled ? "true" : "false")
        << ", colliderBoundsAuthority=" << (motorsportAi.colliderBoundsAuthority ? "true" : "false") << ", collisionEnvelopeMarginM=" << motorsportAi.collisionEnvelopeMarginM
        << ", sweptEnvelopeSeconds=" << motorsportAi.sweptEnvelopeSeconds << ", sideBySideOverlapToleranceM=" << motorsportAi.sideBySideOverlapToleranceM
        << ", divebombCommitGapM=" << motorsportAi.divebombCommitGapM << ", divebombClosingThresholdKmh=" << motorsportAi.divebombClosingThresholdKmh
        << ", switchbackWindowS=" << motorsportAi.switchbackWindowS << ", maximumDefensiveMovesPerStraight=" << motorsportAi.maximumDefensiveMovesPerStraight
        << ", blockingPenaltySeconds=" << motorsportAi.blockingPenaltySeconds << ", unsafeReleasePenaltySeconds=" << motorsportAi.unsafeReleasePenaltySeconds
        << ", pitReleaseLookaheadM=" << motorsportAi.pitReleaseLookaheadM << ", multiclassPassHorizonS=" << motorsportAi.multiclassPassHorizonS
        << ", tireOptimalMinimumC=" << motorsportAi.tireOptimalMinimumC << ", tireOptimalMaximumC=" << motorsportAi.tireOptimalMaximumC
        << ", fuelDensityKgPerLiter=" << motorsportAi.fuelDensityKgPerLiter
        << ", predictiveCollisionAvoidance=" << (motorsportAi.predictiveCollisionAvoidance ? "true" : "false")
        << ", divebombJudgement=" << (motorsportAi.divebombJudgement ? "true" : "false") << ", blockingRules=" << (motorsportAi.blockingRules ? "true" : "false")
        << ", unsafeReleaseStewarding=" << (motorsportAi.unsafeReleaseStewarding ? "true" : "false") << ", tireThermalStrategy=" << (motorsportAi.tireThermalStrategy ? "true" : "false")
        << ", fuelMassAwareness=" << (motorsportAi.fuelMassAwareness ? "true" : "false") << ", componentDamageStrategy=" << (motorsportAi.componentDamageStrategy ? "true" : "false")
        << ", contactEvidenceEnabled=" << (motorsportAi.contactEvidenceEnabled ? "true" : "false") << ", incidentStewardingEnabled=" << (motorsportAi.incidentStewardingEnabled ? "true" : "false")
        << ", incidentMinimumNormalImpulseNs=" << motorsportAi.incidentMinimumNormalImpulseNs << ", incidentMinimumClosingKmh=" << motorsportAi.incidentMinimumClosingKmh
        << ", severeIncidentNormalImpulseNs=" << motorsportAi.severeIncidentNormalImpulseNs << ", severeIncidentClosingKmh=" << motorsportAi.severeIncidentClosingKmh
        << ", avoidableContactPenaltySeconds=" << motorsportAi.avoidableContactPenaltySeconds << ", severeContactPenaltySeconds=" << motorsportAi.severeContactPenaltySeconds
        << ", contactEvidenceCooldownSeconds=" << motorsportAi.contactEvidenceCooldownSeconds << ", retainedIncidentEvidence=" << motorsportAi.retainedIncidentEvidence << " },\n";
    const auto& replay = m_authoring.motorsportReplay;
    out << "  motorsportReplay = { enabled=" << (replay.enabled ? "true" : "false") << ", sampleHz=" << replay.sampleHz
        << ", preRollSeconds=" << replay.preRollSeconds << ", postRollSeconds=" << replay.postRollSeconds
        << ", maximumIncidentClips=" << replay.maximumIncidentClips << ", maximumRecordedCompetitors=" << replay.maximumRecordedCompetitors
        << ", capturePlayer=" << (replay.capturePlayer ? "true" : "false") << ", captureControls=" << (replay.captureControls ? "true" : "false")
        << ", ghostReviewEnabled=" << (replay.ghostReviewEnabled ? "true" : "false") << ", maximumGhostVehicles=" << replay.maximumGhostVehicles
        << ", broadcastDirectorEnabled=" << (replay.broadcastDirectorEnabled ? "true" : "false") << ", autoIncidentCamera=" << (replay.autoIncidentCamera ? "true" : "false")
        << ", incidentCameraDistanceM=" << replay.incidentCameraDistanceM << ", incidentCameraHeightM=" << replay.incidentCameraHeightM
        << ", tracksideCameraLeadM=" << replay.tracksideCameraLeadM << ", helicopterCameraHeightM=" << replay.helicopterCameraHeightM
        << ", cameraSmoothing=" << replay.cameraSmoothing << " },\n";
    out << "  motorsportClasses = {\n";
    for (const auto& cls : m_authoring.motorsportClasses)
        out << "    { id=" << cls.id << ", name=" << luaQuote(cls.name) << ", code=" << luaQuote(cls.code) << ", enabled=" << (cls.enabled ? "true" : "false")
            << ", minimumPowerKw=" << cls.minimumPowerKw << ", maximumPowerKw=" << cls.maximumPowerKw << ", minimumWeightKg=" << cls.minimumWeightKg
            << ", maximumWeightKg=" << cls.maximumWeightKg << ", balanceBallastKg=" << cls.balanceBallastKg << ", maximumEntrants=" << cls.maximumEntrants << " },\n";
    out << "  },\n";
    out << "  motorsportEntrants = {\n";
    for (const auto& entrant : m_authoring.motorsportEntrants)
        out << "    { id=" << entrant.id << ", driverName=" << luaQuote(entrant.driverName) << ", teamName=" << luaQuote(entrant.teamName)
            << ", vehiclePreset=" << luaQuote(entrant.vehiclePreset) << ", enabled=" << (entrant.enabled ? "true" : "false") << ", eventId=" << entrant.eventId
            << ", classId=" << entrant.classId << ", raceNumber=" << entrant.raceNumber << ", aiSkill=" << entrant.aiSkill << ", qualifyingPace=" << entrant.qualifyingPace
            << ", racePace=" << entrant.racePace << ", wetSkill=" << entrant.wetSkill << ", aggression=" << entrant.aggression << ", consistency=" << entrant.consistency
            << ", pitSkill=" << entrant.pitSkill << ", racecraft=" << entrant.racecraft << ", awareness=" << entrant.awareness << ", defending=" << entrant.defending
            << ", tireManagement=" << entrant.tireManagement << ", fuelManagement=" << entrant.fuelManagement << ", strategyRisk=" << entrant.strategyRisk
            << ", mistakeRatePerHour=" << entrant.mistakeRatePerHour << ", reactionTimeS=" << entrant.reactionTimeS << ", preferredLineBias=" << entrant.preferredLineBias
            << ", clandestine=" << (entrant.clandestine ? "true" : "false") << ", gridOverride=" << entrant.gridOverride << " },\n";
    out << "  },\n";
    out << "  motorsportChampionships = {\n";
    for (const auto& championship : m_authoring.motorsportChampionships)
        out << "    { id=" << championship.id << ", name=" << luaQuote(championship.name) << ", enabled=" << (championship.enabled ? "true" : "false") << ", classId=" << championship.classId
            << ", pointsScheme=" << luaQuote(championship.pointsScheme) << ", poleBonus=" << championship.poleBonus << ", fastestLapBonus=" << championship.fastestLapBonus
            << ", dropWorstRounds=" << championship.dropWorstRounds << " },\n";
    out << "  },\n";
    out << "  motorsportRounds = {\n";
    for (const auto& round : m_authoring.motorsportRounds)
        out << "    { id=" << round.id << ", championshipId=" << round.championshipId << ", eventId=" << round.eventId << ", name=" << luaQuote(round.name)
            << ", enabled=" << (round.enabled ? "true" : "false") << ", order=" << round.order << ", pointsMultiplier=" << round.pointsMultiplier << " },\n";
    out << "  },\n";
    const auto& execution = m_authoring.eventExecution;
    out << "  eventExecution = { enabled=" << (execution.enabled ? "true" : "false")
        << ", autoStagePlayer=" << (execution.autoStagePlayer ? "true" : "false")
        << ", autoSavePersonalBests=" << (execution.autoSavePersonalBests ? "true" : "false")
        << ", gridSettleSeconds=" << execution.gridSettleSeconds << ", countdownSeconds=" << execution.countdownSeconds
        << ", falseStartSpeedKmh=" << execution.falseStartSpeedKmh << ", gateDebounceSeconds=" << execution.gateDebounceSeconds
        << ", trackLimitGraceSeconds=" << execution.trackLimitGraceSeconds << ", trackLimitRejoinSeconds=" << execution.trackLimitRejoinSeconds
        << ", fullCourseYellowSpeedKmh=" << execution.fullCourseYellowSpeedKmh << ", virtualSafetyCarSpeedKmh=" << execution.virtualSafetyCarSpeedKmh
        << ", safetyCarSpeedKmh=" << execution.safetyCarSpeedKmh << ", resultsHoldSeconds=" << execution.resultsHoldSeconds
        << ", practiceLoopEnabled=" << (execution.practiceLoopEnabled ? "true" : "false")
        << ", practiceLoopAutoRestart=" << (execution.practiceLoopAutoRestart ? "true" : "false")
        << ", practiceLoopRestoreAngularVelocity=" << (execution.practiceLoopRestoreAngularVelocity ? "true" : "false")
        << ", practiceLoopRestoreGear=" << (execution.practiceLoopRestoreGear ? "true" : "false")
        << ", practiceLoopEndGateWidthM=" << execution.practiceLoopEndGateWidthM
        << ", practiceLoopRestoreDelayS=" << execution.practiceLoopRestoreDelayS << " },\n";

    out << "  worldPoints = {\n";
    for (const auto& point : m_authoring.worldPoints)
    {
        out << "    { id=" << point.id << ", name=" << luaQuote(point.name) << ", type=" << luaQuote(authoring::worldPointTypeName(point.type))
            << ", enabled=" << (point.enabled ? "true" : "false") << ", x=" << point.position.x << ", y=" << point.position.y << ", z=" << point.position.z
            << ", headingDeg=" << point.headingDeg << ", radiusM=" << point.radiusM
            << ", discoverable=" << (point.discoverable ? "true" : "false")
            << ", fastTravelEnabled=" << (point.fastTravelEnabled ? "true" : "false")
            << ", servicePriceMultiplier=" << point.servicePriceMultiplier << " },\n";
    }
    out << "  },\n";
    const auto& police = m_authoring.policeGameplay;
    out << "  policeGameplay = { enabled=" << (police.enabled ? "true" : "false") << ", maxHeatLevel=" << police.maxHeatLevel
        << ", maxPursuitUnits=" << police.maxPursuitUnits << ", civilianWitnessRadiusM=" << police.civilianWitnessRadiusM
        << ", policeDetectionRadiusM=" << police.policeDetectionRadiusM << ", speedToleranceKmh=" << police.speedToleranceKmh
        << ", heatDecayDelayS=" << police.heatDecayDelayS << ", heatDecayPerSecond=" << police.heatDecayPerSecond
        << ", lostSightSeconds=" << police.lostSightSeconds << ", searchDurationS=" << police.searchDurationS
        << ", cooldownDurationS=" << police.cooldownDurationS << ", bustHoldSeconds=" << police.bustHoldSeconds
        << ", backupDelayS=" << police.backupDelayS << ", roadblockMinimumHeat=" << police.roadblockMinimumHeat
        << ", civilianWitnesses=" << (police.civilianWitnesses ? "true" : "false") << ", speedingGeneratesHeat=" << (police.speedingGeneratesHeat ? "true" : "false")
        << ", collisionsGenerateHeat=" << (police.collisionsGenerateHeat ? "true" : "false") << ", illegalRacesGenerateHeat=" << (police.illegalRacesGenerateHeat ? "true" : "false")
        << ", evasionEscalatesHeat=" << (police.evasionEscalatesHeat ? "true" : "false") << " },\n";
    out << "  policePatrolZones = {\n";
    for (const auto& zone : m_authoring.policePatrolZones)
        out << "    { id=" << zone.id << ", name=" << luaQuote(zone.name) << ", enabled=" << (zone.enabled ? "true" : "false")
            << ", x=" << zone.position.x << ", y=" << zone.position.y << ", z=" << zone.position.z << ", radiusM=" << zone.radiusM
            << ", patrolWeight=" << zone.patrolWeight << ", maximumUnits=" << zone.maximumUnits << ", responseMultiplier=" << zone.responseMultiplier
            << ", speedToleranceKmh=" << zone.speedToleranceKmh << ", startHour=" << zone.startHour << ", endHour=" << zone.endHour << ", responsePortalId=" << zone.responsePortalId << " },\n";
    out << "  },\n";
    out << "  policeRoadblockSites = {\n";
    for (const auto& site : m_authoring.policeRoadblockSites)
        out << "    { id=" << site.id << ", name=" << luaQuote(site.name) << ", enabled=" << (site.enabled ? "true" : "false") << ", nodeId=" << site.nodeId
            << ", x=" << site.position.x << ", y=" << site.position.y << ", z=" << site.position.z << ", headingDeg=" << site.headingDeg << ", widthM=" << site.widthM
            << ", minimumHeat=" << site.minimumHeat << ", unitCount=" << site.unitCount << ", spikeStrip=" << (site.spikeStrip ? "true" : "false")
            << ", leaveEscapeGap=" << (site.leaveEscapeGap ? "true" : "false") << ", selectionWeight=" << site.selectionWeight << " },\n";
    out << "  },\n";
    out << "  policeEscapeZones = {\n";
    for (const auto& zone : m_authoring.policeEscapeZones)
        out << "    { id=" << zone.id << ", name=" << luaQuote(zone.name) << ", enabled=" << (zone.enabled ? "true" : "false")
            << ", x=" << zone.position.x << ", y=" << zone.position.y << ", z=" << zone.position.z << ", radiusM=" << zone.radiusM
            << ", searchTimeMultiplier=" << zone.searchTimeMultiplier << ", heatDecayMultiplier=" << zone.heatDecayMultiplier
            << ", breakLineOfSight=" << (zone.breakLineOfSight ? "true" : "false") << ", safehouse=" << (zone.safehouse ? "true" : "false") << " },\n";
    out << "  },\n";
    out << "  clandestineMeets = {\n";
    for (const auto& meet : m_authoring.clandestineMeets)
        out << "    { id=" << meet.id << ", name=" << luaQuote(meet.name) << ", enabled=" << (meet.enabled ? "true" : "false")
            << ", x=" << meet.position.x << ", y=" << meet.position.y << ", z=" << meet.position.z << ", headingDeg=" << meet.headingDeg << ", radiusM=" << meet.radiusM
            << ", openHour=" << meet.openHour << ", closeHour=" << meet.closeHour << ", maximumVehicles=" << meet.maximumVehicles << ", policeRisk=" << meet.policeRisk
            << ", heatMultiplier=" << meet.heatMultiplier << ", eventId=" << meet.eventId << ", discoverable=" << (meet.discoverable ? "true" : "false") << " },\n";
    out << "  }\n";
    out << "}\n";
    out << "if RacingGameplay ~= nil then RacingGameplay.data = StudioGameplay end\n";
    out.close();
    if (!out) { std::filesystem::remove(temp, ec); message = "Could not finish generated runtime gameplay Lua."; return false; }

    std::filesystem::copy_file(temp, generatedFile, std::filesystem::copy_options::overwrite_existing, ec);
    std::filesystem::remove(temp);
    if (ec) { message = "Could not publish generated runtime gameplay Lua: " + ec.message(); return false; }

    message = "Published Heritage gameplay schema v18 with HRACE v7 cone-course/traffic-control overlays and HGAME v12 Autoslalom/Gymkhana event types to Scripts/Generated/StudioGameplay.lua";
    return true;
}

bool HeritageStudioApp::loadRuntimeVehicleSpawn(std::string& message)
{
    if (m_runtimeScenePath.empty() || !std::filesystem::exists(m_runtimeScenePath))
    {
        message = "Runtime entry scene does not exist: " + m_runtimeScenePath.string();
        return false;
    }

    authoring::Vec3 position{};
    authoring::Vec3 rotation{};
    if (!readStudioRuntimeSpawn(m_runtimeScenePath, position, rotation, message))
        return false;

    authoring::SceneObject* spawn = nullptr;
    for (auto& object : m_authoring.sceneObjects)
    {
        if (object.type == authoring::SceneObjectType::VehicleSpawn)
        {
            spawn = &object;
            break;
        }
    }
    if (!spawn)
        spawn = &m_authoring.addSceneObject(authoring::SceneObjectType::VehicleSpawn, "Vehicle Spawn");

    spawn->enabled = true;
    spawn->position = position;
    spawn->rotation = rotation;
    m_sceneSelectedObject = static_cast<int>(spawn - m_authoring.sceneObjects.data());
    return true;
}

void HeritageStudioApp::buildPeugeotCaptureGrid()
{
    m_captureTargets.clear();
    m_captureTargets.push_back({ 850, 0, true });
    for (int rpm = 1000; rpm <= 7000; rpm += 500)
    {
        for (const int throttle : { 25, 50, 75, 100 })
            m_captureTargets.push_back({ rpm, throttle, false });
    }
}

void HeritageStudioApp::updateCaptureAutoAdvance()
{
    if (!m_soundLab)
        return;
    const auto status = m_soundLab->status();
    if (m_captureWasRunning && !status.capturing)
    {
        if (!status.lastRawPath.empty() && status.lastRawPath != m_lastCompletedRawPath)
        {
            m_lastCompletedRawPath = status.lastRawPath;
            if (m_autoAdvanceCapture && status.lastCaptureBank)
            {
                if (m_captureQualityGate && !lastCaptureQualityGood())
                {
                    m_audioAssistantMessage = "Quality Gate paused bank advance: recapture the current cell or disable the gate.";
                }
                else if (m_captureTargetIndex + 1 < static_cast<int>(m_captureTargets.size()))
                {
                    ++m_captureTargetIndex;
                    if (m_skipCompletedCaptureTargets)
                        jumpToNextMissingCaptureTarget();
                }
            }
        }
    }
    m_captureWasRunning = status.capturing;
}

void HeritageStudioApp::applyAudioPreset(audio::lab::EngineSoundPreset preset)
{
    if (!m_soundLab)
        return;
    m_profileUndo = m_soundLab->profile();
    m_hasProfileUndo = true;
    m_soundLab->setProfile(audio::lab::makeEngineSoundPreset(preset));
    m_audioAssistantMessage = std::string("Applied preset: ")
        + audio::lab::engineSoundPresetName(preset)
        + ". Use ENGINE BAY / REAR / CABIN to audition it, then save the .hacoustic profile if you like it.";
}

void HeritageStudioApp::autoTuneAudio(audio::lab::EngineSoundPreset seedPreset)
{
    if (!m_soundLab)
        return;
    const auto analysis = m_soundLab->analysis();
    if (!analysis.valid)
    {
        m_audioAssistantMessage = "Auto Tune needs a raw capture first. Capture 3-6 seconds of steady Engine Simulator output.";
        return;
    }
    m_profileUndo = m_soundLab->profile();
    m_hasProfileUndo = true;
    const auto tuned = audio::lab::autoTuneEngineSoundProfile(analysis, seedPreset);
    m_soundLab->setProfile(tuned);
    m_audioAssistantMessage = std::string("Auto Tune analyzed the current raw capture and adapted ")
        + audio::lab::engineSoundPresetName(seedPreset)
        + " to its level, harshness and spectral balance. This is a starting point, not a claim that the raw recording alone identifies the real Peugeot acoustics.";
}

int HeritageStudioApp::countCompletedCaptureTargets() const
{
    if (!m_soundLab)
        return 0;
    int completed = 0;
    for (const auto& target : m_captureTargets)
    {
        if (m_soundLab->bankCaptureExists(
            "Peugeot206RC", "EW10J4S", target.rpm, target.throttlePercent))
        {
            ++completed;
        }
    }
    return completed;
}

void HeritageStudioApp::jumpToNextMissingCaptureTarget()
{
    if (!m_soundLab || m_captureTargets.empty())
        return;
    const int total = static_cast<int>(m_captureTargets.size());
    for (int offset = 0; offset < total; ++offset)
    {
        const int candidate = (m_captureTargetIndex + offset) % total;
        const auto& target = m_captureTargets[static_cast<std::size_t>(candidate)];
        if (!m_soundLab->bankCaptureExists(
            "Peugeot206RC", "EW10J4S", target.rpm, target.throttlePercent))
        {
            m_captureTargetIndex = candidate;
            return;
        }
    }
    m_audioAssistantMessage = "The Peugeot EW10J4S steady-state bank is complete: all 53 cells exist.";
}

bool HeritageStudioApp::lastCaptureQualityGood() const
{
    if (!m_soundLab)
        return false;
    const auto analysis = m_soundLab->analysis();
    if (!analysis.valid)
        return false;
    const bool usefulLevel = analysis.rmsDb > -42.0f && analysis.peakDb > -30.0f;
    const bool headroom = analysis.peakDb < -0.15f;
    const bool stable = analysis.stabilityDb < 3.5f;
    return usefulLevel && headroom && stable;
}

void HeritageStudioApp::openPathInShell(const std::filesystem::path& path) const
{
#ifdef _WIN32
    std::error_code ec;
    std::filesystem::path target = path;
    if (!std::filesystem::exists(target, ec))
        target = target.parent_path();
    if (target.empty())
        return;
    ShellExecuteW(nullptr, L"open", target.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    (void)path;
#endif
}

std::filesystem::path HeritageStudioApp::executableDirectory()
{
#ifdef _WIN32
    std::array<wchar_t, 32768> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length > 0 && length < buffer.size())
        return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
#endif
    std::error_code ec;
    return std::filesystem::current_path(ec);
}

std::filesystem::path HeritageStudioApp::findRepositoryRoot()
{
    const auto isRoot = [](const std::filesystem::path& candidate)
    {
        std::error_code ec;
        return std::filesystem::exists(candidate / "Modules" / "RacingUnited", ec)
            && std::filesystem::exists(candidate / "Engine" / "HeritageEngine", ec);
    };

    const std::array<std::filesystem::path, 2> starts{
        executableDirectory(), std::filesystem::current_path() };
    for (auto start : starts)
    {
        for (int depth = 0; depth < 8 && !start.empty(); ++depth)
        {
            if (isRoot(start))
                return start;
            const auto parent = start.parent_path();
            if (parent == start)
                break;
            start = parent;
        }
    }
    return std::filesystem::current_path();
}

} // namespace heritage::studio
