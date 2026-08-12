#pragma once

struct GLFWwindow;

namespace heritage::platform::windows {

bool copyBackbufferToClipboard(GLFWwindow* window, int width, int height);

} // namespace heritage::platform::windows
