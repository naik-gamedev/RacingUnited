![Racing United Banner](RacingUnited_GitHub.png)

Racing United is a racing simulator, a celebration of automotive history and culture. The game is designed to embrace both organized motorsport and clandestine racing culture as they exist around the world. Modify and tune vehicles for a wide range of disciplines or joyrides.  

This project is open-source and crowdsourced, built in collaboration with the community. Tutorials and guides will be readily available and regularly updated, constantly refining our craft to exceed the quality and pace of our previous workflow.

[Join our Discord community](https://discord.gg/rTfC2Ev)

## Linux build

Linux builds use CMake and require a C++20 compiler, CMake 3.20+, GLFW 3.3+
development files, and OpenGL development files.

```sh
cmake -S . -B build
cmake --build build -j
```

The resulting binaries are placed in build/bin/:

- HeritageEngine
- RacingUnitedLauncher
