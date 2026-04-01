#include "repo_config.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace mini_git {

namespace {

bool is_valid_remote_name(const std::string& name) {
    if (name.empty()) {
        return false;
    }

    return std::all_of(name.begin(), name.end(), [](unsigned char ch) {
        return std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.';
    });
}

std::vector<RemoteConfig> parse_remotes(const std::string& config_text) {
    std::vector<RemoteConfig> remotes;
    std::istringstream stream(config_text);

    RemoteConfig current_remote;
    bool in_remote_section = false;
    std::string line;

    auto flush_remote = [&]() {
        if (in_remote_section) {
            remotes.push_back(current_remote);
        }
        current_remote = RemoteConfig{};
        in_remote_section = false;
    };

    while (std::getline(stream, line)) {
        line = trim_copy(line);
        if (line.empty()) {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            flush_remote();

            const std::string section = line.substr(1, line.size() - 2);
            const std::string prefix = "remote \"";
            if (section.rfind(prefix, 0) == 0 && section.size() > prefix.size() + 1 && section.back() == '"') {
                current_remote.name = section.substr(prefix.size(), section.size() - prefix.size() - 1);
                in_remote_section = true;
            }
            continue;
        }

        if (!in_remote_section) {
            continue;
        }

        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = trim_copy(line.substr(0, separator));
        const std::string value = trim_copy(line.substr(separator + 1));

        if (key == "url") {
            current_remote.url = value;
        } else if (key == "repo") {
            current_remote.repo = value;
        }
    }

    flush_remote();
    return remotes;
}

void write_config_file(const fs::path& path, const std::vector<RemoteConfig>& remotes) {
    fs::create_directories(path.parent_path());

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Could not write repo config: " + path.string());
    }

    output
        << "[core]\n"
        << "    repositoryformatversion = 0\n"
        << "    filemode = true\n"
        << "    bare = false\n";

    for (const auto& remote : remotes) {
        output
            << "\n[remote \"" << remote.name << "\"]\n"
            << "    url = " << remote.url << '\n'
            << "    repo = " << remote.repo << '\n';
    }
}

}  // namespace

RepoConfig::RepoConfig(const Config& config) : config_(config) {}

std::vector<RemoteConfig> RepoConfig::list_remotes() const {
    const fs::path path = config_.git_dir / "config";
    if (!fs::exists(path)) {
        return {};
    }

    return parse_remotes(read_text_file(path));
}

std::optional<RemoteConfig> RepoConfig::get_remote(const std::string& name) const {
    const auto remotes = list_remotes();
    const auto iterator = std::find_if(remotes.begin(), remotes.end(), [&](const auto& remote) {
        return remote.name == name;
    });

    if (iterator == remotes.end()) {
        return std::nullopt;
    }

    return *iterator;
}

void RepoConfig::add_remote(const RemoteConfig& remote) const {
    if (!is_valid_remote_name(remote.name)) {
        throw std::runtime_error("Invalid remote name: " + remote.name);
    }
    if (remote.url.empty()) {
        throw std::runtime_error("Remote URL cannot be empty");
    }
    if (remote.repo.empty()) {
        throw std::runtime_error("Remote repo name cannot be empty");
    }

    auto remotes = list_remotes();
    const auto duplicate = std::find_if(remotes.begin(), remotes.end(), [&](const auto& existing) {
        return existing.name == remote.name;
    });

    if (duplicate != remotes.end()) {
        throw std::runtime_error("Remote already exists: " + remote.name);
    }

    remotes.push_back(remote);
    write_config_file(config_.git_dir / "config", remotes);
}

}  // namespace mini_git
