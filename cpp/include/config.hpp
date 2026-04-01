#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace mini_git {

struct Config {
    std::filesystem::path project_root;
    std::filesystem::path config_path;
    std::filesystem::path working_dir;
    std::filesystem::path git_dir;
    std::filesystem::path objects_dir;
    std::filesystem::path refs_dir;
    std::filesystem::path heads_dir;
    std::filesystem::path tags_dir;
    std::filesystem::path index_file;
    std::filesystem::path head_file;
    std::filesystem::path stash_file;
};

Config load_config(const std::filesystem::path& start_path = std::filesystem::current_path());
std::int64_t current_timestamp();
std::string read_text_file(const std::filesystem::path& path);
std::string trim_copy(const std::string& value);
std::string relative_repo_path(const std::filesystem::path& base, const std::filesystem::path& target);

}  // namespace mini_git
