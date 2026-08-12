#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

// Forward declaration instead of #include <GLFW/glfw3.h>: the header only needs to know
// that GLFWwindow is *some* type because we store a pointer to it. Keeping the GLFW header
// out of here means files that include App.hpp don't drag GLFW in with them.
struct GLFWwindow;

namespace sider {

// Owns the window and every Vulkan object needed before we can start drawing.
//
// The Vulkan objects below form a chain of dependencies, and each one is created from the
// one above it:
//
//   VkInstance        the Vulkan library itself, per-application
//     +-- VkSurfaceKHR      the OS window we present into (created via GLFW)
//     +-- VkPhysicalDevice  a GPU in the machine (queried, not created - so not destroyed)
//           +-- VkDevice          our logical connection to that GPU
//                 +-- VkQueue     where we submit work (owned by the VkDevice)
//
// Destruction has to run in the exact reverse order, which is what ~App() does.
class App {
public:
    App();
    ~App();

    // Vulkan handles are raw pointers/integers with manual lifetime management. If this
    // class were copied, both copies would destroy the same handles in their destructor -
    // a double free. Deleting the copy operations makes that a compile error instead.
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // Sets everything up and blocks until the user closes the window.
    void run();

private:
    void initWindow();
    void initVulkan();
    void mainLoop();

    // Called by initVulkan() in this exact order - each step depends on the previous ones.
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();

    static constexpr uint32_t kWidth = 1280;
    static constexpr uint32_t kHeight = 720;

    GLFWwindow* m_window = nullptr;

    // Every handle starts as VK_NULL_HANDLE (Vulkan's "no object" value) so the destructor
    // can tell what was actually created - if construction throws halfway through, only the
    // objects that exist get destroyed.

    // The loaded Vulkan library plus the global state we asked for (extensions, layers).
    VkInstance m_instance = VK_NULL_HANDLE;
    // Validation-layer callback handle. Only created in Debug builds.
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    // Vulkan's handle to the OS window's drawable area.
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    // The GPU we picked. Owned by the instance - we never destroy this one.
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    // Our logical connection to that GPU: the handle almost every later Vulkan call needs.
    VkDevice m_device = VK_NULL_HANDLE;

    // Queues are where commands get submitted. They are owned by the VkDevice, so they are
    // retrieved (vkGetDeviceQueue) rather than created, and are never destroyed.
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;  // accepts draw/compute/transfer commands
    VkQueue m_presentQueue = VK_NULL_HANDLE;   // can push finished images to the surface

    // Indices of the queue families the two queues above came from. On most desktop GPUs
    // these are the same family, but the spec does not guarantee it, so we track both.
    uint32_t m_graphicsFamily = 0;
    uint32_t m_presentFamily = 0;
};

}  // namespace sider
