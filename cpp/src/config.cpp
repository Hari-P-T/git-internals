#include "config.hpp"

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace fs = std::filesystem;

namespace mini_git {

namespace {

std::optional<std::pair<fs::path, fs::path>> find_project_root(const fs::path& start_path) {
    fs::path current = fs::absolute(start_path);
    if (fs::is_regular_file(current)) {
        current = current.parent_path();
    }

    while (true) {
        fs::path src_candidate = current / "src" / "constants" / "constants";
        if (fs::exists(src_candidate)) {
            return std::make_pair(current, src_candidate);
        }

        fs::path root_candidate = current / "constants" / "constants";
        if (fs::exists(root_candidate)) {
            return std::make_pair(current, root_candidate);
        }

        if (current == current.root_path()) {
            break;
        }
        current = current.parent_path();
    }

    return std::nullopt;
}

std::unordered_map<std::string, std::string> load_env(const fs::path& env_path) {
    std::ifstream input(env_path);
    if (!input) {
        throw std::runtime_error("Could not open config file: " + env_path.string());
    }

    std::unordered_map<std::string, std::string> env;
    std::string line;
    while (std::getline(input, line)) {
        line = trim_copy(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        env[trim_copy(line.substr(0, separator))] = trim_copy(line.substr(separator + 1));
    }
    return env;
}

const std::string& require_value(
    const std::unordered_map<std::string, std::string>& env,
    const std::string& key
) {
    const auto iterator = env.find(key);
    if (iterator == env.end()) {
        throw std::runtime_error("Missing config key: " + key);
    }
    return iterator->second;
}

}  // namespace

Config load_config(const fs::path& start_path) {
    const auto discovered = find_project_root(start_path);
    if (!discovered) {
        throw std::runtime_error(
            "Could not locate src/constants/constants or constants/constants from " +
            fs::absolute(start_path).string()
        );
    }

    const auto& [project_root, config_path] = *discovered;
    const auto env = load_env(config_path);

    Config config;
    config.project_root = project_root;
    config.config_path = config_path;
    config.working_dir = project_root / require_value(env, "WORKING_DIR");
    config.git_dir = config.working_dir / require_value(env, "GIT_DIR");
    config.objects_dir = config.git_dir / require_value(env, "OBJECTS_DIR");
    config.refs_dir = config.git_dir / require_value(env, "REFS_DIR");
    config.heads_dir = config.refs_dir / require_value(env, "HEADS_DIR");
    config.tags_dir = config.refs_dir / require_value(env, "TAGS_DIR");
    config.index_file = config.git_dir / require_value(env, "INDEX_FILE");
    config.head_file = config.git_dir / require_value(env, "HEAD_FILE");
    config.stash_file = config.refs_dir / require_value(env, "STASH_FILE");
    return config;
}

std::int64_t current_timestamp() {
    if (const char* fixed_time = std::getenv("MINI_GIT_FIXED_TIME")) {
        return std::stoll(fixed_time);
    }
    return static_cast<std::int64_t>(std::time(nullptr));
}

std::string read_text_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string trim_copy(const std::string& value) {
    const std::size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string relative_repo_path(const fs::path& base, const fs::path& target) {
    return fs::relative(target, base).generic_string();
}

}  // namespace mini_git
