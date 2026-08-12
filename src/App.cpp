#include "App.hpp"

// CMake defines GLFW_INCLUDE_VULKAN for this target, which makes this header pull in
// <vulkan/vulkan.h> itself and expose the Vulkan-specific GLFW functions we use below.
#include <GLFW/glfw3.h>

#include <cstring>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <vector>

namespace sider {

// Anonymous namespace: everything in here has internal linkage, i.e. it is private to this
// .cpp file and cannot collide with symbols in other translation units.
namespace {

// SIDER_DEBUG is defined by CMake only for Debug builds (see CMakeLists.txt). Validation
// layers catch nearly every Vulkan mistake but cost real performance, so they are a
// debug-only tool. Using a constexpr bool rather than #ifdef around the call sites means
// the validation code still gets compiled and type-checked in Release builds.
#ifdef SIDER_DEBUG
constexpr bool kEnableValidation = true;
#else
constexpr bool kEnableValidation = false;
#endif

// The single meta-layer shipped by the Vulkan SDK. It bundles all the individual
// validation checks (object lifetimes, parameter validity, thread safety, ...).
const char* const kValidationLayers[] = {"VK_LAYER_KHRONOS_validation"};

// Extensions the *GPU* must support. Presenting to a window is not core Vulkan - it is an
// extension, because Vulkan also runs headless (compute servers, offscreen rendering).
const char* const kDeviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// Called by the validation layers whenever they have something to report.
// VKAPI_ATTR/VKAPI_CALL pin the calling convention so the driver can call back into us.
VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
              VkDebugUtilsMessageTypeFlagsEXT,  // unnamed: we don't filter by message type
              const VkDebugUtilsMessengerCallbackDataEXT* data,
              void*) {  // unnamed: the pUserData pointer we passed at creation time (none here)
    // The severity values increase in order (verbose < info < warning < error), so this
    // comparison filters out the noisy verbose/info spam and keeps real problems.
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[vulkan] " << data->pMessage << '\n';
    }
    // VK_FALSE = "do not abort the call that triggered this message". Returning VK_TRUE is
    // reserved for testing the validation layers themselves.
    return VK_FALSE;
}

// Filled in twice (once for the standalone messenger, once chained into instance creation),
// so the setup lives in one place.
void fillDebugMessengerInfo(VkDebugUtilsMessengerCreateInfoEXT& info) {
    info = {};
    // Almost every Vulkan struct starts with an sType tag identifying its own type. That is
    // how the driver walks the optional pNext chain of extension structs.
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    // Which severities invoke the callback at all. We subscribe broadly and filter inside
    // debugCallback, so raising the log level later is a one-line change.
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    // General = loader/driver info, Validation = spec violations, Performance = legal but
    // suboptimal usage.
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debugCallback;
}

// Asks the loader whether the validation layer is actually installed. It ships with the
// Vulkan SDK, so this fails on machines that only have a GPU driver.
bool validationLayersSupported() {
    // The two-call idiom used all over Vulkan: call once with a null data pointer to learn
    // the count, size a buffer, then call again to fill it.
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> available(count);
    vkEnumerateInstanceLayerProperties(&count, available.data());

    // Layer names are fixed-size char arrays, so they need strcmp, not ==.
    for (const char* wanted : kValidationLayers) {
        bool found = false;
        for (const auto& layer : available) {
            if (std::strcmp(wanted, layer.layerName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

// A GPU exposes several queue families, each supporting a subset of operations. We need one
// that can draw and one that can present to our surface - often, but not always, the same.
struct QueueFamilies {
    // optional<> distinguishes "family 0" from "no family found". Index 0 is a perfectly
    // valid answer, so a sentinel value like -1 would be more error-prone.
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;

    bool complete() const { return graphics.has_value() && present.has_value(); }
};

QueueFamilies findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    QueueFamilies result;
    for (uint32_t i = 0; i < count; ++i) {
        // queueFlags is a bitmask of capabilities (graphics, compute, transfer, sparse).
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            result.graphics = i;
        }

        // Presentation support is not a queue flag: it depends on the surface, because the
        // window may live on a different GPU than the one we are querying.
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            result.present = i;
        }

        if (result.complete()) {
            break;
        }
    }
    return result;
}

// A GPU can be Vulkan-capable without supporting presentation to a window (compute-only
// accelerators, some virtualised devices), so the swapchain extension has to be checked.
bool supportsDeviceExtensions(VkPhysicalDevice device) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    for (const char* wanted : kDeviceExtensions) {
        bool found = false;
        for (const auto& ext : available) {
            if (std::strcmp(wanted, ext.extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

// Vulkan reports failure through return codes rather than exceptions. This wrapper turns a
// non-success code into an exception so call sites stay readable and no error is ignored.
void check(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(what) + " failed (VkResult " +
                                 std::to_string(static_cast<int>(result)) + ")");
    }
}

}  // namespace

App::App() = default;

// Destruction order is the reverse of creation: a child object must never outlive the
// parent it was created from. Each null check means a partially-constructed App (one that
// threw during initVulkan) still cleans up correctly.
App::~App() {
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
    }
    if (m_surface != VK_NULL_HANDLE) {
        // Created by GLFW, but destroyed with the plain Vulkan call - GLFW has no
        // glfwDestroyWindowSurface. The surface belongs to the instance.
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }
    if (m_debugMessenger != VK_NULL_HANDLE) {
        // Extension functions are not exported by the loader library, so they have to be
        // looked up at runtime and cast to the matching PFN_ function-pointer type.
        auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy != nullptr) {
            destroy(m_instance, m_debugMessenger, nullptr);
        }
    }
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
    }
    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
    }
    // Safe even if glfwInit() failed or was never reached.
    glfwTerminate();
}

void App::run() {
    initWindow();
    initVulkan();
    mainLoop();
}

void App::initWindow() {
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("glfwInit failed");
    }
    // True when a Vulkan loader is present. Catching it here gives a clear message instead
    // of a confusing failure deeper in instance creation.
    if (glfwVulkanSupported() != GLFW_TRUE) {
        throw std::runtime_error("no Vulkan loader found on this system");
    }

    // GLFW defaults to creating an OpenGL context. NO_API tells it to skip that - Vulkan
    // manages its own context through the instance and device.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(static_cast<int>(kWidth), static_cast<int>(kHeight), "SiderEngine",
                                nullptr, nullptr);
    if (m_window == nullptr) {
        throw std::runtime_error("glfwCreateWindow failed");
    }
}

void App::initVulkan() {
    createInstance();
    setupDebugMessenger();
    // The surface must exist before device selection: whether a GPU can present at all is
    // a question about a specific surface.
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
}

void App::createInstance() {
    if (kEnableValidation && !validationLayersSupported()) {
        throw std::runtime_error(
            "validation layers requested but not available - install the Vulkan SDK");
    }

    // Purely informational: drivers use these names to apply per-application workarounds.
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "SiderEngine";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "SiderEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    // The highest Vulkan version we promise to use correctly. Asking for 1.3 requires a
    // reasonably recent driver; lower this if instance creation ever fails on old hardware.
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // GLFW knows which platform-specific extensions it needs to create a surface later
    // (VK_KHR_surface plus VK_KHR_win32_surface on Windows), so we ask instead of hardcoding.
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensions == nullptr) {
        throw std::runtime_error("glfwGetRequiredInstanceExtensions failed");
    }
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (kEnableValidation) {
        // The extension that provides the debug messenger used above.
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Declared outside the if so it stays alive until vkCreateInstance reads it below.
    VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
    if (kEnableValidation) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(std::size(kValidationLayers));
        createInfo.ppEnabledLayerNames = kValidationLayers;

        // Chicken-and-egg problem: the standalone messenger needs an instance, so it cannot
        // report errors in vkCreateInstance/vkDestroyInstance themselves. Hanging this
        // struct off pNext creates a temporary messenger that covers exactly those two calls.
        fillDebugMessengerInfo(debugInfo);
        createInfo.pNext = &debugInfo;
    }

    check(vkCreateInstance(&createInfo, nullptr, &m_instance), "vkCreateInstance");
}

// The messenger that stays alive for the rest of the program's lifetime.
void App::setupDebugMessenger() {
    if (!kEnableValidation) {
        return;
    }

    auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
    if (create == nullptr) {
        throw std::runtime_error("vkCreateDebugUtilsMessengerEXT not available");
    }

    VkDebugUtilsMessengerCreateInfoEXT info{};
    fillDebugMessengerInfo(info);
    check(create(m_instance, &info, nullptr, &m_debugMessenger), "vkCreateDebugUtilsMessengerEXT");
}

void App::createSurface() {
    // GLFW hides the platform differences here. Doing this by hand on Windows would mean
    // filling a VkWin32SurfaceCreateInfoKHR with the HWND and HINSTANCE.
    check(glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface),
          "glfwCreateWindowSurface");
}

void App::pickPhysicalDevice() {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) {
        throw std::runtime_error("no Vulkan-capable GPU found");
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    // Prefer a discrete GPU, but accept any device that meets the requirements. Laptops
    // typically report both an integrated and a discrete GPU here.
    int bestScore = -1;
    for (VkPhysicalDevice device : devices) {
        // Hard requirements - a device failing either of these is unusable to us.
        const QueueFamilies families = findQueueFamilies(device, m_surface);
        if (!families.complete() || !supportsDeviceExtensions(device)) {
            continue;
        }

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        // Soft preference. Extend this later with VRAM size, feature support, limits, etc.
        const int score = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 1000 : 1;

        if (score > bestScore) {
            bestScore = score;
            m_physicalDevice = device;
            // Cache the indices so createLogicalDevice() doesn't have to query again.
            m_graphicsFamily = *families.graphics;
            m_presentFamily = *families.present;
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("no suitable GPU found");
    }

    // Printed as a sanity check: it confirms the loader found the right GPU.
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
    std::cout << "GPU: " << props.deviceName << '\n'
              << "Driver Vulkan API: " << VK_API_VERSION_MAJOR(props.apiVersion) << '.'
              << VK_API_VERSION_MINOR(props.apiVersion) << '.'
              << VK_API_VERSION_PATCH(props.apiVersion) << '\n';
}

void App::createLogicalDevice() {
    // Relative scheduling priority within the family, 0.0 to 1.0. With a single queue the
    // value has no practical effect, but the pointer must be valid.
    const float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;

    // Request one queue per *distinct* family. When graphics and present resolve to the
    // same family - the common case - asking for it twice is invalid usage.
    for (uint32_t family : {m_graphicsFamily, m_presentFamily}) {
        bool alreadyAdded = false;
        for (const auto& info : queueInfos) {
            if (info.queueFamilyIndex == family) {
                alreadyAdded = true;
                break;
            }
        }
        if (alreadyAdded) {
            continue;
        }

        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &priority;
        queueInfos.push_back(queueInfo);
    }

    // Optional hardware features (anisotropic filtering, geometry shaders, ...) must be
    // opted into explicitly. All false for now; enable them here as the engine needs them.
    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.pEnabledFeatures = &features;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(std::size(kDeviceExtensions));
    createInfo.ppEnabledExtensionNames = kDeviceExtensions;
    // Note: device-level layers are deprecated. The instance layers apply to the device too.

    check(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device), "vkCreateDevice");

    // Queues are created together with the device; these calls just fetch the handles.
    // If both families are the same index, both variables end up holding the same queue.
    vkGetDeviceQueue(m_device, m_graphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_presentFamily, 0, &m_presentQueue);
}

void App::mainLoop() {
    while (glfwWindowShouldClose(m_window) == GLFW_FALSE) {
        // Processes OS events (input, resize, close). Without this the window would be
        // reported as unresponsive by Windows. Drawing will happen here later.
        glfwPollEvents();
    }
    // The GPU runs asynchronously: when the loop exits it may still be executing work that
    // references objects we are about to destroy. This blocks until it has drained.
    vkDeviceWaitIdle(m_device);
}

}  // namespace sider
