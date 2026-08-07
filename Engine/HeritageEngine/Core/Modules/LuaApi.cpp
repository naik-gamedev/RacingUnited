#include "LuaApi.hpp"

#include <array>
#include <sstream>
#include <type_traits>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace heritage::modules {
namespace {

#ifdef _WIN32
std::string windowsErrorMessage(DWORD errorCode)
{
    if (errorCode == 0)
        return "unknown Windows loader error";

    char* buffer = nullptr;
    const DWORD length = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<char*>(&buffer),
        0,
        nullptr);

    std::string message = (length && buffer)
        ? std::string(buffer, length)
        : "Windows error " + std::to_string(errorCode);

    if (buffer)
        LocalFree(buffer);

    while (!message.empty()
        && (message.back() == '\r' || message.back() == '\n'))
    {
        message.pop_back();
    }
    return message;
}
#endif

} // namespace

LuaApi::~LuaApi()
{
    unload();
}

bool LuaApi::load(
    const std::filesystem::path& projectRoot,
    std::string& errorMessage)
{
    if (isLoaded())
    {
        errorMessage.clear();
        return true;
    }

    std::vector<std::filesystem::path> candidates;
#ifdef _WIN32
    candidates.push_back(projectRoot / "ThirdParty" / "Lua" / "bin" / "lua54.dll");
    candidates.emplace_back("lua54.dll");
#else
    candidates.push_back(projectRoot / "ThirdParty" / "Lua" / "bin" / "liblua5.4.so");
    candidates.emplace_back("liblua5.4.so.0");
    candidates.emplace_back("liblua5.4.so");
    candidates.emplace_back("liblua54.so");
#endif

    std::ostringstream failures;
    bool firstFailure = true;
    for (const auto& candidate : candidates)
    {
        std::string candidateError;
        if (loadLibraryCandidate(candidate, candidateError))
        {
            if (resolveAllSymbols(errorMessage))
            {
                const LuaNumber version = lua_version(nullptr);
                if (version >= 504.0 && version < 505.0)
                {
                    errorMessage.clear();
                    return true;
                }

                std::ostringstream versionError;
                versionError
                    << "Heritage Engine loaded an incompatible Lua library (version "
                    << version << "). Lua 5.4 is required.";
                errorMessage = versionError.str();
                unload();
                return false;
            }

            unload();
            return false;
        }

        if (!firstFailure)
            failures << '\n';
        firstFailure = false;
        failures << candidate.string() << ": " << candidateError;
    }

    errorMessage =
        "Lua 5.4 runtime library was not found.\n\n"
        "Run Tools\\SetupLua.ps1 once, then rebuild HeritageEngine.\n\n"
        "Search attempts:\n" + failures.str();
    return false;
}

void LuaApi::unload()
{
    if (m_libraryHandle)
    {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(m_libraryHandle));
#else
        dlclose(m_libraryHandle);
#endif
    }

    m_libraryHandle = nullptr;
    m_loadedPath.clear();
    clearSymbols();
}

bool LuaApi::loadLibraryCandidate(
    const std::filesystem::path& path,
    std::string& errorMessage)
{
#ifdef _WIN32
    SetLastError(0);
    HMODULE handle = LoadLibraryW(path.wstring().c_str());
    if (!handle)
    {
        errorMessage = windowsErrorMessage(GetLastError());
        return false;
    }
    m_libraryHandle = handle;
#else
    dlerror();
    void* handle = dlopen(path.string().c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle)
    {
        const char* message = dlerror();
        errorMessage = message ? message : "unknown dynamic loader error";
        return false;
    }
    m_libraryHandle = handle;
#endif

    m_loadedPath = path;
    errorMessage.clear();
    return true;
}

bool LuaApi::resolveAllSymbols(std::string& errorMessage)
{
    auto resolve = [&](const char* symbol, auto& destination) -> bool
    {
#ifdef _WIN32
        FARPROC address = GetProcAddress(
            static_cast<HMODULE>(m_libraryHandle),
            symbol);
        destination = reinterpret_cast<std::remove_reference_t<decltype(destination)>>(address);
#else
        dlerror();
        void* address = dlsym(m_libraryHandle, symbol);
        destination = reinterpret_cast<std::remove_reference_t<decltype(destination)>>(address);
#endif
        if (destination)
            return true;

        errorMessage = "The loaded Lua library does not export required symbol: ";
        errorMessage += symbol;
        return false;
    };

#define HERITAGE_RESOLVE_LUA(symbol) \
    do { if (!resolve(#symbol, symbol)) return false; } while (false)

    HERITAGE_RESOLVE_LUA(luaL_newstate);
    HERITAGE_RESOLVE_LUA(lua_close);
    HERITAGE_RESOLVE_LUA(luaL_openlibs);
    HERITAGE_RESOLVE_LUA(luaL_loadfilex);
    HERITAGE_RESOLVE_LUA(lua_pcallk);
    HERITAGE_RESOLVE_LUA(lua_version);
    HERITAGE_RESOLVE_LUA(lua_gettop);
    HERITAGE_RESOLVE_LUA(lua_checkstack);
    HERITAGE_RESOLVE_LUA(lua_settop);
    HERITAGE_RESOLVE_LUA(lua_type);
    HERITAGE_RESOLVE_LUA(lua_toboolean);
    HERITAGE_RESOLVE_LUA(lua_tolstring);
    HERITAGE_RESOLVE_LUA(lua_tonumberx);
    HERITAGE_RESOLVE_LUA(lua_tointegerx);
    HERITAGE_RESOLVE_LUA(lua_getglobal);
    HERITAGE_RESOLVE_LUA(lua_getfield);
    HERITAGE_RESOLVE_LUA(lua_rawgeti);
    HERITAGE_RESOLVE_LUA(lua_rawlen);
    HERITAGE_RESOLVE_LUA(lua_setglobal);
    HERITAGE_RESOLVE_LUA(lua_createtable);
    HERITAGE_RESOLVE_LUA(lua_setfield);
    HERITAGE_RESOLVE_LUA(lua_pushcclosure);
    HERITAGE_RESOLVE_LUA(lua_pushstring);
    HERITAGE_RESOLVE_LUA(lua_pushlstring);
    HERITAGE_RESOLVE_LUA(lua_pushnumber);
    HERITAGE_RESOLVE_LUA(lua_pushinteger);
    HERITAGE_RESOLVE_LUA(lua_pushboolean);
    HERITAGE_RESOLVE_LUA(lua_pushnil);

#undef HERITAGE_RESOLVE_LUA

    errorMessage.clear();
    return true;
}

void LuaApi::clearSymbols()
{
    luaL_newstate = nullptr;
    lua_close = nullptr;
    luaL_openlibs = nullptr;
    luaL_loadfilex = nullptr;
    lua_pcallk = nullptr;
    lua_version = nullptr;
    lua_gettop = nullptr;
    lua_checkstack = nullptr;
    lua_settop = nullptr;
    lua_type = nullptr;
    lua_toboolean = nullptr;
    lua_tolstring = nullptr;
    lua_tonumberx = nullptr;
    lua_tointegerx = nullptr;
    lua_getglobal = nullptr;
    lua_getfield = nullptr;
    lua_rawgeti = nullptr;
    lua_rawlen = nullptr;
    lua_setglobal = nullptr;
    lua_createtable = nullptr;
    lua_setfield = nullptr;
    lua_pushcclosure = nullptr;
    lua_pushstring = nullptr;
    lua_pushlstring = nullptr;
    lua_pushnumber = nullptr;
    lua_pushinteger = nullptr;
    lua_pushboolean = nullptr;
    lua_pushnil = nullptr;
}

} // namespace heritage::modules
