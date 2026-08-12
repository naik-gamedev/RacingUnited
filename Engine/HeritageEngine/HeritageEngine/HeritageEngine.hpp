#pragma once

#include <memory>

namespace heritage::engine {

struct EngineRuntimeState;

// Top-level executable runtime coordinator for Heritage Engine.
//
// Subsystems own their mechanisms. HeritageEngine owns process-level startup,
// frame orchestration and orderly shutdown. Keep the platform entry point in
// main.cpp intentionally boring.
class HeritageEngine final
{
public:
    HeritageEngine();
    ~HeritageEngine();

    HeritageEngine(const HeritageEngine&) = delete;
    HeritageEngine& operator=(const HeritageEngine&) = delete;

    int run(int argc, char** argv);

private:
    std::unique_ptr<EngineRuntimeState> m_state;
};

} // namespace heritage::engine
