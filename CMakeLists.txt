cmake_minimum_required(VERSION 3.20)

project(RacingUnited VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)

if(WIN32)
    set(GLFW_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/GLFW/include)
else()
    find_path(GLFW_INCLUDE_DIR GLFW/glfw3.h REQUIRED)
endif()

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/IMGUI)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_library(racingunited_imgui OBJECT ${IMGUI_SOURCES})
target_include_directories(racingunited_imgui PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/GLAD/include
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/GLFW/include
)

if(NOT WIN32)
    target_compile_definitions(racingunited_imgui PRIVATE
        IMGUI_IMPL_GLFW_DISABLE_X11
        IMGUI_IMPL_GLFW_DISABLE_WAYLAND
    )
endif()

function(configure_racingunited_target target)
    target_include_directories(${target} PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/GLAD/include
        ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/GLFW/include
    )
    target_link_libraries(${target} PRIVATE
        glfw
        OpenGL::GL
        ${CMAKE_DL_LIBS}
    )
endfunction()

add_executable(HeritageEngine
    Engine/HeritageEngine/HeritageEngine/main.cpp
    ThirdParty/GLAD/src/glad.c
    $<TARGET_OBJECTS:racingunited_imgui>
)
configure_racingunited_target(HeritageEngine)

add_executable(RacingUnitedLauncher
    Engine/Launcher/launcher_main.cpp
    ThirdParty/GLAD/src/glad.c
    $<TARGET_OBJECTS:racingunited_imgui>
)
configure_racingunited_target(RacingUnitedLauncher)

set_target_properties(HeritageEngine RacingUnitedLauncher PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
