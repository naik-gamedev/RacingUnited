#pragma once

namespace heritage::graphics
{

struct AntiAliasingSettings
{
    int msaaSamples = 1;
    bool useFxaa = false;
};

const char* const* antiAliasingOptionNames();
int antiAliasingOptionCount();
AntiAliasingSettings resolveAntiAliasing(int optionIndex);

} // namespace heritage::graphics
