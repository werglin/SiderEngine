#include <cstdlib>
#include <exception>
#include <iostream>

#include "App.hpp"

int main() {
    try {
        // App's destructor runs automatically when this scope ends - including when run()
        // throws - so every Vulkan object is released on both the success and failure path.
        sider::App app;
        app.run();
    } catch (const std::exception& e) {
        // Anything that went wrong during setup surfaces here as a readable message
        // instead of a crash with no explanation.
        std::cerr << "fatal: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
