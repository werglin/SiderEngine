cmake_minimum_required(VERSION 3.21)

project(SiderEngine
    VERSION 0.1.0
    DESCRIPTION "Vulkan game engine"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # error out instead of silently falling back to C++17
set(CMAKE_CXX_EXTENSIONS OFF)        # -std=c++20, not -std=gnu++20: keeps the code portable
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # feeds compile_commands.json to the C++ IntelliSense

# Only matters when configuring by hand without a preset; the presets set this explicitly.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# CMake's built-in FindVulkan module locates the SDK through the VULKAN_SDK env variable.
# QUIET + a manual check just to replace the default error with an actionable one.
find_package(Vulkan QUIET)
if(NOT Vulkan_FOUND)
    message(FATAL_ERROR
        "Vulkan SDK not found.\n"
        "Install it from https://vulkan.lunarg.com/sdk/home#windows, then restart your\n"
        "editor/terminal so the VULKAN_SDK environment variable is picked up.\n"
        "Currently VULKAN_SDK=\"$ENV{VULKAN_SDK}\"")
endif()
message(STATUS "Vulkan ${Vulkan_VERSION} at ${Vulkan_INCLUDE_DIR}")

# FetchContent clones these at configure time into build/_deps and builds them as part of
# this project - no manual dependency install, and the versions are pinned in-repo.
include(FetchContent)

# GLFW reads these options when it is added below, so they have to be set beforehand.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(glfw glm)

# Every new .cpp file has to be listed here. (Globbing is possible but is discouraged:
# CMake would not notice a newly added file until you reconfigure by hand.)
add_executable(SiderEngine
    src/main.cpp
    src/App.cpp)

# Lets sources use #include "App.hpp" regardless of how deep they sit in src/.
target_include_directories(SiderEngine PRIVATE src)

# These imported targets carry their own include dirs and link flags, so nothing else has
# to be spelled out. PRIVATE = used to build this target, not propagated to dependents.
target_link_libraries(SiderEngine PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm)

target_compile_definitions(SiderEngine PRIVATE
    GLFW_INCLUDE_VULKAN            # makes <GLFW/glfw3.h> pull in the Vulkan header
    GLM_FORCE_RADIANS              # no degree-based overloads; angles are always radians
    GLM_FORCE_DEPTH_ZERO_TO_ONE    # Vulkan clip space is z in [0,1], OpenGL's is [-1,1]
    $<$<CONFIG:Debug>:SIDER_DEBUG>)  # generator expression: only defined in Debug builds

if(MSVC)
    target_compile_options(SiderEngine PRIVATE /W4)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTY VS_STARTUP_PROJECT SiderEngine)
else()
    target_compile_options(SiderEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()
