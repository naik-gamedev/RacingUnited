#include "WindowsDirectInputBackend.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <objbase.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace heritage::input {
namespace {

// Racing wheels should not require half of their full mechanical travel just to
// bind an axis. Capture now measures movement from the value present when the
// binding cell is clicked, so 0.08 is safely above normal G29 noise while
// requiring only a small deliberate wheel turn or pedal press.
constexpr float kAxisCaptureThreshold = 0.08f;

// INPUT06: DirectInput axis neutral is learned once from a short *stable*
// startup window, rather than from the first sample or by assuming every axis
// should eventually return to zero. Logitech G29 pedals are endpoint-resting
// axes (typically released near +1), while steering is centre-resting. Treating
// a pedal's midpoint as neutral turns pedal release into the opposite action.
constexpr float kNeutralSettleDelta = 0.012f;
constexpr std::uint16_t kNeutralSettleStableFrames = 30;
constexpr float kEndpointNeutralThreshold = 0.80f;

bool directInputButtonPressed(BYTE value)
{
    // DirectInput defines button state through the high bit. The low seven
    // bits are not a pressed/not-pressed contract and must not be treated as
    // truthy state.
    return (value & 0x80u) != 0;
}

std::string lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

std::string wideToUtf8(const wchar_t* value)
{
    if (!value || !*value)
        return {};

    const int required = WideCharToMultiByte(
        CP_UTF8,
        0,
        value,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 1)
        return {};

    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value,
        -1,
        result.data(),
        required,
        nullptr,
        nullptr);
    if (!result.empty() && result.back() == '\0')
        result.pop_back();
    return result;
}

std::string guidToString(const GUID& guid)
{
    wchar_t buffer[64]{};
    if (StringFromGUID2(guid, buffer, static_cast<int>(sizeof(buffer) / sizeof(buffer[0]))) <= 0)
        return {};

    std::string result = wideToUtf8(buffer);
    if (!result.empty() && result.front() == '{')
        result.erase(result.begin());
    if (!result.empty() && result.back() == '}')
        result.pop_back();
    return lower(result);
}

bool guidEqualsText(const GUID& guid, const std::string& text)
{
    return guidToString(guid) == lower(text);
}

constexpr std::array<DWORD, WindowsDirectInputBackend::kAxisCount> kAxisStateOffsets = {
    static_cast<DWORD>(offsetof(DIJOYSTATE2, lX)),
    static_cast<DWORD>(offsetof(DIJOYSTATE2, lY)),
    static_cast<DWORD>(offsetof(DIJOYSTATE2, lZ)),
    static_cast<DWORD>(offsetof(DIJOYSTATE2, lRx)),
    static_cast<DWORD>(offsetof(DIJOYSTATE2, lRy)),
    static_cast<DWORD>(offsetof(DIJOYSTATE2, lRz)),
    static_cast<DWORD>(offsetof(DIJOYSTATE2, rglSlider)),
    static_cast<DWORD>(offsetof(DIJOYSTATE2, rglSlider) + sizeof(LONG))
};

LONG axisRaw(const DIJOYSTATE2& state, int index)
{
    switch (index)
    {
    case 0: return state.lX;
    case 1: return state.lY;
    case 2: return state.lZ;
    case 3: return state.lRx;
    case 4: return state.lRy;
    case 5: return state.lRz;
    case 6: return state.rglSlider[0];
    case 7: return state.rglSlider[1];
    default: return 0;
    }
}

float normalizeAxis(LONG raw, LONG minimumRaw, LONG maximumRaw)
{
    // DirectInput racing hardware is not required to expose -32768..32767.
    // Logitech wheels in particular may keep a native 0..65535 range when a
    // requested DIPROP_RANGE is ignored by the driver. Assuming a signed
    // range makes a centred wheel look like +1.0 and limits the opposite
    // direction to only ~0.5, which prevents Heritage's 0.55 capture threshold
    // from ever seeing steering-left or some pedal axes. Always normalize from
    // the range actually reported by the device.
    if (maximumRaw <= minimumRaw)
        return 0.0f;
    const double minimum = static_cast<double>(minimumRaw);
    const double maximum = static_cast<double>(maximumRaw);
    const double clamped = std::clamp(static_cast<double>(raw), minimum, maximum);
    return std::clamp(
        static_cast<float>((clamped - minimum) / (maximum - minimum) * 2.0 - 1.0),
        -1.0f,
        1.0f);
}

int povDirectionIndex(DWORD pov)
{
    if ((pov & 0xFFFFu) == 0xFFFFu)
        return -1;

    const int angle = static_cast<int>(pov % 36000u);
    return ((angle + 2250) / 4500) % 8;
}

WindowsDirectInputBackend::ControlType povTypeFromIndex(int direction)
{
    using Type = WindowsDirectInputBackend::ControlType;
    switch (direction)
    {
    case 0: return Type::PovUp;
    case 1: return Type::PovUpRight;
    case 2: return Type::PovRight;
    case 3: return Type::PovDownRight;
    case 4: return Type::PovDown;
    case 5: return Type::PovDownLeft;
    case 6: return Type::PovLeft;
    case 7: return Type::PovUpLeft;
    default: return Type::PovUp;
    }
}

int povIndexFromType(WindowsDirectInputBackend::ControlType type)
{
    using Type = WindowsDirectInputBackend::ControlType;
    switch (type)
    {
    case Type::PovUp: return 0;
    case Type::PovUpRight: return 1;
    case Type::PovRight: return 2;
    case Type::PovDownRight: return 3;
    case Type::PovDown: return 4;
    case Type::PovDownLeft: return 5;
    case Type::PovLeft: return 6;
    case Type::PovUpLeft: return 7;
    default: return -1;
    }
}

bool parseHexBytePair(const std::string& text, std::size_t offset, unsigned& value)
{
    if (offset + 4 > text.size())
        return false;

    auto nibble = [](char character) -> int {
        if (character >= '0' && character <= '9')
            return character - '0';
        if (character >= 'a' && character <= 'f')
            return 10 + character - 'a';
        if (character >= 'A' && character <= 'F')
            return 10 + character - 'A';
        return -1;
    };

    const int a = nibble(text[offset]);
    const int b = nibble(text[offset + 1]);
    const int c = nibble(text[offset + 2]);
    const int d = nibble(text[offset + 3]);
    if (a < 0 || b < 0 || c < 0 || d < 0)
        return false;

    const unsigned firstByte = static_cast<unsigned>((a << 4) | b);
    const unsigned secondByte = static_cast<unsigned>((c << 4) | d);
    value = firstByte | (secondByte << 8);
    return true;
}

bool productIsGlfwStandardGamepad(const GUID& productGuid)
{
    const unsigned productVid = LOWORD(productGuid.Data1);
    const unsigned productPid = HIWORD(productGuid.Data1);

    for (int joystick = GLFW_JOYSTICK_1; joystick <= GLFW_JOYSTICK_LAST; ++joystick)
    {
        if (glfwJoystickPresent(joystick) != GLFW_TRUE
            || glfwJoystickIsGamepad(joystick) != GLFW_TRUE)
        {
            continue;
        }

        const char* guidText = glfwGetJoystickGUID(joystick);
        if (!guidText)
            continue;

        const std::string guid = guidText;
        unsigned vid = 0;
        unsigned pid = 0;
        // GLFW uses SDL's 32-character GUID format on Windows. Bytes 4-5
        // contain the USB vendor ID and bytes 8-9 contain the product ID,
        // both in little-endian text order.
        if (parseHexBytePair(guid, 8, vid)
            && parseHexBytePair(guid, 16, pid)
            && vid == productVid
            && pid == productPid)
        {
            return true;
        }
    }

    return false;
}

} // namespace

class WindowsDirectInputBackend::Impl
{
public:
    struct Device
    {
        IDirectInputDevice8W* handle = nullptr;
        GUID instanceGuid{};
        GUID productGuid{};
        std::string instanceGuidText;
        std::string productGuidText;
        std::string name;
        std::array<bool, kAxisCount> axisPresent{};
        std::array<LONG, kAxisCount> axisMinimumRaw{};
        std::array<LONG, kAxisCount> axisMaximumRaw{};
        int buttonCount = 0;
        int povCount = 0;
        bool connected = false;
        bool hasState = false;
        DIJOYSTATE2 state{};
        DIJOYSTATE2 previousState{};
        std::array<float, kAxisCount> axes{};
        std::array<float, kAxisCount> previousAxes{};
        std::array<float, kAxisCount> neutralAxes{};
        std::array<float, kAxisCount> neutralCandidateAxes{};
        std::array<std::uint16_t, kAxisCount> neutralStableFrames{};
        std::array<bool, kAxisCount> neutralCalibrated{};
        std::array<float, kAxisCount> captureAxes{};
        bool captureBaselineValid = false;

        Device()
        {
            axisMinimumRaw.fill(-32768);
            axisMaximumRaw.fill(32767);
        }
        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;

        Device(Device&& other) noexcept
        {
            *this = std::move(other);
        }

        Device& operator=(Device&& other) noexcept
        {
            if (this == &other)
                return *this;
            release();
            handle = std::exchange(other.handle, nullptr);
            instanceGuid = other.instanceGuid;
            productGuid = other.productGuid;
            instanceGuidText = std::move(other.instanceGuidText);
            productGuidText = std::move(other.productGuidText);
            name = std::move(other.name);
            axisPresent = other.axisPresent;
            axisMinimumRaw = other.axisMinimumRaw;
            axisMaximumRaw = other.axisMaximumRaw;
            buttonCount = other.buttonCount;
            povCount = other.povCount;
            connected = other.connected;
            hasState = other.hasState;
            state = other.state;
            previousState = other.previousState;
            axes = other.axes;
            previousAxes = other.previousAxes;
            neutralAxes = other.neutralAxes;
            neutralCandidateAxes = other.neutralCandidateAxes;
            neutralStableFrames = other.neutralStableFrames;
            neutralCalibrated = other.neutralCalibrated;
            captureAxes = other.captureAxes;
            captureBaselineValid = other.captureBaselineValid;
            return *this;
        }

        ~Device()
        {
            release();
        }

        void release()
        {
            if (handle)
            {
                handle->Unacquire();
                handle->Release();
                handle = nullptr;
            }
        }
    };

    IDirectInput8W* directInput = nullptr;
    HWND window = nullptr;
    bool initialized = false;
    std::vector<Device> devices;

    ~Impl()
    {
        shutdown();
    }

    void shutdown()
    {
        devices.clear();
        if (directInput)
        {
            directInput->Release();
            directInput = nullptr;
        }
        window = nullptr;
        initialized = false;
    }

    Device* find(const std::string& guid)
    {
        const std::string target = lower(guid);
        for (Device& device : devices)
        {
            if (device.instanceGuidText == target)
                return &device;
        }
        return nullptr;
    }

    const Device* find(const std::string& guid) const
    {
        const std::string target = lower(guid);
        for (const Device& device : devices)
        {
            if (device.instanceGuidText == target)
                return &device;
        }
        return nullptr;
    }

    struct EnumerationContext
    {
        Impl* self = nullptr;
        std::vector<std::string> seen;
    };

    static void configureAxesForCurrentDataFormat(Device& device)
    {
        if (!device.handle)
            return;

        device.axisPresent.fill(false);

        // DirectInput's EnumObjects callback exposes dwOfs in the DEVICE'S
        // NATIVE/raw format, not in the application's current c_dfDIJoystick2
        // format. Treating that native offset as DIJOYSTATE2 lX/lY/... is
        // therefore invalid and can silently lose or misidentify wheel axes.
        // Query each canonical DIJOYSTATE2 slot through DIPH_BYOFFSET after
        // SetDataFormat instead. This is exactly what DirectInput defines as
        // the offset into the CURRENT data format.
        for (std::size_t axisIndex = 0; axisIndex < kAxisCount; ++axisIndex)
        {
            const DWORD stateOffset = kAxisStateOffsets[axisIndex];

            DIDEVICEOBJECTINSTANCEW object{};
            object.dwSize = sizeof(object);
            if (FAILED(device.handle->GetObjectInfo(
                &object, stateOffset, DIPH_BYOFFSET)))
            {
                continue;
            }

            DIPROPRANGE requestedRange{};
            requestedRange.diph.dwSize = sizeof(requestedRange);
            requestedRange.diph.dwHeaderSize = sizeof(requestedRange.diph);
            requestedRange.diph.dwHow = DIPH_BYOFFSET;
            requestedRange.diph.dwObj = stateOffset;
            requestedRange.lMin = -32768;
            requestedRange.lMax = 32767;
            device.handle->SetProperty(DIPROP_RANGE, &requestedRange.diph);

            // Always retain the effective range actually used by DirectInput.
            // The driver is allowed to refuse/modify a requested range.
            DIPROPRANGE effectiveRange{};
            effectiveRange.diph.dwSize = sizeof(effectiveRange);
            effectiveRange.diph.dwHeaderSize = sizeof(effectiveRange.diph);
            effectiveRange.diph.dwHow = DIPH_BYOFFSET;
            effectiveRange.diph.dwObj = stateOffset;
            if (SUCCEEDED(device.handle->GetProperty(
                DIPROP_RANGE, &effectiveRange.diph))
                && effectiveRange.lMax > effectiveRange.lMin)
            {
                device.axisMinimumRaw[axisIndex] = effectiveRange.lMin;
                device.axisMaximumRaw[axisIndex] = effectiveRange.lMax;
            }

            DIPROPDWORD deadzone{};
            deadzone.diph.dwSize = sizeof(deadzone);
            deadzone.diph.dwHeaderSize = sizeof(deadzone.diph);
            deadzone.diph.dwHow = DIPH_BYOFFSET;
            deadzone.diph.dwObj = stateOffset;
            deadzone.dwData = 0;
            device.handle->SetProperty(DIPROP_DEADZONE, &deadzone.diph);

            device.axisPresent[axisIndex] = true;
        }
    }

    bool createDevice(const DIDEVICEINSTANCEW& instance)
    {
        if (productIsGlfwStandardGamepad(instance.guidProduct))
            return true;

        const std::string instanceGuidText = guidToString(instance.guidInstance);
        if (instanceGuidText.empty())
            return true;
        if (find(instanceGuidText))
            return true;

        Device device;
        device.instanceGuid = instance.guidInstance;
        device.productGuid = instance.guidProduct;
        device.instanceGuidText = instanceGuidText;
        device.productGuidText = guidToString(instance.guidProduct);
        device.name = wideToUtf8(instance.tszProductName);
        if (device.name.empty())
            device.name = wideToUtf8(instance.tszInstanceName);
        if (device.name.empty())
            device.name = "DirectInput controller";

        if (FAILED(directInput->CreateDevice(
            instance.guidInstance,
            &device.handle,
            nullptr)))
        {
            return true;
        }

        if (FAILED(device.handle->SetDataFormat(&c_dfDIJoystick2))
            || FAILED(device.handle->SetCooperativeLevel(
                window,
                DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
        {
            return true;
        }

        DIDEVCAPS capabilities{};
        capabilities.dwSize = sizeof(capabilities);
        if (SUCCEEDED(device.handle->GetCapabilities(&capabilities)))
        {
            device.buttonCount = static_cast<int>((std::min)(
                capabilities.dwButtons,
                static_cast<DWORD>(kButtonCount)));
            device.povCount = static_cast<int>((std::min)(
                capabilities.dwPOVs,
                static_cast<DWORD>(kPovCount)));
        }

        configureAxesForCurrentDataFormat(device);
        device.handle->Acquire();
        devices.push_back(std::move(device));
        return true;
    }

    static BOOL CALLBACK enumerateDeviceCallback(
        const DIDEVICEINSTANCEW* instance,
        VOID* context)
    {
        EnumerationContext* enumeration = static_cast<EnumerationContext*>(context);
        if (!enumeration || !enumeration->self || !instance)
            return DIENUM_CONTINUE;

        if (productIsGlfwStandardGamepad(instance->guidProduct))
            return DIENUM_CONTINUE;

        const std::string guid = guidToString(instance->guidInstance);
        if (!guid.empty())
            enumeration->seen.push_back(guid);
        enumeration->self->createDevice(*instance);
        return DIENUM_CONTINUE;
    }

    void refreshDevices()
    {
        if (!directInput)
            return;

        // PERF06: DirectInput EnumDevices can block long enough to produce a
        // visible frametime spike. Enumeration is therefore explicit: startup
        // and the Input settings refresh button only. Per-frame polling below
        // continues to read already-known devices without walking hardware.
        EnumerationContext context;
        context.self = this;
        directInput->EnumDevices(
            DI8DEVCLASS_GAMECTRL,
            enumerateDeviceCallback,
            &context,
            DIEDFL_ATTACHEDONLY);

        devices.erase(
            std::remove_if(devices.begin(), devices.end(),
                [&context](const Device& device) {
                    return std::find(
                        context.seen.begin(),
                        context.seen.end(),
                        device.instanceGuidText) == context.seen.end();
                }),
            devices.end());
    }

    static bool acquireAndPoll(Device& device)
    {
        if (!device.handle)
            return false;

        HRESULT result = device.handle->Poll();
        if (FAILED(result))
        {
            do
            {
                result = device.handle->Acquire();
            }
            while (result == DIERR_INPUTLOST);

            if (result == DIERR_OTHERAPPHASPRIO || result == DIERR_NOTACQUIRED)
                return false;
            if (FAILED(result) || FAILED(device.handle->Poll()))
                return false;
        }

        DIJOYSTATE2 state{};
        if (FAILED(device.handle->GetDeviceState(sizeof(state), &state)))
            return false;

        device.previousState = device.state;
        device.previousAxes = device.axes;
        device.state = state;

        for (std::size_t axis = 0; axis < kAxisCount; ++axis)
        {
            if (!device.axisPresent[axis])
                continue;
            device.axes[axis] = normalizeAxis(
                axisRaw(state, static_cast<int>(axis)),
                device.axisMinimumRaw[axis],
                device.axisMaximumRaw[axis]);
        }

        if (!device.hasState)
        {
            device.previousState = device.state;
            device.previousAxes = device.axes;
            device.neutralAxes = device.axes;
            device.neutralCandidateAxes = device.axes;
            device.neutralStableFrames.fill(1);
            device.neutralCalibrated.fill(false);
            device.hasState = true;
        }
        else
        {
            for (std::size_t axis = 0; axis < kAxisCount; ++axis)
            {
                if (!device.axisPresent[axis] || device.neutralCalibrated[axis])
                    continue;

                // Do not assume zero means neutral. A G29 steering axis rests
                // near zero, but its independent accelerator/brake/clutch axes
                // normally rest at an endpoint. Wait until the driver/device
                // has produced a stable value for a short startup window, then
                // freeze that value as this session's rest position. This also
                // ignores transient values while G HUB/DirectInput settles.
                const float current = device.axes[axis];
                if (std::abs(current - device.neutralCandidateAxes[axis])
                    <= kNeutralSettleDelta)
                {
                    // Gently converge the candidate so small analogue noise does
                    // not keep resetting the settle timer.
                    device.neutralCandidateAxes[axis] =
                        device.neutralCandidateAxes[axis] * 0.85f + current * 0.15f;
                    if (device.neutralStableFrames[axis]
                        < kNeutralSettleStableFrames)
                    {
                        ++device.neutralStableFrames[axis];
                    }
                }
                else
                {
                    device.neutralCandidateAxes[axis] = current;
                    device.neutralStableFrames[axis] = 1;
                }

                if (device.neutralStableFrames[axis]
                    >= kNeutralSettleStableFrames)
                {
                    device.neutralAxes[axis] =
                        device.neutralCandidateAxes[axis];
                    device.neutralCalibrated[axis] = true;
                }
            }
        }

        device.connected = true;
        return true;
    }

    void update()
    {
        for (Device& device : devices)
        {
            device.connected = false;
            acquireAndPoll(device);
        }
    }

    static float directionalAxisValue(
        const Device& device,
        int axis,
        bool positive,
        bool previous)
    {
        if (axis < 0 || axis >= static_cast<int>(kAxisCount)
            || !device.axisPresent[static_cast<std::size_t>(axis)]
            || !device.hasState
            || !device.neutralCalibrated[static_cast<std::size_t>(axis)])
        {
            return 0.0f;
        }

        const float current = previous
            ? device.previousAxes[static_cast<std::size_t>(axis)]
            : device.axes[static_cast<std::size_t>(axis)];
        const float neutral = device.neutralAxes[static_cast<std::size_t>(axis)];

        // INPUT09: no backend noise/dead-zone threshold is permitted here.
        // Once a DirectInput axis has a calibrated rest position, every
        // representable movement away from that rest position must propagate
        // to the binding. User/profile analogue settings are the sole authority
        // for any intentional deadzone.
        // Endpoint-resting pedals only have meaningful travel away from their
        // released endpoint. A stale opposite-direction binding (for example
        // Brake=AxisY+ on the same inverted G29 accelerator axis used by
        // Throttle=AxisY-) must never turn *release* into brake application.
        if ((positive && neutral >= kEndpointNeutralThreshold)
            || (!positive && neutral <= -kEndpointNeutralThreshold))
        {
            return 0.0f;
        }

        if (positive)
        {
            const float denominator = (std::max)(1.0f - neutral, 0.0001f);
            return std::clamp((current - neutral) / denominator, 0.0f, 1.0f);
        }

        const float denominator = (std::max)(neutral + 1.0f, 0.0001f);
        return std::clamp((neutral - current) / denominator, 0.0f, 1.0f);
    }
};

WindowsDirectInputBackend::WindowsDirectInputBackend()
    : m_impl(std::make_unique<Impl>())
{
}

WindowsDirectInputBackend::~WindowsDirectInputBackend() = default;

bool WindowsDirectInputBackend::initialize(
    GLFWwindow* glfwWindow,
    std::string& message)
{
    shutdown();
    m_impl = std::make_unique<Impl>();

    if (!glfwWindow)
    {
        message = "DirectInput received a null GLFW window.";
        return false;
    }

    m_impl->window = glfwGetWin32Window(glfwWindow);
    if (!m_impl->window)
    {
        message = "DirectInput could not obtain the native Windows window.";
        return false;
    }

    const HRESULT result = DirectInput8Create(
        GetModuleHandleW(nullptr),
        DIRECTINPUT_VERSION,
        IID_IDirectInput8W,
        reinterpret_cast<void**>(&m_impl->directInput),
        nullptr);
    if (FAILED(result) || !m_impl->directInput)
    {
        message = "DirectInput 8 could not be initialized.";
        return false;
    }

    m_impl->initialized = true;
    m_impl->refreshDevices();
    m_impl->update();

    message = "DirectInput 8 ready: "
        + std::to_string(m_impl->devices.size())
        + " additional controller"
        + (m_impl->devices.size() == 1 ? "" : "s")
        + " detected.";
    return true;
}

void WindowsDirectInputBackend::shutdown()
{
    if (m_impl)
        m_impl->shutdown();
}

void WindowsDirectInputBackend::update()
{
    if (m_impl && m_impl->initialized)
        m_impl->update();
}

void WindowsDirectInputBackend::refreshDevices()
{
    if (!m_impl || !m_impl->initialized)
        return;

    m_impl->refreshDevices();
    m_impl->update();
}

bool WindowsDirectInputBackend::available() const
{
    return m_impl && m_impl->initialized;
}

std::vector<WindowsDirectInputBackend::DeviceInfo>
WindowsDirectInputBackend::devices() const
{
    std::vector<DeviceInfo> result;
    if (!m_impl)
        return result;

    result.reserve(m_impl->devices.size());
    for (const Impl::Device& device : m_impl->devices)
    {
        DeviceInfo info;
        info.name = device.name;
        info.instanceGuid = device.instanceGuidText;
        info.productGuid = device.productGuidText;
        info.axisCount = static_cast<int>(std::count(
            device.axisPresent.begin(),
            device.axisPresent.end(),
            true));
        info.buttonCount = device.buttonCount;
        info.povCount = device.povCount;
        info.connected = device.connected;
        result.push_back(std::move(info));
    }
    return result;
}

std::string WindowsDirectInputBackend::deviceName(
    const std::string& instanceGuid) const
{
    if (!m_impl)
        return {};
    const Impl::Device* device = m_impl->find(instanceGuid);
    return device ? device->name : std::string{};
}

float WindowsDirectInputBackend::value(
    const std::string& instanceGuid,
    ControlType type,
    int controlIndex) const
{
    if (!m_impl)
        return 0.0f;
    const Impl::Device* device = m_impl->find(instanceGuid);
    if (!device || !device->connected || !device->hasState)
        return 0.0f;

    switch (type)
    {
    case ControlType::Button:
        return controlIndex >= 0
            && controlIndex < device->buttonCount
            && directInputButtonPressed(device->state.rgbButtons[controlIndex])
            ? 1.0f : 0.0f;
    case ControlType::AxisPositive:
        return Impl::directionalAxisValue(*device, controlIndex, true, false);
    case ControlType::AxisNegative:
        return Impl::directionalAxisValue(*device, controlIndex, false, false);
    default:
        break;
    }

    const int requestedDirection = povIndexFromType(type);
    if (requestedDirection < 0
        || controlIndex < 0
        || controlIndex >= device->povCount)
    {
        return 0.0f;
    }
    return povDirectionIndex(device->state.rgdwPOV[controlIndex])
        == requestedDirection ? 1.0f : 0.0f;
}

void WindowsDirectInputBackend::beginCapture()
{
    if (!m_impl)
        return;

    // Capture from the position the hardware is in when the binding cell is
    // clicked. A G29 steering wheel can therefore bind either direction with
    // a small deliberate turn, and an inverted clutch pedal can bind from its
    // released end-stop without relying on startup-neutral assumptions.
    for (Impl::Device& device : m_impl->devices)
    {
        if (!device.connected || !device.hasState)
        {
            device.captureBaselineValid = false;
            continue;
        }
        device.captureAxes = device.axes;
        device.captureBaselineValid = true;
    }
}

bool WindowsDirectInputBackend::captureBinding(std::string& binding)
{
    binding.clear();
    if (!m_impl)
        return false;

    for (Impl::Device& device : m_impl->devices)
    {
        if (!device.connected || !device.hasState)
            continue;

        const std::string prefix = "DInput[" + device.instanceGuidText + "]:";

        for (int button = 0; button < device.buttonCount; ++button)
        {
            const bool current = directInputButtonPressed(
                device.state.rgbButtons[button]);
            const bool previous = directInputButtonPressed(
                device.previousState.rgbButtons[button]);
            if (current && !previous)
            {
                binding = prefix + "Button" + std::to_string(button + 1);
                return true;
            }
        }

        for (int pov = 0; pov < device.povCount; ++pov)
        {
            const int current = povDirectionIndex(device.state.rgdwPOV[pov]);
            const int previous = povDirectionIndex(device.previousState.rgdwPOV[pov]);
            if (current >= 0 && current != previous)
            {
                binding = prefix + "Pov" + std::to_string(pov + 1)
                    + povDirectionName(povTypeFromIndex(current));
                return true;
            }
        }

        int strongestAxis = -1;
        float strongestMovement = 0.0f;
        for (int axis = 0; axis < static_cast<int>(kAxisCount); ++axis)
        {
            const std::size_t axisSlot = static_cast<std::size_t>(axis);
            if (!device.axisPresent[axisSlot])
                continue;

            // Compare against the exact value present when capture began.
            // Scan every axis first and bind the one with the largest deliberate
            // movement. Racing-wheel pedal potentiometers and force-feedback
            // steering can exhibit small simultaneous noise on other axes;
            // first-axis-wins capture can therefore select the wrong control.
            const float baseline = device.captureBaselineValid
                ? device.captureAxes[axisSlot]
                : device.previousAxes[axisSlot];
            const float movement = device.axes[axisSlot] - baseline;
            if (std::abs(movement) > std::abs(strongestMovement))
            {
                strongestMovement = movement;
                strongestAxis = axis;
            }
        }

        if (strongestAxis >= 0
            && std::abs(strongestMovement) >= kAxisCaptureThreshold)
        {
            const std::size_t axisSlot = static_cast<std::size_t>(strongestAxis);
            const float baseline = device.captureBaselineValid
                ? device.captureAxes[axisSlot]
                : device.previousAxes[axisSlot];
            binding = prefix + axisName(strongestAxis)
                + (strongestMovement >= 0.0f ? "+" : "-");

            // Binding capture starts while the control is released. Preserve
            // that exact position as the session neutral for asymmetric or
            // inverted pedal axes instead of trusting the first startup poll.
            device.neutralAxes[axisSlot] = baseline;
            device.neutralCandidateAxes[axisSlot] = baseline;
            device.neutralStableFrames[axisSlot] = kNeutralSettleStableFrames;
            device.neutralCalibrated[axisSlot] = true;
            device.captureBaselineValid = false;
            return true;
        }
    }

    return false;
}

std::string WindowsDirectInputBackend::axisName(int axisIndex)
{
    static constexpr std::array<const char*, kAxisCount> names = {
        "AxisX", "AxisY", "AxisZ", "AxisRx",
        "AxisRy", "AxisRz", "Slider1", "Slider2"
    };
    return axisIndex >= 0 && axisIndex < static_cast<int>(names.size())
        ? std::string(names[static_cast<std::size_t>(axisIndex)])
        : std::string{};
}

int WindowsDirectInputBackend::axisIndexFromName(const std::string& name)
{
    const std::string requested = lower(name);
    for (int index = 0; index < static_cast<int>(kAxisCount); ++index)
    {
        if (lower(axisName(index)) == requested)
            return index;
    }
    return -1;
}

std::string WindowsDirectInputBackend::povDirectionName(ControlType type)
{
    switch (type)
    {
    case ControlType::PovUp: return "Up";
    case ControlType::PovUpRight: return "UpRight";
    case ControlType::PovRight: return "Right";
    case ControlType::PovDownRight: return "DownRight";
    case ControlType::PovDown: return "Down";
    case ControlType::PovDownLeft: return "DownLeft";
    case ControlType::PovLeft: return "Left";
    case ControlType::PovUpLeft: return "UpLeft";
    default: return {};
    }
}

bool WindowsDirectInputBackend::povDirectionFromName(
    const std::string& name,
    ControlType& type)
{
    const std::string requested = lower(name);
    static constexpr std::array<ControlType, 8> directions = {
        ControlType::PovUp,
        ControlType::PovUpRight,
        ControlType::PovRight,
        ControlType::PovDownRight,
        ControlType::PovDown,
        ControlType::PovDownLeft,
        ControlType::PovLeft,
        ControlType::PovUpLeft
    };
    for (const ControlType candidate : directions)
    {
        if (lower(povDirectionName(candidate)) == requested)
        {
            type = candidate;
            return true;
        }
    }
    return false;
}

} // namespace heritage::input

#else

namespace heritage::input {

class WindowsDirectInputBackend::Impl
{
};

WindowsDirectInputBackend::WindowsDirectInputBackend()
    : m_impl(std::make_unique<Impl>())
{
}

WindowsDirectInputBackend::~WindowsDirectInputBackend() = default;

bool WindowsDirectInputBackend::initialize(GLFWwindow*, std::string& message)
{
    message = "DirectInput is available on Windows only.";
    return false;
}

void WindowsDirectInputBackend::shutdown() {}
void WindowsDirectInputBackend::update() {}
void WindowsDirectInputBackend::refreshDevices() {}
bool WindowsDirectInputBackend::available() const { return false; }
std::vector<WindowsDirectInputBackend::DeviceInfo>
WindowsDirectInputBackend::devices() const { return {}; }
std::string WindowsDirectInputBackend::deviceName(const std::string&) const { return {}; }
float WindowsDirectInputBackend::value(
    const std::string&,
    ControlType,
    int) const { return 0.0f; }
void WindowsDirectInputBackend::beginCapture() {}
bool WindowsDirectInputBackend::captureBinding(std::string&) { return false; }

std::string WindowsDirectInputBackend::axisName(int axisIndex)
{
    static constexpr std::array<const char*, kAxisCount> names = {
        "AxisX", "AxisY", "AxisZ", "AxisRx",
        "AxisRy", "AxisRz", "Slider1", "Slider2"
    };
    return axisIndex >= 0 && axisIndex < static_cast<int>(names.size())
        ? std::string(names[static_cast<std::size_t>(axisIndex)])
        : std::string{};
}

int WindowsDirectInputBackend::axisIndexFromName(const std::string& name)
{
    std::string requested = name;
    std::transform(requested.begin(), requested.end(), requested.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    for (int index = 0; index < static_cast<int>(kAxisCount); ++index)
    {
        std::string candidate = axisName(index);
        std::transform(candidate.begin(), candidate.end(), candidate.begin(),
            [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
        if (candidate == requested)
            return index;
    }
    return -1;
}

std::string WindowsDirectInputBackend::povDirectionName(ControlType type)
{
    switch (type)
    {
    case ControlType::PovUp: return "Up";
    case ControlType::PovUpRight: return "UpRight";
    case ControlType::PovRight: return "Right";
    case ControlType::PovDownRight: return "DownRight";
    case ControlType::PovDown: return "Down";
    case ControlType::PovDownLeft: return "DownLeft";
    case ControlType::PovLeft: return "Left";
    case ControlType::PovUpLeft: return "UpLeft";
    default: return {};
    }
}

bool WindowsDirectInputBackend::povDirectionFromName(
    const std::string& name,
    ControlType& type)
{
    std::string requested = name;
    std::transform(requested.begin(), requested.end(), requested.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    static constexpr std::array<ControlType, 8> directions = {
        ControlType::PovUp,
        ControlType::PovUpRight,
        ControlType::PovRight,
        ControlType::PovDownRight,
        ControlType::PovDown,
        ControlType::PovDownLeft,
        ControlType::PovLeft,
        ControlType::PovUpLeft
    };
    for (const ControlType candidate : directions)
    {
        std::string candidateName = povDirectionName(candidate);
        std::transform(candidateName.begin(), candidateName.end(), candidateName.begin(),
            [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
        if (candidateName == requested)
        {
            type = candidate;
            return true;
        }
    }
    return false;
}

} // namespace heritage::input

#endif
