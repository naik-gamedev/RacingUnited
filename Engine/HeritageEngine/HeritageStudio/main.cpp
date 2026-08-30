#include "HeritageStudioApp.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    heritage::studio::HeritageStudioApp app;
    return app.run();
}
#else
int main()
{
    heritage::studio::HeritageStudioApp app;
    return app.run();
}
#endif
