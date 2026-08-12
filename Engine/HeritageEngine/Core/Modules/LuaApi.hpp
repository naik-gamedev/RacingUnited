#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

struct lua_State;

#if defined(_WIN32)
#define HERITAGE_LUA_CALL __cdecl
#else
#define HERITAGE_LUA_CALL
#endif

namespace heritage::modules {

using LuaInteger = long long;
using LuaNumber = double;
using LuaKContext = std::intptr_t;
using LuaCFunction = int (HERITAGE_LUA_CALL*)(lua_State* state);
using LuaKFunction = int (HERITAGE_LUA_CALL*)(
    lua_State* state,
    int status,
    LuaKContext context);

// Dynamically loads the Lua 5.4 C API. Heritage Engine does not link against
// an import library, which keeps Debug and Release builds independent from the
// compiler used to build lua54.dll.
class LuaApi
{
public:
    LuaApi() = default;
    ~LuaApi();

    LuaApi(const LuaApi&) = delete;
    LuaApi& operator=(const LuaApi&) = delete;

    bool load(
        const std::filesystem::path& projectRoot,
        std::string& errorMessage);
    void unload();

    bool isLoaded() const { return m_libraryHandle != nullptr; }
    const std::filesystem::path& loadedPath() const { return m_loadedPath; }

    lua_State* (HERITAGE_LUA_CALL* luaL_newstate)() = nullptr;
    void (HERITAGE_LUA_CALL* lua_close)(lua_State*) = nullptr;
    void (HERITAGE_LUA_CALL* luaL_openlibs)(lua_State*) = nullptr;
    int (HERITAGE_LUA_CALL* luaL_loadfilex)(
        lua_State*, const char*, const char*) = nullptr;
    int (HERITAGE_LUA_CALL* lua_pcallk)(
        lua_State*, int, int, int, LuaKContext, LuaKFunction) = nullptr;

    LuaNumber (HERITAGE_LUA_CALL* lua_version)(lua_State*) = nullptr;
    int (HERITAGE_LUA_CALL* lua_gettop)(lua_State*) = nullptr;
    int (HERITAGE_LUA_CALL* lua_checkstack)(lua_State*, int) = nullptr;
    void (HERITAGE_LUA_CALL* lua_settop)(lua_State*, int) = nullptr;
    int (HERITAGE_LUA_CALL* lua_type)(lua_State*, int) = nullptr;
    int (HERITAGE_LUA_CALL* lua_toboolean)(lua_State*, int) = nullptr;
    const char* (HERITAGE_LUA_CALL* lua_tolstring)(
        lua_State*, int, std::size_t*) = nullptr;
    LuaNumber (HERITAGE_LUA_CALL* lua_tonumberx)(
        lua_State*, int, int*) = nullptr;
    LuaInteger (HERITAGE_LUA_CALL* lua_tointegerx)(
        lua_State*, int, int*) = nullptr;

    int (HERITAGE_LUA_CALL* lua_getglobal)(lua_State*, const char*) = nullptr;
    int (HERITAGE_LUA_CALL* lua_getfield)(lua_State*, int, const char*) = nullptr;
    int (HERITAGE_LUA_CALL* lua_rawgeti)(lua_State*, int, LuaInteger) = nullptr;
    void (HERITAGE_LUA_CALL* lua_rawseti)(lua_State*, int, LuaInteger) = nullptr;
    std::size_t (HERITAGE_LUA_CALL* lua_rawlen)(lua_State*, int) = nullptr;
    void (HERITAGE_LUA_CALL* lua_setglobal)(lua_State*, const char*) = nullptr;
    void (HERITAGE_LUA_CALL* lua_createtable)(lua_State*, int, int) = nullptr;
    void (HERITAGE_LUA_CALL* lua_setfield)(
        lua_State*, int, const char*) = nullptr;

    void (HERITAGE_LUA_CALL* lua_pushcclosure)(
        lua_State*, LuaCFunction, int) = nullptr;
    const char* (HERITAGE_LUA_CALL* lua_pushstring)(
        lua_State*, const char*) = nullptr;
    const char* (HERITAGE_LUA_CALL* lua_pushlstring)(
        lua_State*, const char*, std::size_t) = nullptr;
    void (HERITAGE_LUA_CALL* lua_pushnumber)(lua_State*, LuaNumber) = nullptr;
    void (HERITAGE_LUA_CALL* lua_pushinteger)(lua_State*, LuaInteger) = nullptr;
    void (HERITAGE_LUA_CALL* lua_pushboolean)(lua_State*, int) = nullptr;
    void (HERITAGE_LUA_CALL* lua_pushnil)(lua_State*) = nullptr;

private:
    bool loadLibraryCandidate(
        const std::filesystem::path& path,
        std::string& errorMessage);
    bool resolveAllSymbols(std::string& errorMessage);
    void clearSymbols();

    void* m_libraryHandle = nullptr;
    std::filesystem::path m_loadedPath;
};

} // namespace heritage::modules

#undef HERITAGE_LUA_CALL
