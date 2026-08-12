// Heritage Engine executable entry point.
// Runtime ownership lives in HeritageEngine; keep this file intentionally boring.

#include "HeritageEngine.hpp"

int main(int argc, char** argv)
{
    heritage::engine::HeritageEngine engine;
    return engine.run(argc, argv);
}
