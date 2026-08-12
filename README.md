# SiderEngine

A Vulkan game engine, built with C++20 / CMake.

## Prerequisites

| Tool | Notes |
| --- | --- |
| [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows) | Required. Installs the headers, `vulkan-1` loader, validation layers and `glslc`. Sets `VULKAN_SDK` for you — restart VS Code afterwards. |
| CMake ≥ 3.21 | Already installed (3.30.4). |
| MinGW-w64 GCC | Already installed at `C:\mingw64` (GCC 14.2). |
| Git | Needed at configure time: GLFW and GLM are fetched automatically. |

GLFW 3.4 and GLM 1.0.1 are pulled in by CMake `FetchContent` on the first configure,
so no manual dependency setup is needed.

## Building

From the project root:

```bash
cmake --preset mingw-debug
```

```bash
cmake --build --preset mingw-debug
```

The binary lands in `build/mingw-debug/SiderEngine.exe`.

For an optimized build, swap `mingw-debug` for `mingw-release` — that also disables
the validation layers.

## In VS Code

Install the recommended extensions (VS Code will prompt), then:

- **Ctrl+Shift+B** — build
- **F5** — build and debug with gdb
- The CMake Tools status bar picks up the presets from `CMakePresets.json`

## Current state

The bootstrap brings up the core Vulkan objects and holds a window open:

- GLFW window with no client API
- `VkInstance` with the GLFW-required extensions
- `VK_LAYER_KHRONOS_validation` + debug messenger (Debug builds only)
- Window surface
- Physical device selection (prefers a discrete GPU with graphics + present queues and swapchain support)
- Logical device with graphics and present queues

Next up: swapchain, render pass, graphics pipeline.

## Layout

```
src/            engine sources
  main.cpp      entry point
  App.hpp/.cpp  window + Vulkan bootstrap
CMakeLists.txt  build definition
CMakePresets.json
.vscode/        editor, build task and debugger config
```
