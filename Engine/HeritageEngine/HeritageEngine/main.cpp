// Heritage Engine executable entry point.
// Runtime ownership lives in HeritageEngine; keep this file intentionally boring.

#include "HeritageEngine.hpp"
#include "../Core/Diagnostics/RuntimeCrashCapture.hpp"

#include <exception>

int main(int argc, char** argv)
{
    heritage::diagnostics::installRuntimeCrashCapture(argc, argv);
    try
    {
        heritage::engine::HeritageEngine engine;
        return engine.run(argc, argv);
    }
    catch (const std::exception& error)
    {
        return heritage::diagnostics::reportUncaughtException(error);
    }
    catch (...)
    {
        return heritage::diagnostics::reportUnknownException();
    }
}
