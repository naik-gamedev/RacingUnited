#pragma once

#include <string>

#include "GeneratedBuildIdentity.hpp"

namespace heritage::diagnostics {

inline const char* compiledConfiguration()
{
#ifdef _DEBUG
    return "Debug";
#else
    return "Release";
#endif
}

inline std::string buildIdentity()
{
    std::string value = "Heritage Engine";
    value += " | milestone=";
    value += generated::kMilestone;
    value += " | configuration=";
    value += compiledConfiguration();
    value += " | git=";
    value += generated::kGitCommit;
    value += " | dirty=";
    value += generated::kGitDirty;
    value += " | generated_utc=";
    value += generated::kGeneratedUtc;
    return value;
}

} // namespace heritage::diagnostics
