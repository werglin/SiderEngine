cmake_minimum_required(VERSION 3.21)

project(SiderEngine
    VERSION 0.1.0
    DESCRIPTION "Vulkan game engine"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

find_package(Vulkan QUIET)
if(NOT Vulkan_FOUND)
    message(FATAL_ERROR
        "Vulkan SDK not found.\n"
        "Install it from https://vulkan.lunarg.com/sdk/home#windows, then restart your\n"
        "editor/terminal so the VULKAN_SDK environment variable is picked up.\n"
        "Currently VULKAN_SDK=\"$ENV{VULKAN_SDK}\"")
endif()
message(STATUS "Vulkan ${Vulkan_VERSION} at ${Vulkan_INCLUDE_DIR}")

include(FetchContent)

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

add_executable(SiderEngine
    src/main.cpp
    src/App.cpp)

target_include_directories(SiderEngine PRIVATE src)

target_link_libraries(SiderEngine PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm)

target_compile_definitions(SiderEngine PRIVATE
    GLFW_INCLUDE_VULKAN
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    $<$<CONFIG:Debug>:SIDER_DEBUG>)

if(MSVC)
    target_compile_options(SiderEngine PRIVATE /W4)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTY VS_STARTUP_PROJECT SiderEngine)
else()
    target_compile_options(SiderEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()
