#include <iostream>

#include "commands.hpp"
#include "config.hpp"

int main(int argc, char** argv) {
    try {
        const auto config = mini_git::load_config();
        return mini_git::run_cli(config, argc, argv);
    } catch (const std::exception& error) {
        std::cout << "Error: " << error.what() << '\n';
        return 0;
    }
}
