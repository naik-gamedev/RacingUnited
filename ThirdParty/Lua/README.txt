Heritage Engine Lua Runtime
===========================

Run Tools\SetupLua.ps1 to download the Windows x64 Lua 5.4 shared library to:

    ThirdParty\Lua\bin\lua54.dll

Heritage Engine dynamically loads Lua at runtime. It does not need Lua headers
or an import library and does not link the DLL into the executable.

Binary source:
https://github.com/dyne/luabinaries/releases/latest/download/lua54.dll

The LuaBinaries project supplies prebuilt binaries and is not the official Lua
distribution. Lua itself is developed at https://www.lua.org/.
