#include "App.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

int main() {
    try {
        sider::App app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
