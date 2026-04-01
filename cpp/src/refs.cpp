#include "refs.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "config.hpp"

namespace fs = std::filesystem;

namespace mini_git {

namespace {

void write_text_exact(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Could not write file: " + path.string());
    }
    output << content;
}

}  // namespace

Refs::Refs(const Config& config) : config_(config) {}

std::string Refs::read_head_ref() const {
    const std::string data = trim_copy(read_text_file(config_.head_file));
    const std::size_t separator = data.find(' ');
    if (separator == std::string::npos) {
        throw std::runtime_error("Invalid HEAD file");
    }
    return data.substr(separator + 1);
}

std::string Refs::current_branch() const {
    const std::string ref = read_head_ref();
    const std::string prefix = "refs/heads/";
    if (ref.rfind(prefix, 0) == 0) {
        return ref.substr(prefix.size());
    }
    return ref;
}

fs::path Refs::branch_path(const std::string& name) const {
    return config_.heads_dir / name;
}

bool Refs::branch_exists(const std::string& name) const {
    return fs::exists(branch_path(name));
}

std::string Refs::read_branch(const std::string& name) const {
    return trim_copy(read_text_file(branch_path(name)));
}

void Refs::write_branch(const std::string& name, const std::string& commit) const {
    write_text_exact(branch_path(name), commit);
}

void Refs::set_head_branch(const std::string& name) const {
    write_text_exact(config_.head_file, "ref: refs/heads/" + name + "\n");
}

bool Refs::ref_exists(const std::string& ref) const {
    return fs::exists(config_.git_dir / ref);
}

std::string Refs::read_ref_value(const std::string& ref) const {
    return trim_copy(read_text_file(config_.git_dir / ref));
}

}  // namespace mini_git
