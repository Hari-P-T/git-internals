#pragma once

#include "config.hpp"

namespace mini_git {

void print_help();
int run_cli(const Config& config, int argc, char** argv);

}  // namespace mini_git
