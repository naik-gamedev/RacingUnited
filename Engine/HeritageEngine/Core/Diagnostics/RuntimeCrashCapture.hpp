#pragma once

#include <exception>

namespace heritage::diagnostics {

// Installs best-effort process-level crash capture. Diagnostics are written
// beneath <project-root>/UserData/Diagnostics without changing runtime policy.
void installRuntimeCrashCapture(int argc, char** argv) noexcept;

// Top-level C++ exception reporters used by the deliberately-small executable
// entry point. Both functions persist a diagnostic and return a process code.
int reportUncaughtException(const std::exception& error) noexcept;
int reportUnknownException() noexcept;

} // namespace heritage::diagnostics
